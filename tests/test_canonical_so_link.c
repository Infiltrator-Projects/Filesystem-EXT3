/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <infiltratr/filesystem_module_v2.h>
#include <stdio.h>

extern const IfsFsModuleApiV2 *infiltrator_filesystem_api_v2(void);

int main(void)
{
    const IfsFsModuleApiV2 *api = infiltrator_filesystem_api_v2();
    const uint64_t namespace_ops = IFS_FS_CORE_PROBE | IFS_FS_CORE_MOUNT |
        IFS_FS_CORE_STAT | IFS_FS_CORE_LIST | IFS_FS_CORE_READ |
        IFS_FS_CORE_CREATE | IFS_FS_CORE_WRITE;
    if (!ifs_fs_api_v2_valid(api, "ext3"))
        return 1;
    const IfsFsCoreOpsV2 *core = (const void *)ifs_fs_group_v2(api, IFS_FS_GROUP_CORE);
    if (!core || (core->header.capabilities & namespace_ops) != namespace_ops ||
        !core->write || !core->create || api->query_group(IFS_FS_GROUP_CORE, 1, 1)) {
        fputs("Invalid EXT3 hosted CORE group\n", stderr);
        return 1;
    }
    return 0;
}
