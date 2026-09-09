# V1.3 compact flashcard fonts

The body renderer now shares the existing `entry_font.c` / `fonts.pack` glyph lookup with the editor. It composes into the same bounded 224×16 packed EWRAM line and allocates only final sprite chunks. Native-size glyph-boundary batching, coordinates and advances are retained. Scales 8, 7, 6, 5 and 4 eighths retain coverage-preserving shrinkage. UI sprites, Ghoulam shaping/artwork, two-sided cached layout, input and persistence are unchanged.

Korean is the one authorized pixel change: all 11,172 Hangul syllables now use the existing reader-style component composition, not the old direct Hangul bitmap block. No direct Hangul override is linked. `tests/test_flashcard_font_parity.py` checks them against the unmodified reader drawing routine and every other bank glyph against its actual V1.2 BMP pixels and advances.

## Exact legacy exceptions

`tools/build_compact_flashcard_fonts.py` generates `include/flashcard_overrides.h` from the immutable legacy BMP/header oracles. The 94 printable ASCII glyphs preserve their original pixels and eight-pixel advance; space remains eight pixels without ink. Nine unassigned Greek codepoints preserve inherited sprite artwork: U+0378, U+0379, U+0380–U+0383, U+038B, U+038D and U+03A2. Those arise from the old sprite generator treating variable-font missing entries as glyph offsets. The candidate uses safe, explicit stored columns instead of repeating that read. This is compatibility artwork, not newly supported Greek characters.

The five former active BMP/JSON pairs are under `tests/fixtures/legacy-fonts/`, outside the device graphics inputs. Their original headers remain developer-only oracles in `include/`; no production source includes them. Unrelated historical fonts and source/license notices are retained.

## Inherited coverage limitations (not fixed here)

A field selects one bank with priority Hangul, CJK, Japanese, Greek/Cyrillic, Latin. This is not a universal mixed-script font. ASCII is shared, but combining non-ASCII scripts can select a bank that lacks one of the characters. The existing Japanese bank contains U+3000–U+3009 and U+3040–U+30FF, while the unchanged parser accepts the wider U+3000–U+30FF range. A missing bank entry retains Butano's device debug error and, with assertions disabled, its index-zero fallback (the bank's first non-ASCII glyph). Host tests can inspect that release fallback without triggering a device assertion. No parser or language expansion is included.

Unsupported raw or malformed input given directly to the new host API is bounded; product callers still use the unchanged parser. Font packs are trusted linked ROM assets, not external SD input.

## Tests and limits of evidence

- `bash tests/run_host_tests.sh`: existing host suites plus exhaustive real glyph/advance/scale parity and production final-tile framebuffer comparisons.
- `bash tests/run_compact_sanitizers.sh`: actual compact lookup, clipping, invalid input, production compositor and first-fit deferred OBJ allocator under ASan/UBSan.
- `bash tests/run_entry_sanitizers.sh`: unchanged editor/persistence sanitizer gates.
- `python3 tests/check_entry_memory.py`: actual ELF EWRAM/IWRAM/application-frame budgets.

Hardware descriptors/allocation are host doubles, not an emulator. Guest redraw-cycle/response-frame checks, exact-ROM menu/Credits traversal and hardware readability remain separate QA. The five-page Credits sequence starts with only the exact personal block, followed by retained framework, font, license and Ghoulam attribution.

## Retained attributions

SuperFW font packing, renderer and Hangul composition: David Guillen Fandos, GPL v3 or later. The original notices are retained. Font sources include UNSCII (`viznut.fi/unscii`) and Unifont (`unifoundry.com/unifont`), including the reusable Hangul components; see `references/superfw/res/fonts/README.md` and retained source files. GBAWriter supplies the existing input/editor integration. Butano supplies the engine and UI sprite font, under its retained zlib notices. Ghoulam Regular (2025), Imad AlFil / mloukhiyye, remains CC BY 4.0; full source/license links and extraction changes are in `docs/arabic.md` and the full controls manual. dict.cc is the vocabulary interchange format reference.
