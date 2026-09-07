# v0.2.9: scoped buffered SD I/O

## Changes

- Opening computes physical-file size and both existing checksums from the same buffered stream as the card index. An independent whole-file identity verification remains before installation: this is **not a single-pass open**. Scanner and identity reads reject premature EOF against the opened FatFS file size; read and close failures still abort.
- Grouped output computes its identity as complete buffered writes succeed. The durable ready journal uses that identity, rather than reopening and hashing the temporary separately. Failed/partial writes, sync failures and close failures do not publish a ready transaction.
- Both pre-rename and post-rename replacement indexing compute readback identity and compare it with the intended output identity, including all separators and trailing bytes. Extra blank tail corruption cannot be accepted simply because card counts and boxes match.
- Exact bytewise comparison of **every ordered raw vocabulary row and its box** remains before and after promotion. These comparisons use separate bounded windows for source and replacement, reusing the existing two 512-byte scratch buffers. Nearby rows reuse buffered bytes; shuffled/distant source rows can still require seeks. The index/readback and exact-row comparison remain separate validation stages.

## Unchanged contracts

TXT row content, Unicode bytes, tabs and annotations are preserved; grouped output uses CRLF and exactly four blank physical separators, including empty boxes. The 10,000-card cap and 191-content-byte row limit are unchanged. Rejected input remains read-only. No permanent sidecars, UI/render changes, font changes, paging or scrolling are introduced.

Source identity is still checked before save and immediately before renames. Reservation/ready journal layout, recovery ownership, alias/chain fail-closed checks, rollback, post-rename validation and backup retirement retain the existing [recovery contract](file-io-v0.2.7.md#save-and-recovery). Committed index installation and stale-State/save-outcome behavior are unchanged, including `SAVED - REOPEN FAILED`.

These identities detect accidental changes; they are not cryptographic authentication or a concurrent-writer locking protocol. The independent checks remain important and do not certify arbitrary concurrent mutation or underlying FAT integrity.

## Verification scope

Run `bash tests/run_host_tests.sh --correctness-only`. This explicitly skips `test_vocab_file_io_perf`; the original runner default remains available. Release CI uses correctness-only mode. No performance measurements, benchmark runs, timing instrumentation or performance comparisons were performed for this change.

Focused production-adapter regressions cover independent source/output journal identities across buffered boundaries, mixed LF/CRLF, a maximum-length row, non-ASCII bytes and final EOF; recovery/retry; premature EOF during open; and otherwise-parseable extra blank output corruption. Existing fault cases retain source-edit, partial-write, rollback, alias, failed reopen and stale-State coverage.

The real ROM build uses `make -j2 LIBBUTANO=/home/halim/Development/gba/butano/butano`. API-level host tests and a ROM build are not real flashcard validation. Emulator, independent sample checks and release publication are separate coordinating-session gates.
