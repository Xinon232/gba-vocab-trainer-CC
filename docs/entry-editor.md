# Entry editor — local implementation

## Reference and provenance

Reference repository: https://github.com/Xinon232/gbawriter.git

Imported from `30ae9561e601f4b145ddfe46a0dc56a7448078ac` (remote HEAD and tag `v0.3.1`, verified with `git ls-remote`). The local Writer tracked tree matched this commit; its unrelated untracked `latestprompt` was not read or changed. GitHub `/releases/latest` returned 404 because the releases are prereleases. The authenticated releases-list endpoint subsequently verified v0.3.1 as the newest published release, marked prerelease (published 2026-09-07T22:03:06Z), targeting the same exact commit. The release metadata is retained in the evidence directory.

`writer_core.cpp`, `writer_layout.cpp`, `writer_layout.h` and the nine runtime font assets under `references/gbawriter/` are imported verbatim. `writer_core.h` only changes the capacity from a 24KiB document to a 189-byte field. Its core/frame/layout regression tests are retained. GPL-3.0 and upstream SuperFW notices remain in place. No Writer storage, reader footer, dictionary metadata, new sidecar persistence, or on-screen keyboard is imported.

`EntryEditor::consume` adapts Writer's Application event consumer at the save/menu boundary; `entry_render.cpp` adapts its bus-safe bitmap primitives and glyph loop. The body retains x=8, width=220, 16px glyphs and 18px line pitch; heading/field labels reserve the top 36 pixels. The normal footer documents the two-step commands. The deletion-only preview scales long entries to show both captured fields in full.

## Typing controls

Hold a D-pad group and press B / A / R for its first / second / third letter:

- Up: abc; Right: def; Down: hij; Left: klm.
- Hold L for the second layer: Up nop; Right qrs; Down tuw; Left xyz.
- Keep Down held and press R twice for g; L+Down with R twice gives v.
- A alone inserts space; B alone deletes the previous UTF-8 character; both retain Writer's held-repeat schedule.
- Isolated R taps cycle normal → Shift → Caps → normal.
- Start+Left/Right moves a character; Start+Up/Down moves visual rows; Start+L/R moves pages.
- Select starts one provisional punctuation character. While held, directions cycle digits/signs, L/R cycles common punctuation, and letter chords select/cycle accents. Release commits it.
- Start+Select retains Writer's status-bar toggle and cancels a live Select provisional character; it cannot trigger the learning shortcuts while the editor is active.
- Start+A advances from field 1, or confirms field 2. Start+B returns with drafts intact, or cancels field 1.
- Writer's isolated Start newline event is rejected, with feedback: a vocabulary field cannot contain a physical row/tab delimiter.

## Storage contract

The first two tab-separated fields must be nonempty. Existing additional columns are retained byte-for-byte but are not exposed as translation text or edited. The entire raw row, including extra columns, remains limited to 191 content bytes, with 10,000 rows per list. Invalid/overlong rows still make the source read-only. Add writes only the two vocabulary fields.

The live index is not mutated before commit. A bounded EWRAM planned index overlays one replacement row, then uses the existing ownership/identity/alias-safe transaction, exact pre-install row validation and installed fingerprint/index validation. Successful operations leave only the TXT. Transient `.gbvN.tmp/.bak/.txn` files retain the existing recovery semantics.

On a clean list, a sequential physical splice preserves unrelated bytes, existing separators, mixed LF/CRLF, whitespace and an unterminated EOF. Add uses the first observed newline convention, or CRLF for a file without a newline. If learning progress/reordering is pending, final confirmation saves it together with the mutation through the inherited grouped CRLF serializer; the box-separator normalization in that existing progress-save contract still applies. No new columns, comments or metadata are written.

## Verification commands

```sh
bash tests/run_host_tests.sh --correctness-only
make clean LIBBUTANO=/home/halim/Development/gba/butano/butano
make -j2 LIBBUTANO=/home/halim/Development/gba/butano/butano
python3 tests/check_entry_memory.py
```

The memory script gates actual ELF placement, free EWRAM and an explicit IWRAM/application-frame budget; it is not a complete recursive stack-depth proof. Emulator evidence and the final exact ROM hash are recorded outside the working tree in `/home/halim/Desktop/gbavocab/entry-editor-evidence/` and the completion handoff. Host FatFS API failure tests are not a real flashcard or sector-backed FAT test. mGBA uses the actual no-SD built-in fallback, not debugger-seeded document content.
