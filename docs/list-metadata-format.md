# Per-list settings (v1.6.0-pre.3)

All list files are directly in `/gbavocab` at the SD-card root. For example,
`Spanish.txt` is associated with `Spanish.sav`, not `Spanish.txt.sav`. The TXT
contains vocabulary and learning-box separators only. The SAV contains the
front/back language identifiers, mode and preferred dictionary filename, not vocabulary or learning progress. It is an
application-managed SD file, not cartridge SRAM.

The association is by filename basename. Copy or rename both files to retain the
pair. Without the companion metadata, Add from dictionary asks the user to
choose/confirm a pair; the app does not infer it from words or filenames. A chosen
pair is RAM-only until manual save, and is included in dirty-list unload guards.
The metadata filename preserves the complete supported TXT basename, including
UTF-8 bytes (the inherited browser limit is 63 filename bytes including `.txt`).
The inherited TXT replacement transaction requires room for its recovery suffix;
long names that cannot fit that suffix fail without modifying the original TXT.

## Binary SAV v1

The original base is exactly 36 bytes. It remains accepted with no appended records
(default Alternate mode, no preference), and is never rewritten during migration:

| Offset | Length | Value |
|---|---:|---|
| 0 | 8 | ASCII `GVPAIR1` and NUL |
| 8 | 12 | Front language code, NUL-terminated and zero-padded |
| 20 | 12 | Back language code, NUL-terminated and zero-padded |
| 32 | 4 | Little-endian CRC32 of bytes 0–31 (IEEE polynomial) |

Codes are distinct, 1–11 ASCII characters, begin with `a`–`z`, and contain only
lowercase letters, digits or hyphens. They describe TXT column order, not a
ROM-specific dictionary ID. Reversing search direction does not reverse the
stored list pair.

## Appended settings records

A new mode-only SAV may use `GVPAIR0` + NUL at offset 0, empty zero-filled
front/back fields, and the same base CRC. A later committed record establishes
the pair; once present it cannot be changed by any later record. Existing valid
TXT footer pairs are preserved when loading an empty-pair SAV.

Records start at byte 36 and occupy fixed 112-byte slots:

| Offset | Length | Value |
|---|---:|---|
| 0 | 8 | ASCII `GVSET001` |
| 8 | 12 | Canonical front code, zero-padded |
| 20 | 12 | Canonical back code, zero-padded |
| 32 | 64 | Preferred matching `.dict` filename, NUL-terminated/zero-padded, or empty |
| 96 | 1 | Mode: 1 front, 2 back, 3 Alternate |
| 97 | 7 | Zero reserved bytes |
| 104 | 4 | Little-endian IEEE CRC32 over bytes 0–103 |
| 108 | 4 | ASCII `OK01` commit marker |

The filename must be valid UTF-8, at most 63 bytes, end in `.dict` (ASCII
case-insensitive), and contain no control bytes or FAT path/reserved characters.
Catalog language matching is checked before remembering a selected filename.
No ROM-specific index is persisted. Settings-only saves append here without
rewriting TXT. All settings share the existing dirty/manual-save/unload guards.

Write body, sync, commit, sync, close, then reopen/read back before success.
Unchanged settings are deduplicated. Short successful reads are completed in a
bounded loop. Partial tails are padded to the next slot without changing prior
bytes. Slots without the final commit marker are retired; malformed committed
slots block saving/loading settings rather than falling back silently. There is
no automatic SAV compaction or replacement. Back up metadata before resetting.

## Writes, verification and recovery

Existing valid metadata must match the selected pair exactly. Existing corrupt
or conflicting metadata is not overwritten. New metadata is written to
`Spanish.sav.tmp` with CREATE_NEW, checked writes, sync and close, then read back
and validated before rename to `Spanish.sav`. The installed SAV is reopened and
validated again. If both names exist, treat them as ambiguous and never unlink
either: a returned FAT rename error can leave names sharing a cluster chain.
An active-session retry may promote an already verified matching staging record
when no canonical SAV exists. A corrupt/unrecognized staging record is retained
for recovery, not silently reclaimed. A load with a staging file is blocked.

Ordinary successful saves retain only the TXT and SAV. A failed write before any
rename may remove only that attempt's newly created temporary file after its
handle is closed and canonical absence is confirmed. A failed close is tracked
through the shared spare FIL and must be resolved before reusing it.

## Legacy TXT footer migration

The old optional final `# gbavocab: front=en; back=de` line remains accepted for
migration. Migration occurs on manual save: persist and read-back verify SAV
first; only then replace TXT transactionally. A migration-only save copies every
physical TXT byte except the exact footer line and its own terminator. Mixed
LF/CRLF, preceding separators, blank lines after the footer and unterminated EOF
are not normalized by migration-only copying. No new save emits a footer.

Saves with pending entry/learning changes continue to use the inherited grouped
CRLF serializer and box-separator contract. They remove the legacy footer only
after metadata verification. Malformed, duplicate/nonterminal footers or a
footer conflicting with a valid SAV block saving and clear the usable pair.
Metadata failures leave the TXT footer intact. Existing transaction journals,
source identity checks, rollback and FAT alias protections remain in use for TXT.

## Verification

`bash tests/run_list_metadata_tests.sh` tests production storage integration with
the existing FAT API adapter. `--sanitize` independently compiles the same new
storage/parser paths with ASan and UBSan. These are not physical SD-card tests.
The new tests are included in `tests/run_host_tests.sh`.
