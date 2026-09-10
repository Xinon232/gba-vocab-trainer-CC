# gbavocab V1.5 verification

Base: gbavocab `v1.4.0`, commit `2904573fc1048f95a92978246a5c32d087a3d06f`.
Input reference: gbawriter **tag `v1.2.0`**, not its moving default branch. The resulting `src/writer_core.cpp` is byte-identical to that tagged input engine. Vocab retains its field capacity and production host adapter.

## Scope

- Up `abc`, L+Up `def`; Right `hij`, L+Right `klm`; Down `nop`, L+Down `qrs`; Left `tuw`, L+Left `xyz`. B/A/R choose first/second/third.
- With unmodified Right continuously held, two R presses replace j with g. With unmodified Left held, w becomes v. No timing window. Existing accents follow their base letters and retain cycling, press-order and case semantics.
- Active-group labels, Controls and the complete seven-page manual reflect this layout. The opening title is `gbavocab V1.5`.
- `sample file.txt` is a separate optional UTF-8 file, not embedded vocabulary. It contains the approved 20 English–Spanish pairs, four per box, using actual tabs and one empty line between boxes.
- Only `src/writer_core.cpp` and `src/home_screen.cpp` changed under production source paths. Entry/draft control adapters, learning, persistence, all fonts, artwork and colors are unchanged from v1.4.0.

## Executed checks

- Clean devkitARM/Butano ROM build. Butano pinned at `77dcbcb3d8783596a9f333c64eedbccec77b05dc` (same as repository CI).
- `bash tests/run_host_tests.sh`: full host suite, including I/O performance tests, passed.
- `bash tests/run_host_tests.sh --correctness-only`: passed again with the exact sample/parser test included.
- `bash tests/run_entry_sanitizers.sh` and `bash tests/run_compact_sanitizers.sh`: ASan/UBSan passed.
- `python3 tests/check_entry_memory.py`: actual ELF placement/application-frame budget passed; not a recursive stack proof.
- Independent literal mapping tests exercise all eight groups, all selectors and normal/Shift/Caps. The engine and production EntryEditor accent matrix covers every retained variant, both press orders, cycling, release permutations, rejection, no-deadline holds and special g/v cancellation/case.
- Expected RED failures were captured before the mapping and g/v implementation. Inherited tests had obsolete pre-v1.4 feedback RGB and pre-v1.4 title/version assertions; those expectations were corrected without changing production colors.
- Exact-ROM mGBA 0.10.5 cold boot; all 22 Controls pages, five Credits pages, no-card LOAD/NEW and returns. The home image below the title is pixel-identical to the baseline.
- Exact-ROM XTest keypad run: 35 exact UTF-8 readbacks across all 24 group letters, g/v after a held delay, both accent orders, cycling, Shift, front-to-back advance and return with both drafts retained. Start+B step-1 cancellation returned to the menu. All eight lower/uppercase active-group screens were captured and visually reviewed.
- PDF: all seven pages visually reviewed, every Markdown block extracted on its intended page, no bounds overflow, author metadata verified, byte-identical deterministic rebuild.
- Sample: exact approved UTF-8 bytes and actual production parser verified 20 pairs, five boxes, four entries per box.

## Limitations

The emulator typing run used a debugger-seeded **RAM-only list**, initially empty draft, and real keypad events for every draft byte. It did not save to SD or perform final entry confirmation; host tests cover confirmation/persistence. No ROM patch or guest PC/SP relocation was used. No physical GBA/Supercard test was performed.

Existing storage limitations remain, including the four historical native FAT injected-error orphan-allocation cases documented in `file-io-simple-save.md`. The native sector-backed fault suite was not rerun for this input-only release; passing host adapter tests do not resolve those defects or establish physical-device correctness. Keep backups and never remove power/card during saving.

## Verified local release asset SHA-256

```text
7d6e00d84cbf6b0cd317e39a1dea15b8748d797f6869c4e071db8529635a5e92  gbavocab.gba
c850cf927cf443c58643800d156f1464ab11fe437b8dafdb0852568f12d99d2e  gbavocab-full-controls.pdf
0ac00876410851674e21f36f06764aa6d99ac248f3d85dd5885a171518acf442  sample file.txt
```

Publication is a new final release, not a replacement of historical assets. Release promotion does not implicitly update `main`. Local logs, screenshots, frozen file hashes and remote verification are retained separately from product source in the release owner's evidence directory.
