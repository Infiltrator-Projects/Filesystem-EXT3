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
    uint64_t fail_at;
} TestDisk;
static int read_blocks(void *context,uint64_t offset,void *out,uint32_t n)
{
    TestDisk *d=context;
    if (!d || !out || !n || offset%d->sector || n%d->sector ||
        n>d->sector || offset>d->bytes || n>d->bytes-offset ||
        offset>INT64_MAX) return -1;
    if (offset==d->fail_at ||
        pread(d->fd,out,n,(off_t)offset)!=(ssize_t)n) return -1;
    ++d->reads;
    return 0;
}
static void *allocate(void *ctx,size_t n){(void)ctx;return malloc(n);}
static void release(void *ctx,void *p){(void)ctx;free(p);}
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
        d[i].fail_at=UINT64_MAX;
        d[i].fd=open(argv[1],O_RDONLY|O_CLOEXEC|O_NOFOLLOW);
        ENSURE(d[i].fd>=0);
        d[i].bytes=(uint64_t)st.st_size;
        h[i]=host(&d[i]);
        ENSURE(ifs_fs_host_v2_valid(&h[i]));
        uint32_t variant=0u;
        ENSURE(core->probe(&h[i],&variant)==IFS_FS_OK && variant==3u);
        IfsFsVolumeInfoV2 info={.structure_bytes=sizeof(info)};
        ENSURE(core->mount(&h[i],&handles[i],&info)==IFS_FS_OK &&
               handles[i] && info.variant==3u && info.block_bytes==1024u);
        char name[256]={0};
        IfsFsEntryV2 entry={.structure_bytes=sizeof(entry),.name=name,
                            .name_capacity=sizeof(name)};
        ENSURE(core->stat(handles[i],"/boot/kernel.bin",&entry)==IFS_FS_OK &&
               entry.size_bytes==sizeof(expected)-1u);
        char actual[sizeof(expected)]={0};
        ENSURE(core->read(handles[i],"/boot/kernel.bin",0u,actual,
                          sizeof(expected)-1u)==IFS_FS_OK &&
               memcmp(actual,expected,sizeof(expected)-1u)==0);
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
        ENSURE(defrag && defrag->map &&
               defrag->map(handles[i],"/boot/kernel.bin",0u,extents,2u,
                           &count)==IFS_FS_OK && count>0u);
        /* On a one-KiB EXT3 filesystem, group descriptor zero starts at
         * byte 2048 and records the allocation bitmap's block address. */
        unsigned char group[4]={0};
        ENSURE(pread(d[i].fd,group,sizeof(group),2048u)==4);
        const uint64_t bitmap=(uint64_t)group[0] |
            ((uint64_t)group[1]<<8) | ((uint64_t)group[2]<<16) |
            ((uint64_t)group[3]<<24);
        ENSURE(bitmap>=3u && bitmap*1024u<d[i].bytes);
        d[i].fail_at=(bitmap*1024u / d[i].sector)*d[i].sector;
        count=17u;
        ENSURE(defrag->map(handles[i],"/boot/kernel.bin",0u,extents,2u,
                           &count)==IFS_FS_IO_ERROR && count==0u);
        d[i].fail_at=UINT64_MAX;
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
