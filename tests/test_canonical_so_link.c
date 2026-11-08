/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * Link deliberately against EXT3.so, not a second compiled format core.
 * The same core is compiled by the independent ext3.ko adapter.
 * This is not a real-media or journal-recovery qualification.
 */
#include "ext3_core.h"
#include <stdio.h>
#include <stdint.h>

static int verify(int condition, const char *what)
{
    if (!condition) {
        fprintf(stderr, "EXT3.so canonical core check failed: %s\n", what);
        return 1;
    }
    return 0;
}

int main(void)
{
    int failures = 0;

    failures += verify(ifs_ext3_unsupported_incompat_features(
                        IFS_EXT3_FEATURE_INCOMPAT_FILETYPE) == 0U,
                       "known feature");
    failures += verify(ifs_ext3_unsupported_incompat_features(0x80000000U) ==
                       0x80000000U, "unsupported feature");
    failures += verify(ifs_ext3_validate_directory_record(
                           0U, 12U, UINT32_MAX, 2U, 4096U, 100U) ==
                       IFS_EXT3_DIRECTORY_RECORD_NAME_TOO_LONG,
                       "overflowed name length");
    failures += verify(ifs_ext3_validate_directory_record(
                           0U, 12U, 1U, 2U, 4096U, 100U) ==
                       IFS_EXT3_DIRECTORY_RECORD_OK, "valid entry");
    return failures ? 1 : 0;
}
