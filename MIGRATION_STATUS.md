# EXT3 source consolidation — 6 November 2026

This repository now contains all 19 native filesystem core, design and Linux VFS driver files from `Infiltrator-Projects/Filesystems` main `a253fb724ce1fd760a361219281effb6abd71367`. Its 3 matching canonical source files were preserved; the 16 Linux driver files were imported verbatim with original authorship.

`standalone/kernel.json` now builds `ext3.ko` from this repository's own source rather than downloading a pinned central Filesystems source archive. `standalone/config.json` no longer selects an old EXT4 repository revision: the existing independent local CMake project builds `EXT3.so` directly, sharing only filesystem-neutral Libraries/Common ABI dependencies. The original pinned third-party backend path is no longer the source for the published source-embedded native RUN.

This consolidates code ownership, but compile/test results, journal safety, independent real-media read/write/recovery, and full root/EFI qualification must still be separately established. Do not delete the central EXT3 code until its own CMake tests and application consumers have migrated and the post-change CI passes.

## Strict Common V2 read-only EXT3 volume lifecycle (1.0.9)
CORE.PROBE, CORE.MOUNT/UNMOUNT, and VOLUME.QUERY now use the same
canonical EXT3 format-core geometry and feature checks over Common V2
partition-relative host I/O. The returned free-space figure is the
**recorded superblock free-block count**, not a verified allocation bitmap;
counts outside media geometry are refused. Stale sessions, concurrent
mounts, foreign EXT4, unsafe features and write-capable-only hosts fail
closed. The existing independent EXT3 journalled filesystem worker,
transaction writer and recovery remain unchanged. No Common V2 DEFRAG
MAP/PLAN/EXECUTE/VERIFY/RECOVER are advertised or qualified here.

## Production Common V2-only library boundary (1.0.12)

`EXT3.so` now links only this independent repository's canonical EXT3
format core and the strict Common V2 provider. The retired Defragmenter
CLI, legacy V1 host services, fixture adapters and path-oriented worker are
retained only as non-installed test targets, preserving historical bitmap
and descriptor-cache regression coverage without shipping the old ABI.

The production provider remains a **read-only metadata-session provider**:
`CORE.PROBE / MOUNT / UNMOUNT` and `VOLUME.QUERY`. It does not yet expose
file namespace `STAT / LIST / READ`, journal replay, writable mounts,
filesystem formatting or native Common V2 defragmentation. Neither the
standalone Linux module nor Linux's native EXT3 implementation is an allowed
substitute for the generic Common V2 Kernel VFS/ELF loader.


## Unreleased EXT3 Common V2 namespace and UEFI reader

Production now compiles an independently owned EXT3 reader using the same native
ext-family inode and block engine for hosted .so and freestanding EFI ELF.
The EXT3 journal flag is mandatory. Pending journal replay, unclean volumes,
external journals, and incompatible features are refused; no journal replay
or writable operation is faked. The canonical EXT3 native format core is
preserved and linked into hosted EXT3.so. The older metadata-only synthetic
provider remains only in a test target, never the production .so.

The copied ext-family reader retains source copyright and licence notices.
Qualification must pass the real production BOOT static validator, actual
Common V2 namespace tests and OVMF EFI-stub kernel/initrd reads before a
new EXT3 version or DEB/RUN release. This section is not a release claim.
