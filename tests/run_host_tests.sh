#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
BUILD=$(mktemp -d /tmp/gbavocab-host-XXXXXX)
trap 'rm -rf "$BUILD"' EXIT
ulimit -c 0
CORRECTNESS_ONLY=false
case "${1:-}" in
  "") ;;
  --correctness-only) CORRECTNESS_ONLY=true ;;
  *) printf 'Usage: %s [--correctness-only]\n' "$0" >&2; exit 2 ;;
esac
CXX=${CXX:-g++}
COMMON=(-std=c++17 -O2 -Wall -Wextra -Iinclude src/writer_core.cpp)
for name in test_pair_dirty test_sample_file test_entry_shortcuts test_entry_state test_vocab test_vocab_10k test_vocab_grouped test_vocab_file_io_perf test_state test_feedback_hold test_integration test_save_status test_empty_boxes test_scan_limits test_scan_visit test_long_display test_embedded_control test_switch test_page_input test_save_navigation test_text_layout test_body_pixels test_arabic_import test_arabic_text test_arabic_pixels; do
  if $CORRECTNESS_ONLY && [[ "$name" == test_vocab_file_io_perf ]]; then continue; fi
  "$CXX" "${COMMON[@]}" src/vocab.cpp src/vocab_file_io.cpp src/state.cpp "tests/$name.cpp" -o "$BUILD/$name"
  "$BUILD/$name"
done
"$CXX" "${COMMON[@]}" -DVOCAB_HOST_FATFS src/vocab.cpp tests/host_fatfs.cpp tests/test_io_windows.cpp -o "$BUILD/io_windows"
"$CXX" "${COMMON[@]}" -DVOCAB_HOST_FATFS src/vocab.cpp tests/host_fatfs.cpp tests/test_simple_save.cpp -o "$BUILD/simple_save"
"$BUILD/simple_save"
for mode in capacity generated maximum short-scanner short-identity installed-offset installed-box installed-count installed-row installed-tail; do
  "$BUILD/io_windows" "$mode"
done
"$CXX" "${COMMON[@]}" -DVOCAB_HOST_FATFS src/vocab.cpp src/vocab_file_io.cpp tests/host_fatfs.cpp tests/test_fatfs_transaction.cpp -o "$BUILD/fatfs"
"$BUILD/fatfs"
"$CXX" "${COMMON[@]}" -DVOCAB_HOST_FATFS src/vocab.cpp src/vocab_file_io.cpp tests/host_fatfs.cpp tests/test_load_handles.cpp -o "$BUILD/load_handles"
"$BUILD/load_handles"
"$CXX" "${COMMON[@]}" -DVOCAB_HOST_FATFS src/vocab.cpp src/vocab_file_io.cpp src/state.cpp tests/host_fatfs.cpp tests/test_fatfs_faults.cpp -o "$BUILD/fatfs_faults"
for mode in transient-backed-probe transient-installed-probe installed-no-original identity-boundaries load-truncated-stream output-tail-corruption alias-backup-crash alias-temp-crash alias-backup-error alias-temp-error alias-backup-temp recovery-open recovery-close stat-owned create-collision recovery-read recovery-stat cleanup-stat append edit precommit journal-short temp-short temp-crash rollback-park rollback-restore rename-restore reopen-ok reopen-fail; do
  "$BUILD/fatfs_faults" "$mode"
done
"$CXX" "${COMMON[@]}" src/vocab.cpp src/vocab_file_io.cpp src/state.cpp tests/test_ui_regressions.cpp -o "$BUILD/ui_regressions"
"$BUILD/ui_regressions" boxes
"$BUILD/ui_regressions" gesture
gcc -std=c11 -O2 -Wno-discarded-qualifiers -Ireferences/gbawriter/src -Ireferences/gbawriter/src/fonts -c src/entry_font.c -o "$BUILD/body_font.o"
python3 tests/setup_renderer_mocks.py "$BUILD/renderer_mocks"
"$CXX" -I"$BUILD/renderer_mocks" "${COMMON[@]}" src/vocab.cpp src/vocab_file_io.cpp src/state.cpp src/flashcard_font.cpp tests/host_compact_font.cpp "$BUILD/body_font.o" tests/test_compact_body.cpp -o "$BUILD/compact_body"
"$BUILD/compact_body"
"$CXX" "${COMMON[@]}" src/flashcard_font.cpp tests/host_compact_font.cpp "$BUILD/body_font.o" tests/test_compact_measure.cpp -Wl,--wrap=entry_font_columns -o "$BUILD/compact_measure"
"$BUILD/compact_measure"
python3 tests/test_native_direct_budget.py
"$CXX" -I"$BUILD/renderer_mocks" "${COMMON[@]}" src/vocab.cpp src/vocab_file_io.cpp src/state.cpp src/flashcard_font.cpp tests/host_compact_font.cpp "$BUILD/body_font.o" tests/test_native_hide_reuse.cpp -o "$BUILD/native_hide"
"$BUILD/native_hide"
python3 tests/test_flashcard_font_parity.py
python3 tests/test_compact_body_wiring.py
python3 tests/test_body_copy_budget.py
python3 tests/test_body_column_budget.py
"$CXX" -I"$BUILD/renderer_mocks" "${COMMON[@]}" src/vocab.cpp src/vocab_file_io.cpp src/state.cpp src/render.cpp src/flashcard_font.cpp tests/host_compact_font.cpp "$BUILD/body_font.o" tests/test_feedback_render.cpp -o "$BUILD/feedback_render"
"$BUILD/feedback_render"
"$CXX" -I"$BUILD/renderer_mocks" "${COMMON[@]}" src/vocab.cpp src/vocab_file_io.cpp src/state.cpp src/render.cpp src/flashcard_font.cpp tests/host_compact_font.cpp "$BUILD/body_font.o" tests/test_renderer_regressions.cpp -o "$BUILD/renderer_regressions"
"$BUILD/renderer_regressions"
"$BUILD/renderer_regressions" tests/fixtures/arabic/sample1.txt
"$BUILD/renderer_regressions" tests/fixtures/arabic/sample2.txt
"$CXX" "${COMMON[@]}" src/writer_layout.cpp tests/test_arabic_layout.cpp -o "$BUILD/arabic_layout"
"$BUILD/arabic_layout"
"$CXX" "${COMMON[@]}" tests/test_solo_r.cpp -o "$BUILD/solo_r"
"$BUILD/solo_r"
"$CXX" "${COMMON[@]}" tests/test_input_layout_v15.cpp -o "$BUILD/input_layout_v15"
"$BUILD/input_layout_v15"
python3 tests/test_layout_v15_docs.py
for name in core frames layout; do
  "$CXX" "${COMMON[@]}" src/writer_layout.cpp "tests/test_writer_$name.cpp" -o "$BUILD/writer_$name"
  "$BUILD/writer_$name"
