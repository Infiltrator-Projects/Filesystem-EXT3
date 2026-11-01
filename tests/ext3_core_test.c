/* SPDX-License-Identifier: GPL-3.0-or-later
 * Non-destructive test of the imported host-neutral EXT3/JBD format core.
 * This is NOT a complete EXT3.so, journal replay, read/write or media test.
 */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include "../native/filesystems/ext3/core/ext3_core.h"

static void geometry_and_groups(void)
{
    ifs_ext3_u32 count = 0, group = 99, offset = 99;
    ifs_ext3_u64 first = 0, last = 0;
    assert(ifs_ext3_validate_geometry(4096, 256, 4096, 8192, 8192, 8192)
           == IFS_EXT3_GEOMETRY_OK);
    assert(ifs_ext3_validate_geometry(4096, 192, 4096, 8192, 8192, 8192)
           == IFS_EXT3_GEOMETRY_INVALID_INODE_SIZE);
    assert(ifs_ext3_validate_geometry(4096, 256, 1024, 8192, 8192, 8192)
           == IFS_EXT3_GEOMETRY_FRAGMENT_SIZE_MISMATCH);
    assert(ifs_ext3_validate_geometry(4096, 256, 4096, 32769, 8192, 8192)
           == IFS_EXT3_GEOMETRY_BLOCKS_PER_GROUP_TOO_LARGE);
    assert(ifs_ext3_validate_geometry(4096, 256, 4096, 0, 8192, 8192)
           == IFS_EXT3_GEOMETRY_ZERO_GROUP_VALUE);
    assert(ifs_ext3_compute_group_count(8194, 1, 8192, &count)
           == IFS_EXT3_LAYOUT_OK && count == 2);
    assert(ifs_ext3_compute_group_count(8194, 1, 0, &count)
           == IFS_EXT3_LAYOUT_INVALID_BLOCKS_PER_GROUP);
    assert(ifs_ext3_compute_group_count(1, 1, 8192, &count)
           == IFS_EXT3_LAYOUT_INVALID_FIRST_DATA_BLOCK);
    assert(ifs_ext3_block_group_position(8193, 1, 8192, 2, &group, &offset)
           == IFS_EXT3_BLOCK_GROUP_OK && group == 1 && offset == 0);
    assert(ifs_ext3_block_group_position(0, 1, 8192, 2, &group, &offset)
           == IFS_EXT3_BLOCK_GROUP_OUT_OF_RANGE);
    assert(ifs_ext3_group_bounds(1, 1, 8192, 8194, &first, &last)
           == IFS_EXT3_BLOCK_GROUP_OK && first == 8193 && last == 8193);
    assert(ifs_ext3_group_bounds(2, 1, 8192, 8194, &first, &last)
           == IFS_EXT3_BLOCK_GROUP_OUT_OF_RANGE);
    assert(ifs_ext3_sparse_super_group(0));
    assert(ifs_ext3_sparse_super_group(1));
    assert(ifs_ext3_sparse_super_group(9));
    assert(!ifs_ext3_sparse_super_group(4));
}

