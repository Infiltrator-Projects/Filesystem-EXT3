# EXT3 migration — 2026-10-30

Source import only, NOT a complete EXT3.so. The canonical EXT3 core and format design were copied without modification from Infiltrator-Projects/Filesystems commit ca2fa1210c3c1f2b6f52cd15b3da6dfa83d453ad. The Linux EXT3 implementation remains in the old repository; the shared EXT2/3/4 Defragmenter relocation worker remains active in Defragmenter (staging reference will be held under Filesystem-EXT4). Existing consumers have not been changed. Common is the owner of shared ABI definitions, not filesystem format code.
