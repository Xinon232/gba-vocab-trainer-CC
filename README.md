# GBA Vocab Trainer

Current source version: **v0.2.11**.

A simple 5-box vocabulary trainer for the Game Boy Advance, built with Butano and targeted at SuperFW / Supercard SD-style setups.

The trainer can load vocabulary from `.txt` files compatible with dict.cc-style vocab-trainer exports. Put text files on the SD card and select them from the in-game file browser.

Each vocabulary file can contain up to 10,000 entries. The text is streamed from the SD card, so smaller files retain their normal per-file loading, saving, and training performance.

Current flashcard text support uses SuperFW-derived fonts for broad language compatibility:

- Latin Extended (`U+0080–U+024F`) for Western/Central European languages and phonetic/diacritic-heavy entries
- Greek and Cyrillic (`U+0370–U+04FF`) including Russian, Ukrainian, Bulgarian, Serbian/Macedonian-style Cyrillic extensions, and Greek
- Japanese punctuation, Hiragana, and Katakana (`U+3000–U+30FF`)
- CJK Unified Ideographs (`U+4E00–U+9FEF`) plus SuperFW's included CJK Extension-B subset (`U+20000–U+200CC`) for Chinese/Japanese/Korean Han characters
- Korean Hangul syllables (`U+AC00–U+D7A3`)
- Arabic is still rendered with the existing experimental Arabic font path and is not considered fully supported yet because shaping/joining is incomplete

## Controls

Training screen:

- R: hold to reveal the answer
- A: mark the current word correct and move it to the next box; shows a green feedback flash with word + answer before advancing, and holding A keeps that feedback visible until release
- B: reset the current word back to box 1; shows a red feedback flash with word + answer before advancing, and holding B keeps that feedback visible until release
- D-pad Left / Right: switch between boxes 1-5
- D-pad Up: undo the most recent A/B decision, if you stayed in the same box
- D-pad Down: ask to shuffle only the current box
- L: press to immediately cycle direction mode: front-to-back, back-to-front, alternating
- Long cards stay on one screen: the complete prompt and revealed answer wrap and use smaller body text only when necessary. There is no paging or scrolling; Left / Right always switches boxes.
- Start alone: save/export the current progress **on release**
- Select alone: open the file browser **on release**
- Start + Select: open **Entry editor** (both press orders; release tails are consumed)

Entry editor:

- Choose **Add entry**, **Edit entry**, or **Delete entry** with Up / Down and A; B returns.
- Add/Edit use two drafts: **Word / front (1/2)**, then **Translation / back (2/2)**.
- **Start+A** advances, then saves both fields atomically. **Start+B** returns to the previous field with drafts intact; from step 1 it cancels without changing the TXT.
- Add inserts at the top of Box 1. Edit captures the displayed card when the menu opens and keeps its box, position within that box, and learning progress.
- Delete shows both captured fields and **Are you sure?**, initially **No**. Select Yes and press A to delete. An empty TXT remains open and can receive new entries.
- Typing uses GBAWriter's actual input engine, UTF-8 caret/deletion, visual wrapping and writing font—not an on-screen keyboard. Hold Up/Right/Down/Left for `abc`/`def`/`hij`/`klm`; B/A/R selects the first/second/third letter. Hold L for `nop`/`qrs`/`tuw`/`xyz`. Isolated A inserts space; B deletes; R cycles Shift/Caps. Start+directions moves the caret; Start+L/R moves by a page. Select provides Writer's punctuation/accent cycling; Start+Select toggles its status bar.
- Existing additional columns remain byte-for-byte unchanged during Edit; only the first two fields appear in the drafts. [Reference, controls and persistence details](docs/entry-editor.md).
- Fields have 189-byte draft buffers; the confirmed two-field row remains limited to **191 UTF-8 bytes including its tab**, and the list to **10,000 entries**. Empty/blank-only fields and newline/tab input are rejected with feedback. A failure retains drafts; a committed-but-failed reopen is reported without offering a duplicate retry.
- Mutations use the existing validated TXT replacement/recovery transaction. Clean-list mutations preserve unrelated physical bytes (including LF/CRLF and unterminated EOF). Pending box movement/shuffle uses the existing grouped CRLF save format. No permanent `.sav` or sidecar is introduced. Without an SD-backed TXT, the editor is usable for drafting but reports **NO SD FILE - not saved** on confirmation.

