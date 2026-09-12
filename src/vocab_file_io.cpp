// vocab_file_io.cpp — SD/FatFS streaming implementation.

#include "vocab_file_io.h"
#include "vocab_scanner.h"
#include "writer_core.h"

#include <cstring>

#if defined(__DEVKITARM__) || defined(VOCAB_HOST_FATFS)
#include "fatfs/ff.h"
#ifdef VOCAB_HOST_FATFS
#define BN_DATA_EWRAM_BSS
#else
#include "bn_core.h"
#include "gbahw.h"
extern "C" {
#include "supercard_driver.h"
}
#endif
#endif

static char s_names[VOCAB_MAX_BROWSER_FILES][VOCAB_FILENAME_MAX];
static int s_name_count = 0;
static bool s_sd_ready = false;
static bool s_loaded_from_sd = false;
static char s_loaded_name[VOCAB_FILENAME_MAX];
static uint32_t s_loaded_generation = 0;
static VocabIoStats s_io_stats = {};
static bool s_save_installed_index = false;
static const char* s_last_error = "SD I/O ERROR";
// Reserved virtual offsets cannot alias accepted physical source files.
constexpr uint32_t PENDING_ROW_BASE = UINT32_MAX - VOCAB_PENDING_ROWS;
#if defined(__DEVKITARM__) || defined(VOCAB_HOST_FATFS)
BN_DATA_EWRAM_BSS static char s_pending_rows[VOCAB_PENDING_ROWS][VOCAB_RAW_LINE_MAX];
#else
static char s_pending_rows[VOCAB_PENDING_ROWS][VOCAB_RAW_LINE_MAX];
#endif
static const char* pending_row(uint32_t offset) {
    return offset >= PENDING_ROW_BASE && offset < UINT32_MAX ?
        s_pending_rows[offset - PENDING_ROW_BASE] : nullptr;
}
bool vocab_file_save_installed_index() { return s_save_installed_index; }
const char* vocab_file_last_error() { return s_last_error; }

void vocab_file_io_reset_stats()
{
    s_io_stats = {};
}

VocabIoStats vocab_file_io_stats()
{
    return s_io_stats;
}

// One parsed card (~193 bytes), keyed by source generation + array generation
// + physical source offset. This keeps ordinary/feedback/animation frames off
// the SD card while remaining bounded regardless of vocabulary-file size.
static bool s_card_cache_valid = false;
static uint32_t s_card_cache_loaded_generation = 0;
static uint32_t s_card_cache_array_generation = 0;
static uint32_t s_card_cache_offset = 0;
#if defined(__DEVKITARM__) || defined(VOCAB_HOST_FATFS)
BN_DATA_EWRAM_BSS static LineBuf s_card_cache_line;
#else
static LineBuf s_card_cache_line;
#endif
static int s_card_cache_misses = 0;

static void invalidate_card_cache()
{
    s_card_cache_valid = false;
}

static void begin_loaded_file_generation()
{
    ++s_loaded_generation;
    if (s_loaded_generation == 0) {
        ++s_loaded_generation;
    }
    invalidate_card_cache();
}

template<typename OpenFn>
static bool ensure_source_open(bool& is_open, OpenFn open_fn)
{
    if (is_open) return true;
    if (!open_fn()) return false;
    is_open = true;
    return true;
}

template<typename CloseFn>
static bool close_source(bool& is_open, CloseFn close_fn)
{
    if (!is_open) return true;
    const bool ok = close_fn();
    if (ok) is_open = false;
    return ok;
}

void vocab_file_cache_reset_stats_for_tests()
{
    s_card_cache_misses = 0;
    invalidate_card_cache();
}

int vocab_file_cache_misses_for_tests()
{
    return s_card_cache_misses;
}

template<typename ReopenFn>
static bool finalize_committed_save(VocabFile& live, const VocabFile& reindexed,
                                    ReopenFn reopen_fn)
{
    // The replacement is already the authoritative TXT. Install its validated
    // offsets before reopening so a transient reopen failure can never make a
    // retry apply old offsets to the newly grouped file.
    s_save_installed_index = true;
    live = reindexed;
    vocab_clear_dirty(live);
    live.array_generation = 0;
    begin_loaded_file_generation();
    return reopen_fn();
}

#ifndef __DEVKITARM__
bool vocab_file_close_failure_keeps_source_open_for_tests()
{
    bool open = true;
    bool closed = close_source(open, []() { return false; });
    return !closed && open;
}

bool vocab_file_finalize_committed_save_for_tests(VocabFile& live,
                                                   const VocabFile& reindexed,
                                                   bool reopen_succeeds)
{
    return finalize_committed_save(live, reindexed,
                                   [reopen_succeeds]() { return reopen_succeeds; });
}

VocabIoStats vocab_file_persistent_source_for_tests()
{
    VocabIoStats stats = {};
    bool open = false;
    auto open_source = [&stats]() { ++stats.file_opens; return true; };
    auto close_source_fn = [&stats]() { ++stats.closes; return true; };

    ensure_source_open(open, open_source);  // load
    ensure_source_open(open, open_source);  // first card
    ensure_source_open(open, open_source);  // transition
    ensure_source_open(open, open_source);  // another transition
    close_source(open, close_source_fn);    // replacement save
    ensure_source_open(open, open_source);  // reopen saved source
    return stats;
}
#endif

enum class ReplacementOutcome { committed, restored, recovery_required };
template<typename Ops>
static ReplacementOutcome run_replacement_transaction(Ops& ops)
{
    if (!ops.rename_original_to_backup()) return ReplacementOutcome::restored;
    if (!ops.rename_temporary_to_original()) {
        return ops.restore_backup() ? ReplacementOutcome::restored : ReplacementOutcome::recovery_required;
    }
    if (!ops.reindex_replacement()) {
        return ops.park_failed_replacement() && ops.restore_backup() ?
            ReplacementOutcome::restored : ReplacementOutcome::recovery_required;
    }
    // The replacement has already been validated and is now the authoritative
    // TXT. Failure to remove the backup is a recoverable cleanup condition, not
    // a failed save: treating it as retryable would apply stale offsets to the
    // newly grouped file. Startup/reload recovery removes a leftover backup.
    ops.remove_backup();
    return ReplacementOutcome::committed;
}

#ifndef __DEVKITARM__
class HostTransactionOps {
public:
    explicit HostTransactionOps(VocabIoFailurePoint failure) :
        failure_(failure), original_(true), backup_(false), temporary_(true),
        reindex_ok_(false), stats_{} {}

    bool rename_original_to_backup() {
        ++stats_.renames;
        if (failure_ == VOCAB_IO_FAIL_BACKUP_RENAME) return false;
        original_ = false; backup_ = true; return true;
    }
    bool rename_temporary_to_original() {
        ++stats_.renames;
        if (failure_ == VOCAB_IO_FAIL_REPLACEMENT_RENAME) return false;
        temporary_ = false; original_ = true; return true;
    }
    bool reindex_replacement() {
        ++stats_.index_scans;
        reindex_ok_ = failure_ != VOCAB_IO_FAIL_REINDEX;
        return reindex_ok_;
    }
    bool park_failed_replacement() {
        ++stats_.renames;
        if (!original_ || temporary_) return false;
        original_ = false; temporary_ = true; return true;
    }
    bool restore_backup() {
        ++stats_.renames;
        if (!backup_ || original_) return false;
        backup_ = false; original_ = true; return true;
    }
    bool remove_backup() {
        ++stats_.unlinks;
        if (!backup_ || failure_ == VOCAB_IO_FAIL_BACKUP_UNLINK) return false;
        backup_ = false; return true;
    }

    bool original() const { return original_; }
    bool backup() const { return backup_; }
    bool temporary() const { return temporary_; }
    const VocabIoStats& stats() const { return stats_; }

private:
    VocabIoFailurePoint failure_;
    bool original_;
    bool backup_;
    bool temporary_;
    bool reindex_ok_;
    VocabIoStats stats_;
};

VocabTransactionTestResult vocab_file_transaction_for_tests(VocabIoFailurePoint failure)
{
    VocabTransactionTestResult result = {};
    result.dirty = true;
    result.original_valid = true;
    if (failure == VOCAB_IO_FAIL_WRITE || failure == VOCAB_IO_FAIL_CLOSE) {
        result.stats.write_calls = 1;
        result.stats.closes = failure == VOCAB_IO_FAIL_CLOSE ? 1 : 0;
        result.temporary_valid = false;
        return result;
    }

    HostTransactionOps ops(failure);
    result.success = run_replacement_transaction(ops) == ReplacementOutcome::committed;
    result.dirty = !result.success;
    result.original_valid = ops.original();
    result.backup_valid = ops.backup();
    result.temporary_valid = ops.temporary();
    result.stats = ops.stats();
    return result;
}
#endif

