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

static int probe(const IfsFsHostRuntimeV2 *h, uint32_t *variant)
{
    const uint32_t minimum = 4096u;
    uint8_t *raw;
    const uint8_t *sb;
    uint32_t sector, span, off, block_bytes, groups, blocks, inodes, incompat, ro, compat;
    int result = IFS_FS_CORRUPT;
    if (!variant || !ifs_fs_host_v2_valid(h)) return IFS_FS_INVALID;
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
    groups = le32(sb + 32);
    inodes = le32(sb + 40);
    if (!blocks || (uint64_t)blocks * block_bytes > h->volume_bytes ||
        ifs_ext3_unsupported_incompat_features(incompat) ||
        ifs_ext3_unsupported_ro_compat_features(ro) ||
        ifs_ext3_validate_geometry(block_bytes, le16(sb + 88), block_bytes,
                                   groups, le32(sb + 36), inodes)
          != IFS_EXT3_GEOMETRY_OK)
        goto done;
    *variant = 3u;
    result = IFS_FS_OK;
done:
    h->release(h->context, raw);
    return result;
}

static const IfsFsCoreOpsV2 core = {
    .header = {IFS_FS_GROUP_ABI_V2, sizeof(IfsFsCoreOpsV2),
               IFS_FS_GROUP_CORE, 0u, IFS_FS_CORE_PROBE},
    .probe = probe
};
static const IfsFsGroupHeaderV2 *query(uint32_t group, uint32_t lo, uint32_t hi)
{
    return group == IFS_FS_GROUP_CORE && lo <= IFS_FS_GROUP_ABI_V2 &&
           hi >= IFS_FS_GROUP_ABI_V2 ? &core.header : NULL;
}
static const IfsFsModuleApiV2 api = {
    .abi_version = IFS_FS_COMMON_ABI_V2,
    .structure_bytes = sizeof(IfsFsModuleApiV2),
    .format_id = "ext3",
    .runtime_profiles = IFS_FS_PROFILE_HOSTED,
    .group_mask = IFS_FS_GROUP_BIT(IFS_FS_GROUP_CORE),
    .query_group = query
};
__attribute__((visibility("default")))
const IfsFsModuleApiV2 *infiltrator_filesystem_api_v2(void)
{ return &api; }
