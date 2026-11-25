/* SPDX-License-Identifier: GPL-3.0-or-later */
#define _GNU_SOURCE
#define _FILE_OFFSET_BITS 64
#include <infiltratr/filesystem_module_v2.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/random.h>
#include <errno.h>
extern const IfsFsModuleApiV2 *infiltrator_filesystem_api_v2(void);
typedef struct {int fd;uint64_t bytes;uint32_t sector;unsigned writes,flushes,fail_flush;} Media;
static int read_at(void *ctx,uint64_t at,void *p,uint32_t n)
{
    Media *m=ctx;
    if(at%m->sector || n%m->sector || n>m->sector || at>m->bytes || n>m->bytes-at)return -1;
    return pread(m->fd,p,n,(off_t)at)==(ssize_t)n?0:-1;
}
static int write_at(void *ctx,uint64_t at,const void *p,uint32_t n)
{
    Media *m=ctx;
    if(at%m->sector || n%m->sector || n>m->sector || at>m->bytes || n>m->bytes-at)return -1;
    ++m->writes;
    return pwrite(m->fd,p,n,(off_t)at)==(ssize_t)n?0:-1;
}
static int flush(void *ctx)
{
    Media *m=ctx;
    if(++m->flushes==m->fail_flush)return -1;
    return fsync(m->fd);
}
static void *allocate(void *ctx,size_t n){(void)ctx;return malloc(n);}
static void release(void *ctx,void *p){(void)ctx;free(p);}
static int entropy(void *ctx,void *p,uint32_t n)
{(void)ctx;return getrandom(p,n,0)==(ssize_t)n?0:-1;}
static int fail_entropy(void *ctx,void *p,uint32_t n)
{(void)ctx;(void)p;(void)n;return -1;}
#define CHECK(x) do {if(!(x)){fprintf(stderr,"EXT3 format line %d: %s\n",__LINE__,#x);goto done;}}while(0)
int main(int argc,char **argv)
{
    const IfsFsModuleApiV2 *api=infiltrator_filesystem_api_v2();
    if(!ifs_fs_api_v2_valid(api,"ext3"))return 1;
    const IfsFsCoreOpsV2 *core=(const void*)ifs_fs_group_v2(api,IFS_FS_GROUP_CORE);
    const IfsFsVolumeOpsV2 *vol=(const void*)ifs_fs_group_v2(api,IFS_FS_GROUP_VOLUME);
    const IfsFsDefragOpsV2 *defrag=(const void*)ifs_fs_group_v2(api,IFS_FS_GROUP_DEFRAG);
    if(!core || !vol || !vol->format || !defrag || !defrag->plan ||
       !defrag->execute || !defrag->verify || !defrag->recover ||
       !(defrag->header.capabilities&IFS_FS_DEFRAG_RECOVER) || !defrag->release_plan)return 1;
    char temp[]="/tmp/ifs-ext3-format.XXXXXX";
    if(argc!=1 && argc!=4)return 2;
    Media m={.bytes=64ull*1024u*1024u,.sector=512u};
    if(argc==4) {
        m.bytes=strtoull(argv[2],NULL,10)*1024u*1024u;
        m.sector=(uint32_t)strtoul(argv[3],NULL,10);
        m.fd=open(argv[1],O_CREAT|O_EXCL|O_RDWR|O_NOFOLLOW,0600);
    } else m.fd=mkstemp(temp);
    if(m.fd<0)return 2;
    if(argc==1)unlink(temp);
    int result=1;
    CHECK(ftruncate(m.fd,(off_t)m.bytes)==0);
    IfsFsHostRuntimeV2 h={
        .abi_version=IFS_FS_HOST_ABI_V2,.structure_bytes=sizeof(h),
        .capabilities=IFS_FS_HOST_EXCLUSIVE|IFS_FS_HOST_DURABLE,
        .volume_bytes=m.bytes,.logical_sector_bytes=m.sector,
        .physical_sector_bytes=m.sector,.maximum_transfer_bytes=m.sector,
        .context=&m,.read_at=read_at,.write_at=write_at,.flush=flush,
        .allocate=allocate,.release=release,.random_bytes=entropy
    },ro=h;
    ro.capabilities=IFS_FS_HOST_READ_ONLY;ro.write_at=NULL;ro.flush=NULL;
    CHECK(vol->format(&ro,3u,"EXT3TEST",0u)==IFS_FS_ACCESS);
    CHECK(vol->format(&h,2u,"EXT3TEST",0u)==IFS_FS_UNSUPPORTED);
    CHECK(vol->format(&h,3u,"01234567890123456",0u)==IFS_FS_INVALID);
    CHECK(!m.writes && !m.flushes);
    /* Entropy failure is a preflight rejection, not partial format. */
    h.random_bytes=NULL;
    CHECK(vol->format(&h,3u,"EXT3TEST",0u)==IFS_FS_UNSUPPORTED);
    h.random_bytes=fail_entropy;
    CHECK(vol->format(&h,3u,"EXT3TEST",0u)==IFS_FS_IO_ERROR);
    CHECK(!m.writes && !m.flushes);
    h.random_bytes=entropy;
    if(argc==1) {
        m.fail_flush=2u;
        CHECK(vol->format(&h,3u,"EXT3TEST",0u)==IFS_FS_IO_ERROR);
        uint32_t rejected=0u;
        CHECK(core->probe(&ro,&rejected)!=IFS_FS_OK);
        m.fail_flush=0u;
    }
    CHECK(vol->format(&h,3u,"EXT3TEST",0u)==IFS_FS_OK);
    /* Read the actual superblock UUID and verify its version/variant.
     * A second explicit format of the disposable image must change it. */
    uint8_t uuid[16],second_uuid[16];
    CHECK(pread(m.fd,uuid,sizeof(uuid),1024u+104u)==(ssize_t)sizeof(uuid));
    CHECK((uuid[6]&0xf0u)==0x40u && (uuid[8]&0xc0u)==0x80u);
    if(argc==1) {
        CHECK(vol->format(&h,3u,"EXT3TEST",0u)==IFS_FS_OK);
        CHECK(pread(m.fd,second_uuid,sizeof(second_uuid),1024u+104u)==(ssize_t)sizeof(second_uuid));
        CHECK(memcmp(uuid,second_uuid,sizeof(uuid))!=0);
    }
    uint32_t v=0u;void *session=NULL;
    IfsFsVolumeInfoV2 info={.structure_bytes=sizeof(info)};
    char name[256];
    IfsFsEntryV2 entry={.structure_bytes=sizeof(entry),.name=name,.name_capacity=sizeof(name)};
    CHECK(core->probe(&ro,&v)==IFS_FS_OK && v==3u);
    CHECK(core->mount(&ro,&session,&info)==IFS_FS_OK && session);
    CHECK(info.variant==3u && info.block_bytes==4096u && info.total_bytes==m.bytes);
    CHECK(info.free_bytes>0u && info.free_bytes<info.total_bytes);
    CHECK(core->stat(session,"/",&entry)==IFS_FS_OK && entry.type==IFS_FS_ENTRY_DIRECTORY);
    CHECK(core->directory_entry(session,"/",0u,&entry)==IFS_FS_END);
    CHECK(vol->format(&h,3u,"EXT3TEST",0u)==IFS_FS_BUSY);
    CHECK(core->unmount(session)==IFS_FS_OK);
    CHECK(core->create && core->write);
    if(argc==1) {
        CHECK(core->mount(&h,&session,&info)==IFS_FS_OK && session);
        CHECK(core->create(session,"/V2-A",IFS_FS_ENTRY_FILE)==IFS_FS_OK);
        CHECK(core->create(session,"/V2-B",IFS_FS_ENTRY_FILE)==IFS_FS_OK);
        uint8_t payload[65536],actual[65536];
        for(unsigned i=0;i<4u;++i) {
            memset(payload,(int)(i+1u),sizeof(payload));
            CHECK(core->write(session,"/V2-A",(uint64_t)i*sizeof(payload),payload,sizeof(payload))==IFS_FS_OK);
            memset(payload,(int)(i+9u),sizeof(payload));
            CHECK(core->write(session,"/V2-B",(uint64_t)i*sizeof(payload),payload,sizeof(payload))==IFS_FS_OK);
        }
        CHECK(core->unmount(session)==IFS_FS_OK);
        CHECK(core->mount(&ro,&session,&info)==IFS_FS_OK);
        for(unsigned i=0;i<4u;++i) {
            CHECK(core->read(session,"/V2-A",(uint64_t)i*sizeof(actual),actual,sizeof(actual))==IFS_FS_OK);
            memset(payload,(int)(i+1u),sizeof(payload));
            CHECK(!memcmp(actual,payload,sizeof(actual)));
            CHECK(core->read(session,"/V2-B",(uint64_t)i*sizeof(actual),actual,sizeof(actual))==IFS_FS_OK);
            memset(payload,(int)(i+9u),sizeof(payload));
            CHECK(!memcmp(actual,payload,sizeof(actual)));
        }
        void *plan=NULL;
        uint64_t moved=0;
        const unsigned prior=m.writes;
        uint64_t repaired=UINT64_MAX;
        CHECK(defrag->recover(session,0u,&repaired)==IFS_FS_ACCESS && !repaired);
        CHECK(defrag->plan(session,"/",0u,&plan)==IFS_FS_OK && plan);
        CHECK(defrag->execute(session,plan,&moved)==IFS_FS_ACCESS && !moved);
        CHECK(m.writes==prior);
        defrag->release_plan(session,plan);
        CHECK(core->unmount(session)==IFS_FS_OK);
        CHECK(core->mount(&h,&session,&info)==IFS_FS_OK);
        CHECK(defrag->plan(session,"/",0u,&plan)==IFS_FS_OK && plan);
        CHECK(core->create(session,"/BLOCKED",IFS_FS_ENTRY_FILE)==IFS_FS_BUSY);
        CHECK(defrag->execute(session,plan,&moved)==IFS_FS_OK && moved==8u*sizeof(payload));
        CHECK(defrag->verify(session,plan)==IFS_FS_OK);
        defrag->release_plan(session,plan);
        CHECK(defrag->verify(session,plan)==IFS_FS_INVALID);
        /* Start Growth on an independently recreated fragmented corpus,
         * not the packed result of the preceding operation. */
        CHECK(core->unmount(session)==IFS_FS_OK);
        CHECK(vol->format(&h,3u,"EXT3TEST",0u)==IFS_FS_OK);
        CHECK(core->mount(&h,&session,&info)==IFS_FS_OK);
        CHECK(core->create(session,"/V2-A",IFS_FS_ENTRY_FILE)==IFS_FS_OK);
        CHECK(core->create(session,"/V2-B",IFS_FS_ENTRY_FILE)==IFS_FS_OK);
        for(unsigned i=0;i<4u;++i) {
            memset(payload,(int)(i+1u),sizeof(payload));
            CHECK(core->write(session,"/V2-A",(uint64_t)i*sizeof(payload),payload,sizeof(payload))==IFS_FS_OK);
            memset(payload,(int)(i+9u),sizeof(payload));
            CHECK(core->write(session,"/V2-B",(uint64_t)i*sizeof(payload),payload,sizeof(payload))==IFS_FS_OK);
        }
        IfsFsExtentV2 before[8];uint32_t before_count=0;
        CHECK(defrag->map(session,"/V2-A",0u,before,8u,&before_count)==IFS_FS_OK &&
              before_count>1u);
        CHECK(defrag->map(session,"/V2-B",0u,before,8u,&before_count)==IFS_FS_OK &&
              before_count>1u);
        CHECK(defrag->plan(session,"/",10u,&plan)==IFS_FS_OK && plan);
        CHECK(defrag->execute(session,plan,&moved)==IFS_FS_OK);
        CHECK(defrag->verify(session,plan)==IFS_FS_OK);
        defrag->release_plan(session,plan);
        CHECK(core->unmount(session)==IFS_FS_OK);
        CHECK(core->mount(&ro,&session,&info)==IFS_FS_OK);
        IfsFsExtentV2 extents[4];uint32_t written=0;
        CHECK(defrag->map(session,"/V2-A",0u,extents,4u,&written)==IFS_FS_OK &&
              written==1u && extents[0].length==4u*sizeof(payload));
        /* Independent physical proof of the 10% reservation. The fixture
         * uses 4096-byte blocks and native primary group bitmaps. Both
         * contiguous file runs must have ceil(64/10) trailing free blocks. */
        const char *growth_paths[]={"/V2-A","/V2-B"};
        for(unsigned f=0u;f<2u;++f) {
            CHECK(defrag->map(session,growth_paths[f],0u,extents,4u,&written)==IFS_FS_OK &&
                  written==1u && extents[0].length==4u*sizeof(payload));
            uint64_t end=(extents[0].physical_offset+extents[0].length)/4096u;
            for(unsigned n=0u;n<7u;++n) {
                uint64_t block=end+n,group=block/32768u,bit=block%32768u;
                unsigned char b=0xffu;
                CHECK(pread(m.fd,&b,1u,(off_t)((group*32768u+2u)*4096u+bit/8u))==1);
                CHECK(!(b&(1u<<(bit%8u))));
            }
        }
        for(unsigned i=0;i<4u;++i) {
            CHECK(core->read(session,"/V2-A",(uint64_t)i*sizeof(actual),actual,sizeof(actual))==IFS_FS_OK);
            memset(payload,(int)(i+1u),sizeof(payload));CHECK(!memcmp(actual,payload,sizeof(actual)));
            CHECK(core->read(session,"/V2-B",(uint64_t)i*sizeof(actual),actual,sizeof(actual))==IFS_FS_OK);
            memset(payload,(int)(i+9u),sizeof(payload));CHECK(!memcmp(actual,payload,sizeof(actual)));
        }
        CHECK(core->unmount(session)==IFS_FS_OK);
        /* Fail the checkpoint barrier after a durable journal commit.
         * A subsequent exclusive mount must replay through the same SO. */
        CHECK(core->mount(&h,&session,&info)==IFS_FS_OK);
        m.fail_flush=m.flushes+5u;
        memset(payload,0x5a,sizeof(payload));
        CHECK(core->write(session,"/V2-A",4u*sizeof(payload),payload,sizeof(payload))!=IFS_FS_OK);
        CHECK(core->unmount(session)==IFS_FS_IO_ERROR);
        m.fail_flush=0u;
        uint32_t dirty_variant=0u;
        const unsigned recovery_writes=m.writes;
        CHECK(core->probe(&ro,&dirty_variant)==IFS_FS_OK && dirty_variant==3u &&
              m.writes==recovery_writes);
        CHECK(core->mount(&h,&session,&info)==IFS_FS_OK);
        repaired=UINT64_MAX;
        CHECK(defrag->recover(session,0u,&repaired)==IFS_FS_OK && repaired==1u);
        CHECK(defrag->recover(session,0u,&repaired)==IFS_FS_OK && repaired==0u);
        CHECK(core->read(session,"/V2-A",4u*sizeof(payload),actual,sizeof(actual))==IFS_FS_OK);
        CHECK(!memcmp(payload,actual,sizeof(actual)));
        /* Nested namespace creation and relocation share the journal
         * writer; directory chains and content must survive remount. */
        CHECK(core->create(session,"/TREE",IFS_FS_ENTRY_DIRECTORY)==IFS_FS_OK);
        CHECK(core->create(session,"/TREE/CHILD",IFS_FS_ENTRY_DIRECTORY)==IFS_FS_OK);
        CHECK(core->create(session,"/TREE/CHILD/EDGE",IFS_FS_ENTRY_FILE)==IFS_FS_OK);
        CHECK(core->write(session,"/TREE/CHILD/EDGE",0u,"Z",1u)==IFS_FS_OK);
        CHECK(defrag->map(session,"/TREE/CHILD/EDGE",0u,extents,4u,&written)==IFS_FS_OK &&
              written==1u && extents[0].length==1u);
        CHECK(defrag->map(session,"/TREE/CHILD",0u,extents,4u,&written)==IFS_FS_OK &&
              written==1u && extents[0].length==4096u);
        CHECK(defrag->plan(session,"/",10u,&plan)==IFS_FS_OK && plan);
        CHECK(defrag->execute(session,plan,&moved)==IFS_FS_OK);
        CHECK(defrag->verify(session,plan)==IFS_FS_OK);
        defrag->release_plan(session,plan);
        CHECK(core->unmount(session)==IFS_FS_OK);
        CHECK(core->mount(&ro,&session,&info)==IFS_FS_OK);
        CHECK(core->read(session,"/TREE/CHILD/EDGE",0u,actual,1u)==IFS_FS_OK && actual[0]=='Z');
        CHECK(core->stat(session,"/TREE/CHILD",&entry)==IFS_FS_OK &&
              entry.type==IFS_FS_ENTRY_DIRECTORY);
        CHECK(core->unmount(session)==IFS_FS_OK);
    } else {
        CHECK(core->mount(&h,&session,&info)==IFS_FS_OK && session);
        CHECK(core->unmount(session)==IFS_FS_OK);
    }
    puts("EXT3 Common V2 native format, probe, root and mount PASS");
    result=0;
done:
    close(m.fd);
    if(result && argc==4)unlink(argv[1]);
    return result;
}