#if defined(__DEVKITARM__) || defined(VOCAB_HOST_FATFS)
#include "list_pair_storage.h"
static FATFS s_fatfs;
// FIL addresses never change: FatFS locks and host adapters own these objects.
BN_DATA_EWRAM_BSS static FIL s_source_pool[2];
static FIL* s_loaded_source = &s_source_pool[0];
static FIL* s_candidate_source = &s_source_pool[1];
static bool s_loaded_source_open = false;
static bool s_candidate_source_open = false;
// Save-only reindex scratch: bounded metadata (offsets/fields/dirty/counts), not
// vocabulary text. Keeping it static places it in normal EWRAM/BSS rather than
// on the small GBA stack and preserves the live dirty state if reindex fails.
BN_DATA_EWRAM_BSS static VocabFile s_reindex_scratch;
// Exclusive workspaces: scanner/identity, output writer, random source rows.
// Save writing uses output + source; installed validation uses scanner only.
// No scanner, identity helper or writer is invoked while sharing a live window.
static constexpr UINT IO_WINDOW_BYTES = 4096;
static constexpr uint32_t IO_SECTOR_BYTES = 512;
static_assert(IO_WINDOW_BYTES % IO_SECTOR_BYTES == 0, "whole sector windows");
BN_DATA_EWRAM_BSS alignas(4) static char s_sequential_read_buffer[IO_WINDOW_BYTES];
BN_DATA_EWRAM_BSS alignas(4) static char s_save_write_buffer[IO_WINDOW_BYTES];
BN_DATA_EWRAM_BSS alignas(4) static char s_source_row_buffer[IO_WINDOW_BYTES];

// Public filenames are basenames; every production operation is rooted here.
static const char* storage_path(const char* name, char (&out)[VOCAB_FILENAME_MAX + 10]) {
#if defined(__DEVKITARM__) || defined(VOCAB_ROOT_DIRECTORY)
    if (!name || std::strlen(name) >= VOCAB_FILENAME_MAX ||
        std::strchr(name, '/') || std::strchr(name, '\\') || std::strchr(name, ':')) return nullptr;
    std::strcpy(out, "/gbavocab/"); std::strcpy(out + 10, name); return out;
#else
    (void)out; return name;
#endif
}
static FRESULT tracked_open(FIL* fp, const char* path, BYTE mode)
{
    ++s_io_stats.file_opens;
    char full[VOCAB_FILENAME_MAX + 10];
    const char* resolved = storage_path(path, full);
    return resolved ? f_open(fp, resolved, mode) : FR_INVALID_NAME;
}

static FRESULT tracked_close(FIL* fp)
{
    ++s_io_stats.closes;
    return f_close(fp);
}

static bool ensure_loaded_source_open()
{
    return ensure_source_open(s_loaded_source_open, []() {
        return tracked_open(s_loaded_source, s_loaded_name,
                            FA_READ | FA_OPEN_EXISTING) == FR_OK;
    });
}

static bool close_loaded_source()
{
    return close_source(s_loaded_source_open, []() {
        return tracked_close(s_loaded_source) == FR_OK;
    });
}

static bool close_candidate_source()
{
    return close_source(s_candidate_source_open, []() {
        return tracked_close(s_candidate_source) == FR_OK;
    });
}

static FRESULT tracked_seek(FIL* fp, FSIZE_t offset)
{
    ++s_io_stats.seeks;
    return f_lseek(fp, offset);
}

static FRESULT tracked_read(FIL* fp, void* buffer, UINT bytes, UINT* read)
{
    ++s_io_stats.read_calls;
    FRESULT result = f_read(fp, buffer, bytes, read);
    if (read) s_io_stats.bytes_read += *read;
    return result;
}

static FRESULT tracked_write(FIL* fp, const void* buffer, UINT bytes, UINT* written)
{
    ++s_io_stats.write_calls;
    FRESULT result = f_write(fp, buffer, bytes, written);
    if (written) s_io_stats.bytes_written += *written;
    return result;
}

static FRESULT tracked_sync(FIL* fp)
{
    return f_sync(fp);
}

static FRESULT tracked_rename(const char* from, const char* to)
{
    ++s_io_stats.renames;
    char a[VOCAB_FILENAME_MAX + 10], b[VOCAB_FILENAME_MAX + 10];
    const char* source = storage_path(from, a); const char* target = storage_path(to, b);
    return source && target ? f_rename(source, target) : FR_INVALID_NAME;
}

static FRESULT tracked_unlink(const char* path)
{
    ++s_io_stats.unlinks;
    char full[VOCAB_FILENAME_MAX + 10];
    const char* resolved = storage_path(path, full);
    return resolved ? f_unlink(resolved) : FR_INVALID_NAME;
}
#endif

static bool str_eq_local(const char* a, const char* b)
{
    if (!a || !b) return false;
    int i = 0;
    while (a[i] && b[i]) {
        if (a[i] != b[i]) return false;
        i++;
    }
    return a[i] == b[i];
}

static bool has_txt_ext(const char* name)
{
    if (!name) return false;
    int len = 0;
    while (name[len]) len++;
    if (len < 5 || len >= VOCAB_FILENAME_MAX) return false;
    const char* e = name + len - 4;
    return (e[0] == '.') &&
           (e[1] == 't' || e[1] == 'T') &&
           (e[2] == 'x' || e[2] == 'X') &&
           (e[3] == 't' || e[3] == 'T');
}

static bool make_sidecar_name(const char* original, const char* suffix,
                              char out[VOCAB_FILENAME_MAX], int slot = 1)
{
    if (!original || !suffix || !has_txt_ext(original)) return false;
    int len = 0;
    while (original[len]) {
        if (len >= VOCAB_FILENAME_MAX - 1) return false;
        out[len] = original[len];
        ++len;
    }
    if (len < 4 || suffix[0] != '.' || !suffix[1] || !suffix[2] || !suffix[3] || suffix[4]) {
        return false;
    }
    char owner[] = ".gbv1";
    owner[4] = char('0' + slot);
    if (len + 9 >= VOCAB_FILENAME_MAX) return false;
    for (int i = 0; i < 5; ++i) out[len++] = owner[i];
    for (int i = 0; i < 4; ++i) out[len++] = suffix[i];
    out[len] = 0;
    return !str_eq_local(original, out);
}

bool vocab_file_sidecar_name_for_tests(const char* original, const char* suffix,
                                       char out[VOCAB_FILENAME_MAX])
{
    return make_sidecar_name(original, suffix, out);
}

static void add_name(const char* name)
{
    if (!name || s_name_count >= VOCAB_MAX_BROWSER_FILES) return;
    for (int i = 0; i < s_name_count; i++) {
        if (str_eq_local(s_names[i], name)) return;
    }
    int j = 0;
    while (name[j] && j < VOCAB_FILENAME_MAX - 1) {
        s_names[s_name_count][j] = name[j];
        j++;
    }
    s_names[s_name_count][j] = 0;
    s_name_count++;
}

#if !defined(__DEVKITARM__) && !defined(VOCAB_NO_DEMOS)
#include "../tests/legacy_builtin_fixture.h"
#else
static void ensure_names_for_tests_or_fallback() {}
bool vocab_file_read_builtin_or_stub(const char*, char*, int, int&) { return false; }
#endif

static void set_loaded_name(const char* filename)
{
    int i = 0;
    while (filename && filename[i] && i < VOCAB_FILENAME_MAX - 1) {
        s_loaded_name[i] = filename[i];
        i++;
    }
    s_loaded_name[i] = 0;
}

#if defined(__DEVKITARM__) || defined(VOCAB_HOST_FATFS)
static bool recover_sd_sidecars(const char* filename);
static void scan_sd_root()
{
    DIR dir;
    FILINFO info;
#if defined(__DEVKITARM__) || defined(VOCAB_ROOT_DIRECTORY)
    if (f_opendir(&dir, "/gbavocab") != FR_OK) return;
#else
    if (f_opendir(&dir, "/") != FR_OK) return;
#endif
    while (true) {
        if (f_readdir(&dir, &info) != FR_OK) break;
        if (!info.fname[0]) break;
        if ((info.fattrib & AM_DIR) == 0) {
            if (has_txt_ext(info.fname)) add_name(info.fname);
            int len = int(std::strlen(info.fname));
            if (len > 9 && len < VOCAB_FILENAME_MAX &&
                std::memcmp(info.fname + len - 9, ".gbv", 4) == 0 &&
                info.fname[len - 5] >= '1' && info.fname[len - 5] <= '9' &&
                std::memcmp(info.fname + len - 4, ".txn", 4) == 0) {
                char original[VOCAB_FILENAME_MAX];
                std::memcpy(original, info.fname, len - 9);
                original[len - 9] = 0;
                if (has_txt_ext(original) && recover_sd_sidecars(original)) add_name(original);
            }
        }
    }
    f_closedir(&dir);
}
#endif

bool vocab_file_init()
{
#if defined(__DEVKITARM__) || defined(VOCAB_HOST_FATFS)
    // A failed close must not be followed by remounting or forgetting a lock.
    // INVALID_OBJECT means a previous filesystem reset already invalidated it.
    auto close_for_remount = [](FIL* file, bool& open) {
        if (!open) return true;
        FRESULT result = tracked_close(file);
        if (result != FR_OK && result != FR_INVALID_OBJECT) return false;
        open = false;
        return true;
    };
    if (!close_for_remount(s_candidate_source, s_candidate_source_open) ||
        !close_for_remount(s_loaded_source, s_loaded_source_open)) return false;
#endif
    s_name_count = 0;
    s_loaded_from_sd = false;
    s_loaded_name[0] = 0;
#if defined(__DEVKITARM__) || defined(VOCAB_HOST_FATFS)
    // SuperFW initializes the SuperCard hardware before mounting FatFS:
    // faster WAITCNT, map SDRAM, enable SD interface, then run sdcard_init.
    // Without this the hardware probe stalls for a few seconds and mount
    // reports unavailable storage without a vocabulary fallback.
#ifdef VOCAB_HOST_FATFS
    s_sd_ready = f_mount(&s_fatfs, "0:", 1) == FR_OK;
#else
    REG_WAITCNT = 0x40c0;
    set_supercard_mode(MAPPED_SDRAM, true, true);
    t_card_info sd_info;
    unsigned sd_ret = sdcard_init(&sd_info);
    s_sd_ready = (sd_ret == 0) && (f_mount(&s_fatfs, "0:", 1) == FR_OK);
 #endif
    if (s_sd_ready) {
        scan_sd_root();
    }
#else
    s_sd_ready = false;
#endif
    ensure_names_for_tests_or_fallback();
    return s_sd_ready;
}

