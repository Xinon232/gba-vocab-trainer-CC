# gbavocab V1.1 candidate — full controls

Learn vocabulary using flashcards and five learning boxes. Create and edit your own word lists on your Game Boy Advance.

Put UTF-8 TXT vocabulary files in `/gbavocab` at the SD-card root, for example `/gbavocab/Spanish.txt`. Each entry uses a word, a TAB character, and its translation. Optional additional columns are preserved. Files elsewhere on the card are not the list library.

Author: Halim Jarrar

## 1. Getting started and managing lists

Use a compatible SuperFW / Supercard SD-style setup. Copy the runnable `gbavocab.gba` release ROM to your card and launch it with your firmware. Source builds produce the same `gbavocab.gba` filename. Back up your vocabulary TXT files before using any application that edits them.

The home screen reads `gbavocab V1.1` and `files: /gbavocab`. There is no built-in vocabulary, demo list or automatic fallback. Missing storage is reported; Controls and Credits remain available even without a list.

- Up / Down: choose LOAD LIST or NEW LIST. LOAD LIST is first and selected by default.
- A: open the selected action.
- B on home: resume the active list, if one is open.
- Select on home, LOAD LIST or NEW LIST: open Controls.
- Start on those screens: open Credits.
- Left / Right in Controls or Credits: previous / next page; B: return.

### LOAD LIST

- Up / Down: select a TXT file; the visible list scrolls with selection.
- Left / Right: jump backward / forward by five files, stopping at the ends.
- A: load the selected file. B: return home, not directly to learning.

### NEW LIST

The app checks real storage for an unused name from `LIST001.TXT` through `LIST999.TXT`, including names reserved by interrupted recovery. It displays the chosen filename before creation. A creates that exact empty TXT in `/gbavocab`; B returns without creating it. Existing files are never overwritten. If no name or storage is available, creation is unavailable. Rename the TXT on your PC if you want a descriptive name; there is no filename-typing screen.

### Switching with unsaved changes

Loading or creating another list while the active list is dirty asks: A = Save, B = Discard, Select = Cancel. Save writes pending entries and learning boxes before switching. Discard switches without saving the old changes. Cancel leaves the active list alone. A failed save or load keeps the active data; do not power off after a failure. Simply opening home or help does not discard a list.

<!-- PAGEBREAK -->

## 2. Learning with flashcards

Each entry has a front (word) and back (translation), and belongs to one of five boxes. New entries go first in Box 1. The initial direction mode is alternating, starting with the front. Direction mode and navigation are session state, not a permanent settings file.

- Hold R: reveal the answer. Release R to hide it during normal learning.
- A: mark correct, advance one box (Box 5 stays Box 5), then show the next card in the current box.
- B: mark wrong and return the entry to Box 1. An entry already in Box 1 moves to its end so another card can come next.
- Hold the A or B used for a judgment: keep its word and answer visible in green / red feedback until release, after the brief minimum feedback display. Holding does not repeatedly grade cards.
- Left / Right: previous / next box, wrapping between boxes 1 and 5. Empty boxes remain selectable. These buttons never page or scroll a flashcard.
- Up: undo the most recent eligible A/B decision while still in the same box, returning to that card. This is single-step learning undo, not entry-edit undo. A in Box 5 does not arm a box-change undo. Changing boxes, requesting shuffle or committing an entry clears the relevant undo history.
- Down: ask to shuffle only the current box. In that prompt, A confirms and B cancels.
- L: immediately cycle front-to-back, back-to-front, alternating, then repeat. In alternating mode the prompt side changes after a judgment.

Both the full prompt and revealed answer fit on one screen. Long cards wrap and use smaller body text when necessary; short cards keep their normal size. This changes only display, never TXT content. There is no flashcard paging or scrolling.

### Learning shortcuts: press and release matter

- Start alone, then release: manually save the active TXT, including all confirmed pending entry changes and current learning boxes. Use this in normal learning, after judgment feedback finishes.
- Select alone, then release: open home / list management.
- Start + Select held together: open Entry editor for the captured card. Either press order works. This chord consumes both release tails: releasing it does not also save or open home.

Release the opening buttons before using the next screen. Learning shortcuts are not active inside the text drafts: the same buttons have the typing meanings on pages 3–5.

### Saving learning progress

Grading and shuffling affect the active list in memory. Entry Autosave is not an independent learning-progress autosave switch. Use learning Start to save before powering off; an Autosave ON entry confirmation also includes already-pending learning changes. Wait for saving to finish. A failure is not a successful save: retain the session and recovery files and resolve the storage problem.

<!-- PAGEBREAK -->

## 3. Entry editor: drafts and confirmation

