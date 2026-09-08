# Entry editor

Learn vocabulary with flashcards and create your own word lists on your Game Boy Advance. Load or create UTF-8 TXT lists in `/gbavocab` at the SD-card root. The [full current controls](full-controls.md) cover home, learning, every typing group, punctuation, accents, repeat, caret navigation and saving. The accompanying PDF is authored by Halim Jarrar.

## Reference and provenance

Reference repository: https://github.com/Xinon232/gbawriter.git

Imported from `30ae9561e601f4b145ddfe46a0dc56a7448078ac` (remote HEAD and tag `v0.3.1`, verified with `git ls-remote`). The local Writer tracked tree matched this commit; its unrelated untracked `latestprompt` was not read or changed. GitHub `/releases/latest` returned 404 because the releases are prereleases. The authenticated releases-list endpoint subsequently verified v0.3.1 as the newest published release, marked prerelease (published 2026-09-07T22:03:06Z), targeting the same exact commit. The release metadata is retained in the evidence directory.

The original import brought `writer_core.cpp`, `writer_layout.cpp`, `writer_layout.h` and the nine runtime font assets under `references/gbawriter/` across verbatim. `writer_core.h` changes the capacity from a 24KiB document to a 189-byte field. Its core/frame/layout regression tests are retained. This provenance describes the import, not a claim that every current file still matches upstream. GPL-3.0 and upstream SuperFW notices remain in place. No Writer storage, reader footer, dictionary metadata, new sidecar persistence, or on-screen keyboard is imported.

`EntryEditor::consume` adapts Writer's Application event consumer at the save/menu boundary; `entry_render.cpp` adapts its bus-safe bitmap primitives and glyph loop. The body retains x=8, width=220, 16px glyphs and 18px line pitch; heading/field labels reserve the top 36 pixels. The normal footer documents the two-step commands. The deletion-only preview scales long entries to show both captured fields in full.

## Typing controls

Learning Start+Select opens the editor in either press order and consumes release tails; isolated learning Start saves and Select opens home on release. Within drafts these buttons belong exclusively to Writer's input engine.

Hold a D-pad group and press B / A / R for its first / second / third letter:

- Up: abc; Right: def; Down: hij; Left: klm.
- Hold L for the second layer: Up nop; Right qrs; Down tuw; Left xyz.
- Keep Down held and press R twice for g; L+Down with R twice gives v.
- A alone inserts space; B alone deletes the previous UTF-8 character; both retain Writer's held-repeat schedule.
- Isolated R taps cycle normal → Shift → Caps → normal on release. Shift applies to the next accepted letter, including accents; spaces and punctuation retain it. Caps is persistent until another isolated R tap. The ß alternate has no uppercase substitution.
- Start+Left/Right moves a character; Start+Up/Down moves visual rows; Start+L/R moves pages.
- Select outside a still-held producing letter chord starts one provisional punctuation character. While held, directions cycle digits/signs, L/R cycles common punctuation, and letter chords select/cycle accents. Release Select to keep it.
- Alternatively, after typing a letter, keep its exact direction, producing B/A/R button and L layer (if used) continuously held, then press Select to accent that same letter without a duplicate letter or period. There is no timer; a released/changed chord is no longer eligible. Its original case is retained, and a base without alternates stays unchanged. For example, keep Up+B held, then press Select: a becomes á. Keep Select and the group held; release/repress the producing B/A/R button to cycle. Release Select to keep the result.
- Start+Select retains Writer's status-bar toggle. It removes only a newly inserted Select provisional character, not an existing letter converted to an accent; it cannot trigger learning shortcuts inside the editor.
- Start+A advances from field 1, or confirms field 2. Start+B returns with drafts intact, or cancels field 1.
- Writer's isolated Start newline event is rejected, with feedback: a vocabulary field cannot contain a physical row/tab delimiter.

Full symbol sequences, every accent and the held-repeat schedule (24-frame delay, 5-frame interval for isolated A/B and Start navigation) are listed in [full controls](full-controls.md). No filename editor, clipboard or on-screen keyboard is implied by importing the input engine.

## Confirmation and session Autosave

The menu order is Add entry, Edit entry, Delete entry, Autosave. Up/Down selects; A opens an action or toggles Autosave; B returns to learning. Autosave is OFF at every app startup, RAM-only, and survives list/menu changes during that session. `EntryEditor::open` does not reset it.

