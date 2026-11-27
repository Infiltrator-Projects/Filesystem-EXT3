# EXT3: technical reference

## Original format and scope

Stephen Tweedie's ext3 work appeared in the late 1990s and entered mainline Linux 2.4. It added journalling to ext2 while retaining its inode and block-group layout, enabling journal addition to an existing ext2 volume without copying every file. Journal I/O and indirect mapping remained costs; ext4 later addressed these limitations.

This reference uses Linux v2.6.32 and the original JBD layer, not later JBD2 extensions. Distinguish ordinary ext metadata, the native circular journal and independent provider recovery. A journal is neither a backup nor a complete guarantee of user-data durability; compatible ordinary features do not justify ignoring pending recovery.

## Superblock geometry and field inventory

All ordinary ext2/ext3 metadata integers are little-endian. The primary superblock is 1024 bytes long at partition-relative byte 1024, regardless of block size. Relative 32-bit fields: inode count 0, block count 4, reserved blocks 8, free blocks 12, free inodes 16, first data block 20, log block size 24, log fragment size 28, blocks per group 32, fragments per group 36, inodes per group 40, mount time 44, write time 48. Mount counts at 52/54 and magic 0xEF53 at 56 are 16-bit. State 58, errors policy 60, minor revision 62, last check 64, check interval 68, creator OS 72, revision 76, reserve UID/GID 80/82. Block bytes B=1024 shifted left by the validated log block size. First data block is ordinarily 1 for 1024-byte blocks and 0 for larger blocks. Check total geometry against the supplied device.

## Revision and feature admission

For dynamic revision, first ordinary inode at 84, 16-bit inode size 88, superblock group number 90, compatible/incompatible/read-only-compatible flags at 92/96/100, UUID 104, label 120 and last mounted path 136. Revision zero instead uses 128-byte inodes and first ordinary inode 11. Validate inode size and alignment, group counts and bitmap capacity (one bitmap block covers at most 8B allocation units). Unknown incompatible features prohibit mounting; unknown read-only-compatible flags prohibit writing. A repair tool needs a stronger understanding of all features than a read-only parser. FILETYPE incompatibility bit 0x2 changes the directory entry interpretation; sparse-super RO bit 0x1 changes backup placement; LARGE_FILE RO bit 0x2 permits large regular files. Do not admit ext4 extent, 64-bit or bigalloc features under a legacy format name.

## Group descriptors and redundant geometry

Let F be first data block, G blocks per group, T total blocks. Group count is ceil((T-F)/G); group g starts at F+g*G and ends at min(T,F+(g+1)*G). In the conventional non-META_BG layout, the primary descriptor table begins at block F+1. Each 32-byte descriptor contains bitmap block 0, inode bitmap block 4, inode table block 8, free block count 12, free inode count 14, used directory count 16, reserved/padding 18..31. The descriptor for group g begins g*32 bytes into the table. The inode-table span is ceil(inodes_per_group*inode_size/B). Validate all its blocks and metadata reservations before using a group. With sparse-super, backup superblocks and descriptor tables are in groups 0,1 and positive powers of 3,5 or 7. META_BG and resize reservations require their own placement rules; never apply the simple table formula to those without validation.

## Bitmaps and worked allocation address

A set bit means allocated in the legacy ext block and inode bitmaps, least-significant bit first in each byte. For block p>=F, group=floor((p-F)/G), local=(p-F) mod G. The byte within that group's block bitmap is floor(local/8), mask=1 shifted left by local mod 8. For B=4096, F=0, G=32768, block 33000 maps to group 1, local 232, byte 29, bit 0. For inode n>=1, group=floor((n-1)/inodes_per_group), local=(n-1) mod inodes_per_group. Inode 0 is not valid. Check last partial groups, reserved inodes, superblock replicas, descriptors and inode tables; metadata must not be offered as free storage. Free counters are summaries and must reconcile with bitmap state and actual ownership.

## Inode location and base fields

The inode byte position is inode_table_block*B+local_inode_index*inode_size. For B=4096, inode size 256, 8192 inodes per group and inode 8200, group 1 and local index 7 give offset 1792 into that group's inode table. The root is inode 2. Base 128-byte inode fields: mode 0, UID low 2, size low 4, atime 8, ctime 12, mtime 16, deletion time 20, GID low 24, links 26, i_blocks 28, flags 32, OS-specific 36. Fifteen 32-bit block pointers start at 40; generation 100, external EA block 104, directory ACL or regular-file size high 108, fragment address 112, OS-specific bytes 116..127. Linux UID/GID high halves at 120/122. ctime means inode-status change, not creation. i_blocks counts 512-byte units, including indirect and applicable EA blocks, not logical file blocks. Decode large regular-file size only under the admitted format.

