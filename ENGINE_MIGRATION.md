# EXT3 native engine ownership migration
Source baseline: `Infiltrator-Projects/Filesystem-EXT4` commit `5c4e11f04a9b0f5c56aae96e622534614d3486b0`.
All imported EXT filesystem core/fixture/ABI/host-adapter code is copied into this repository, preserving original license headers and format-specific rejection. The independently built `EXT3.so` no longer checks out or links to `Filesystem-EXT4`: only filesystem-neutral `Libraries` is fetched, at a pinned revision.

This is a **source-ownership and independent build qualification step**, not a claim of universal EXT3 disk compatibility. The original canonical EXT3/JBD format core remains here and is tested separately; further integration of its journal invariants into the relocation engine and exhaustive real-media/interruption testing remain required.

The published v1.0.0 DEB and native RUN currently use the earlier pinned external engine, and Defragmenter keeps its validated reference snapshot. Do not switch either packaging or Defragmenter to this new independent build without end-to-end corpus, destructive-image and recovery qualification. No physical media are used by these tests.
