# Filesystem-EXT3

## 1. Purpose and format scope

Third extended filesystem, including JBD journal semantics. This repository owns the format-specific implementation of `EXT3.so` for InfiltratorOS.

## 2. On-disk architecture and invariants

Ext block groups, bitmaps, inode tables and indirect block mappings remain format structures; journal state governs whether a consistent read or write is possible. Native format must acquire a unique volume identity before destructive writes.

## 3. Common V2 implementation and capabilities

Hosted Common V2 source declares probe, mount, stat, read, write and format. This is broader than the earlier probe-only documentation. Check the operation mask and journal-state restrictions before calling a mutation. The canonical entry point is `infiltrator_filesystem_api_v2`. Consumers must negotiate the actual profile and operation masks and use bounded, partition-relative host I/O. A source-level callback does not by itself establish that every format variant or host profile is qualified.

## 4. Integrity, mutation, allocation and recovery

Core tests and hosted media tests do not prove journal replay, power-failure recovery, production firmware or kernel-root mounting. For destructive operations, require the applicable exclusive writable host, durability contract and format-specific validation. Never silently substitute a kernel or FUSE driver or treat a rejected or unavailable operation as success. Preserve original disk-format semantics; relocation and recovery are separate from ordinary file I/O.

## 5. Build, installation and release

The target installation is `/System/Filesystems/EXT3.so` and `/System/Resources/Documentation/Filesystem-EXT3.pdf`. The project release policy is a portable amd64 DEB and a CPU-native source-first RUN, with GitHub's automatic source archive; do not treat this policy statement as verification of the current installer payload. The release workflow is the authoritative executable build and package specification. Historical adapters and test artifacts are not additional production filesystem authorities.

## 6. Qualification and limitations

[Recorded qualification workflow](https://github.com/Infiltrator-Projects/Filesystem-EXT3/actions/runs/45520241461) · [Current workflows](https://github.com/Infiltrator-Projects/Filesystem-EXT3/actions?query=branch%3Amain). A successful workflow establishes the tested scope only, not complete original-format parity, physical-media crash safety, or production BOOT/kernel integration. Use disposable images for destructive qualification.

## 7. Technical references and provenance

- [Provider implementation](module/ext3_common.c)
- [Native design](native/filesystems/ext3/DESIGN.md)
- [Migration and source history](MIGRATION_STATUS.md)
- [Original-format reference source](docs/EXT3.pages) and [PDF](docs/EXT3.pdf)
- [Project philosophy](docs/PHILOSOPHY.md)
- [Common ABI](https://github.com/Infiltrator-Projects/Libraries)

The original-format reference and historical migration notes describe different evidence from the current production provider. When descriptions disagree, the current exported descriptor, implementation, executable tests and exact-revision release evidence take precedence.