## Direct and indirect file mapping

Let N=B/4. Inode pointers 0..11 map logical blocks 0..11; pointer 12 addresses N single-indirect entries, 13 addresses N*N double-indirect entries, and 14 addresses N*N*N triple-indirect entries. For logical k>=12, subtract 12, then subtract each preceding level's coverage until the correct level is selected. Divide by the appropriate power of N to select each successive pointer index. Example with B=4096 and N=1024: logical block 1040 is the fifth block of the double-indirect region (zero-based index 4), so use inode pointer 13, first index 0, then index 4. A zero pointer represents a hole over its subtree coverage; an invalid pointer is corruption, not a hole. Verify ownership, range and cycles for every indirect block; clip reads to EOF and never expose allocation slack.

## Directory records and indexed namespaces

Old directory records contain inode 0 (32 bits), record length 4 (16 bits), 16-bit name length 6 and name at 8. With FILETYPE, name length is byte 6 and file type byte 7. In either case minimum record length is align_up(8+name_length,4). Inode zero marks an unused entry; never advance by zero or cross a block boundary. Filetype values 1..7 denote regular, directory, character, block, FIFO, socket and symlink. An indexed directory uses HTree root and interior records hidden behind directory-compatible layouts; use the hash version and seed, validate sorted hash intervals and collisions, and still compare exact names in leaf directory entries. Preserve dot/dot-dot and link counts. A fast symlink can store up to 60 target bytes in i_block; determine eligibility from native allocation/type rules, not length alone.

Historical EXT directory hash families include legacy, half-MD4 and TEA, with signed and unsigned byte variants. The recorded hash version selects the algorithm; using a host-language default signedness can route a lookup to the wrong leaf.

## Extended attributes and special objects

An external EA block uses magic 0xEA020000, reference count, block count and hash in its header, followed by variable entries. Each entry has one-byte name length and namespace index, 16-bit value offset, 32-bit value-block field, value length and hash. Entries and values have four-byte alignment; verify separate bounds and shared-block reference counts. Do not treat an external EA block as ordinary file payload or reclaim it while shared. On supported larger inode variants the inode tail may contain inline extended attributes; validate the extra-inode-size field before decoding that area. Preserve ACL namespaces and unknown attributes. Device nodes encode device identity in i_block rather than data pointers. Distinguish special files, sparse files and symlinks when reconstructing ownership.

## Structural checking and formatting

Reconstruct all metadata reservations, allocated inode identities, direct and indirect trees, EA ownership, namespace reachability and hard-link counts. Check duplicate ownership against native sharing rules, compare bitmaps and counters, and distinguish damaged references from genuinely free space. A formatter must establish valid superblock copies, descriptors, bitmaps, reserved inode records, root inode and root directory, and all required features before publishing a usable state. An independent formatter oracle can create fixtures; it must not be a production fallback. Test block sizes 1,2,4 KiB, different inode sizes, sparse-super groups, partial final groups, direct/indirect transitions, sparse subtrees, large file sizes, invalid record lengths and metadata overlap.

Online growth can extend the final group or add groups. Validate the new descriptor and bitmap reservations and their addressability before publishing changed superblock geometry; reconcile local and global free counts with the newly admitted space.

## Journal discovery and feature gates

The ext3 HAS_JOURNAL compatible bit is 0x4; the RECOVER incompatible bit is 0x4; JOURNAL_DEV incompatible bit is 0x8. Superblock offsets: reserved GDT blocks 206 (16 bits), journal UUID 208..223, journal inode 224, journal device 228, last orphan 232, four directory hash seed words 236..251, default hash version 252. An internal journal is normally inode 8; follow the actual recorded inode through its native mapping. An external journal requires its matching device and identity; the numeric device field alone is not reliable outside its original OS. The ordinary ext filesystem remains little-endian but JBD record integers are big-endian. If the recovery flag is set, do not present unreplayed home blocks as a guaranteed coherent live namespace.

## JBD journal superblock

