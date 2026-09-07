# v0.2.7: TXT preservation and usable long cards

## TXT contract

- Five positional boxes, separated by exactly four blank physical lines. Leading/consecutive separators represent empty boxes; the scanner never collapses them.
- Source/target row bytes, tabs, spaces, annotations, and encoding are preserved. Output uses CRLF. No custom metadata rows, footer, or extra column is added.
- Memory and FatFS share `vocab_scanner.h`. The limit is 191 **content** bytes, independently of LF, CRLF, or final EOF; reads continue past rejected rows and past card 10,000 to detect unsafe truncation.
- Rejected rows make a source read-only. The UI says `READ ONLY: skipped rows`; START cannot overwrite it. Repair malformed/extra-column rows, split files above 10,000 cards, or shorten rows above 191 bytes externally before saving.
- Display scratch supports conversion expansion for a full-length field. Raw storage limits and display byte capacity are deliberately separate.

## Save and recovery

The write path first checks the current source against its load-time whole-file identity and reserves a namespaced transaction slot with a `CREATE_NEW` journal. Its first record (`GBAVOCAB-TXN-2`) contains the original filename and original identity and is synced and closed **before** payload creation. The temporary is then created with `CREATE_NEW`, written, synced, closed, and validated against every expected ordered raw row and its intended box. A second ready record (`GBAVOCAB-TXN-1`) containing original/replacement identities is appended, synced, and closed without overwriting the reservation. The source identity is checked again immediately before renaming the original to backup and promoting the temporary. After replacement the exact row/box comparison is repeated before backup retirement.

Names are `original.txt.gbv1.tmp`, `original.txt.gbv1.bak`, and `original.txt.gbv1.txn`, with bounded slots `gbv1` through `gbv9` to skip retained or unknown artifacts. Generic legacy `original.tmp` and `original.bak` are never automatically used or deleted. A 64-byte path buffer currently limits savable original names to 54 bytes; longer names fail rather than truncate.

At startup the directory scan also discovers transaction journals, so a missing original can be restored and appear in the browser. Recovery compares complete physical-file length and two streaming checksums, not a nonzero parsed-row count. Journal magic, checksum, and matching original filename establish transaction ownership. This is accidental-corruption detection, not a cryptographic tamper-proof scheme.

A partial replacement found after interruption is retained as a namespaced temporary when restoring the matching backup. Unknown/partial recovery material is not automatically erased at startup; intact originals remain retryable in another free slot, but exhausting all nine slots requires external inspection. A torn initialization or orphan namespaced file is not sufficient proof of ownership. A valid reservation with an incomplete payload also retains that payload because a crash around `CREATE_NEW` cannot prove that it was not a collision. Probe/read errors abort recovery without interpreting unreadability as absence or identity mismatch. Normal successful transactions leave no journal/sidecar; failed cleanup retains recovery artifacts.

FatFS can leave two names sharing one allocated chain after a partial rename. Before recovery mutations and live rollback/cleanup, opened `FIL.obj.fs` and `FIL.obj.sclust` identities are compared across original, temporary, backup, and journal. A shared nonzero first cluster, invalid nonempty-file identity, or identity-probe/open/close error stops with `RECOVERY REQUIRED`, preserving artifacts instead of unlinking a chain still used by another name. Retry/startup does not bypass this check through another slot. Zero-cluster empty files are not treated as aliases. There is no automatic FAT repair: preserve the card and inspect/recover it externally. This bounded check detects shared first clusters, not arbitrary overlapping chain tails or corruption elsewhere on the volume.

If rollback cannot restore a verified original, old offsets are not used to display an uncertain replacement. Conversely, once replacement validation commits the new TXT, its index is installed and dirty state cleared even if reopening fails. Both UI save paths remap the selected card and invalidate stale undo/navigation based on index installation, independently of the boolean save result; a reopen failure remains visible as `SAVED - REOPEN FAILED`.

## UI

- Selecting a different list with dirty progress asks **A Save / B Discard / SELECT Cancel**. Save/load failure retains the old viable file and progress; navigation resets only when a new list is successfully installed.
- A reordered successful save clears undo and remembered per-box numerical indices and preserves the selected card.
- Existing fonts, colors, buttons, and feedback styling remain. Text wraps at UTF-8 codepoint boundaries using the selected font generator's pixel width, two lines per side per page.
- **Hold L + Left/Right** pages text without changing box/mode. A plain L tap changes direction on release; paging resets on card/box navigation. A shorter side stays on its last available page while the longer side continues. Read/display failures replace the stale card with an error message.

## Interchange verification

The coordinating session uploaded an actual synthetic TXT emitted by `vocab_export_grouped` through dict.cc's file input, reached `FINISHED / All entries imported`, and read back both accented/annotated vocabulary pairs from Maintain. **dict.cc normalizes empty groups:** its import pipeline trims leading/trailing blanks and collapses repeated separators. Therefore vocabulary interchange was verified, but learning-state gaps round-trip only locally, not through the website. The private user sample was not uploaded or added to this repository.

## Executed verification

`bash tests/run_host_tests.sh` builds/runs the host regressions, including production FatFS I/O linked against a host file adapter. Tests exercise replacement corruption/rollback/retry, owned startup recovery, missing-file load retention, legacy-file isolation, shared scanner limits, raw-row preservation, empty boxes, prompt choices, UTF-8 layout, and stale undo invalidation. The adapter uses an isolated `mkdtemp` directory, not a shared destructive fixture path. The old memory-only writer estimate was removed; real adapter I/O statistics remain available through `vocab_file_io_stats()`.

The adapter also models partial rename aliases with shared allocated-chain identities and destructive FAT-style unlink semantics (not POSIX hard-link retention). Integrated regressions cover original/backup and original/temporary aliases after both returned errors and simulated interruption, backup/temporary aliases, same-process retry and startup refusal with zero mutations, unknown-file preservation, and recovery probe/open/read/close errors. This is an API-level model, not a sector-level FAT or hardware certification.

The implementation worker also built the real ROM with:

```
make -j2 LIBBUTANO=/home/halim/Development/gba/butano/butano
```

The incremental target build completed through `Linking ROM...`, `Fixing vocab.gba...`, `ROM fixed!`. Emulator/hardware acceptance and independent fault-path review are separate release gates; host test success is not proof against every SD error or interruption.
