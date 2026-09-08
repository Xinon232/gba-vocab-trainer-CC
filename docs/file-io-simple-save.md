# gbavocab V1.0: current simpler-save contract

These notes supersede the safety/pass-count descriptions in the historical `file-io-v0.2.*.md` documents. The editor, input, Autosave and learning controls are unchanged; see [full controls](full-controls.md).

## Practical use

Keep backups of your vocabulary TXT files. Wait for saving to finish; do not remove power or the card while saving. Do not externally edit, replace or swap the loaded TXT/card while the app is using it. After an error, keep the session and recovery files where possible; back up the card before attempting manual recovery. Temporary files and checks do not guarantee atomic replacement or recovery after arbitrary faults.

## Ordinary successful save, with no old transaction artifacts

1. Read original rows through the existing 4 KiB cache to construct output; pending rows come from RAM. Clean immediate entry mutations use their sequential physical splice, preserving unrelated bytes and line endings. Pending learning/entry saves retain grouped CRLF output and the five-box separator convention.
2. Buffer replacement writes and check complete writes, flush/sync and close. Compute intended physical identity during successful writes and record intended offsets, boxes, field counts and total count during construction.
3. Scan the installed canonical TXT once. Check parser/rejected rows, physical identity/length, total count, field counts, intended box assignment and every planned offset. This scan does not read original or backup payload.
4. Install the validated index, then reopen without another payload reread. Keep the original backup until installed validation succeeds. Unchanged saves retain their no-rewrite bypass.

There is no independent complete-original identity read before writing or immediately before promotion, and no independent temporary-file validation or exact original/output row comparison before installation. This is reduced validation, not equivalent guarantees or a measured speed ranking.

Metadata work remains: closes/reopens, bounded transaction-name probes, a conservative source-size check, allocation-chain/alias probes, renames and cleanup. These are not full TXT scans. Load retains its combined index/fingerprint scan and exact stable file handle; card caches and all three 4 KiB workspaces remain.

## Temporary ownership and exceptional recovery

The bounded `.gbv1` through `.gbv9` temporary/backup/journal scheme is retained. Reservation `GBAVOCAB-TXN-2`, appended ready `GBAVOCAB-TXN-1`, older accepted journal magic, record layout/checksum and identity interpretation are unchanged. The before identity comes from the already-indexed source; the after identity comes from checked writes. Ready is appended after complete output write/sync/close, not after an independent exact-row proof.

CREATE_NEW reservations, unknown/torn reservation preservation, occupied-slot preservation and allocation-chain/alias checks remain. Generic unowned `.tmp`/`.bak` files are not adopted or deleted. Unresolved identity-probe errors block destructive cleanup/adoption. Normal successful cleanup leaves only the TXT; no new permanent sidecars, settings files, `.sav` writes or TXT metadata are introduced.

Independent full-identity reads still exist on exceptional paths: old-artifact recovery, quarantined-source reconciliation and failed-save rollback/readback. Old journals can therefore add payload reads on a retry. Recovery remains best-effort: ambiguous replacements are not adopted with stale physical offsets. Pending RAM rows remain available even if physical-source access must be quarantined. Tested transient pre-promotion failures restore the original and usable live source while retaining dirty pending data; tested installed failures can reconcile a validated intended index without a duplicate retry.

## Explicit validation limits

- Same-size external source changes are no longer independently detected. An observed size change before construction is rejected, but later changes are not guaranteed to be caught. Tests demonstrate a same-size edit being consumed and a late append being overwritten.
- No independent source-versus-output byte-equivalence proof remains. Consistently wrong but structurally valid generated rows with a matching identity/index can pass. Tests explicitly accept resealed same-size changes and reordered rows when the intended identity is also changed.
- Bytes differing from the actual write-derived identity, malformed/truncated/tail-corrupted output and incorrect generated offsets, boxes or counts still fail validation. Fingerprints are noncryptographic, not adversarial authentication.
- No guarantee covers arbitrary persistent faults, torn sectors or power cuts. Real flashcard storage and future writes on media containing orphan allocations remain unqualified.

## Known injected-error filesystem-cleanup limitation

The storage owner's final native run used production C++ and FatFS on fresh sector-backed 16 MiB FAT16 images, injecting one transient pre-transfer read or write request failure per fault case. It recorded **174 content/live-state/retry passes**: the no-fault baseline, 152 read ordinals and 21 write ordinals. Read-only `fsck.fat -n` passed on 170 final images and returned 1 on four:

| Injected case | Orphan allocation | Content/live-state/retry |
|---|---|---|
| read-133 | 2 clusters / 4096 bytes | PASS |
| write-16 | 2 clusters / 4096 bytes | PASS |
| read-150 | 1 cluster / 2048 bytes | PASS |
| write-19 | 1 cluster / 2048 bytes | PASS |

These are allocated clusters no longer reachable by a directory entry: lost allocation space, not observed normal-operation TXT data loss. Passing content checks does not prove filesystem allocation health. The native runner deliberately exits 1; do not describe the native filesystem suite as all-green or as power-loss qualification.

A separate write-19 trace reached `f_unlink -> remove_chain -> get_fat -> move_window -> sync_window -> disk_write` during journal cleanup. The injected error occurred after marking the directory entry removed but before releasing its chain. A later flush can persist the deletion while leaving the chain orphaned; retrying a missing pathname cannot reclaim it. This diagnoses the traced category, not every low-level site in all four cases. Broader FatFS allocation repair was outside the storage change's scope and remains unresolved.

## Evidence and scope

These results are reported from the storage handoff, not rerun by the documentation worker:

- `/home/halim/gba-suite-release/gbavocab-simple-save-HANDOFF.md`
- `/home/halim/gba-suite-release/gbavocab-simple-save-native-final-comments/run-Uyaigr/`
- `/home/halim/gba-suite-release/gbavocab-simple-save-evidence/`

The handoff reports host correctness, sanitizer, ROM build, memory-budget and exact-ROM no-card cold-boot passes separately. Neither host tests nor a no-card emulator boot substitute for real storage/hardware qualification. This document does not grant publication approval.
