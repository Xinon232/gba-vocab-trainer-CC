// Exercise private bounded I/O primitives without exporting a device test API.
#include "../src/vocab_file_io.cpp"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>
#include <functional>
extern std::function<FRESULT(const std::string&,const std::string&)> fat_hook;
extern std::string fat_root;
extern void fat_reset();
int main(int argc, char** argv) {
    assert(argc == 2);
    if (std::string(argv[1]) == "capacity") {
        assert(sizeof s_sequential_read_buffer == 4096);
        assert(sizeof s_save_write_buffer == 4096);
        puts("PASS bounded 4KiB scanner/output capacities"); return 0;
    }
    char dir[] = "/tmp/gbavocab-windows-XXXXXX";
    assert(mkdtemp(dir)); fat_root = dir;
    struct Cleanup { ~Cleanup() { fat_reset(); std::filesystem::remove_all(fat_root); } } cleanup;
    std::string text;
    const int rows = std::string(argv[1]) == "maximum" ? VOCAB_MAX_LINES : 137;
    for (int i = 0; i < rows; ++i) {
        std::string row = std::to_string(i) + " café ";
        row += std::string(189 - row.size(), 'W'); row += "\tb";
        assert(row.size() == 191); text += row;
        if (i != rows - 1) text += (i % 2 ? "\r\n" : "\n");
    }
    std::ofstream(fat_root + "/cards.txt", std::ios::binary) << text;
    assert(vocab_file_init());
    VocabFile live; char fallback[2048]; int used = 0;
    assert(vocab_file_load("cards.txt", live, fallback, sizeof fallback, used));
    if (std::string(argv[1]) == "short-scanner" || std::string(argv[1]) == "short-identity") {
        extern UINT fat_read_limit;
        fat_read_limit = 17;
        if (std::string(argv[1]) == "short-scanner") {
            FIL file; assert(tracked_open(&file, "cards.txt", FA_READ) == FR_OK);
            FatFsSequentialSource scanner(file); char byte; uint32_t offset;
            assert(!scanner.next(byte, offset) && scanner.failed());
            assert(tracked_close(&file) == FR_OK);
        } else {
            FileIdentity identity;
            assert(!identify_file("cards.txt", identity));
        }
        FIL file; assert(tracked_open(&file, "cards.txt", FA_READ) == FR_OK);
        FatFsRawRowReader reader(file); char row[VOCAB_RAW_LINE_MAX]; int length;
        assert(!reader.read(0, row, length));
        assert(tracked_close(&file) == FR_OK);
        puts("PASS scanner/identity/source windows reject successful short non-EOF reads"); return 0;
    }
    // Empty boxes 1/3, and backward/shuffled source offsets across windows.
    for (int i = 0; i < live.line_count; ++i)
        for (int k = 0; k < (i % 2 ? 3 : 1); ++k) vocab_advance(live, i);
    assert(vocab_shuffle_field(live, 2, 47));
    assert(vocab_shuffle_field(live, 4, 91));
    std::vector<char> exported(text.size() + rows + 16);
    int length = vocab_export_grouped(live, text.data(), int(text.size()), exported.data(), int(exported.size()));
    assert(length > 0);
    VocabFile reference; assert(vocab_open(reference, exported.data(), length) == live.line_count);
    const std::string mode = argv[1];
    if (mode.rfind("installed-", 0) == 0) {
        bool promoted = false;
        fat_hook = [&](auto op, auto path) {
            if (op == "renamed" && path == "cards.txt") {
                promoted = true;
                if (mode == "installed-offset") ++s_reindex_scratch.line_offsets[0];
                else if (mode == "installed-box") {
                    --s_reindex_scratch.field_counts[s_reindex_scratch.field[0] - 1];
                    s_reindex_scratch.field[0] = 5;
                    ++s_reindex_scratch.field_counts[4];
                } else if (mode == "installed-count") ++s_reindex_scratch.field_counts[0];
                else {
                    std::string corrupt(exported.data(), length);
                    if (mode == "installed-tail") corrupt += "\r\n";
                    else corrupt[reference.line_offsets[0]] = 'Q';
                    std::ofstream(fat_root + "/cards.txt", std::ios::binary) << corrupt;
                }
            }
            // Leave a physical backup to inspect when installed validation fails.
            if (promoted && op == "rename" && path == "cards.txt.gbv1.tmp") return FR_DISK_ERR;
            return FR_OK;
        };
        int written = 0; char output[4096];
        assert(!vocab_file_save_grouped(live, fallback, used, output, sizeof output, written));
        assert(promoted && vocab_any_dirty(live) && !vocab_file_save_installed_index());
        std::ifstream backup(fat_root + "/cards.txt.gbv1.bak", std::ios::binary);
        std::string preserved{std::istreambuf_iterator<char>(backup), {}};
        assert(preserved == text);
        assert(std::filesystem::exists(fat_root + "/cards.txt.gbv1.txn"));
        LineBuf row; assert(!vocab_file_show(live, fallback, used, 0, row));
        puts("PASS installed corruption/index/box/count failure retains original backup and dirty state"); return 0;
    }
    bool created = false; FileIdentity identity;
    assert(write_sd_grouped_temp(live, "planned.tmp", created, identity));
    assert(created);
    // The index must exist BEFORE readback; readback must verify, not invent it.
    assert(s_reindex_scratch.line_count == reference.line_count);
    for (int i = 0; i < reference.line_count; ++i) {
        assert(s_reindex_scratch.line_offsets[i] == reference.line_offsets[i]);
        assert(s_reindex_scratch.field[i] == reference.field[i]);
    }
    assert(validate_replacement("planned.tmp", "cards.txt", live, s_reindex_scratch, identity));
    auto installed = [&](const FileIdentity& id) {
        return validate_replacement("planned.tmp", nullptr, live, s_reindex_scratch, id,
                                    ReplacementValidation::installed_fingerprint);
    };
    assert(installed(identity));
    // Generated offsets are not authority: a wrong planned offset must fail.
    ++s_reindex_scratch.line_offsets[0];
    assert(!validate_replacement("planned.tmp", "cards.txt", live, s_reindex_scratch, identity));
    assert(!installed(identity));
    --s_reindex_scratch.line_offsets[0];
    assert(validate_replacement("planned.tmp", "cards.txt", live, s_reindex_scratch, identity));
    // Reseal physical identity around parseable same-size corruption to prove
    // exact ordered source-row comparison is independent from hash/index checks.
    std::string changed(exported.data(), length);
    changed[reference.line_offsets[0]] = 'Q';
    std::ofstream(fat_root + "/planned.tmp", std::ios::binary) << changed;
    FileIdentity altered;
    assert(identify_file("planned.tmp", altered));
    assert(!validate_replacement("planned.tmp", "cards.txt", live, s_reindex_scratch, altered));
    assert(!installed(identity)); // Intended identity catches physical corruption.
    assert(installed(altered)); // Resealed noncryptographic identity is NOT exact proof.
    changed.assign(exported.data(), length);
    std::string first = changed.substr(reference.line_offsets[0], 191);
    changed.replace(reference.line_offsets[0], 191, changed.substr(reference.line_offsets[1], 191));
    changed.replace(reference.line_offsets[1], 191, first);
    std::ofstream(fat_root + "/planned.tmp", std::ios::binary) << changed;
    assert(identify_file("planned.tmp", altered));
    assert(!validate_replacement("planned.tmp", "cards.txt", live, s_reindex_scratch, altered));
    assert(!installed(identity));
    assert(installed(altered));
    puts("PASS generated index checked against streamed persisted rows and offsets, shuffled max rows, empty boxes, resealed corruption");
}