File browser:

- D-pad Up / Down: move through files
- D-pad Left / Right: jump by 5 files
- A: load selected `.txt` file; unsaved progress prompts **A Save / B Discard / Select Cancel**. Failed save/load keeps the previous list.
- B: return to training

## File format

The project is intended for dict.cc-style tab-separated vocabulary text files, for example:

```text
Haus	house
Hund	dog
дом	house
```

The importer keeps all five box positions when reopening its own TXT files, including empty first and middle boxes. Exactly one empty physical line separates each pair of boxes (four separators total); vocabulary row bytes are retained and saved with CRLF endings. The app adds no metadata rows, extra columns, footers, or persistent `.sav` files. Existing additional columns are preserved verbatim.

The first two tab-separated fields must be nonempty; existing additional columns are retained outside the editable fields. The entire raw row, including any additional columns, is limited to 191 content bytes. Invalid/overlong rows and entries beyond 10,000 make the loaded source **read-only**, with a visible warning, rather than allowing a save to drop unseen material. Long display text wraps at codepoint boundaries using the selected font's pixel measurements. Both sides fit on one screen, with body-only size reduction when needed (down to half size for extreme entries). Short cards retain their original typography and positions. Both sides are measured together, so revealing the answer never resizes the prompt. TXT content is never shortened to fit the display.

Transient replacement files use `name.txt.gbv1.tmp`, `.gbv1.bak`, and `.gbv1.txn`. Successful saves compare every ordered raw row and box exactly once before installation, then verify the installed physical fingerprint and complete index before retiring the backup. Generic same-stem `.tmp`/`.bak` files are never used. Names longer than 54 bytes can be browsed but cannot be saved with the current 64-byte transaction-path buffer; shorten the filename first. See [v0.2.11 I/O notes](docs/file-io-v0.2.11.md) for the approved validation fault model and retained same-handle loading, [v0.2.10 I/O notes](docs/file-io-v0.2.10.md) for bounded windows, [v0.2.9 identity notes](docs/file-io-v0.2.9.md), and [recovery details](docs/file-io-v0.2.7.md).

**dict.cc compatibility:** a synthetic file emitted by the actual exporter was uploaded through dict.cc's file input and both Unicode/annotated pairs were read back from Maintain. Vocabulary interchange works, but dict.cc itself trims leading/trailing empty groups and collapses consecutive empty lines: do not expect empty-box learning-state gaps to survive a round trip through the website. The private user sample was not uploaded.

If no SD-card vocabulary file is loaded yet, the built-in starter list shows one language-name sample for each main supported font group/language family, including English, French, German, Spanish, Portuguese, Italian, Dutch, Polish, Czech, Turkish, Greek, Russian, Ukrainian, Japanese, Chinese, and Korean.

## Build

Requirements:

- devkitPro / devkitARM
- Butano
- Python 3

Build:

```bash
make LIBBUTANO=/path/to/butano/butano
```

The ROM output is `vocab.gba`. Run correctness tests without the existing I/O performance suite with `bash tests/run_host_tests.sh --correctness-only` (also used by release CI). The original default remains `bash tests/run_host_tests.sh` (g++, Python 3, and Pillow; no Butano dependency). Install Pillow in your Python environment; CI pins `Pillow==12.3.0`.

## Notes

This is an early public source snapshot. It is useful for experimentation and for testing dict.cc vocabulary files on GBA hardware, but it is not a polished release yet.

### Credits

- [SuperFW](https://github.com/davidgfnet/superfw) by David Guillen Fandos: source of the matching flashcard font packs used for Latin Extended, Greek/Cyrillic, Japanese kana, CJK ideographs, and Korean Hangul coverage.
- Butano common sprite fonts: used for the small UI text.
- [dict.cc](https://www.dict.cc/): target vocabulary-export format and language-data workflow this trainer is designed around.