From a loaded list, press Start + Select. The menu captures the currently displayed entry for Edit and Delete. An empty box has no editable selected entry, but Add remains available.

- Up / Down: choose Add entry, Edit entry, Delete entry, or Autosave in that order.
- A: open the selected operation; on Autosave, toggle OFF / ON.
- B: return to learning.

### Add and Edit: two separate field drafts

Step 1/2 is Word / front. Step 2/2 is Translation / back. Add starts with empty drafts; Edit prefills the captured entry's two fields, with the caret initially at the end. Additional columns are kept outside the drafts.

- Hold Start and press A in step 1: advance to step 2. The first field must contain text.
- Hold Start and press A in step 2: confirm both fields together. This applies or saves according to Autosave; it is not a save on every keystroke.
- Hold Start and press B in step 2: return to step 1, keeping both drafts.
- Hold Start and press B in step 1: cancel back to the entry menu without changing the active entry or TXT. Starting Add/Edit again begins fresh drafts.

A confirmed Add goes at the top of Box 1. Edit keeps the captured entry's identity, box, relative position within the box and learning progress; it does not replace a newly selected or reordered card by accident. Committing returns to learning and clears transient judgment / undo state. A canceled draft makes no mutation.

### Delete

The confirmation shows the captured word and translation and asks “Are you sure?”. No is selected initially. Left / Right switches No / Yes; Up / Down does not choose an answer. A on Yes confirms deletion; A on No or B cancels to the entry menu. Deleting the final entry leaves an empty, usable TXT that can receive new entries.

### Autosave: exact session behavior

Autosave is OFF on every app start. Its toggle is RAM-only, below Delete entry, and survives entry-menu visits and list changes for the rest of that session. It is not written to a settings file.

OFF: confirmed additions, edits and deletions immediately affect the active in-memory list, but cause zero TXT rewrites. Learning Start manually writes all pending changes plus learning boxes. The Save choice when switching lists can also flush them.

ON: the next confirmed addition, edit or deletion saves all pending changes together with that new mutation. Merely switching ON does not flush anything. Typing and canceled drafts never save. An uncommitted failure keeps the draft / active data; if installation completed but a later reopen failed, the app reports the error without offering a duplicate Add retry.

<!-- PAGEBREAK -->

## 4. Typing letters, case, spaces and caret

The drafts use GBAWriter's button-driven input engine, not an on-screen keyboard. Hold one D-pad direction, then tap B, A or R to choose letter 1, 2 or 3. Release the letter button between taps. Multiple D-pad directions are not a letter group.

### Letter groups: B / A / R in that order

- Up: a / b / c
- Right: d / e / f
- Down: h / i / j
- Left: k / l / m
- L + Up: n / o / p
- L + Right: q / r / s
- L + Down: t / u / w
- L + Left: x / y / z

For g: keep Down held and tap R twice. The first tap inserts j; the second replaces it with g. For v: keep L + Down held and tap R twice; w becomes v. This is a continuous-held-group gesture, not a fast double-tap deadline. Releasing the direction or L resets the sequence; A, B, Start or Select also interrupts it. The replacement keeps the case of the first letter.

### Spaces, deletion and repeat

A alone inserts a space at the caret. B alone deletes the preceding complete UTF-8 character, not an individual byte. Hold either button alone to repeat: the first action occurs on press, repeat starts after 24 frames and continues every 5 frames. Repeat must begin with a fresh solo press and stops when another button joins; releasing a letter chord into A/B does not start repeat. Letter and Select-symbol presses themselves do not auto-repeat.

### Shift and Caps

From normal, a short isolated R release arms Shift. Hold R alone for 48 frames (twice the initial A-repeat delay; about 0.8 seconds) to enable Caps while held, once per hold. Any companion before the threshold cancels eligibility until R is released and freshly pressed alone; releasing the companion cannot restart it. From active Shift/Caps, another isolated R—short or long—clears on release and cannot rearm in that hold. Shift uppercases the next accepted letter; Caps stays active. Spaces, digits and punctuation do not consume Shift. Select-first accented letters consume one-shot Shift when the Select session is released. Letter-first conversion retains the original letter's case even after that letter consumed Shift. The ß alternate remains ß even with Shift / Caps. Rejected insertion does not consume Shift. The status line shows the active letter group and Shift or Caps.

### Caret and visual rows

Hold Start and press Left / Right to move one UTF-8 character backward / forward. Start + Up / Down moves to the previous / next visual row, retaining the desired horizontal position where possible. Start + L / R moves by a previous / next page of draft rows (five with status or an error message visible, otherwise six). Hold Start with one navigation key to repeat on the same 24-frame / 5-frame schedule. Movement stops at text boundaries.