The common header has big-endian magic 0xC03B3998 at 0, block type 4 and transaction sequence 8. Type 3 is version-one superblock and type 4 version-two. The journal superblock contains block size 12, maximum length in blocks 16, first log block 20, expected sequence 24, log start 28, abort errno 32. Version two adds feature masks at 36/40/44, UUID at 48, user count 64, dynamic-superblock location 68, maximum transaction/data blocks 72/76 and filesystem UUID list from 256. A zero start indicates no outstanding log according to the native journal state; corroborate with filesystem state and feature gates. The original JBD supports revoke incompatibility bit 1. Reject unsupported JBD2 checksum or 64-bit tag variants instead of borrowing their layouts.

## Descriptor tags and replay payload

A descriptor (type 1) has a 12-byte header followed by eight-byte tags: 32-bit home block number and flags. Flags are ESCAPE=1, SAME_UUID=2, DELETED=4, LAST_TAG=8. When SAME_UUID is absent, a 16-byte UUID follows the tag. The descriptor identifies subsequent logged block images in journal order; use the native tag interpretation and stop at LAST_TAG. An escaped image restores the original leading journal magic during replay. Verify tag boundaries, matching volume identity, target range and journal wrap before interpreting a logged image. A type-2 commit record with the expected sequence marks completion in the original baseline, subject to durable media ordering. A descriptor or apparently newer payload without its valid commit is not authoritative.

## Revokes and three-pass recovery

A revoke block has type 5, common header and a 32-bit used-byte count at 12; 32-bit revoked home block numbers follow from 16. Bound the count, alignment and every block number. Historical JBD recovery scans complete transactions to identify the valid sequence window, collects revocations, then replays eligible images in sequence order. A later or same-transaction revoke suppresses stale earlier logged images of that block under native sequence comparison. For example, if transaction 10 logs block 900 and committed transaction 11 revokes block 900, transaction 10's image must not resurrect its former contents. Sequence wrap, circular traversal and error handling require the native recovery.c rules. Replay onto a disposable overlay first when performing forensic inspection.

## Data modes, commit and checkpoint

Journal mode logs ordinary data as well as metadata; ordered mode writes relevant data before committing metadata that exposes it; writeback mode does not provide that same data ordering. Metadata integrity is not a guarantee of the latest user bytes. Checkpointing copies committed metadata to home locations and advances the journal tail only after durable publication. Do not reuse the log region while it still carries required recovery records. The filesystem's orphan list supports interrupted truncate/unlink; its inode-chain semantics must be applied with proper ownership checks after journal recovery. An independent defragmenter must update mappings, allocation and affected journal metadata coherently; an application log alone does not establish native JBD compatibility.

The superblock's last-orphan field names the first inode of the historical chain. During recovery, each inode's deletion-time field carries the next inode number. Validate range, ownership, termination and absence of cycles before following it. Cleanup must finish interrupted unlink or truncation without leaking or freeing still-referenced blocks.

## Implementation and qualification boundary

Historical source 70efd2b1529b exposed hosted read/write and format. Its native formatter requires unique volume identity before destructive writes. Journal replay, crash recovery, firmware and kernel root mounting remain independent qualification questions.

Common V2 is the independent provider interface: https://github.com/Infiltrator-Projects/Libraries/blob/main/docs/FILESYSTEM_COMMON_ABI_V2.md . Negotiate real operation masks and media profiles. Use bounded host I/O, exclusive unmounted ownership and durable flush for mutation; distinguish unsupported, corrupt, I/O and stale-handle results. Test Media and Defragmenter use the same engine. Hosted, freestanding, BOOT, kernel, recovery and package qualification are independent. Current capability and release status belongs to the repository README and exact-source qualification records; historical evidence is not a current status claim.

Historical source or test reference: https://github.com/Infiltrator-Projects/Filesystem-EXT3/actions/runs/45520241461 .

## References and validation

Pinned headers: https://github.com/torvalds/linux/blob/v2.6.32/include/linux/ext3_fs.h and https://github.com/torvalds/linux/blob/v2.6.32/include/linux/jbd.h . Native algorithms in fs/ext3 and fs/jbd at that revision, including recovery.c, revoke.c, commit.c and checkpoint.c. Validate using independently formatted ext3 media, internal/external journals, multiple committed and uncommitted transactions, wrapped sequence ranges, escaped payloads, revocation, aborted logs, orphaned inodes and all data modes. Verify recovered namespace and payload against native tools on copies. Exact replay and crash testing remain required for independent production writes.