bool vocab_file_sd_ready() { return s_sd_ready; }
bool vocab_file_loaded_from_sd() { return s_loaded_from_sd; }

int vocab_file_count()
{
    ensure_names_for_tests_or_fallback();
    return s_name_count;
}

const char* vocab_file_name(int index)
{
    ensure_names_for_tests_or_fallback();
    if (index < 0 || index >= s_name_count) return nullptr;
    return s_names[index];
}

template<typename Source>
static int scan_sequential_source(Source& source, VocabFile& vf)
{
    return vocab_scan(source, vf);
}

#ifndef __DEVKITARM__
class MemorySequentialSource {
public:
    MemorySequentialSource(const char* data, int data_len, int chunk_size) :
        data_(data), data_len_(data_len), chunk_size_(chunk_size),
        source_pos_(0), buffer_pos_(0), buffer_len_(0), read_calls_(0)
    {
        if (chunk_size_ < 1) chunk_size_ = 1;
        if (chunk_size_ > 1024) chunk_size_ = 1024;
    }

    bool next(char& out, uint32_t& absolute_offset)
    {
        if (buffer_pos_ >= buffer_len_) {
            if (source_pos_ >= data_len_) return false;
            int remaining = data_len_ - source_pos_;
            buffer_len_ = remaining < chunk_size_ ? remaining : chunk_size_;
            std::memcpy(buffer_, data_ + source_pos_, (size_t)buffer_len_);
            buffer_start_ = source_pos_;
            source_pos_ += buffer_len_;
            buffer_pos_ = 0;
            ++read_calls_;
        }
        absolute_offset = (uint32_t)(buffer_start_ + buffer_pos_);
        out = buffer_[buffer_pos_++];
        return true;
    }

    int read_calls() const { return read_calls_; }

private:
    const char* data_;
    int data_len_;
    int chunk_size_;
    int source_pos_;
    int buffer_start_ = 0;
    int buffer_pos_;
    int buffer_len_;
    int read_calls_;
    alignas(4) char buffer_[1024];
};

int vocab_file_scan_buffered_for_tests(const char* data, int data_len, int chunk_size,
                                       VocabFile& vf, int& bulk_read_calls)
{
    if (!data || data_len < 0) {
        vf.reset();
        bulk_read_calls = 0;
        return 0;
    }
    MemorySequentialSource source(data, data_len, chunk_size);
    int loaded = scan_sequential_source(source, vf);
    bulk_read_calls = source.read_calls();
    return loaded;
}
#endif

#if defined(__DEVKITARM__) || defined(VOCAB_HOST_FATFS)
// Identity is accumulated from the same bytes consumed by indexing/writing.
struct FileIdentity { uint32_t size, hash, sum; };
static FileIdentity empty_identity() { return {0, 2166136261u, 5381u}; }
static void extend_identity(FileIdentity& id, const char* bytes, UINT count)
{
    id.size += count;
    for (UINT i = 0; i < count; ++i) {
        unsigned char c = bytes[i];
        id.hash = (id.hash ^ c) * 16777619u;
        id.sum = id.sum * 33u + c;
    }
}
static bool same_identity(const FileIdentity& a, const FileIdentity& b)
{
    return a.size == b.size && a.hash == b.hash && a.sum == b.sum;
}

class FatFsSequentialSource {
public:
    explicit FatFsSequentialSource(FIL& fp) :
        fp_(fp), buffer_start_(0), buffer_pos_(0), buffer_len_(0), failed_(false)
    {
    }

    bool next(char& out, uint32_t& absolute_offset)
    {
        if (buffer_pos_ >= buffer_len_) {
            buffer_start_ = (uint32_t)f_tell(&fp_);
            if (buffer_start_ > f_size(&fp_)) { failed_ = true; return false; }
            const FSIZE_t remaining = f_size(&fp_) - buffer_start_;
            const UINT expected = remaining < IO_WINDOW_BYTES ? UINT(remaining) : IO_WINDOW_BYTES;
            UINT bytes_read = 0;
            FRESULT result = tracked_read(&fp_, s_sequential_read_buffer,
                                          sizeof(s_sequential_read_buffer), &bytes_read);
            if (result != FR_OK || bytes_read != expected) {
                failed_ = true;
                return false;
            }
            extend_identity(identity_, s_sequential_read_buffer, bytes_read);
            buffer_pos_ = 0;
            buffer_len_ = (int)bytes_read;
            if (buffer_len_ == 0) return false;
        }
        absolute_offset = buffer_start_ + (uint32_t)buffer_pos_;
        out = s_sequential_read_buffer[buffer_pos_++];
        return true;
    }

    bool failed() const { return failed_; }
    const FileIdentity& identity() const { return identity_; }

private:
    FIL& fp_;
    uint32_t buffer_start_;
    int buffer_pos_;
    int buffer_len_;
    bool failed_;
    FileIdentity identity_ = empty_identity();
};

static bool scan_sd_index(FIL& fp, VocabFile& vf, FileIdentity& identity)
{
    FatFsSequentialSource source(fp);
    ++s_io_stats.index_scans;
    int loaded = scan_sequential_source(source, vf);
    identity = source.identity();
    bool size_ok = identity.size == f_size(&fp);
    if (loaded == 0 && !vf.rejected_rows) vf.loaded = true;
    if (source.failed() || !size_ok || loaded < 0 || (loaded == 0 && vf.rejected_rows)) {
        vf.reset();
        return false;
    }
    return true;
}

enum class Presence { missing, present, error };
static Presence probe_path(const char* path)
{
    FILINFO info;
    char full[VOCAB_FILENAME_MAX + 10];
    const char* resolved = storage_path(path, full);
    FRESULT r = resolved ? f_stat(resolved, &info) : FR_INVALID_NAME;
    if (r == FR_OK) return Presence::present;
    if (r == FR_NO_FILE || r == FR_NO_PATH) return Presence::missing;
    return Presence::error;
}

// FatFS rename can copy a directory entry before removing the old entry. Two
// names may then own the same chain: unlinking either frees BOTH files' data.
// Content equality is not enough. Probe every transaction name before mutation,
// including the journal; never repair FAT or guess through an I/O error.
enum class ChainCheck { distinct, alias, error };
static ChainCheck check_transaction_chains(const char* original, const char* tmp,
                                           const char* bak, const char* txn)
{
    const char* paths[] = {original, tmp, bak, txn};
    FATFS* volumes[4] = {};
    DWORD clusters[4] = {};
    for (int i = 0; i < 4; ++i) {
        Presence p = probe_path(paths[i]);
        if (p == Presence::error) return ChainCheck::error;
        if (p == Presence::missing) continue;
        FIL f;
        if (tracked_open(&f, paths[i], FA_READ) != FR_OK) return ChainCheck::error;
        volumes[i] = f.obj.fs;
        clusters[i] = f.obj.sclust;
        // Zero is legitimate only for an unallocated empty file; it is not a
        // shared chain. A nonempty file without a chain is unsafe to mutate.
        bool valid = volumes[i] && (clusters[i] >= 2 || (!clusters[i] && !f_size(&f)));
        if (tracked_close(&f) != FR_OK || !valid) return ChainCheck::error;
        for (int k = 0; k < i; ++k) {
            if (clusters[i] && clusters[i] == clusters[k] && volumes[i] == volumes[k])
                return ChainCheck::alias;
        }
    }
    return ChainCheck::distinct;
}
static bool transaction_chains_safe(const char* original, const char* tmp,
                                     const char* bak, const char* txn)
{
    if (check_transaction_chains(original, tmp, bak, txn) == ChainCheck::distinct) return true;
    s_last_error = "RECOVERY REQUIRED";
    return false;
}

