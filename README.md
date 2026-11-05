# Filesystem-EXT3

**Source migration in progress — this is not a complete filesystem driver.**

This repository preserves the host-neutral EXT3/JBD format core imported from `Infiltrator-Projects/Filesystems`. It does **not** yet build a qualified standalone `EXT3.so`, Linux `.ko` or UEFI `.efi` driver; consuming applications have not been migrated to a self-contained EXT3 module.

The actual migration boundary and remaining work are recorded in [MIGRATION_STATUS.md](MIGRATION_STATUS.md). The filesystem format and journal invariants are described in [the original format design](native/filesystems/ext3/DESIGN.md).

## Verified source-core scope

The independent [C regression tests](tests/ext3_core_test.c) check a bounded subset of geometry, group mapping, directory records, hash/indirect mapping interfaces and journal-header/superblock rejection paths. The [EXT3 core CI workflow](.github/workflows/ext3-core.yml) builds and executes these tests with GCC and Clang under address and undefined-behaviour sanitizers.

Passing these tests establishes **only host-neutral core-level validation**. It does not establish real media read/write compatibility, journal replay/recovery, formatting, filesystem installation, Defragmenter integration, or safe production use.

## Installer files

- `/System/Filesystems/EXT3.so`
- `/System/Resources/Documentation/Filesystem-EXT3.pdf`
- `/System/Kernel/Filesystems/ext3.ko`

The published Debian package installs these three regular files. A manual native RUN compiles the shared object and Linux adapter locally, generates the PDF, and replaces all three files. It uses the running kernel headers by default. Use `--kernel-build DIRECTORY` for a prepared kernel build with `Module.symvers`. The kernel module is stored for manual use.

InfiltratorOS calls `--system-package-mode` and decides which outputs need updating. Its worker builds a missing or older SO, repairs the PDF independently, and stores the matching released kernel module when available. Use `--build-only --output DIRECTORY` to build a Debian package.