Draft text wraps visually without inserting line breaks; overlong words split visually. The blinking caret and viewport follow the text. Start alone attempts Writer's newline on release, but vocabulary fields reject it with feedback: no tabs, carriage returns or physical newlines may be typed. There is no separate bound Home/End, selection, clipboard or forward-delete shortcut.

<!-- PAGEBREAK -->

## 5. Punctuation, digits, signs and accents

Press Select outside a still-held producing letter chord to insert one provisional period at the caret. Keep Select held while choosing a replacement below; release Select to keep it in the draft. Each cycle replaces that same character. Release Select and begin again for a second symbol.

### Symbol cycles while Select is held

- Up moves forward and Down backward through: 1 2 3 4 5 6 7 8 9 0
- Right moves forward and Left backward through: . ( ) / ; @ # % & _ + = -
- R alone moves forward and L alone backward through: . , ' " : ! ?

Cycles wrap around. Starting from the provisional period, Up selects 1 and Down selects 0; Right selects an opening parenthesis and Left selects a hyphen; R selects a comma and L selects a question mark. For the L/R punctuation cycle, release any D-pad direction first. Directions choose symbol cycles on fresh presses, not merely by remaining held.

### Accent cycles while Select is held

Use the normal letter chord for a supported base letter to replace the provisional character with its first alternate. Repeat that letter button while keeping its direction (and L layer, where needed) held to cycle the remaining alternates. Changing or releasing the group resets that letter's cycle. A letter with no listed alternate does not insert its plain form in this mode.

- a: á ä à â ã å æ     c: ç č ć
- e: é è ë ê     i: í ï ì î
- n: ñ ń     o: ó ö ô ò õ ø œ
- s: ß š ś     u: ü ú ù û
- y: ý ÿ     z: ž ź ż

Shift / Caps selects the corresponding uppercase accented form, except ß which stays unchanged. For example, hold Select and Up, then tap B once for á or twice for ä. Release Select to keep it. To start from a clean accent session without first cycling a symbol, hold the direction before pressing Select, then tap the letter button.

Both press orders work. Hold Select, then enter the letter combination. Or keep the just-typed letter's exact direction, producing B/A/R button and L layer (if used) continuously held, then press Select. The same letter becomes its first alternate, retaining its original case with no duplicate letter or period. No time deadline applies; releasing or changing the chord ends eligibility. An unsupported base stays unchanged in letter-first order. Example: Up+B held, then press Select: a becomes á. Keep Select and the group held; release and repress its producing B/A/R button to cycle, then release Select to keep the result.

### Status toggle and modifier priority

Start + Select inside a draft toggles the status bar, not the learning Entry editor. Either press order works. It removes a newly inserted Select provisional character, but retains an existing letter converted to an accent. Both buttons' release tails are consumed. Release them before continuing.

While Select is held, typing belongs to the symbol / accent session; do not try to advance a field using Start+A within it. Release Select first, then hold Start and press A. Start+B likewise means previous field / cancel only outside the Select session. A plain B in normal typing is backspace, not cancel.

Typing chords provide Latin letters and accents, not an Arabic keyboard. For imported Arabic display and caret behavior, see page 7.

<!-- PAGEBREAK -->

## 6. Limits, TXT persistence and recovery

### Entry and RAM limits

Both editable fields must contain nonblank text. Each draft buffer holds at most 189 UTF-8 bytes, but the confirmed entire row has a shared limit of 191 content bytes including the TAB and all additional columns. Accented and other non-ASCII characters can use multiple bytes. Two individually valid drafts may exceed the combined row limit; shorten them before confirming. The list limit is 10,000 entries.

With Autosave OFF, RAM holds up to 128 concurrently added or edited rows. Editing an already-pending row reuses its slot. Deleting consumes no slot and frees a slot if that row had one. This is not a limit of 128 button presses or total lifetime changes. At capacity, a new pending row is rejected with “RAM FULL - save list first”; the draft is retained and the active list is not changed.

Manual save is available in learning, not inside a draft. If RAM is full, note the rejected draft text before using Start+B to return / cancel, leave the editor with B, then press and release Start in learning. After a successful save, reopen the editor and re-enter that unconfirmed draft. Canceling and reopening does not preserve draft text.

Invalid, overlong or excess imported rows make the source read-only with a warning rather than allowing a save to silently drop unseen data. Additional columns remain byte-for-byte unchanged during Edit; only the first two fields are displayed as the card's word and translation. An unavailable or uneditable target is reported rather than replaced.

### How the TXT stores progress