// Transient transaction identity. Checksums cover all physical bytes, including
// separators and EOF; row-count-only validation cannot identify a complete file.
static FileIdentity s_loaded_identity;
static bool s_source_verified = true;
struct SaveJournal {
    char magic[16];
    char original[VOCAB_FILENAME_MAX];
    FileIdentity before, after;
    uint32_t check;
};
static uint32_t journal_check(const SaveJournal& j)
{
    const unsigned char* p = reinterpret_cast<const unsigned char*>(&j);
    uint32_t h = 2166136261u;
    for (unsigned i = 0; i < sizeof(j) - sizeof(j.check); ++i) h = (h ^ p[i]) * 16777619u;
    return h;
}
static bool identify_file(const char* name, FileIdentity& id)
{
    FIL f;
    if (tracked_open(&f, name, FA_READ) != FR_OK) return false;
    id = empty_identity();
    bool ok = true;
    while (ok) {
        if (id.size > f_size(&f)) { ok = false; break; }
        const FSIZE_t remaining = f_size(&f) - id.size;
        const UINT expected = remaining < IO_WINDOW_BYTES ? UINT(remaining) : IO_WINDOW_BYTES;
        UINT n = 0;
        if (tracked_read(&f, s_sequential_read_buffer, sizeof s_sequential_read_buffer, &n) != FR_OK ||
            n != expected) { ok = false; break; }
        if (!n) break;
        extend_identity(id, s_sequential_read_buffer, n);
    }
    if (id.size != f_size(&f)) ok = false;
    if (tracked_close(&f) != FR_OK) ok = false;
    return ok;
}
enum class IdentityMatch { match, mismatch, error };
static IdentityMatch matches_file(const char* name, const FileIdentity& expected)
{
    FileIdentity actual;
    if (!identify_file(name, actual)) return IdentityMatch::error;
    return same_identity(actual, expected) ?
        IdentityMatch::match : IdentityMatch::mismatch;
}
static bool write_journal(const char* filename, const FileIdentity* temporary, const char* journal_name, bool& created)
{
    SaveJournal j = {};
    created = false;
    std::memcpy(j.magic, temporary ? "GBAVOCAB-TXN-1" : "GBAVOCAB-TXN-2", 14);
    std::memcpy(j.original, filename, std::strlen(filename) + 1);
    j.before = s_loaded_identity;
    if (temporary) j.after = *temporary;
    j.check = journal_check(j);
    FIL f;
    if (tracked_open(&f, journal_name, FA_WRITE | (temporary ? FA_OPEN_EXISTING : FA_CREATE_NEW)) != FR_OK) return false;
    created = !temporary;
    if (temporary && tracked_seek(&f, sizeof j) != FR_OK) { tracked_close(&f); return false; }
    UINT n = 0;
    bool ok = tracked_write(&f, &j, sizeof j, &n) == FR_OK && n == sizeof j;
    if (ok && tracked_sync(&f) != FR_OK) ok = false;
    if (tracked_close(&f) != FR_OK) ok = false;
    return ok;
}
enum class JournalRead { valid, invalid, error };
static JournalRead read_journal(const char* path, const char* filename, SaveJournal& j, bool final = false)
{
    FIL f;
    if (tracked_open(&f, path, FA_READ) != FR_OK) return JournalRead::error;
    UINT n = 0;
    bool size_ok = final ? f_size(&f) == 2 * sizeof j : f_size(&f) >= sizeof j;
    bool io_ok = (!final || tracked_seek(&f, sizeof j) == FR_OK) && tracked_read(&f, &j, sizeof j, &n) == FR_OK;
    if (tracked_close(&f) != FR_OK) io_ok = false;
    if (!io_ok) return JournalRead::error;
    if (!size_ok || n != sizeof j ||
        std::memcmp(j.magic, "GBAVOCAB-TXN-", 13) ||
        (j.magic[13] != 0 && j.magic[13] != '1' && j.magic[13] != '2') ||
        j.check != journal_check(j) || std::memcmp(j.original, filename, std::strlen(filename) + 1))
        return JournalRead::invalid;
    return JournalRead::valid;
}
static bool recover_candidate(const char* filename, int slot)
{
    char tmp[VOCAB_FILENAME_MAX], bak[VOCAB_FILENAME_MAX], txn[VOCAB_FILENAME_MAX];
    if (!make_sidecar_name(filename, ".tmp", tmp, slot) ||
        !make_sidecar_name(filename, ".bak", bak, slot) ||
        !make_sidecar_name(filename, ".txn", txn, slot)) return probe_path(filename) == Presence::present;
    // Names alone never prove ownership. Legacy generic .bak/.tmp are not used.
    if (probe_path(txn) == Presence::error) return false;
    if (probe_path(txn) == Presence::missing) return probe_path(filename) == Presence::present;
    if (!transaction_chains_safe(filename, tmp, bak, txn)) return false;
    SaveJournal j = {};
    JournalRead jr = read_journal(txn, filename, j);
    if (jr == JournalRead::error) return false;
    // A torn initialization cannot prove ownership. Preserve it byte-for-byte;
    // another bounded transaction slot makes the intact source retryable.
    if (jr == JournalRead::invalid) return true;
    bool reservation = j.magic[13] == '2';
    if (reservation) {
        SaveJournal final = {};
        JournalRead fr = read_journal(txn, filename, final, true);
        if (fr == JournalRead::error) return false;
        if (fr == JournalRead::valid && final.magic[13] == '1' &&
            std::memcmp(&final.before, &j.before, sizeof j.before) == 0) {
            j = final;
        } else {
            // The durable reservation precedes payload CREATE_NEW. No original
            // rename is permitted until a complete ready record is durable.
            Presence pb = probe_path(bak), pt = probe_path(tmp);
            if (pb != Presence::missing || pt == Presence::error) return false;
            Presence po = probe_path(filename);
            if (po == Presence::error) return false;
            if (po == Presence::missing) return true; // a later slot may own the backup
            IdentityMatch original = matches_file(filename, j.before);
            if (original == IdentityMatch::error) return false;
            // A crash before CREATE_NEW returned cannot distinguish an empty
            // owned payload from a collision. Preserve all incomplete payloads
            // (and their reservation) and retry in another slot, never guess.
            if (pt == Presence::present || original != IdentityMatch::match) return true;
            return tracked_unlink(txn) == FR_OK;
        }
    }
    // Finish every probe and identity read before any destructive action. An
    // unreadable original is not evidence that a backup should replace it.
    Presence po = probe_path(filename), pb = probe_path(bak), pt = probe_path(tmp);
    if (po == Presence::error || pb == Presence::error || pt == Presence::error) return false;
    IdentityMatch before = po == Presence::present ? matches_file(filename, j.before) : IdentityMatch::mismatch;
    IdentityMatch after = po == Presence::present ? matches_file(filename, j.after) : IdentityMatch::mismatch;
    IdentityMatch backup = pb == Presence::present ? matches_file(bak, j.before) : IdentityMatch::mismatch;
    IdentityMatch temp = pt == Presence::present ? matches_file(tmp, j.after) : IdentityMatch::mismatch;
    if (before == IdentityMatch::error || after == IdentityMatch::error ||
        backup == IdentityMatch::error || temp == IdentityMatch::error) return false;
    if (pb == Presence::present && backup != IdentityMatch::match) return false;
    if (before != IdentityMatch::match && after != IdentityMatch::match) {
        if (backup != IdentityMatch::match) return pb == Presence::missing && po == Presence::present;
        if (po == Presence::present) {
            if (pt == Presence::present) {
                if (temp != IdentityMatch::match || tracked_unlink(tmp) != FR_OK) return false;
            }
            if (tracked_rename(filename, tmp) != FR_OK) return false;
            pt = Presence::present;
            temp = IdentityMatch::mismatch; // parked material is retained
        }
        if (tracked_rename(bak, filename) != FR_OK) return false;
        pb = Presence::missing;
    }
    if (!transaction_chains_safe(filename, tmp, bak, txn)) return false;
    if (pb == Presence::present && tracked_unlink(bak) != FR_OK) return false;
    if (pt == Presence::present) {
        if (temp != IdentityMatch::match) return true; // unknown material retained
        if (tracked_unlink(tmp) != FR_OK) return false;
    }
    return tracked_unlink(txn) == FR_OK;
}
static bool recover_sd_sidecars(const char* filename)
{
    // Recover existing journals before checking original presence: a later slot
    // may own the backup of a temporarily missing original.
    for (int slot = 1; slot <= 9; ++slot) {
        char txn[VOCAB_FILENAME_MAX];
        if (!make_sidecar_name(filename, ".txn", txn, slot)) break;
        Presence p = probe_path(txn);
        if (p == Presence::error) return false;
        if (p == Presence::present && !recover_candidate(filename, slot)) return false;
    }
    return probe_path(filename) == Presence::present;
}
#endif