Add/Edit use Word/front (1/2) and Translation/back (2/2). Start+A advances and then confirms both fields; Start+B from 2/2 returns with drafts intact, and from 1/2 cancels to the menu without mutation. Reopening Add/Edit starts fresh drafts. Add inserts first in Box 1. Edit targets the captured card and preserves its box, relative position and learning progress. Delete previews the captured fields, defaults to No, and uses Left/Right for No/Yes, A to confirm and B to cancel. Up/Down does not change its answer. The final deletion leaves an empty usable list.

OFF confirmations apply immediately to the active list in RAM with zero TXT rewriting. Learning Start saves all pending entry changes plus box progress; choosing Save before switching lists does likewise. ON saves every confirmed mutation together with all pending changes. Switching ON alone does not flush. No per-keystroke save occurs and canceling a draft never applies it. This does not introduce separate learning-progress autosave.

Before loading or creating another list, a dirty list prompts A Save / B Discard / Select Cancel. Failed save/load keeps active data. NEW LIST displays an unused LIST001.TXT–LIST999.TXT name checked against real storage and recovery ownership; A creates exactly that name without overwriting. PC renaming is supported; no filename-typing UI exists.

## Storage contract

The first two tab-separated fields must be nonempty. Existing additional columns are retained byte-for-byte but are not exposed as translation text or edited. The entire raw row, including extra columns, remains limited to 191 content bytes, with 10,000 rows per list. Invalid/overlong rows still make the source read-only. Add writes only the two vocabulary fields.

OFF uses a bounded EWRAM pending-row pool and updates the active index only after validation. Up to 128 concurrently added/edited rows can remain pending; an edit of a pending row reuses its slot, and deletion consumes no slot. At capacity, `RAM FULL - save list first` rejects a new pending row without changing the active list and retains its draft. Manual save is in learning: note the rejected draft before canceling back to learning to save, then re-enter it after reopening the editor.

ON uses a separate planned index and replacement overlay with the existing ownership journal and allocation-chain/alias probes. Output writes/sync/close are checked; one installed TXT scan checks the write-derived fingerprint and planned offsets, boxes and counts before the original backup is retired. An ordinary artifact-free save has no independent full-original identity rereads or pre-install exact-row comparison. Pending-row offsets are resolved by the same serializer, so the next confirmed mutation flushes pending entries and progress too. Normal successful cleanup leaves only the TXT; applying OFF is not a disk save. Transient `.gbvN.tmp/.bak/.txn` files support best-effort recovery on failure. Uncommitted failures retain drafts/data; installed-but-failed reopen is reported without inviting duplicate mutation retries.

Keep backups; do not remove power/card while saving or externally edit/swap a loaded source while the app uses it. Same-size external changes are not independently detected, and late changes can be overwritten. There is no independent proof that generated rows equal the original source, no guaranteed atomicity and no arbitrary power-loss recovery guarantee. Exceptional recovery/rollback paths may still read full identities. See [current simpler-save technical notes](file-io-simple-save.md), including the four injected-error orphan-space cases: all passed content/live-state/retry checks, but failed read-only filesystem checks. These are observed allocation-cleanup failures, not evidence of normal-operation TXT loss.

For an immediate ON mutation on a clean list, a sequential physical splice preserves unrelated bytes, existing separators, mixed LF/CRLF, whitespace and an unterminated EOF. Add uses the first observed newline convention, or CRLF for a file without a newline. If entry changes or learning progress/reordering are pending, saving uses the inherited grouped CRLF serializer; its box-separator normalization still applies. Manual learning save flushes all changes with the five learning boxes. No new columns, comments, metadata, permanent `.sav`, settings or sidecar files are written.

No built-in/demo vocabulary ships. Arabic-specific shaping/font/glyph support is removed while preserving user-authored bytes and other supported scripts. General UTF-8 handling remains; retained font coverage is finite. Developer fixtures are not a user-facing fallback.

## Verification commands

```sh
bash tests/run_host_tests.sh --correctness-only
make clean LIBBUTANO=/home/halim/Development/gba/butano/butano
make -j2 LIBBUTANO=/home/halim/Development/gba/butano/butano
python3 tests/check_entry_memory.py
```

The memory script gates actual ELF placement, free EWRAM and an explicit IWRAM/application-frame budget; it is not a complete recursive stack-depth proof. These are commands for the implementation/build owner, not claims that the documentation worker ran them. Historical import evidence lived under `/home/halim/Desktop/gbavocab/entry-editor-evidence/`; its former built-in fallback is not part of this release. Current controls-source hashes and PDF visual QA are recorded in `/home/halim/gba-suite-release/gbavocab-pdf-HANDOFF.md`. Parent owns the final ROM build and review. Host API tests, sector-backed FAT tests, emulator checks and real hardware checks are distinct evidence classes and must not be substituted for one another.
