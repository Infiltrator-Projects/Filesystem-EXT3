/* SPDX-License-Identifier: GPL-3.0-or-later
 * Reject foreign filesystem identities without legacy fixture APIs or I/O.
 */
#include <infiltratr/filesystem_module_v2.h>
#include <stdio.h>
extern const IfsFsModuleApiV2 *infiltrator_filesystem_api_v2(void);
int main(void)
{
    const IfsFsModuleApiV2 *api=infiltrator_filesystem_api_v2();
    const IfsFsCoreOpsV2 *core;
    IfsFsHostRuntimeV2 invalid_host={0};
    uint32_t variant=0u;
    if (!ifs_fs_api_v2_valid(api,"ext3") ||
         ifs_fs_api_v2_valid(api,"ext2") ||
         ifs_fs_api_v2_valid(api,"ext4"))
        return 1;
    core=(const void *)ifs_fs_group_v2(api,IFS_FS_GROUP_CORE);
    if (!core || !core->probe ||
        core->probe(&invalid_host,&variant)!=IFS_FS_INVALID) {
        fputs("EXT3 V2 failed foreign identity or invalid host rejection\n",stderr);
        return 1;
    }
    return 0;
}
