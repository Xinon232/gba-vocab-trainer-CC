# v0.2.6 I/O and save reliability

v0.2.6 is an incremental reliability release on the signed v0.2.5 baseline. The v0.2.3 buffered scanner, structural indexer, current-card cache, grouped transactional rewrite, recovery, and dirty-on-success behavior remain in place.

## Remaining changes

- The loaded SD vocabulary now retains one buffered FatFS read handle. Card transitions seek and read through that handle instead of opening and closing the same TXT file for every cache miss. File switches and replacement saves close it first. After a replacement is validated and committed, the new index is installed before reopening so even a transient reopen failure cannot leave stale offsets retrying against the regrouped TXT.
- Unchanged frames still hit the bounded current-card cache and perform no additional reads or display parses.
- Pre-commit save failures remain retryable and now leave a persistent `SAVE FAILED` indicator on screen. Starting a new save replaces it with `save...`; success clears it. Post-commit cleanup/reopen failures never retain stale dirty metadata.
- The obsolete 256-byte `VocabFile::current_line_buf` was removed. One bounded row scratch remains local to the actual display/save operation.
- SRAM `.sav` persistence was removed. Learning progress remains encoded only by the five grouped sections in the vocabulary TXT. The `.tmp` and `.bak` names are transient recovery files used only while safely replacing that TXT; they are not persistent sidecars.

## Existing guarantees retained from v0.2.3-v0.2.5

- Indexing is a single buffered sequential structural pass and does not perform display conversion or Arabic shaping.
- Current-box empty checks use `field_counts[]` and are O(1).
- A clean START save performs no write or reindex.
- A changed save writes and syncs a temporary, replaces with backup recovery, and validates/reindexes the new TXT. Once that replacement is authoritative, its new index is installed atomically in memory; backup cleanup is recoverable on reload and cannot make the committed content retry through stale offsets.
- Vocabulary text is not mirrored into RAM and files up to 10,000 cards remain supported.

## Verification

Host regressions cover persistent-handle lifecycle, unchanged-frame cache behavior, chunk-boundary scanning, 10,000-card indexing, grouped round trips, transaction failure injection, visible save-failure text, and absence of SRAM save dependencies. The release ROM is built separately with devkitARM/Butano when available.