bool vocab_file_load(const char* filename, VocabFile& vf,
                     char* fallback_buf, int fallback_len, int& fallback_used)
{
#if defined(__DEVKITARM__) || defined(VOCAB_HOST_FATFS)
    if (s_sd_ready && filename && has_txt_ext(filename)) {
        // Retain the very FIL used to index/fingerprint, with no independent
        // opening reread. A failed-close spare stays quarantined until closed.
        if (!close_candidate_source() || !recover_sd_sidecars(filename)) return false;
        ListPairStorage pairs(*s_candidate_source,s_candidate_source_open);
        PairMetadata stored_pair;
        auto pair_status=pairs.load(filename,stored_pair);
        if(!close_candidate_source())return false;
        if (tracked_open(s_candidate_source, filename, FA_READ | FA_OPEN_EXISTING) != FR_OK)
            return false;
        s_candidate_source_open = true;
        FileIdentity candidate_identity;
        if (f_size(s_candidate_source) >= PENDING_ROW_BASE ||
            !scan_sd_index(*s_candidate_source, s_reindex_scratch, candidate_identity) ||
            !close_loaded_source()) {
            close_candidate_source();
            return false;
        }
        s_reindex_scratch.pair_blocked = pair_status==ListPairStorage::Result::blocked ||
            (pair_status==ListPairStorage::Result::valid && s_reindex_scratch.languages.present() &&
             !s_reindex_scratch.languages.same(stored_pair)) || s_reindex_scratch.rejected_rows;
        if(s_reindex_scratch.pair_blocked) s_reindex_scratch.languages={};
        else if(pair_status == ListPairStorage::Result::valid) s_reindex_scratch.languages=stored_pair;
        FIL* previous = s_loaded_source;
        s_loaded_source = s_candidate_source;
        s_candidate_source = previous;
        s_loaded_source_open = true;
        s_candidate_source_open = false;
        set_loaded_name(filename);
        vf = s_reindex_scratch;
        s_loaded_identity = candidate_identity;
        s_source_verified = true;
        s_loaded_from_sd = true;
        fallback_used = 0;
        begin_loaded_file_generation();
        return true;
    }
#endif
#if defined(__DEVKITARM__) || defined(VOCAB_NO_DEMOS)
    (void)fallback_buf; (void)fallback_len; (void)fallback_used;
    return false;
#else
    char candidate[VOCAB_FILE_BUFFER_LEN];
    int used = 0;
    if (!vocab_file_read_builtin_or_stub(filename, candidate, sizeof candidate, used) ||
        used >= fallback_len || !fallback_buf) return false;
#if defined(__DEVKITARM__) || defined(VOCAB_HOST_FATFS)
    if (!close_loaded_source()) return false;
#endif
    std::memcpy(fallback_buf, candidate, used + 1);
    fallback_used = used;
    vocab_open(vf, fallback_buf, used);
    s_loaded_from_sd = false;
    set_loaded_name(filename);
    ++s_io_stats.index_scans;
    begin_loaded_file_generation();
    return vf.loaded;
#endif
}

#if defined(__DEVKITARM__) || defined(VOCAB_HOST_FATFS)
static bool read_bounded_raw_line(FIL& fp, uint32_t offset,
                                  char* line, int line_cap, int& line_len)
{
    if (!line || line_cap < 2) return false;
    if (tracked_seek(&fp, (FSIZE_t)offset) != FR_OK) return false;

    UINT bytes_read = 0;
    // Read one byte beyond the maximum accepted content length so a delimiter
    // at index VOCAB_RAW_LINE_MAX - 1 is observable. The buffer itself is
    // exactly VOCAB_RAW_LINE_MAX bytes; the delimiter is replaced by NUL.
    UINT request = (UINT)line_cap;
    if (tracked_read(&fp, line, request, &bytes_read) != FR_OK) return false;

    for (UINT i = 0; i < bytes_read; ++i) {
        if (line[i] == '\r' || line[i] == '\n') {
            line_len = (int)i;
            line[line_len] = 0;
            return line_len > 0;
        }
    }

    // Without CR/LF, only a short read at EOF is a valid final row. Filling
    // the entire buffer proves that content exceeds the 191-byte limit and
    // leaves no room for a terminator.
    if (bytes_read >= (UINT)line_cap || !f_eof(&fp)) return false;
    line_len = (int)bytes_read;
    line[line_len] = 0;
    return line_len > 0;
}
#endif

bool vocab_file_raw_row(const VocabFile& vf, const char* fallback, int used,
                        int index, char out[VOCAB_RAW_LINE_MAX])
{
    out[0] = 0;
    if (index < 0 || index >= vf.line_count) return false;
    if (const char* row = pending_row(vf.line_offsets[index])) {
        std::strcpy(out, row);
        return true;
    }
    int length = 0;
#if defined(__DEVKITARM__) || defined(VOCAB_HOST_FATFS)
    if (s_loaded_from_sd) {
        return s_source_verified && ensure_loaded_source_open() &&
            read_bounded_raw_line(*s_loaded_source, vf.line_offsets[index], out,
                                  VOCAB_RAW_LINE_MAX, length) &&
            vocab_validate_raw_row(out, length);
    }
#endif
    uint32_t pos = vf.line_offsets[index];
    while (pos < uint32_t(used) && fallback[pos] != '\r' && fallback[pos] != '\n') {
        if (length == VOCAB_RAW_LINE_MAX - 1) return false;
        out[length++] = fallback[pos++];
    }
    out[length] = 0;
    return vocab_validate_raw_row(out, length);
}

bool vocab_file_show(const VocabFile& vf, const char* fallback_buf, int fallback_used,
                     int line_idx, LineBuf& out)
{
    if (line_idx < 0 || line_idx >= vf.line_count) {
        invalidate_card_cache();
        return false;
    }

#if defined(__DEVKITARM__) || defined(VOCAB_HOST_FATFS)
    if (s_loaded_from_sd && !s_source_verified && !pending_row(vf.line_offsets[line_idx])) return false;
#endif
    uint32_t source_offset = vf.line_offsets[line_idx];
    if (s_card_cache_valid &&
        s_card_cache_loaded_generation == s_loaded_generation &&
        s_card_cache_array_generation == vf.array_generation &&
        s_card_cache_offset == source_offset) {
        out = s_card_cache_line;
        out.field = vf.field[line_idx];
        return true;
    }

    ++s_card_cache_misses;
    ++s_io_stats.full_display_parses;
    LineBuf parsed;
    bool ok = false;
    if (const char* row = pending_row(source_offset)) {
        ok = parse_line_into(row, int(std::strlen(row)), parsed);
    } else
#if defined(__DEVKITARM__) || defined(VOCAB_HOST_FATFS)
    if (s_loaded_from_sd) {
        if (!ensure_loaded_source_open()) {
            invalidate_card_cache();
            return false;
        }
        char line[VOCAB_RAW_LINE_MAX];
        int line_len = 0;
        if (read_bounded_raw_line(*s_loaded_source, source_offset, line,
                                  sizeof(line), line_len)) {
            ok = parse_line_into(line, line_len, parsed);
        }
    } else
#endif
    {
        // Host/fallback source access is counted as one bounded adapter read so
        // cache tests can mechanically assert that unchanged frames add none.
        int start = (int)source_offset;
        int end = start;
        int limit = start + VOCAB_RAW_LINE_MAX - 1;
        if (limit > fallback_used) limit = fallback_used;
        while (end < limit && fallback_buf[end] != '\r' && fallback_buf[end] != '\n') ++end;
        ++s_io_stats.read_calls;
        s_io_stats.bytes_read += (uint32_t)(end - start);
        ok = vocab_show(const_cast<VocabFile&>(vf), fallback_buf, fallback_used,
                        line_idx, parsed);
    }

    if (!ok) {
        invalidate_card_cache();
        return false;
    }

    parsed.field = vf.field[line_idx];
    s_card_cache_line = parsed;
    s_card_cache_loaded_generation = s_loaded_generation;
    s_card_cache_array_generation = vf.array_generation;
    s_card_cache_offset = source_offset;
    s_card_cache_valid = true;
    out = parsed;
    return true;
}

#if defined(__DEVKITARM__) || defined(VOCAB_HOST_FATFS)
// A save-scoped source cache. Sector-aligned starts preserve FatFS bulk reads
// even after backward/shuffled offsets. Never aliases scanner/output storage.
// A mutation is a bounded overlay on the immutable source. The live index is
// not changed until the existing validated replacement transaction commits.
static const char* s_entry_row = nullptr;
constexpr uint32_t ENTRY_ROW_OFFSET = UINT32_MAX;
BN_DATA_EWRAM_BSS static VocabFile s_entry_plan;
static bool s_entry_direct = false;
static EntryMutation s_entry_operation = EntryMutation::add;
static uint32_t s_entry_start = 0;

class FatFsRawRowReader {
public:
    explicit FatFsRawRowReader(FIL& fp) : fp_(fp) {}
    bool read(uint32_t offset, char* row, int& length)
    {
        length = 0;
        if (const char* pending = pending_row(offset)) {
            length = int(std::strlen(pending));
            std::memcpy(row, pending, length + 1);
            return true;
        }
        if (offset == ENTRY_ROW_OFFSET && s_entry_row) {
            length = int(std::strlen(s_entry_row));
            std::memcpy(row, s_entry_row, length + 1);
            return true;
        }
        while (length < VOCAB_RAW_LINE_MAX) {
            if (!valid_ || offset < start_ || offset - start_ >= size_) {
                if (offset >= f_size(&fp_)) {
                    row[length] = 0;
                    return length > 0 && length < VOCAB_RAW_LINE_MAX;
                }
                valid_ = false;
                start_ = offset - offset % IO_SECTOR_BYTES;
                if (tracked_seek(&fp_, start_) != FR_OK) return false;
                size_ = 0;
                if (tracked_read(&fp_, s_source_row_buffer, IO_WINDOW_BYTES, &size_) != FR_OK ||
                    !size_ || (size_ < IO_WINDOW_BYTES && size_ != f_size(&fp_) - start_)) return false;
                valid_ = true;
            }
            char c = s_source_row_buffer[offset++ - start_];
            if (c == '\r' || c == '\n') {
                row[length] = 0;
                return length > 0;
            }
            row[length++] = c;
        }
        return false;
    }
private:
    FIL& fp_;
    uint32_t start_ = 0;
    UINT size_ = 0;
    bool valid_ = false;
};

class FatFsBufferedWriter {
public:
    explicit FatFsBufferedWriter(FIL& fp) : fp_(fp), used_(0), ok_(true) {}

