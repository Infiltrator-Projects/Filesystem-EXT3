# Filesystem-EXT3

**Independent source builds are available; full filesystem qualification is still in progress.**

This repository preserves the host-neutral EXT3/JBD format core imported from `Infiltrator-Projects/Filesystems`. `EXT3.so` is now built from this repository's own CMake source, and `ext3.ko` is built from the locally owned Linux VFS source tree. A genuinely qualified UEFI `.efi` driver and full Linux-root/real-media recovery qualification remain separate unfinished work.

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
