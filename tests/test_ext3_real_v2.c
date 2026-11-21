/* SPDX-License-Identifier: GPL-3.0-or-later
 * Direct production Common V2 EXT3 namespace acceptance on real mkfs.ext3
 * media. Neither BOOT nor a Linux mount is used to service file I/O.
 */
#define _POSIX_C_SOURCE 200809L
#include <infiltratr/filesystem_module_v2.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

extern const IfsFsModuleApiV2 *infiltrator_filesystem_api_v2(void);
typedef struct {
    int fd;
    uint32_t sector;
    uint64_t bytes;
    unsigned reads;
    unsigned fail_on_read;
} TestDisk;
static int read_blocks(void *context,uint64_t offset,void *out,uint32_t n)
{
    TestDisk *d=context;
    if (!d || !out || !n || offset%d->sector || n%d->sector ||
        n>d->sector || offset>d->bytes || n>d->bytes-offset ||
        offset>INT64_MAX) return -1;
    ++d->reads;
    if ((d->fail_on_read && d->reads==d->fail_on_read) ||
        pread(d->fd,out,n,(off_t)offset)!=(ssize_t)n) return -1;
    return 0;
}
static void *allocate(void *ctx,size_t n){(void)ctx;return malloc(n);}
static void release(void *ctx,void *p){(void)ctx;free(p);}
static int forbidden_write(void *ctx,uint64_t at,const void *data,uint32_t n)
{ (void)ctx;(void)at;(void)data;(void)n;return -1; }
static int forbidden_flush(void *ctx)
{ (void)ctx;return -1; }
#define ENSURE(x) do { if(!(x)) { fprintf(stderr,"EXT3 REAL V2 %u: %s\n",\
(unsigned)__LINE__,#x);return 1; } } while(0)

