# Filesystem-EXT3

**Source migration in progress — this is not a complete filesystem driver.**

This repository preserves the host-neutral EXT3/JBD format core imported from `Infiltrator-Projects/Filesystems`. It does **not** yet build a qualified standalone `EXT3.so`, Linux `.ko` or UEFI `.efi` driver; consuming applications have not been migrated to a self-contained EXT3 module.

The actual migration boundary and remaining work are recorded in [MIGRATION_STATUS.md](MIGRATION_STATUS.md). The filesystem format and journal invariants are described in [the original format design](native/filesystems/ext3/DESIGN.md).

## Verified source-core scope

The independent [C regression tests](tests/ext3_core_test.c) check a bounded subset of geometry, group mapping, directory records, hash/indirect mapping interfaces and journal-header/superblock rejection paths. The [EXT3 core CI workflow](.github/workflows/ext3-core.yml) builds and executes these tests with GCC and Clang under address and undefined-behaviour sanitizers.

Passing these tests establishes **only host-neutral core-level validation**. It does not establish real media read/write compatibility, journal replay/recovery, formatting, filesystem installation, Defragmenter integration, or safe production use.
