# gbavocab custom indexed dictionary, versions 1 and 2

`.dict` here is a **custom gbavocab format**, not StarDict, DICT protocol, or a standard dictionary format. Files live directly in SD-root `/gbavocab`. The updated builder emits version 2; the updated reader accepts versions 1 and 2. **Old ROMs and PC builders reject the version-2 header.** This prevents old software from ignoring replacements or resurrecting deleted entries. Version-1 files remain readable and support legacy additions, but Edit/Delete require PC Open .dict / Save As to a new version-2 file first: the GBA never patches the old header or rewrites its base in place. No ROM patching, numbered filename conventions, subdirectories or per-dictionary SAV are involved. The chooser discovers up to 24 filenames of at most 63 UTF-8 bytes with case-insensitive `.dict` suffix; internal name/pair is displayed. Files beyond discovery capacity are not selected. Do not modify or swap a file while the application has it open.

## Integer, string and checksum conventions

All integers are unsigned little-endian 32-bit. Offsets are absolute file offsets. CRC32 is reflected IEEE (polynomial `0xedb88320`, initial `0xffffffff`, final XOR `0xffffffff`), compatible with Python `zlib.crc32`. CRCs detect accidental damage; they are not cryptographic authentication.

Text is strict UTF-8. Each pair is two nonblank fields with no TAB/newline/control inside either field, at most 191 bytes including their separating TAB. Sorting is bytewise UTF-8 with ASCII A–Z folded to a–z; other bytes match exactly. Equal keys sort by original row number. The producer validates both sorted permutations and all strings before writing. No fuzzy matching or Unicode normalization is performed.

## Immutable indexed base

The base is at most 32 MiB. Its exact end (not physical EOF) is in the header; additions follow it. Nonempty base, fixed 160-byte header:

| Offset | Bytes | Meaning |
|---:|---:|---|
| 0 | 8 | ASCII `GVDIDX01` (legacy) or `GVDIDX02` (mutations supported) |
| 8 | 4 | base end / first append-slot offset |
| 12 | 4 | base entry count N |
| 16 | 32 | NUL-terminated name, 1..31 UTF-8 bytes, zero padding |
| 48 | 12 | front language code, zero padding |
| 60 | 12 | back language code, zero padding |
| 72 | 24 | front display label, 1..23 UTF-8 bytes, zero padding |
| 96 | 24 | back display label, same |
| 120 | 4 | record table offset: exactly 160 |
| 124 | 4 | front index offset: exactly 160 + 12N |
| 128 | 4 | back index offset: exactly 160 + 20N |
| 132 | 4 | string area offset: exactly 160 + 28N |
| 136 | 4 | CRC32 of bytes [160, base end), checked by PC reader |
| 140 | 16 | zero reserved bytes |
| 156 | 4 | CRC32 of header bytes [0,156) |

Codes are distinct, match `[a-z][a-z0-9-]{0,10}`, and describe canonical columns, never dictionary IDs or the current search direction.

Each 12-byte record contains front string offset, back string offset, and CRC32 of `front UTF-8 + NUL + back UTF-8 + NUL`. Strings in each pair are contiguous. Each directional index has N 8-byte cells: a row number followed by CRC32 of that row number's four little-endian bytes. Thus each entry costs 28 table bytes plus the two strings and two terminators. No alignment padding is required before additions.

The GBA validates the header once per view and validates fetched index cells and rows lazily, including bounds, lengths, UTF-8, CRCs and text grammar. It does **not** reread the entire base to verify its whole-base checksum or sorted permutation on every open/query. The PC reader verifies both fully. Maliciously rebuilt valid checksums with incorrectly sorted indexes are outside accidental-damage detection; queries are memory-safe but may not find all rows. Base corruption read during a query fails that query rather than exposing unvalidated text.

## Append area: fixed 208-byte slots, at most 512

Slot J starts at `base_end + 208*J`, for J=0..511. Base and all earlier physical bytes remain unchanged on append, including incomplete slots.

| Slot offset | Bytes | Meaning |
|---:|---:|---|
| 0 | 4 | ASCII `ADD1`, or version-2 `REP2` / `DEL2` |
| 4 | 4 | `ADD1`: physical slot number J; mutation: immutable target identity |
| 8 | 192 | canonical `front TAB back NUL`, remaining bytes zero; `DEL2`: all zero |
| 200 | 4 | CRC32 of slot bytes [0,200) |
| 204 | 4 | commit marker ASCII `OK01` |