Manual learning saves and saves containing pending learning / entry changes use the existing grouped CRLF format. Rows are grouped into Boxes 1–5; exactly one empty physical line separates each pair of boxes, including empty first or middle boxes (four separators total). The app writes no metadata rows, new columns or footers. Reopening the TXT restores those box memberships.

An immediate Autosave ON mutation on a clean list instead splices the affected row while preserving unrelated physical bytes, separators, whitespace, mixed LF/CRLF endings and an unterminated final line. Add uses the first observed newline convention, or CRLF if there is none. Once pending changes require the grouped save path, its established newline / box normalization applies.

No permanent `.sav`, settings or sidecar files. Temporary `name.txt.gbv1.tmp` / `.bak` / `.txn` files use slots 1–9. Keep recovery files after errors; back up before manual recovery. Keep TXT backups; never remove power/card during saving or externally edit/swap the loaded source. Filenames over 54 bytes need PC renaming before saving. Saves check writes and scan the installed TXT, without independent full-original rereads or pre-install row comparison. Same-size external edits can escape detection; recovery/atomicity is not guaranteed. Four injected-error cases left orphan space despite content/retry passes; see `docs/file-io-simple-save.md`.

### Compatibility and credits

Retained display coverage includes Latin Extended, Greek/Cyrillic, Japanese kana, CJK and Hangul from the existing font packs. Coverage is finite, not every Unicode glyph. Ghoulam adds bounded Arabic display (page 7); bundled demo vocabulary remains absent. dict.cc-style vocabulary interchange is supported, but the website may collapse empty groups and lose empty-box gaps on a round trip.

Author: Halim Jarrar. GBAWriter input engine; SuperFW writing fonts by David Guillen Fandos (GPL v3 or later); Butano engine and UI fonts (zlib); dict.cc vocabulary format. Full licenses, credits and technical notes are in the repository.

<!-- PAGEBREAK -->

## 7. Imported Arabic and Ghoulam credits

Arabic may appear in either field, mixed with Latin transliteration, punctuation and numbers. Ghoulam supplies contextual Arabic letter forms and lam-alef ligatures. Logical text wraps first; each resulting line arranges Arabic runs right to left, with Latin and number runs left to right. Paired parentheses, brackets and braces keep enclosed transliteration together within a line. Slash-separated alternatives remain separated.

Harakat are transparent to joining and hidden only in display. The original UTF-8 letters, marks and punctuation remain in the TXT and editor drafts. Ghoulam's empty Arabic-comma glyph displays using the ordinary SuperFW comma instead. Arabic-Indic digits display using SuperFW digits. Other unsupported meaningful characters display a visible question mark. This is bounded Arabic support, not a complete Unicode bidirectional or Arabic-script engine; Persian/Urdu extensions, presentation-form text and explicit bidi embedding controls are not comprehensive coverage.

### Editing imported text

The existing two-field editor and Delete preview use shaped Arabic too. No Arabic keyboard layout has been added. Start + Left / Right still means previous / next logical UTF-8 character, not necessarily left / right on screen. Start + Up / Down chooses a character boundary near the same visual horizontal position in the adjacent wrapped row. A lam-alef ligature has an interior caret position; deleting removes one logical character at a time. Hidden marks keep their logical positions and can require a navigation or deletion step without a visible movement. At a mixed-direction boundary the caret uses the next logical character's leading edge.

The prompt and revealed answer still share a single screen. Short Latin cards retain their previous appearance; long cards may use reduced text. Arabic artwork is rasterized at the font's native 11-pixel scale before any whole-line reduction, retaining its baseline, bearings and connected advances. Reduced text is necessarily small on the GBA screen.

### Ghoulam attribution and extraction notice

Made by Halim Jarrar. (C) 2026. Website: halim-jarrar.de. Contact: monday@halim-jarrar.de. In-app Credits shows these credentials first, with the Arabic font attribution beneath them.

Ghoulam Regular, copyright 2025 Imad AlFil / mloukhiyye, licensed CC BY 4.0. Source: https://mloukhiyye.itch.io/ghoulam-arabic-pixel-art-font-version-1

License: https://creativecommons.org/licenses/by/4.0/

For this app the supplied TTF's actual unencoded GSUB initial, medial, final and lam-alef glyphs were extracted as monochrome ROM bitmap tables at 11 pixels. This conversion, display-only mark filtering and punctuation/digit fallbacks are app changes, not a modified font endorsed by the author. SuperFW remains the font for Latin, numbers and ordinary punctuation. The source font hash and reproducible extractor are included in the local source and evidence.

This is a local V1.1 candidate, not a published release. Test backed-up copies of the supplied lists on your actual SD card and firmware. Emulator/RAM-fixture checks do not verify real Supercard storage or physical-device readability.