static void directories_and_mapping(void)
{
    ifs_ext3_u16 encoded = 0;
    ifs_ext3_u32 major = 0, minor = 0, offsets[4] = {0}, boundary = 0;
    assert(ifs_ext3_validate_directory_record(0, 12, 1, 2, 4096, 100)
           == IFS_EXT3_DIRECTORY_RECORD_OK);
    assert(ifs_ext3_validate_directory_record(0, 11, 1, 2, 4096, 100)
           == IFS_EXT3_DIRECTORY_RECORD_TOO_SHORT);
    assert(ifs_ext3_validate_directory_record(0, 14, 1, 2, 4096, 100)
           == IFS_EXT3_DIRECTORY_RECORD_UNALIGNED);
    assert(ifs_ext3_validate_directory_record(4090, 12, 1, 2, 4096, 100)
           == IFS_EXT3_DIRECTORY_RECORD_CROSSES_BLOCK);
    assert(ifs_ext3_validate_directory_record(0, 12, 1, 101, 4096, 100)
           == IFS_EXT3_DIRECTORY_RECORD_INODE_RANGE);
    assert(ifs_ext3_directory_record_length_to_disk(65536U, 65536U, &encoded) == 0);
    assert(encoded == 0xffffU);
    assert(ifs_ext3_directory_record_length_from_disk(encoded, 65536U) == 65536U);
    assert(ifs_ext3_directory_record_length_to_disk(13U, 4096U, &encoded) != 0);
    assert(ifs_ext3_directory_hash((const ifs_ext3_u8 *)"hello", 5U,
                                   IFS_EXT3_HASH_LEGACY, 0, &major, &minor) == 0);
    assert((major & 1U) == 0U);
    assert(ifs_ext3_directory_hash(0, 1U, IFS_EXT3_HASH_LEGACY,
                                   0, &major, &minor) != 0);
    assert(ifs_ext3_directory_hash((const ifs_ext3_u8 *)"x", 1U, 99,
                                   0, &major, &minor) != 0);
    assert(ifs_ext3_indirect_block_path(0U, 1024U, 10U, offsets, &boundary) == 1);
    assert(offsets[0] == 0U);
    assert(ifs_ext3_indirect_block_path(12U, 1024U, 10U, offsets, &boundary) == 2);
    assert(offsets[0] == IFS_EXT3_IND_BLOCK && offsets[1] == 0U);
    assert(ifs_ext3_indirect_block_path(12U + 1024U, 1024U, 10U,
                                        offsets, &boundary) == 3);
    assert(offsets[0] == IFS_EXT3_DIND_BLOCK);
    assert(ifs_ext3_indirect_block_path(12U + 1024U + 1024U * 1024U,
                                        1024U, 10U, offsets, &boundary) == 4);
    assert(offsets[0] == IFS_EXT3_TIND_BLOCK);
    assert(ifs_ext3_indirect_block_path(12U, 1000U, 10U,
                                        offsets, &boundary) == 0);
}

static void journal_validation(void)
{
    assert(ifs_ext3_validate_journal_header(
               IFS_EXT3_JOURNAL_MAGIC, IFS_EXT3_JOURNAL_DESCRIPTOR_BLOCK, 1)
           == IFS_EXT3_JOURNAL_OK);
    assert(ifs_ext3_validate_journal_header(
               0U, IFS_EXT3_JOURNAL_DESCRIPTOR_BLOCK, 1)
           == IFS_EXT3_JOURNAL_BAD_MAGIC);
    assert(ifs_ext3_validate_journal_header(IFS_EXT3_JOURNAL_MAGIC, 99U, 1)
           == IFS_EXT3_JOURNAL_BAD_TYPE);
    assert(ifs_ext3_validate_journal_header(
               IFS_EXT3_JOURNAL_MAGIC, IFS_EXT3_JOURNAL_COMMIT_BLOCK, 0U)
           == IFS_EXT3_JOURNAL_BAD_ARGUMENT);
    assert(ifs_ext3_validate_journal_superblock(4096, 1024, 1, 0, 0)
           == IFS_EXT3_JOURNAL_OK);
    assert(ifs_ext3_validate_journal_superblock(1000, 1024, 1, 0, 0)
           == IFS_EXT3_JOURNAL_BAD_BLOCK_SIZE);
    assert(ifs_ext3_validate_journal_superblock(4096, 1024, 1024, 0, 0)
           == IFS_EXT3_JOURNAL_BAD_GEOMETRY);
    assert(ifs_ext3_validate_journal_superblock(4096, 1024, 1, 0, 2)
           == IFS_EXT3_JOURNAL_UNSUPPORTED_FEATURE);
    assert(ifs_ext3_unsupported_incompat_features(IFS_EXT3_FEATURE_INCOMPAT_FILETYPE) == 0);
    assert(ifs_ext3_unsupported_incompat_features(0x80000000U) == 0x80000000U);
}

int main(void)
{
    geometry_and_groups();
    directories_and_mapping();
    journal_validation();
    puts("EXT3 imported core: boundary and rejection tests passed (not full EXT3.so qualification)");
    return 0;
}
