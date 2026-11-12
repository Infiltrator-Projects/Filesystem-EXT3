/* SPDX-License-Identifier: GPL-3.0-or-later
 * Strict host-bounded Common V2 identification, reusing the authoritative
 * EXT3 format core for feature and geometry validation.  The historical
 * native path worker remains isolated until V2 operation parity is qualified.
 */
#include <infiltratr/filesystem_module_v2.h>
#include "../native/filesystems/ext3/core/ext3_core.h"
#include <stddef.h>
#include <stdint.h>
#include <string.h>

static uint16_t le16(const uint8_t *p)
{ return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8)); }
static uint32_t le32(const uint8_t *p)
{ return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
         ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24); }

static int inspect(const IfsFsHostRuntimeV2 *h, uint32_t *variant,
                   IfsFsVolumeInfoV2 *volume)
{
    const uint32_t minimum = 4096u;
    uint8_t *raw;
    const uint8_t *sb;
    uint32_t sector, span, off, block_bytes, groups, blocks, inodes, incompat, ro, compat;
    uint32_t free_blocks;
    int result = IFS_FS_CORRUPT;
    if (!variant) return IFS_FS_INVALID;
    *variant=0u;
    if (!ifs_fs_host_v2_valid(h)) return IFS_FS_INVALID;
    sector = h->logical_sector_bytes;
    if (h->volume_bytes < minimum || sector > h->volume_bytes)
        return IFS_FS_NOT_FOUND;
    if (h->maximum_transfer_bytes < sector) return IFS_FS_INVALID;
    span = sector > minimum ? sector : minimum;
    if (span > h->volume_bytes) return IFS_FS_NOT_FOUND;
    raw = h->allocate(h->context, span);
    if (!raw) return IFS_FS_NO_MEMORY;
    for (off = 0; off < span; off += sector)
        if (h->read_at(h->context, off, raw + off, sector)) {
            result = IFS_FS_IO_ERROR;
            goto done;
        }
    sb = raw + 1024u;
    if (le16(sb + 56) != 0xef53u) {
        result = IFS_FS_NOT_FOUND;
        goto done;
    }
    compat = le32(sb + 92);
    incompat = le32(sb + 96);
    ro = le32(sb + 100);
    /* Journalled EXT3 must not claim EXT2 or extents/64-bit EXT4 media. */
    if (!(compat & 4u) || (incompat & (0x0040u | 0x0080u | 0x0200u))) {
        result = IFS_FS_NOT_FOUND;
        goto done;
    }
    block_bytes = le32(sb + 24);
    if (block_bytes > 6u) goto done;
    block_bytes = 1024u << block_bytes;
    blocks = le32(sb + 4);
    free_blocks = le32(sb + 12);
    groups = le32(sb + 32);
    inodes = le32(sb + 40);
    if (!blocks || free_blocks > blocks ||
        (uint64_t)blocks * block_bytes > h->volume_bytes ||
        ifs_ext3_unsupported_incompat_features(incompat) ||
        ifs_ext3_unsupported_ro_compat_features(ro) ||
        ifs_ext3_validate_geometry(block_bytes, le16(sb + 88), block_bytes,
                                   groups, le32(sb + 36), inodes)
          != IFS_EXT3_GEOMETRY_OK)
        goto done;
    *variant = 3u;
    if(volume) {
        *volume=(IfsFsVolumeInfoV2){
            .structure_bytes=sizeof(*volume),
            .variant=3u, .max_name_bytes=255u,
            .block_bytes=block_bytes,
            .total_bytes=(uint64_t)blocks*block_bytes,
            .free_bytes=(uint64_t)free_blocks*block_bytes
        };
    }
    result = IFS_FS_OK;
done:
    h->release(h->context, raw);
    return result;
}

static int probe(const IfsFsHostRuntimeV2 *h,uint32_t *variant)
{ return inspect(h,variant,NULL); }

/* Read-only Common V2 session: the canonical EXT3 core already validated
 * feature flags and geometry; this reports the superblock's RECORDED free
 * block count, not an independently scanned block bitmap. The host alone
 * owns media I/O and writable access is always rejected. */
static struct {
    uintptr_t generation,active;
    IfsFsVolumeInfoV2 info;
} session;
static int mounted(void *handle)
{ return session.active && (uintptr_t)handle==session.active; }
static int mount_volume(const IfsFsHostRuntimeV2 *h, void **volume,
                        IfsFsVolumeInfoV2 *info)
{
    IfsFsVolumeInfoV2 measured={0};
    uint32_t variant=0u;
    int rc;
    if(!volume || !info || info->structure_bytes<sizeof(*info))
        return IFS_FS_INVALID;
    *volume=NULL;
    if(!ifs_fs_host_v2_valid(h))return IFS_FS_INVALID;
    if(!(h->capabilities & IFS_FS_HOST_READ_ONLY))return IFS_FS_ACCESS;
    if(session.active || session.generation==UINTPTR_MAX)return IFS_FS_BUSY;
    rc=inspect(h,&variant,&measured);
    if(rc!=IFS_FS_OK)return rc;
    session.info=measured;
    session.active=++session.generation;
    *info=measured;
    *volume=(void *)session.active;
    return IFS_FS_OK;
}
static int unmount_volume(void *volume)
{
    if(!mounted(volume))return IFS_FS_INVALID;
    session.active=0u;
    session.info=(IfsFsVolumeInfoV2){0};
    return IFS_FS_OK;
}
static int query_volume(void *volume,IfsFsVolumeInfoV2 *info)
{
    if(!mounted(volume) || !info ||
       info->structure_bytes<sizeof(*info))return IFS_FS_INVALID;
    *info=session.info;
    return IFS_FS_OK;
}
static const IfsFsVolumeOpsV2 volume_ops={
    .header={IFS_FS_GROUP_ABI_V2,sizeof(IfsFsVolumeOpsV2),
             IFS_FS_GROUP_VOLUME,0u,IFS_FS_VOLUME_QUERY},
    .query=query_volume
};
static const IfsFsCoreOpsV2 core = {
    .header = {IFS_FS_GROUP_ABI_V2, sizeof(IfsFsCoreOpsV2),
               IFS_FS_GROUP_CORE, 0u, IFS_FS_CORE_PROBE|IFS_FS_CORE_MOUNT},
    .probe = probe,
    .mount = mount_volume,
    .unmount = unmount_volume
};
static const IfsFsGroupHeaderV2 *query(uint32_t group, uint32_t lo, uint32_t hi)
{
    if(lo>IFS_FS_GROUP_ABI_V2 || hi<IFS_FS_GROUP_ABI_V2)return NULL;
    if(group==IFS_FS_GROUP_CORE)return &core.header;
    if(group==IFS_FS_GROUP_VOLUME)return &volume_ops.header;
    return NULL;
}
static const IfsFsModuleApiV2 api = {
    .abi_version = IFS_FS_COMMON_ABI_V2,
    .structure_bytes = sizeof(IfsFsModuleApiV2),
    .format_id = "ext3",
    .runtime_profiles = IFS_FS_PROFILE_HOSTED,
    .group_mask = IFS_FS_GROUP_BIT(IFS_FS_GROUP_CORE) |
                  IFS_FS_GROUP_BIT(IFS_FS_GROUP_VOLUME),
    .query_group = query
};
__attribute__((visibility("default")))
const IfsFsModuleApiV2 *infiltrator_filesystem_api_v2(void)
{ return &api; }