    bool append(const char* data, int len)
    {
        while (len > 0 && ok_) {
            int room = (int)sizeof(s_save_write_buffer) - used_;
            if (room == 0 && !flush()) return false;
            room = (int)sizeof(s_save_write_buffer) - used_;
            int take = len < room ? len : room;
            std::memcpy(s_save_write_buffer + used_, data, (size_t)take);
            used_ += take;
            data += take;
            len -= take;
        }
        return ok_;
    }

    bool flush()
    {
        if (!ok_ || used_ == 0) return ok_;
        UINT written = 0;
        ok_ = tracked_write(&fp_, s_save_write_buffer, (UINT)used_, &written) == FR_OK &&
              written == (UINT)used_;
        if (ok_) extend_identity(identity_, s_save_write_buffer, written);
        used_ = 0;
        return ok_;
    }

    const FileIdentity& identity() const { return identity_; }
    uint32_t position() const { return identity_.size + uint32_t(used_); }

private:
    FileIdentity identity_ = empty_identity();
    FIL& fp_;
    int used_;
    bool ok_;
};

// Clean-list mutations splice the physical TXT so unrelated rows, separators,
// mixed LF/CRLF and an unterminated final row survive byte-for-byte. Dirty box
// movement still uses the established grouped-save serialization below.
static bool write_entry_splice(FIL& in, FatFsBufferedWriter& writer)
{
    const bool add = s_entry_operation == EntryMutation::add;
    const bool remove = s_entry_operation == EntryMutation::remove;
    const char* newline = "\r\n";
    int newline_length = 2;
    if (add && f_size(&in)) {
        char prefix[VOCAB_RAW_LINE_MAX + 2];UINT n = 0;
        if (tracked_read(&in,prefix,sizeof(prefix),&n) != FR_OK) return false;
        for (UINT i=0;i<n;++i) if(prefix[i]=='\n') {
            if (!i || prefix[i-1]!='\r') {newline="\n";newline_length=1;}
            break;
        }
        if (tracked_seek(&in,0) != FR_OK) return false;
    }
    FatFsSequentialSource source(in);
    bool inserted = false, skipping = false, ended = false;
    uint32_t end = s_entry_start, offset = 0;
    char c;
    auto insert = [&]() {
        inserted = true;
        return remove || (writer.append(s_entry_row,int(std::strlen(s_entry_row))) &&
                          (!add || writer.append(newline,newline_length)));
    };
    while(source.next(c,offset)) {
        if (!inserted && offset == s_entry_start) {
            if (!insert()) return false;
            skipping = !add;
        }
        if (skipping) {
            if ((remove && c=='\n') || (!remove && (c=='\r'||c=='\n'))) {
                skipping = false; ended = true;
                end = offset + (remove ? 1 : 0);
                if(remove)continue;
            } else continue;
        }
        if (!writer.append(&c,1)) return false;
    }
    if (source.failed() || source.identity().size != f_size(&in)) return false;
    if (!inserted && !insert()) return false; // add to an empty physical file
    if (!add && !ended) end = f_size(&in);
    const uint32_t delta = writer.position() - uint32_t(f_size(&in));
    s_reindex_scratch = s_entry_plan;
    for(int i=0;i<s_reindex_scratch.line_count;++i) {
        uint32_t old = s_reindex_scratch.line_offsets[i];
        s_reindex_scratch.line_offsets[i] = old == ENTRY_ROW_OFFSET ? s_entry_start :
            old >= end ? old + delta : old;
    }
    s_reindex_scratch.loaded = true;
    vocab_clear_dirty(s_reindex_scratch);
    return true;
}

static bool write_sd_grouped_temp(const VocabFile& vf, const char* tmp_name, bool& created,
                                   FileIdentity& identity)
{
    created = false;
    FIL in;
    if (tracked_open(&in, s_loaded_name, FA_READ | FA_OPEN_EXISTING) != FR_OK) return false;

    FIL out;
    if (tracked_open(&out, tmp_name, FA_WRITE | FA_CREATE_NEW) != FR_OK) {
        tracked_close(&in);
        return false;
    }

    created = true;
    char line[VOCAB_RAW_LINE_MAX];
    FatFsBufferedWriter writer(out);
    FatFsRawRowReader reader(in);
    s_reindex_scratch.reset();
    bool ok = true;
    const bool physical = !s_entry_direct && !vocab_any_dirty(vf) && vf.array_generation==0;
    if(physical) {
        FatFsSequentialSource source(in);char c;uint32_t offset=0;
        while(ok && source.next(c,offset)) {
            if(offset>=vf.legacy_pair_start && offset<vf.legacy_pair_end)continue;
            ok=writer.append(&c,1);
        }
        ok=ok&&!source.failed()&&source.identity().size==f_size(&in);
        s_reindex_scratch=vf;
        s_reindex_scratch.legacy_pair_start=s_reindex_scratch.legacy_pair_end=0;
    }
    if (s_entry_direct) ok = write_entry_splice(in,writer);
    for (int field = 1; !physical && !s_entry_direct && field <= 5 && ok; ++field) {
        for (int i = 0; i < vf.line_count && ok; ++i) {
            if (vf.field[i] != field) continue;
            int line_len = 0;
            const uint32_t offset = writer.position();
            if (!reader.read(vf.line_offsets[i], line, line_len) ||
                !writer.append(line, line_len) || !writer.append("\r\n", 2)) {
                ok = false;
            } else {
                int rank = s_reindex_scratch.line_count++;
                s_reindex_scratch.line_offsets[rank] = offset;
                s_reindex_scratch.field[rank] = uint8_t(field);
                ++s_reindex_scratch.field_counts[field - 1];
            }
        }
        if (field < 5 && ok && !writer.append("\r\n", 2)) ok = false;
    }
    if (!s_entry_direct) s_reindex_scratch.languages=vf.languages;
    s_reindex_scratch.pair_dirty=false;
    if (ok && !writer.flush()) ok = false;
    if (ok && tracked_sync(&out) != FR_OK) ok = false;
    if (tracked_close(&in) != FR_OK) ok = false;
    if (tracked_close(&out) != FR_OK) ok = false;
    if (ok) {
        identity = writer.identity();
        s_reindex_scratch.loaded = true;
    }
    return ok;
}

// One installed scan checks writer identity and intended counts/boxes/offsets.
// This does not independently prove row equivalence to the old source.
static bool validate_replacement(const char* replacement,
                                  const VocabFile& expected, const VocabFile& actual,
                                  const FileIdentity& identity)
{
    if (!actual.loaded || actual.rejected_rows || actual.line_count != expected.line_count ||
        !vocab_field_counts_valid(actual)) return false;
    FIL new_file;
    if (tracked_open(&new_file, replacement, FA_READ) != FR_OK) return false;
    FatFsSequentialSource readback(new_file);
    int expected_box = 1, expected_i = 0;
    uint32_t rejected = 0;
    PairMetadata readback_languages;
    ++s_io_stats.index_scans;
    int loaded = vocab_scan_visit(readback,
        [&](int rank, uint32_t offset, int box, const char*, int) {
            // Match stable grouping of the live (possibly shuffled) source.
            while (expected_box <= 5) {
                while (expected_i < expected.line_count && expected.field[expected_i] != expected_box)
                    ++expected_i;
                if (expected_i < expected.line_count) break;
                ++expected_box;
                expected_i = 0;
            }
            if (rank >= actual.line_count || expected_box > 5 || box != expected_box ||
                actual.field[rank] != box || actual.line_offsets[rank] != offset) return false;
            ++expected_i;
            return true;
        }, rejected, &readback_languages);
    bool ok = loaded == expected.line_count && !rejected && !readback.failed() &&
        !readback_languages.present() && actual.languages.same(expected.languages) &&
        readback.identity().size == f_size(&new_file) && same_identity(readback.identity(), identity);
    if (tracked_close(&new_file) != FR_OK) ok = false;
    return ok;
}

class FatFsReplacementOps {
public:
    FatFsReplacementOps(const char* original, const char* temporary, const char* backup, const char* journal, const VocabFile& expected, const FileIdentity& identity) :
        identity_(identity), original_(original), temporary_(temporary), backup_(backup), journal_(journal), expected_(expected) {}

    bool safe() {
        if (!blocked_ && !transaction_chains_safe(original_, temporary_, backup_, journal_)) blocked_ = true;
        return !blocked_;
    }

    bool rename_original_to_backup() {
        return safe() && tracked_rename(original_, backup_) == FR_OK;
    }
    bool rename_temporary_to_original() {
        return safe() && tracked_rename(temporary_, original_) == FR_OK;
    }
    bool reindex_replacement() {
        return safe() && validate_replacement(original_, expected_, s_reindex_scratch, identity_);
    }
    bool park_failed_replacement() {
        return safe() && tracked_rename(original_, temporary_) == FR_OK;
    }
    bool restore_backup() {
        return safe() && tracked_rename(backup_, original_) == FR_OK;
    }
    bool remove_backup() {
        return safe() && tracked_unlink(backup_) == FR_OK;
    }

private:
    const FileIdentity& identity_;
    const char* original_;
    const char* temporary_;
    const char* backup_;
    const char* journal_;
    bool blocked_ = false;
    const VocabFile& expected_;
};

