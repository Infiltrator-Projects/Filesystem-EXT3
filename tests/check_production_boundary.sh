#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# The installed EXT3.so is not allowed to ship retired worker/service ABIs.
set -eu
module=${1:?EXT3.so required}
sh "$(dirname "$0")/check_v2_exports.sh" "$module"
if nm -D --defined-only "$module" | grep -E     'infiltrator_filesystem_ext_v1|ifs_ext_(worker_main|metadata_main)|ifs_bind_services|ifs_unbind_services|ifs_ext_fixture_run|ldtm_' ; then
    echo 'Installed EXT3.so still exports retired worker/service callbacks' >&2
    exit 1
fi