static IfsFsHostRuntimeV2 host(TestDisk *d)
{
    IfsFsHostRuntimeV2 h={0};
    h.abi_version=IFS_FS_HOST_ABI_V2;
    h.structure_bytes=sizeof(h);
    h.capabilities=IFS_FS_HOST_READ_ONLY;
    h.context=d;
    h.volume_bytes=d->bytes;
    h.logical_sector_bytes=d->sector;
    h.physical_sector_bytes=d->sector;
    h.maximum_transfer_bytes=d->sector;
    h.read_at=read_blocks;
    h.allocate=allocate;
    h.release=release;
    return h;
}
int main(int argc,char **argv)
{
    ENSURE(argc==2);
    const IfsFsModuleApiV2 *api=infiltrator_filesystem_api_v2();
    ENSURE(ifs_fs_api_v2_valid(api,"ext3"));
    const IfsFsCoreOpsV2 *core=
        (const IfsFsCoreOpsV2 *)ifs_fs_group_v2(api,IFS_FS_GROUP_CORE);
    const IfsFsVolumeOpsV2 *volume=
        (const IfsFsVolumeOpsV2 *)ifs_fs_group_v2(api,IFS_FS_GROUP_VOLUME);
    ENSURE(core && volume && core->probe && core->mount && core->unmount &&
           core->stat && core->directory_entry && core->read &&
           !core->write && volume->query);
    ENSURE(core->unmount((void *)(uintptr_t)1u)==IFS_FS_INVALID);
    struct stat st;
    ENSURE(stat(argv[1],&st)==0 && st.st_size>0);
    TestDisk d[2]={{0}};
    d[0].sector=512u;d[1].sector=4096u;
    IfsFsHostRuntimeV2 h[2];
    void *handles[2]={NULL,NULL};
    const char expected[]="EXT3 Common V2 real native namespace";
    for (unsigned i=0;i<2u;++i) {
        d[i].fd=open(argv[1],O_RDONLY|O_CLOEXEC|O_NOFOLLOW);
        ENSURE(d[i].fd>=0);
        d[i].bytes=(uint64_t)st.st_size;
        h[i]=host(&d[i]);
        ENSURE(ifs_fs_host_v2_valid(&h[i]));
        /* The shipped read-only V2 provider must refuse a valid writable
         * host without touching filesystem data or returning a mount. */
        IfsFsHostRuntimeV2 writable=h[i];
        writable.capabilities=IFS_FS_HOST_EXCLUSIVE|IFS_FS_HOST_DURABLE;
        writable.write_at=forbidden_write;
        writable.flush=forbidden_flush;
        ENSURE(ifs_fs_host_v2_valid(&writable));
        IfsFsVolumeInfoV2 denied_info={.structure_bytes=sizeof(denied_info)};
        void *denied=(void *)(uintptr_t)1u;
        unsigned before=d[i].reads;
        ENSURE(core->mount(&writable,&denied,&denied_info)==IFS_FS_ACCESS &&
               denied==NULL && d[i].reads==before);
        uint32_t variant=123u;
        d[i].fail_on_read=1u;
        ENSURE(core->probe(&h[i],&variant)==IFS_FS_IO_ERROR && variant==0u);
        d[i].fail_on_read=0u;d[i].reads=0u;
        ENSURE(core->probe(&h[i],&variant)==IFS_FS_OK && variant==3u);
        IfsFsVolumeInfoV2 info={.structure_bytes=sizeof(info)};
        ENSURE(core->mount(&h[i],&handles[i],&info)==IFS_FS_OK &&
               handles[i] && info.variant==3u && info.block_bytes==1024u);
        char name[256]={0};
        IfsFsEntryV2 entry={.structure_bytes=sizeof(entry),.name=name,
                            .name_capacity=sizeof(name)};
        ENSURE(core->stat(handles[i],"/boot/kernel.bin",&entry)==IFS_FS_OK &&
               entry.size_bytes==sizeof(expected)-1u);
        /* Exercise the public Common V2 STAT/LIST output contract with
         * genuine EXT3 media; a short name buffer must remain untouched. */
        {
            char narrow_name[5] = {'X','X','X','X','X'};
            const char untouched[5] = {'X','X','X','X','X'};
            IfsFsEntryV2 narrow={.structure_bytes=sizeof(narrow),
                                 .name=narrow_name,
                                 .name_capacity=sizeof(narrow_name)};
            ENSURE(core->stat(handles[i],"/boot/kernel.bin",&narrow)==IFS_FS_NO_SPACE);
            ENSURE(memcmp(narrow_name,untouched,sizeof(narrow_name))==0);
            ENSURE(core->directory_entry(handles[i],"/boot",0u,&narrow)==IFS_FS_NO_SPACE);
            ENSURE(memcmp(narrow_name,untouched,sizeof(narrow_name))==0);
            ENSURE(narrow.name_bytes==0u);
        }
        char actual[sizeof(expected)]={0};
        ENSURE(core->read(handles[i],"/boot/kernel.bin",0u,actual,
                          sizeof(expected)-1u)==IFS_FS_OK &&
               memcmp(actual,expected,sizeof(expected)-1u)==0);
        ENSURE(core->read(handles[i],"/boot/kernel.bin",
                          sizeof(expected)-1u,actual,1u)==IFS_FS_INVALID);
        int found=0;
        for (uint32_t j=0;j<16u;++j) {
            int rc=core->directory_entry(handles[i],"/boot",j,&entry);
            if(rc==IFS_FS_END)break;
            ENSURE(rc==IFS_FS_OK);
            if(strcmp(entry.name,"kernel.bin")==0)found=1;
        }
        ENSURE(found && d[i].reads>0u);
        const IfsFsDefragOpsV2 *defrag=(const IfsFsDefragOpsV2 *)
            ifs_fs_group_v2(api,IFS_FS_GROUP_DEFRAG);
        IfsFsExtentV2 extents[2]={{0}};
        uint32_t count=0u;
        d[i].reads=0u;
        ENSURE(defrag && defrag->map &&
               defrag->map(handles[i],"/boot/kernel.bin",0u,extents,2u,
                           &count)==IFS_FS_OK && count>0u);
        /* The final read of a successful map is a native extent/bitmap
         * validation read, not the earlier pathname traversal. Interrupt
         * precisely that callback on the repeat operation. */
        const unsigned map_reads=d[i].reads;
        ENSURE(map_reads>0u);
        d[i].reads=0u;d[i].fail_on_read=map_reads;
        count=17u;
        ENSURE(defrag->map(handles[i],"/boot/kernel.bin",0u,extents,2u,
                           &count)==IFS_FS_IO_ERROR && count==0u);
        d[i].fail_on_read=0u;
    }
    ENSURE(handles[0]!=handles[1]);
    IfsFsVolumeInfoV2 info={.structure_bytes=sizeof(info)};
    ENSURE(volume->query(handles[1],&info)==IFS_FS_OK);
    const void *expired=handles[0];
    ENSURE(core->unmount(handles[0])==IFS_FS_OK);
    ENSURE(core->unmount((void *)expired)==IFS_FS_INVALID);
    ENSURE(volume->query((void *)expired,&info)==IFS_FS_INVALID);
    char scratch[8]={0};
    ENSURE(core->read((void *)expired,"/boot/kernel.bin",0,scratch,1u)==IFS_FS_INVALID);
    ENSURE(core->read(handles[1],"/boot/kernel.bin",0,scratch,1u)==IFS_FS_OK &&
           scratch[0]=='E');
    void *again=NULL;
    info.structure_bytes=sizeof(info);
    ENSURE(core->mount(&h[0],&again,&info)==IFS_FS_OK &&
           again && again!=expired &&
           core->unmount((void *)expired)==IFS_FS_INVALID);
    ENSURE(core->unmount(again)==IFS_FS_OK);
    ENSURE(core->unmount(handles[1])==IFS_FS_OK);
    for(unsigned i=0;i<2u;++i)ENSURE(close(d[i].fd)==0);
    puts("EXT3-REAL-MEDIA-COMMON-V2-512-4096-AND-STALE-HANDLES-PASS");
    return 0;
}
