# Ghoulam Arabic display (local V1.1)

Ghoulam Regular © 2025 Imad AlFil / mloukhiyye, CC BY 4.0.
- Source: https://mloukhiyye.itch.io/ghoulam-arabic-pixel-art-font-version-1
- License: https://creativecommons.org/licenses/by/4.0/
- Supplied original TTF: `tests/fixtures/arabic/Ghoulam-Regular.ttf`.
- Conversion: `tools/extract_ghoulam.py FONT --output include/ghoulam_data.h` using fonttools and freetype-py. Actual GSUB unencoded init/medi/fina/rlig glyphs are rasterized at 11px, preserving advances, bearings and baseline. The generated table records its source SHA256. This is a modified representation, not author endorsement. The supplied file has not been proven byte-identical to a fresh itch.io download.

## Display boundaries

The shared `arabic_text.h` shapes bounded logical clusters and reorders each wrapped line. Both sprite-card sizes and bitmap Entry/Delete paths use the real glyph compositor. Latin remains SuperFW. Ordinary Arabic letters and four lam-alef variants are extracted; missing meaningful artwork is `?`. Arabic comma is ordinary comma; Arabic-Indic digits are SuperFW digits. Harakat are transparent only in display. Source logical bytes, raw row/index capacities, input/Caps behavior and all storage/save validation remain unchanged.

First strong direction chooses each line's base. Latin and digit islands stay LTR; paired brackets take their enclosed first strong direction; neutral sequences follow matching neighbors or the line base. This is a small defined bidi policy, not full UAX #9. Persian/Urdu extensions, encoded presentation forms and explicit bidi embedding/isolate controls are not full-coverage scripts. Arbitrarily nested or unmatched mixed-direction brackets may differ from a desktop Unicode renderer.

Caret positions are byte-mapped from the same shaped line. Left/right input retains previous/next logical character semantics, including hidden marks. Up/down selects the closest visual byte boundary, deterministically choosing the first equal-distance boundary. Lam-alef has an interior caret; neither ligatures nor hidden harakat rewrite the draft. No Arabic typing layout is added.

## Bounds and tests

One 192-cluster shared non-reentrant scratch line lives in EWRAM; font tables live in ROM. Valid input can expand to 575 display bytes but still has at most 191 logical source characters. Full/reduced chunks use the same advances and actual rasterizer. Only final 32px sprite chunks enter VRAM; existing adaptive two-sided resource checks reserve previous-frame allocations and UI. Memory reports are budget gates, not a total stack-depth proof.

`bash tests/run_host_tests.sh` includes both supplied samples, actual production editor/delete pixels, shaping, wrap/caret, compositor, Caps and storage regression suites. `tests/test_arabic_reference.py FONT PROBE` compares real production glyph IDs to offline HarfBuzz (probe built from `tests/arabic_reference_probe.cpp`). No HarfBuzz, TTF parser or large Unicode engine runs on the GBA.

Read `docs/full-controls.md` for all controls. Exact-ROM emulator evidence uses explicitly labeled RAM fixtures, not fabricated SD results. Real SD/firmware testing belongs to the user. No commit, push, tag or release is authorized for this local candidate.
