/* SPDX-License-Identifier: GPL-3.0-or-later
 * Wrong-format and missing-host requests are rejected without disk I/O. */
#include "ifs_ext_module_v1.h"
#include "ifs_ext_fixture_v1.h"
#include <stdio.h>
#include <string.h>
int main(void) {
    const IfsExtModuleV1 *api = infiltrator_filesystem_ext_v1();
    if (!api || !api->format_id || strcmp(api->format_id, "ext3") != 0 ||
        !api->fixture) return 1;
    const LdtmFilesystemSpec ext2 = {.key="ext2", .creator=LDTM_CREATOR_EXT2};
    const LdtmFilesystemSpec ext4 = {.key="ext4", .creator=LDTM_CREATOR_EXT4};
    const LdtmFilesystemSpec ext3 = {.key="ext3", .creator=LDTM_CREATOR_EXT3};
    if (api->fixture(&ext2, "/dev/null", 1, NULL, NULL) != -1 ||
        api->fixture(&ext4, "/dev/null", 1, NULL, NULL) != -1 ||
        api->fixture(&ext3, "/dev/null", 1, NULL, NULL) != -1) {
        fputs("EXT3 module failed to reject an invalid format or host\n", stderr);
        return 1;
    }
    return 0;
}
