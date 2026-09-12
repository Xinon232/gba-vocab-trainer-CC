# gbavocab

Learn vocabulary with flashcards and create your own word lists on your Game Boy Advance.

Use five learning boxes to practise vocabulary, and add, edit or delete entries on the console. Put UTF-8 `.txt` vocabulary lists in `/gbavocab` at the SD-card root (for example `/gbavocab/Spanish.txt`), then choose **LOAD LIST**. Choose **NEW LIST** to create an empty list on the SD card.

The app is built with Butano and targets SuperFW / Supercard SD-style setups. TXT files are compatible with dict.cc-style vocab-trainer exports. The home screen reads `gbavocab V1.6`. V1.2 retains both cards and the existing green/red feedback while A/B is held, then for 24 frames after release (about 0.4 seconds, matching GBAWriter's initial solo-A repeat delay). Holding does not consume this delay. Grading remains on press; only advancement waits longer.

Each vocabulary file can contain up to 10,000 entries. The text is streamed from the SD card, so smaller files retain their normal per-file loading, saving, and training performance.

Current flashcard text support uses SuperFW-derived fonts for broad language compatibility:

- Latin Extended (`U+0080–U+024F`) for Western/Central European languages and phonetic/diacritic-heavy entries
- Greek and Cyrillic (`U+0370–U+04FF`) including Russian, Ukrainian, Bulgarian, Serbian/Macedonian-style Cyrillic extensions, and Greek
- Japanese punctuation, Hiragana, and Katakana (`U+3000–U+30FF`)
- CJK Unified Ideographs (`U+4E00–U+9FEF`) plus SuperFW's included CJK Extension-B subset (`U+20000–U+200CC`) for Chinese/Japanese/Korean Han characters
- Korean Hangul syllables (`U+AC00–U+D7A3`)
- Imported Arabic uses actual Ghoulam contextual glyphs and lam-alef, RTL runs and display-only harakat filtering. Latin/numbers/punctuation remain SuperFW. Both fields, the editor and Delete preview are supported; no Arabic typing layout was added. Original TXT bytes and logical UTF-8 caret positions are retained. See `docs/arabic.md` and the full-controls manual.

## V1.6 local dictionaries and PC builder

Build a standalone ROM with your own UTF-8 dict.cc-style exports using the graphical Windows executable or Linux builder. No GBA compiler is required by end users. Extract the whole ZIP and launch `gbavocab-builder.exe` (Windows) or `gbavocab-builder` (Linux). Add exports, give each dictionary a name and language codes/labels, then choose the output `.gba`. No copyrighted dictionary is bundled.

The PC builds both directional indexes; dictionary text and indexes remain in ROM. Capacity tests use 40,010 synthetic entries, separately from the 10,000-entry learning-list limit. The builder enforces the 32 MiB ROM ceiling and 191-byte editable-pair limit. Search is live ASCII-case-insensitive prefix matching; other Unicode characters match exactly. Up to 16 dictionaries can be packaged.

In lookup, hold **Start+Up/Down** to browse results, **Start+A** to select, **Start+L** to change direction and **Start+R** to choose a dictionary. **Start+Left/Right** retains the typing caret; release Start to type. **Start+B** cancels. Home lookup chooses a destination TXT after selecting a result, using the existing dirty-list Save/Discard/Cancel guard. Entry-menu lookup adds to the active list. One matching dictionary opens directly; multiple matching dictionaries show a chooser.

The optional final TXT footer `# gbavocab: front=en; back=de` identifies list columns independently of ROM dictionary IDs. First use asks for the pair; it stays in RAM until manual save. Remove that line on PC to reset the pair. Older versions may regard it as a rejected row. Full grammar and complete controls: [manual](docs/full-controls.md).

Developer checks: `bash tests/run_dictionary_tests.sh`; `python3 tests/test_builder_app.py` after a ROM build; `python3 builder/app.py --template gbavocab.gba`. Packaged builds use `builder/package.py` and the branch-scoped dictionary-builder Actions workflow. No workflow publishes releases.

## V1.5 input layout

The letter groups, active-group labels, accents and continuously held double-R g/v gestures match gbawriter **v1.2.0**. Learning controls, two-step Start+A / Start+B drafts, colors, fonts and TXT persistence are retained from gbavocab v1.4.0. Copy the optional [sample file.txt](sample%20file.txt) into `/gbavocab` for 20 English–Spanish entries, four in each of the five boxes. The sample is separate from the ROM.

## Compact fonts in V1.3

V1.3 shares compact SuperFW glyph data instead of linking expanded flashcard sprite sheets. Korean syllables now use gbareader-style composed Hangul. Non-Korean pixels and advances, Arabic, automatic sizing, V1.2 feedback, controls and TXT persistence are unchanged. Fonts stay in the ROM; no SD font files are needed. The first Credits page is personal-only, followed by four attribution pages. See [compact-font compatibility and inherited coverage limits](docs/compact-fonts.md).

## Controls

See [Full current controls](docs/full-controls.md) for complete typing, accent, punctuation, caret, repeat and persistence instructions. The release includes `gbavocab-full-controls.pdf`, authored by Halim Jarrar.

Home:

- Up / Down selects **LOAD LIST** (first/default), **NEW LIST** or **LOCAL DICTIONARY**; A opens it. B resumes an active list.
- Select opens Controls; Start opens Credits. Within those pages, Left / Right changes page and B returns.
- NEW LIST displays an unused `LIST001.TXT`–`LIST999.TXT` name chosen from real SD storage. A creates that exact empty TXT in `/gbavocab`; B cancels. Existing files and recovery-owned names are not overwritten. Rename on a PC for a descriptive filename.

Training screen:

- R: hold to reveal the answer
- A: mark the current word correct and move it to the next box; shows a green feedback flash with word + answer before advancing, and holding A keeps that feedback visible until 24 frames after release
- B: reset the current word back to box 1; shows a red feedback flash with word + answer before advancing, and holding B keeps that feedback visible until 24 frames after release
- D-pad Left / Right: switch between boxes 1-5
- D-pad Up: undo the most recent A/B decision, if you stayed in the same box
- D-pad Down: ask to shuffle only the current box
- L: press to immediately cycle direction mode: front-to-back, back-to-front, alternating
- Long cards stay on one screen: the complete prompt and revealed answer wrap and use smaller body text only when necessary. There is no paging or scrolling; Left / Right always switches boxes.
- Start alone: save/export the current progress **on release**
- Select alone: open home / list management **on release**
- Start + Select: open **Entry editor** (both press orders; release tails are consumed)

Entry editor:

- Choose **Add entry**, **Edit entry**, **Delete entry**, or **Add from dictionary** with Up / Down and A; B returns. Dictionary results prefill a NEW Add draft.
- Add/Edit use two drafts: **Word / front (1/2)**, then **Translation / back (2/2)**.
- **Start+A** advances, then confirms both fields together. **Start+B** returns to the previous field with drafts intact; from step 1 it cancels without changing the active list or TXT.
- Add inserts at the top of Box 1. Edit captures the displayed card when the menu opens and keeps its box, position within that box, and learning progress.
- Delete shows both captured fields and **Are you sure?**, initially **No**. Use **Left / Right**, not Up / Down, to select Yes or No; A confirms and B cancels. An empty TXT remains open and can receive new entries.
- **All entry confirmations are RAM-only; the autosave toggle is removed.** Learning **Start** saves all pending entry changes, optional pair metadata and learning boxes; switching lists also offers Save. Typing and canceled drafts never save.
- RAM holds **128 concurrently added/edited rows**. Editing a pending row reuses its slot; deleting consumes none. At capacity, confirmation is rejected with **RAM FULL - save list first** and keeps the draft. Note its text before canceling to learning for manual save, since restarting Add/Edit creates fresh drafts.
- Typing uses GBAWriter's actual input engine, UTF-8 caret/deletion, visual wrapping and writing font—not an on-screen keyboard. Hold Up/Right/Down/Left for `abc`/`hij`/`nop`/`tuw`; B/A/R selects the first/second/third letter. Hold L for `def`/`klm`/`qrs`/`xyz`. Keep Right held and tap R twice for g, or Left held and tap R twice for v; no timing window or L layer applies. Isolated A inserts space; B deletes; a short isolated R release arms Shift from normal, while an uninterrupted R-only hold enables Caps at 48 frames (about 0.8 seconds). Any companion cancels hold eligibility until a fresh R press. From Shift/Caps, short or long isolated R clears on release and cannot rearm during that hold. Start+directions moves the caret; Start+L/R moves by a page. Select provides Writer's punctuation/accent cycling; Start+Select toggles its status bar.
- Existing additional columns remain byte-for-byte unchanged during Edit; only the first two fields appear in the drafts. [Reference, controls and persistence details](docs/entry-editor.md).
- Select can also accent the just-typed letter without inserting another character when its exact producing direction + B/A/R (+ L layer when used) remains continuously held. No timing deadline applies; releasing/changing the chord ends eligibility. See full controls for all accent cycles and case behavior.
- Fields have 189-byte draft buffers; the confirmed raw row remains limited to **191 UTF-8 bytes including its tab and any additional columns**, and the list to **10,000 entries**. Empty/blank-only fields and newline/tab input are rejected with feedback. A failure retains drafts; a committed-but-failed reopen is reported without offering a duplicate retry.
- Persisted mutations use the existing validated TXT replacement/recovery transaction. Manual saves use the existing grouped CRLF save format. No permanent `.sav`, settings file or sidecar is introduced; transient recovery files are retained when failure requires them. Load or create an SD-backed TXT before opening the editor; there is no demo-list drafting fallback.

File browser:

- D-pad Up / Down: move through files
- D-pad Left / Right: jump by 5 files
- A: load selected `.txt` file; unsaved progress prompts **A Save / B Discard / Select Cancel**. Failed save/load keeps the previous list.
- B: return home; home B resumes training

## File format

The project is intended for dict.cc-style tab-separated vocabulary text files, for example:

```text
Haus	house
Hund	dog
дом	house
```

The importer keeps all five box positions when reopening its own TXT files, including empty first and middle boxes. Exactly one empty physical line separates each pair of boxes (four separators total); vocabulary row bytes are retained and saved with CRLF endings. An optional final `# gbavocab: front=en; back=de` line stores the list language pair; it is not an entry or box separator. No persistent `.sav` files are used. Existing additional columns are preserved verbatim.

The first two tab-separated fields must be nonempty; existing additional columns are retained outside the editable fields. The entire raw row, including any additional columns, is limited to 191 content bytes. Invalid/overlong rows and entries beyond 10,000 make the loaded source **read-only**, with a visible warning, rather than allowing a save to drop unseen material. Long display text wraps at codepoint boundaries using the selected font's pixel measurements. Both sides fit on one screen, with body-only size reduction when needed (down to half size for extreme entries). Short cards retain their original typography and positions. Both sides are measured together, so revealing the answer never resizes the prompt. TXT content is never shortened to fit the display.

Replacement saves read source rows to construct output, check writes/sync/close, then scan the installed TXT once to check its write-derived physical fingerprint and planned index before retiring the original backup. They do not independently reread the whole original or compare every original/output row before installation. Same-size external changes are not independently detected: do not edit or swap the loaded TXT/card while the app is using it. Keep backups and do not remove power or the card while saving. Recovery is best-effort, not guaranteed atomicity or power-loss protection.

Transient replacement files use `name.txt.gbv1.tmp`, `.gbv1.bak`, and `.gbv1.txn`, with slots 1–9. Keep recovery files after an error; back up the card before manual recovery. Generic same-stem `.tmp`/`.bak` files are not adopted or deleted. Names longer than 54 bytes can be browsed but cannot be saved with the current 64-byte transaction-path buffer; shorten the filename first. See [current simpler-save contract and technical limitations](docs/file-io-simple-save.md). Earlier versioned I/O notes are historical, not the current safety contract.

**Known technical limitation:** four injected-error native FAT test cases retained orphan allocation space, although their content/live-state/retry checks passed. This is a filesystem-cleanup defect, not observed normal-operation TXT loss, and the native filesystem suite is not all-green. Exact cases, sizes and untested fault classes are documented in the current technical notes.

**dict.cc compatibility:** a synthetic file emitted by the actual exporter was uploaded through dict.cc's file input and both Unicode/annotated pairs were read back from Maintain. Vocabulary interchange works, but dict.cc itself trims leading/trailing empty groups and collapses consecutive empty lines: do not expect empty-box learning-state gaps to survive a round trip through the website. The private user sample was not uploaded.

No built-in, starter or demo vocabulary ships in the ROM. Without SD storage or TXT lists, the home screen gives an honest instruction; Controls and Credits remain accessible. Choose NEW LIST with working storage or place your own lists in `/gbavocab`.

## Build

Requirements:

- devkitPro / devkitARM
- Butano
- Python 3

Build:

```bash
make LIBBUTANO=/path/to/butano/butano
```

The ROM output is `gbavocab.gba`. Run correctness tests without the existing I/O performance suite with `bash tests/run_host_tests.sh --correctness-only` (also used by release CI). The original default remains `bash tests/run_host_tests.sh` (g++, Python 3, and Pillow; no Butano dependency). Install Pillow in your Python environment; CI pins `Pillow==12.3.0`.

## Notes

This is an early public source snapshot. It is useful for experimentation and for testing dict.cc vocabulary files on GBA hardware, but it is not a polished release yet.

### Credits

- [SuperFW](https://github.com/davidgfnet/superfw) by David Guillen Fandos: source of the matching flashcard font packs used for Latin Extended, Greek/Cyrillic, Japanese kana, CJK ideographs, and Korean Hangul coverage.
- UNSCII ([viznut.fi/unscii](https://viznut.fi/unscii/)) and Unifont ([unifoundry.com/unifont](https://unifoundry.com/unifont/)): retained GPL font sources, including Hangul components.
- GBAWriter: retained typing/editor integration.
- Butano engine and common sprite fonts: unchanged framework and small UI text, zlib license.
- Ghoulam Regular (2025), Imad AlFil / mloukhiyye, CC BY 4.0: retained Arabic artwork and attribution; [source, license and extraction details](docs/arabic.md).
- [dict.cc](https://www.dict.cc/): target vocabulary-export format and language-data workflow this trainer is designed around.
