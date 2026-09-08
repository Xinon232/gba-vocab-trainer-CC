# GBAWriter entry-input reference

Source: https://github.com/Xinon232/gbawriter
Commit / remote HEAD / highest reachable tag v0.3.1:
`30ae9561e601f4b145ddfe46a0dc56a7448078ac`.

The local Writer tracked source matched this remote commit. Files were imported
with `git archive <commit>`; its checkout was not modified. `/releases/latest`
returned 404 because this repository publishes prereleases. The authenticated
releases list verified v0.3.1 as the newest published release (prerelease),
published 2026-09-07T22:03:06Z and targeting the same exact commit. Its metadata
is recorded in `entry-editor-evidence/writer-latest-release.json` beside the checkout.

Reused verbatim: `src/writer_core.cpp`, `src/writer_layout.cpp`,
`include/writer_layout.h`, upstream core/frame/layout tests, and this directory's
SuperFW font renderer/UTF-8/font-pack assets. `include/writer_core.h` changes only
TEXT_CAPACITY to 189 bytes for bounded vocabulary-field drafts. Its unused diary
helpers remain in the imported source and are discarded by ROM linker GC.

`EntryEditor::consume` adapts Writer's Application consumer: SAVE and SAVE_MENU
become next/confirm and previous/cancel. Delimiters forbidden by the existing
vocabulary grammar are rejected as edits. Typing sessions, alphabet layers,
Shift/Caps, provisional Select character, repeats, UTF-8 movement/deletion,
word wrapping, vertical/page navigation, and status toggle are retained.

`entry_render.cpp` reuses Writer's bitmap glyph loop and halfword-safe pixel
primitives, with room reserved for the required step heading, field label and
controls footer. Delete previews alone reduce glyph size when needed to show
both complete fields. `entry_font_pack.s` binds the same two font packs under
project-local paths. `string_shims.c` adds Writer's strcmp/strcpy routines for
Butano's freestanding link.

GPL-3.0: see repository LICENSE and retained upstream notices in these files.
No Writer persistence, file-management or diary application layer was imported.
