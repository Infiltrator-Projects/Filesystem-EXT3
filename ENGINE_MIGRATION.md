# EXT3 native engine ownership

The canonical EXT3 format core and EXT3-only Defragmenter adapter are committed and built in this repository. Their initial source ancestry includes a pinned historical snapshot from `Filesystem-EXT4@5c4e11f04a9b0f5c56aae96e622534614d3486b0`, but the package builder no longer clones or executes that external engine.

The Linux `ext3.ko` driver source was consolidated from central `Filesystems@a253fb724ce1fd760a361219281effb6abd71367` on 6 November 2026 and is built locally. Original code attribution remains intact. `EXT3.so` and `ext3.ko` are distinct format-surface adapters, not interchangeable binaries.

Real journalling, recovery, media-interoperability and boot acceptance must be qualified independently before claiming a complete EXT3 implementation.