static bool save_sd_grouped(VocabFile& vf)
{
    char tmp_name[VOCAB_FILENAME_MAX];
    char bak_name[VOCAB_FILENAME_MAX];
    char txn_name[VOCAB_FILENAME_MAX];

    if (!close_candidate_source() || !close_loaded_source()) return false;
    auto fail_and_reopen = []() {
        s_source_verified = matches_file(s_loaded_name, s_loaded_identity) == IdentityMatch::match;
        invalidate_card_cache();
        if (s_source_verified) ensure_loaded_source_open();
        return false;
    };

    // Recovery can change the canonical file. Only that exceptional path (or
    // a quarantined source) needs an identity reread before using old offsets.
    bool recovering = !s_source_verified;
    for (int slot = 1; slot <= 9; ++slot) {
        char txn[VOCAB_FILENAME_MAX];
        if (!make_sidecar_name(s_loaded_name, ".txn", txn, slot)) break;
        Presence p = probe_path(txn);
        if (p == Presence::error) return fail_and_reopen();
        if (p == Presence::present) recovering = true;
    }
    if (!recover_sd_sidecars(s_loaded_name)) return fail_and_reopen();
    if (recovering && matches_file(s_loaded_name, s_loaded_identity) != IdentityMatch::match) {
        s_last_error = "SOURCE CHANGED - RELOAD";
        return fail_and_reopen();
    }
    // Cheap size check, not a claim of independent source-content validation.
    FILINFO info;
    char full[VOCAB_FILENAME_MAX + 10];
    const char* path = storage_path(s_loaded_name, full);
    if (!path || f_stat(path, &info) != FR_OK || info.fsize != s_loaded_identity.size) {
        s_last_error = "SOURCE CHANGED - RELOAD";
        return fail_and_reopen();
    }

    // Never reclaim an unknown/torn initialization. Try another bounded slot;
    // ordinary successful saves still leave no sidecars. Probe errors abort.
    bool available = false;
    for (int slot = 1; slot <= 9; ++slot) {
        if (!make_sidecar_name(s_loaded_name, ".tmp", tmp_name, slot) ||
            !make_sidecar_name(s_loaded_name, ".bak", bak_name, slot) ||
            !make_sidecar_name(s_loaded_name, ".txn", txn_name, slot)) return fail_and_reopen();
        Presence pt = probe_path(tmp_name), pb = probe_path(bak_name), pj = probe_path(txn_name);
        if (pt == Presence::error || pb == Presence::error || pj == Presence::error) return fail_and_reopen();
        if (pt == Presence::missing && pb == Presence::missing && pj == Presence::missing) { available = true; break; }
    }
    if (!available) { s_last_error = "RECOVERY SLOTS FULL"; return fail_and_reopen(); }
    bool journal_created = false, created = false;
    if (!write_journal(s_loaded_name, nullptr, txn_name, journal_created)) {
        if (journal_created && transaction_chains_safe(s_loaded_name, tmp_name, bak_name, txn_name))
            tracked_unlink(txn_name); // only this attempt's CREATE_NEW
        return fail_and_reopen();
    }
    auto abandon_precommit = [&]() {
        if (!transaction_chains_safe(s_loaded_name, tmp_name, bak_name, txn_name)) return fail_and_reopen();
        // Explicit ownership, not helper failure or the existence of original.
        // Keep reservation if deleting our payload fails, allowing restart retry.
        if ((!created || tracked_unlink(tmp_name) == FR_OK)) tracked_unlink(txn_name);
        return fail_and_reopen();
    };
    FileIdentity source_before = s_loaded_identity, replacement_identity;
    if (!write_sd_grouped_temp(vf, tmp_name, created, replacement_identity)) return abandon_precommit();
    bool appended = false;
    // Output identity was accumulated from checked writes. No independent
    // original passes or pre-install source-vs-temp comparison on normal saves.
    if (!write_journal(s_loaded_name, &replacement_identity, txn_name, appended)) return abandon_precommit();
    FatFsReplacementOps replacement(s_loaded_name, tmp_name, bak_name, txn_name, vf, replacement_identity);
    ReplacementOutcome outcome = run_replacement_transaction(replacement);
    // A transient metadata failure may follow a completed rename. Reconcile
    // only on this failure path, before recovery can retire the backup and
    // leave the old live offsets paired with the new canonical bytes.
    bool safe = replacement.safe();
    if (!safe || outcome != ReplacementOutcome::committed) {
        outcome = ReplacementOutcome::recovery_required;
        safe = transaction_chains_safe(s_loaded_name, tmp_name, bak_name, txn_name);
        if (safe) {
            if (probe_path(s_loaded_name) == Presence::missing &&
                matches_file(bak_name, source_before) == IdentityMatch::match) {
                tracked_rename(bak_name, s_loaded_name);
                if (!transaction_chains_safe(s_loaded_name, tmp_name, bak_name, txn_name)) {
                    s_source_verified = false;
                    invalidate_card_cache();
                    return false;
                }
            }
            IdentityMatch original = matches_file(s_loaded_name, source_before);
            if (original == IdentityMatch::match) {
                outcome = ReplacementOutcome::restored;
            } else if (original == IdentityMatch::mismatch &&
                       validate_replacement(s_loaded_name, vf, s_reindex_scratch, replacement_identity)) {
                outcome = ReplacementOutcome::committed;
                // Ownership is from this live attempt; chain probes above
                // exclude aliases. Failed cleanup retains the journal.
                if (probe_path(bak_name) == Presence::present) tracked_unlink(bak_name);
            }
        }
    }
    // Never roll back, unlink or adopt through aliases or ambiguous probes.
    if (!safe) {
        s_source_verified = false;
        invalidate_card_cache();
        return false;
    }
    if (outcome != ReplacementOutcome::committed) {
        // These files were created by this live attempt. Unlike startup recovery,
        // ownership of even a corrupted parked replacement is known here.
        if (matches_file(s_loaded_name, source_before) == IdentityMatch::match && probe_path(bak_name) == Presence::missing) {
            if ((probe_path(tmp_name) == Presence::missing || (probe_path(tmp_name) == Presence::present && tracked_unlink(tmp_name) == FR_OK))) tracked_unlink(txn_name);
        }
        invalidate_card_cache();
        if (outcome == ReplacementOutcome::recovery_required) {
            s_last_error = "RECOVERY REQUIRED";
            s_source_verified = false; // never reopen an unverified promotion with old offsets
            return false;
        }
        return fail_and_reopen();
    }

    if (probe_path(bak_name) == Presence::missing) tracked_unlink(txn_name);

    // Reopening is part of the user-visible result, but the replacement has
    // already committed. Install the validated index first so a failed reopen
    // cannot leave retryable old offsets targeting the regrouped TXT.
    s_loaded_from_sd = true;
    s_loaded_identity = replacement_identity;
    s_source_verified = true;
    bool reopened = finalize_committed_save(vf, s_reindex_scratch,
                                   []() { return ensure_loaded_source_open(); });
    if (!reopened) s_last_error = "SAVED - REOPEN FAILED";
    return reopened;
}
#endif

bool vocab_file_save_grouped(VocabFile& vf, const char* fallback_buf, int fallback_used,
                             char* out_buf, int out_len, int& out_used)
{
    s_save_installed_index = false;
    s_last_error = "SD I/O ERROR";
    if(vf.pair_blocked){s_last_error="LIST PAIR: RELOAD / REPAIR";return false;}
    if(!s_loaded_from_sd && (vf.pair_dirty || vf.legacy_pair_end)) {
        s_last_error="NO SD: PAIR NOT SAVED";return false;
    }
    if (vf.rejected_rows) { s_last_error = "READ ONLY: skipped rows"; return false; }
#if defined(__DEVKITARM__) || defined(VOCAB_HOST_FATFS)
    ListPairStorage pairs(*s_candidate_source,s_candidate_source_open);
    if(s_loaded_from_sd && vf.languages.present() && !pairs.save(s_loaded_name,vf.languages)) {
        s_last_error="LIST PAIR SAVE FAILED";return false;
    }
#endif
    // Dirty bits cover field movement; array_generation covers reorder/shuffle.
    // A clean unchanged file is an immediate success with no I/O or reindex.
    if (!vocab_any_dirty(vf) && vf.array_generation == 0 && !vf.legacy_pair_end) {
        out_used = 0;
#if defined(__DEVKITARM__) || defined(VOCAB_HOST_FATFS)
        if (s_loaded_from_sd) {
            bool ok=s_source_verified && ensure_loaded_source_open();
            if(ok)vf.pair_dirty=false;
            return ok;
        }
#endif
        return true;
    }
#if defined(__DEVKITARM__) || defined(VOCAB_HOST_FATFS)
    if (s_loaded_from_sd) {
        out_used = 0;
        return save_sd_grouped(vf);
    }
#endif
    int written = vocab_export_grouped(vf, fallback_buf, fallback_used, out_buf, out_len);
    if (written < 0) return false;
    out_used = written;
    ++s_io_stats.write_calls;
    s_io_stats.bytes_written += (uint32_t)written;
    vocab_clear_dirty(vf);
    vf.array_generation = 0;
    begin_loaded_file_generation();
    return true;
}

