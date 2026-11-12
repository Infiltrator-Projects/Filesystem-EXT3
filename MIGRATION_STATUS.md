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