done
"$CXX" "${COMMON[@]}" src/writer_layout.cpp src/entry_editor.cpp src/vocab.cpp tests/test_entry_editor.cpp -o "$BUILD/entry_editor"
"$BUILD/entry_editor"
"$CXX" "${COMMON[@]}" src/writer_layout.cpp src/entry_editor.cpp src/vocab.cpp tests/test_entry_columns.cpp -o "$BUILD/entry_columns"
"$BUILD/entry_columns"
"$CXX" "${COMMON[@]}" -DVOCAB_HOST_FATFS src/vocab.cpp src/vocab_file_io.cpp tests/host_fatfs.cpp tests/test_entry_storage.cpp -o "$BUILD/entry_storage"
"$BUILD/entry_storage"
"$CXX" "${COMMON[@]}" -DVOCAB_HOST_FATFS src/vocab.cpp src/vocab_file_io.cpp tests/host_fatfs.cpp tests/test_entry_storage_failures.cpp -o "$BUILD/entry_storage_failures"
"$BUILD/entry_storage_failures"
gcc -std=c11 -Wno-discarded-qualifiers -Ireferences/gbawriter/src -Ireferences/gbawriter/src/fonts -c src/entry_font.c -o "$BUILD/entry_font.o"
"$CXX" "${COMMON[@]}" -Ireferences/gbawriter/src/fonts src/writer_layout.cpp src/entry_editor.cpp src/entry_render.cpp src/vocab.cpp tests/test_entry_render.cpp "$BUILD/entry_font.o" -o "$BUILD/entry_render"
"$BUILD/entry_render" references/gbawriter/res/fonts.pack references/gbawriter/res/reader-symbols.pack
"$CXX" "${COMMON[@]}" -Ireferences/gbawriter/src/fonts src/writer_layout.cpp src/entry_editor.cpp src/entry_render.cpp src/vocab.cpp tests/test_arabic_entry.cpp "$BUILD/entry_font.o" -o "$BUILD/arabic_entry"
"$BUILD/arabic_entry"
python3 tests/test_entry_wiring.py
python3 tests/test_no_sav_persistence.py
python3 tests/test_ui_palette.py
python3 tests/test_ui_io_contract.py
for name in deferred; do
  "$CXX" "${COMMON[@]}" -DVOCAB_HOST_FATFS src/vocab.cpp src/vocab_file_io.cpp tests/host_fatfs.cpp "tests/test_entry_$name.cpp" -o "$BUILD/entry_$name"
  "$BUILD/entry_$name"
done
"$CXX" "${COMMON[@]}" src/writer_layout.cpp src/entry_editor.cpp src/vocab.cpp tests/test_entry_autosave.cpp -o "$BUILD/entry_autosave"
"$BUILD/entry_autosave"
"$CXX" "${COMMON[@]}" -DVOCAB_HOST_FATFS -DVOCAB_NO_DEMOS -DVOCAB_ROOT_DIRECTORY src/vocab.cpp src/vocab_file_io.cpp tests/host_fatfs.cpp tests/test_root_directory.cpp -o "$BUILD/root_directory"
"$BUILD/root_directory"
"$CXX" "${COMMON[@]}" -DVOCAB_HOST_FATFS src/vocab.cpp tests/host_fatfs.cpp tests/test_pending_unverified.cpp -o "$BUILD/pending_unverified"
"$BUILD/pending_unverified"
python3 tests/test_no_arabic_fonts.py
gcc -std=c11 -Wall -Wextra -Wno-discarded-qualifiers -Wno-old-style-declaration -Ireferences/gbawriter/src -Ireferences/gbawriter/src/fonts tests/test_entry_font_coverage.c -o "$BUILD/entry_font_coverage"
"$BUILD/entry_font_coverage"
"$CXX" -std=c++17 -Iinclude -DHOME_SCREEN_HOST_TEST src/home_screen.cpp tests/test_credit_credentials.cpp -o "$BUILD/credit_credentials"
"$BUILD/credit_credentials"
bash tests/run_list_metadata_tests.sh
bash tests/run_home_tests.sh
bash tests/run_select_accent_tests.sh
python3 tests/test_select_accent_docs.py
bash tests/run_dictionary_tests.sh
printf '\nPASS all host suites (production FatFS adapter included)\n'
