# v0.2.10: bounded SD windows and combined readback validation

## Scoped changes

- Scanner/identity reads, grouped output writes, and source-row reads each have an exclusive **4 KiB**, four-byte-aligned static EWRAM window. Random source cache refills begin on 512-byte sector boundaries, including backward/shuffled offsets. The source cache now also serves grouped writing; nearby rows can reuse already buffered bytes.
- Grouped writing records offsets, boxes and counts in the existing save-only index scratch as rows are appended. Buffered-but-not-yet-flushed output contributes to the next offset; failed writes, sync or close do not publish a successful output identity or install this index.
- The shared scanner can visit accepted raw rows without constructing another index. Before and after promotion, a single replacement scan checks every generated offset and box, compares every ordered raw row with its original/backup source, validates grammar/counts, and computes the complete physical-byte identity. The separate replacement-row reread is removed. A generated index is not evidence of persisted correctness and is installed only after these checks succeed.
- Scanner, identity and source windows reject successful short non-EOF reads. Actual opened file size, read/seek/close errors, complete writes and output sync remain correctness gates.

This reduces duplicate work in the implementation; **no timing, throughput, benchmark or read-count comparison was run**. It does not establish a measured speedup or rank this release against v0.2.6 or any other release. Independent source identity checks remain: opening and saving are not whole-operation single-pass algorithms.

## Buffer ownership and capacity

| Phase | Scanner/identity window | Output window | Source window |
|---|---|---|---|
| Load/index or standalone identity | Exclusive reader | Idle | Idle |
| Grouped temporary writing | Idle | Exclusive writer | Exclusive source-row reader |
| Pre/post-promotion validation | Exclusive replacement scanner | Idle | Exclusive original/backup-row reader |
| Journal/recovery probes | Identity helper only when no live scanner | Idle | Idle |

All windows have operation-local cursor state; no source window survives a rename, retry or reopen. There is no heap allocation, extra `VocabFile`, CLMT map, or per-card text cache expansion. The existing single-card display cache is unchanged.

The verified local ELF places all three 4096-byte windows in `.sbss` EWRAM. EWRAM heap begins at `0x02025ea8`, leaving 106,840 bytes before `0x02040000`. The linker provides a 21,052-byte IWRAM user-stack budget between static storage and `__sp_usr`; largest individual compiler-reported frames are 2,152 bytes in the I/O translation unit and 3,328 in `main`. These are placement/capacity and individual-frame checks, **not a complete nested runtime stack-depth proof**. Recheck the actual release ELF if toolchain or build configuration changes.

`diskio.c` forwards multi-sector counts to `sdcard_read_blocks` / `sdcard_write_blocks`. The production Supercard driver uses CMD18/CMD25 with the requested block count; no driver change is required. Source inspection establishes API/path support, not real-hardware testing.

### Fast-seek decision

`FF_USE_FASTSEEK` remains **0** in both configuration headers. A bounded cluster map needs a fragmentation-dependent sizing/fallback policy, setup work, and verified invalidation across close/replacement/recovery. It is not necessary for these scoped duplicate-work reductions, so this release adds neither the configuration/API variation nor map RAM. No claim is made that maps are unsuitable for every file; they remain a separately reviewable candidate.

## Preserved contracts

- Exact raw vocabulary row bytes, tabs, Unicode/legacy bytes and annotations. Accepted CRLF/LF/final-EOF input grammar is unchanged; grouped output uses CRLF and four physical blank separators, preserving empty first/middle boxes.
- Unchanged 191-content-byte row and 10,000-card limits; rejected input remains read-only.
- No metadata rows, extra columns, footers or permanent sidecars. Transaction/recovery names and journal format are unchanged.
- Independent source-change checks, exact ordered-row comparisons, physical tail identity, alias/chain fail-closed checks, reservation/ready ownership, rollback, post-promotion validation and backup retirement remain. Identities are accidental-change detection, not cryptographic authentication or concurrent-writer locking.
- Committed index installation, dirty-state behavior, `SAVED - REOPEN FAILED`, and State remapping/undo contracts remain unchanged.
- Renderer, UI, controls, fonts, layout, paging and scrolling are untouched.

## Verification

`bash tests/run_host_tests.sh --correctness-only` includes the production FatFS API adapter and skips the existing optional I/O performance suite. New tests cover scanner visitation/abort and unchanged grammar; generated index/readback agreement; empty boxes; shuffled/backward offsets; maximum rows across windows; a 10,000-card maximum-row fixture; wrong generated offsets; parseable changed/reordered rows with recomputed physical identity; and successful short non-EOF reads. Existing source-edit, physical-tail, alias, interrupted-save/recovery, rollback and committed-State faults remain enabled.

Actual ROM build:

```sh
make -j2 LIBBUTANO=/path/to/butano/butano USERCXXFLAGS=-fstack-usage
```

Local host/ROM and sample-copy sanitizer QA do not prove real flashcard behavior. Exact-ROM emulator boot is a separate coordinator gate and exercises fallback UI, not Supercard storage. Native sector-backed FatFS QA was prepared but not executed because filesystem-image formatting was blocked by tool policy; no native-filesystem or hardware pass is claimed. Publication requires independent review, CI, and verified release/tag/downloaded-asset checks by the coordinator.