bool vocab_file_next_unused_name(char out[VOCAB_FILENAME_MAX])
{
    out[0] = 0;
#if defined(__DEVKITARM__) || defined(VOCAB_HOST_FATFS)
    if (!s_sd_ready) { s_last_error = "NO SD CARD"; return false; }
    for (int i = 1; i <= 999; ++i) {
        char name[] = "LIST000.TXT";
        name[4] = char('0' + i / 100);
        name[5] = char('0' + (i / 10) % 10);
        name[6] = char('0' + i % 10);
        Presence found = probe_path(name);
        if (found == Presence::error) { s_last_error = "SD I/O ERROR"; return false; }
        if (found == Presence::missing) {
            // An interrupted transaction may own a temporarily absent name.
            // Never create over that recovery namespace.
            bool reserved = false;
            for (int slot = 1; slot <= 9; ++slot) {
                char transaction[VOCAB_FILENAME_MAX];
                make_sidecar_name(name, ".txn", transaction, slot);
                Presence journal = probe_path(transaction);
                if (journal == Presence::error) { s_last_error = "SD I/O ERROR"; return false; }
                if (journal == Presence::present) reserved = true;
            }
            if (!reserved) { std::strcpy(out, name); return true; }
        }
    }
    s_last_error = "NO UNUSED LIST NAME";
#else
    s_last_error = "NO SD CARD";
#endif
    return false;
}

bool vocab_file_create(const char* filename, VocabFile& vf)
{
    s_last_error = "CREATE FAILED";
#if defined(__DEVKITARM__) || defined(VOCAB_HOST_FATFS)
    if (!s_sd_ready) { s_last_error = "NO SD CARD"; return false; }
    if (!filename || !has_txt_ext(filename) || std::strchr(filename, '/') ||
        std::strchr(filename, '\\') || std::strchr(filename, ':') ||
        std::strlen(filename) > 54) { s_last_error = "INVALID FILENAME"; return false; }
    for (int slot = 1; slot <= 9; ++slot) {
        char transaction[VOCAB_FILENAME_MAX];
        make_sidecar_name(filename, ".txn", transaction, slot);
        if (probe_path(transaction) != Presence::missing) {
            s_last_error = "RECOVERY NAME - not created"; return false;
        }
    }
    if (!close_candidate_source()) return false;
#if defined(__DEVKITARM__) || defined(VOCAB_ROOT_DIRECTORY)
    FRESULT directory = f_mkdir("/gbavocab");
    if (directory != FR_OK && directory != FR_EXIST) return false;
#endif
    // Never truncate an existing file. Reuse the quarantinable spare handle
    // so a failed close cannot leave an untracked FatFS lock.
    FRESULT opened = tracked_open(s_candidate_source, filename, FA_WRITE | FA_CREATE_NEW);
    if (opened != FR_OK) {
        if (opened == FR_EXIST) s_last_error = "FILE EXISTS";
        return false;
    }
    s_candidate_source_open = true;
    bool synced = tracked_sync(s_candidate_source) == FR_OK;
    bool closed = close_candidate_source();
    if (!synced || !closed) { s_last_error = "CREATE FAILED - check SD"; return false; }
    char unused[1]; int used = 0;
    if (!vocab_file_load(filename, vf, unused, sizeof unused, used)) {
        s_last_error = "CREATED - load failed"; return false;
    }
    add_name(filename);
    return true;
#else
    (void)filename; (void)vf;
    s_last_error = "NO SD CARD";
    return false;
#endif
}

bool vocab_file_defer(VocabFile& vf, EntryMutation operation, int target,
                      const char* raw_row, int& result_index)
{
    s_save_installed_index = false;
    result_index = target;
    if (!s_loaded_from_sd || !vf.loaded) { s_last_error = "NO SD FILE - not applied"; return false; }
    if (vf.rejected_rows) { s_last_error = "READ ONLY: skipped rows"; return false; }
    if (operation != EntryMutation::add && (target < 0 || target >= vf.line_count)) {
        s_last_error = "NO SELECTED ENTRY"; return false;
    }
    if (operation == EntryMutation::add && vf.line_count >= VOCAB_MAX_LINES) {
        s_last_error = "ENTRY LIMIT: 10000"; return false;
    }
    int slot = -1;
    if (operation != EntryMutation::remove) {
        if (!raw_row || !vocab_validate_raw_row(raw_row, int(std::strlen(raw_row))) ||
            !writer::valid_utf8(raw_row, std::strlen(raw_row))) {
            s_last_error = "INVALID ENTRY / 191 bytes max"; return false;
        }
        bool used[VOCAB_PENDING_ROWS] = {};
        for (int i = 0; i < vf.line_count; ++i) {
            if (i == target && operation == EntryMutation::edit) continue;
            if (pending_row(vf.line_offsets[i])) used[vf.line_offsets[i] - PENDING_ROW_BASE] = true;
        }
        for (int i = 0; i < VOCAB_PENDING_ROWS; ++i) if (!used[i]) {slot = i; break;}
        if (slot < 0) { s_last_error = "RAM FULL - save list first"; return false; }
        std::strcpy(s_pending_rows[slot], raw_row);
    }
    if (operation == EntryMutation::add) {
        for (int i = vf.line_count; i > 0; --i) {
            vf.line_offsets[i] = vf.line_offsets[i - 1];
            vf.field[i] = vf.field[i - 1];
        }
        vf.line_offsets[0] = PENDING_ROW_BASE + slot;
        vf.field[0] = 1;
        ++vf.line_count; ++vf.field_counts[0]; result_index = 0;
    } else if (operation == EntryMutation::edit) {
        vf.line_offsets[target] = PENDING_ROW_BASE + slot;
    } else {
        --vf.field_counts[vf.field[target] - 1]; --vf.line_count;
        for (int i = target; i < vf.line_count; ++i) {
            vf.line_offsets[i] = vf.line_offsets[i + 1];
            vf.field[i] = vf.field[i + 1];
        }
        if (result_index >= vf.line_count) result_index = vf.line_count - 1;
    }
    // A structural change invalidates all row-index dirty bits and the card
    // cache. Conservatively dirty the surviving rows; generation also covers
    // deletion of the final row. No I/O takes place until a confirmed save.
    std::memset(vf.dirty, 0xff, sizeof(vf.dirty));
    if (++vf.array_generation == 0) ++vf.array_generation;
    invalidate_card_cache();
    s_last_error = "";
    return true;
}

bool vocab_file_mutate(VocabFile& vf, EntryMutation operation, int target,
                       const char* raw_row, int& result_index)
{
    s_save_installed_index = false;
    s_last_error = "SD I/O ERROR";
    result_index = target;
#if defined(__DEVKITARM__) || defined(VOCAB_HOST_FATFS)
    if (!s_loaded_from_sd) { s_last_error = "NO SD FILE - not saved"; return false; }
    if (!vf.loaded || vf.rejected_rows) { s_last_error = "READ ONLY: skipped rows"; return false; }
    if (operation != EntryMutation::add && (target < 0 || target >= vf.line_count)) {
        s_last_error = "NO SELECTED ENTRY"; return false;
    }
    if (operation == EntryMutation::add && vf.line_count >= VOCAB_MAX_LINES) { s_last_error = "ENTRY LIMIT: 10000"; return false; }
    if (operation != EntryMutation::remove &&
        (!raw_row || !vocab_validate_raw_row(raw_row, int(std::strlen(raw_row))) ||
         !writer::valid_utf8(raw_row,std::strlen(raw_row)))) {
        s_last_error = "INVALID ENTRY / 191 bytes max"; return false;
    }
    s_entry_plan = vf;
    int committed_index = 0;
    if (operation == EntryMutation::add) {
        for (int i = vf.line_count; i > 0; --i) {
            s_entry_plan.line_offsets[i] = vf.line_offsets[i - 1];
            s_entry_plan.field[i] = vf.field[i - 1];
        }
        s_entry_plan.line_offsets[0] = ENTRY_ROW_OFFSET;
        s_entry_plan.field[0] = 1;
        ++s_entry_plan.line_count;
        ++s_entry_plan.field_counts[0];
    } else {
        if (operation == EntryMutation::edit) s_entry_plan.line_offsets[target] = ENTRY_ROW_OFFSET;
        for (int i = 0; i < vf.line_count; ++i)
            if (vf.field[i] < vf.field[target] || (vf.field[i] == vf.field[target] && i < target))
                ++committed_index;
    }
    if (operation == EntryMutation::remove) {
        --s_entry_plan.field_counts[vf.field[target] - 1];
        --s_entry_plan.line_count;
        for (int i = target; i < s_entry_plan.line_count; ++i) {
            s_entry_plan.line_offsets[i] = vf.line_offsets[i + 1];
            s_entry_plan.field[i] = vf.field[i + 1];
        }
        if (committed_index >= s_entry_plan.line_count) committed_index = s_entry_plan.line_count - 1;
    }
    s_entry_row = raw_row;
    s_entry_direct = !vocab_any_dirty(vf) && vf.array_generation == 0;
    s_entry_operation = operation;
    s_entry_start = operation == EntryMutation::add ? 0 : vf.line_offsets[target];
    struct OverlayScope {
        ~OverlayScope() { s_entry_row = nullptr; s_entry_direct = false; }
    } overlay_scope;
    const bool saved = save_sd_grouped(s_entry_plan);
    if (s_save_installed_index) { vf = s_entry_plan; result_index = committed_index; }
    return saved;
#else
    (void)vf; (void)operation; (void)raw_row;
    s_last_error = "NO SD FILE - not saved";
    return false;
#endif
}

bool vocab_file_export_grouped_stub(const VocabFile& vf, const char* source, int source_len,
                                    char* out, int out_len, int& out_used)
{
    int written = vocab_export_grouped(vf, source, source_len, out, out_len);
    if (written < 0) return false;
    out_used = written;
    return true;
}