Append sequence: freshly reopen and scan the bounded addition area; return success without another append when the exact canonical row already exists; check slot capacity; seek physical EOF; pad only the remainder of an incomplete final slot with zeros; write the next slot's first 204 bytes; sync; write four commit bytes; sync; close; reopen and read back all 208 expected bytes; close. Only then report a newly appended record as successful. No base rewrite, rename, unlink, truncation or separate dictionary SAV occurs.

A final short slot, or a complete slot without the exact commit marker, is uncommitted and ignored. It permanently consumes a physical slot. Retry skips it and appends after it, preserving every previous byte. Repeated interruption can exhaust slots before 512 valid pairs; use PC Open .dict / Save As to produce a new compacted indexed base. A full valid committed row remains recognized after an ambiguous returned sync/close/readback error, so retry is idempotent. Duplicate detection applies to exact prior additions (case-sensitive canonical bytes), not existing base entries or different translations.

A version-2 base identity is its original canonical record-table ordinal `0..N-1`, **not its current match position or text**. An added-row identity is `0x80000000 | original_ADD1_slot`. `REP2` replaces that identity's live canonical pair; `DEL2` removes it from both directions. Subsequent mutations retain the original identity, including after key changes. Mutation targets must refer to a base ordinal or an earlier committed ADD1, never a replacement slot or uncommitted hole. The latest valid mutation wins; deletion is terminal (repeated deletion is accepted, replacement after deletion is corruption). Distinct duplicate senses remain distinct. All-deleted live views are valid, although PC output still requires a nonempty compacted base.

All operation types and retired slots share the **512 physical-slot limit**. Repeated edits consume slots too. Exact replacement retries against the latest committed state and repeated deletion retries return success without another write. PC Open .dict / Save As applies the complete log, drops superseded/deleted rows and holes, and rebuilds both indexes in a new version-2 file. Canonical identities are local to one file and are reassigned on compaction; never retain a selected-row identity across replacing that file. Dictionary edits do not modify copies already stored in learning-list TXT files.

A slot with the exact commit marker but invalid magic/target/sequence/padding/UTF-8/CRC blocks further appends. Version-2 lookup fails closed as a whole: it never exposes the immutable base as a fallback that could resurrect a deleted/replaced row. Legacy version-1 additions retain their independent base-read behavior with an error notice. Invalid records are not silently discarded. Back up the file and filesystem before PC recovery. CRC/commit checks do not promise atomic physical sector writes; real Supercard power-loss behavior is unverified.

## Runtime budgets and evidence

The dictionary screen exclusively borrows the inactive persistent TXT candidate FIL; no TXT load/save runs until the catalog is destroyed. A failed close retains its existing quarantine flag for the next borrower, including across screen exit. No additional persistent sector-sized FIL is allocated. One 256-byte read cache buffers indexed FatFS reads. Base prefix lookup uses two binary searches: O(log N) record comparisons, not a full scan. Each record fetch reads at most 193 string bytes, an index cell and one record cell. Additions use a bounded 512-element 16-bit matching-slot index; query changes scan only the append area (at most 106,496 bytes), not the base and not every rendering frame. The persistent overlay match index remains 512 16-bit physical-slot ordinals. A bounded temporary 512-entry canonical-identity table collapses revisions and validates targets while reading only the append log. A separate 512-entry 32-bit sparse rank index suppresses affected base matches. For affected base rows whose original key matches the prefix, their rank is found by binary search over the selected directional index, using the canonical row ordinal to break duplicate-key ties. Thus no query or mutation scans the entire base. Temporary identity and sparse-rank tables use separate bounded stack frames, not extra permanent EWRAM; the existing reserve and stack gates remain unchanged. The RAM identity reconciliation is bounded quadratic in the 512-slot limit; no claim of logarithmic overlay CPU work is made. Visible result rows are cached by the screen. These limits are explicit trade-offs, not physical flashcard speed claims.

`bash tests/run_dictionary_tests.sh` tests 40,010 synthetic base rows in both directions, reports production host-backed FatFS API read calls/bytes/seeks/writes, validates canonical selection and additions, covers every partial append boundary and every single-byte slot mutation, faults and retries, and 24-file discovery. `--sanitize` enables ASan/UBSan for the pure parser and production adapter tests. `tests/check_entry_memory.py` retains the existing 20 KiB EWRAM and conservative application stack gates. Neither host wall-clock measurements nor host-backed API files certify actual SD or FAT sector durability.

Old unpublished dictionary SAV formats are unsupported. They are never automatically deleted, migrated or interpreted as indexed dictionaries. Preserve them for manual recovery. Per-list metadata is separate and documented in `list-metadata-format.md`: matching TXT/SAV basenames, all directly in `/gbavocab`.
