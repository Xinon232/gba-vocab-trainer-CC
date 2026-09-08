# v0.2.11: approved validation passes and retained loading handle

> Historical version notes, not the current V1.0 save guarantees. See [current simpler-save contract and limitations](file-io-simple-save.md).

## Scope and fault model

This release changes only the approved load/save validation contract over v0.2.10. It does not change TXT grammar, rendering, input, learning-box grouping, transaction ownership, journal format, aliases, recovery, or committed-State outcomes.

- **Before installation:** compare the replacement against every ordered original raw row and intended box exactly once. The same replacement scan verifies generated offsets, boxes, counts, grammar, and complete physical fingerprint.
- **After installation:** scan the installed physical bytes and verify the intended fingerprint, generated index/offsets, boxes and counts. Do not repeat the exact original-row comparison. Keep the original backup until this validation succeeds; inherited rollback/restore or fail-closed recovery retains the known-good original when validation fails. Chain-identity probes of transaction names still occur and are not payload comparisons.
- **Opening:** open the candidate once, index and fingerprint through that same `FIL`, and retain that exact object for training. There is no independent opening whole-file reread or close/reopen handoff.

The fingerprint is the existing size plus two noncryptographic accumulators. It detects accidental physical changes, not malicious collisions. A resealed changed or reordered file can pass fingerprint/index validation while failing the pre-installation exact-row validator; tests deliberately show this distinction. No second opening pass detects concurrent writers, card swaps, or inconsistent reads. This is an explicitly accepted tradeoff, not equivalent fault coverage or cryptographic authentication. Opening still rejects I/O errors and successful short reads against the opened size, and retains the existing row/entry limits and read-only rejected-row behavior.

**Independent source verification before saving and again before promotion remains.** A source edit after loading must not be overwritten using the old index. Recovery can still read transaction payloads as before; the no-original-payload-read contract describes successful post-installation validation, not rollback or startup recovery.

## FIL lifecycle and bounded memory

Two stable `FIL` objects live in explicit `.sbss` EWRAM. Only active/candidate pointers swap; the objects themselves are never copied. The index/fingerprint candidate becomes the live training handle only after scanning succeeds and the old active handle closes successfully. Failed candidate open/scan or old-active close leaves the old `VocabFile`, source name, fingerprint, generation and display cache intact.

If candidate cleanup fails, the spare stays marked open and cannot be overwritten. The next SD load, mutating save, or remount must close it before proceeding. This also covers failed same-filename reloads: a spare lock must not survive into a source rename. Remount aborts on close errors rather than forgetting a live object; an already invalid object can be discarded during remount. No extra index or text cache is added. The three exclusive 4 KiB windows, existing metadata scratch, and one-card display cache remain bounded and unchanged.

## Preserved contracts

- 10,000 cards and 191 raw content bytes per row; unchanged accepted CRLF/LF/final-EOF parsing and rejected-input read-only policy.
- Exact raw row bytes, tabs, annotations, UTF-8/legacy bytes, stable box grouping, four physical blank separators, empty first/middle boxes and CRLF output.
- No metadata rows, extra columns, footers, permanent sidecars, fast-seek map, renderer/input/font/layout changes, paging or scrolling.
- Source-before-overwrite checks, complete writes/syncs, alias/chain identity fail-closed checks, reservation/ready ownership, rollback/recovery and backup retirement.
- Existing committed-index installation, dirty state, `SAVED - REOPEN FAILED`, selected-card State remapping and stale-undo clearing.

## Verification and limits

Run `bash tests/run_host_tests.sh --correctness-only`. It retains prior correctness faults and adds semantic same-FIL training assertions, failed candidate/active-close cache and live-handle preservation, spare-close retries and repeated switches, same-name spare save/remount guards, no original payload reads after successful promotion, exact-versus-fingerprint regressions, and installed row/tail/offset/box/count failures retaining the original backup when rollback is blocked.

Build the actual ROM with `make -j2 LIBBUTANO=/path/to/butano/butano`. Local implementation QA records source/ROM hashes, host-suite and ROM build logs, and sanitizer checks using isolated copies of the private sample and maximum-row edge fixture. No timing, throughput, benchmark or read-count measurements were run; this release makes no measured speedup claim. The host adapter is not native sector-backed FatFS or hardware evidence. Native image formatting was not attempted. Independent review, exact-ROM emulator QA, CI and publication remain separate coordinator gates.
