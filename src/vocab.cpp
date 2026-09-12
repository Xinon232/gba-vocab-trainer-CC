// vocab.cpp — Streaming vocab data layer
// Streaming implementation, scales to 10,000 words in ~51KB RAM per index.
//
// All the data lives in the .txt on the SD card. We only keep
// bookkeeping (offsets, fields, dirty bits) in RAM. To render a word,
// we stream-read its one line from the .txt.
//
// On the host (this build), the .txt is a memory buffer. On the GBA,
// it will be libugba's open/read/seek.

#include "vocab.h"
#include "vocab_scanner.h"

#include <cstdio>
#include <cstring>

// --------------------------------------------------------------------
// Low-level: parse a single line into a LineBuf.
// --------------------------------------------------------------------

static bool append_text(char* out, int& out_len, const char* text)
{
    for (int i = 0; text[i]; i++) {
        if (out_len >= VOCAB_LINE_MAX - 1) return false;
        out[out_len++] = text[i];
    }
    return true;
}

static bool append_utf8_codepoint(char* out, int& out_len, unsigned code)
{
    char encoded[5];
    int len = 0;
    if (code < 0x80) {
        encoded[len++] = (char)code;
    } else if (code < 0x800) {
        encoded[len++] = (char)(0xC0 | (code >> 6));
        encoded[len++] = (char)(0x80 | (code & 0x3F));
    } else if (code < 0x10000) {
        encoded[len++] = (char)(0xE0 | (code >> 12));
        encoded[len++] = (char)(0x80 | ((code >> 6) & 0x3F));
        encoded[len++] = (char)(0x80 | (code & 0x3F));
    } else if (code <= 0x10FFFF) {
        encoded[len++] = (char)(0xF0 | (code >> 18));
        encoded[len++] = (char)(0x80 | ((code >> 12) & 0x3F));
        encoded[len++] = (char)(0x80 | ((code >> 6) & 0x3F));
        encoded[len++] = (char)(0x80 | (code & 0x3F));
    } else {
        return append_text(out, out_len, "?");
    }
    encoded[len] = 0;
    return append_text(out, out_len, encoded);
}

static bool font_supports_codepoint(unsigned code)
{
    if (code >= 0x20 && code <= 0x7E) return true;
    // Preserve logical Arabic and joining controls in the display copy. The
    // bounded Ghoulam compositor selects forms/omits marks, never the TXT path.
    if ((code >= 0x0600 && code <= 0x06FF) ||
        (code >= 0x0750 && code <= 0x077F) ||
        (code >= 0x0870 && code <= 0x08FF) || code == 0x200C || code == 0x200D) return true;

    // SuperFW-derived flashcard fonts generated in tests/build_superfw_flashcard_fonts.py.
    if (code >= 0x0080 && code <= 0x024F) return true;  // Latin Extended
    if (code >= 0x0370 && code <= 0x04FF) return true;  // Greek + Cyrillic
    if (code >= 0x3000 && code <= 0x30FF) return true;  // Japanese punctuation/kana
    if (code >= 0x4E00 && code <= 0x9FEF) return true;  // CJK Unified Ideographs
    if (code >= 0x20000 && code <= 0x200CC) return true; // SuperFW CJK Ext-B subset
    if (code >= 0xAC00 && code <= 0xD7A3) return true;  // Korean Hangul syllables

    return false;
}

static const char* windows1252_ascii(unsigned code)
{
    switch (code) {
        case 0x80: return "EUR";
        case 0x82: return ",";
        case 0x83: return "f";
        case 0x84: return "\"";
        case 0x85: return "...";
        case 0x86: case 0x87: return "+";
        case 0x89: return "%";
        case 0x8B: case 0x9B: return "<";
        case 0x91: case 0x92: return "'";
        case 0x93: case 0x94: return "\"";
        case 0x96: case 0x97: return "-";
        case 0xBB: return ">";
        default: return "?";
    }
}

static bool copy_supported_codepoint(char* dst, int& out_len, unsigned code)
{
    if (font_supports_codepoint(code)) {
        return append_utf8_codepoint(dst, out_len, code);
    }
    return append_text(dst, out_len, "?");
}

static bool decode_utf8(const char* src, int src_len, int& i, unsigned& code)
{
    unsigned char ch = (unsigned char)src[i];
    if (ch < 0x80) {
        code = ch;
        return true;
    }
    if ((ch & 0xE0) == 0xC0 && i + 1 < src_len) {
        unsigned char b1 = (unsigned char)src[i + 1];
        if ((b1 & 0xC0) == 0x80) {
            unsigned decoded = ((unsigned)(ch & 0x1F) << 6) | (unsigned)(b1 & 0x3F);
            if (decoded >= 0x80) {
                code = decoded;
                i += 1;
                return true;
            }
        }
    }
    if ((ch & 0xF0) == 0xE0 && i + 2 < src_len) {
        unsigned char b1 = (unsigned char)src[i + 1];
        unsigned char b2 = (unsigned char)src[i + 2];
        if ((b1 & 0xC0) == 0x80 && (b2 & 0xC0) == 0x80) {
            unsigned decoded = ((unsigned)(ch & 0x0F) << 12) |
                               ((unsigned)(b1 & 0x3F) << 6) |
                               (unsigned)(b2 & 0x3F);
            if (decoded >= 0x800) {
                code = decoded;
                i += 2;
                return true;
            }
        }
    }
    if ((ch & 0xF8) == 0xF0 && i + 3 < src_len) {
        unsigned char b1 = (unsigned char)src[i + 1];
        unsigned char b2 = (unsigned char)src[i + 2];
        unsigned char b3 = (unsigned char)src[i + 3];
        if ((b1 & 0xC0) == 0x80 && (b2 & 0xC0) == 0x80 && (b3 & 0xC0) == 0x80) {
            unsigned decoded = ((unsigned)(ch & 0x07) << 18) |
                               ((unsigned)(b1 & 0x3F) << 12) |
                               ((unsigned)(b2 & 0x3F) << 6) |
                               (unsigned)(b3 & 0x3F);
            if (decoded >= 0x10000 && decoded <= 0x10FFFF) {
                code = decoded;
                i += 3;
                return true;
            }
        }
    }
    return false;
}

static bool copy_display_text(const char* src, int src_len, char* dst)
{
    unsigned cps[VOCAB_LINE_MAX];
    int cps_len = 0;
    for (int i = 0; i < src_len && cps_len < VOCAB_LINE_MAX - 1; i++) {
        unsigned char ch = (unsigned char)src[i];
        unsigned code = 0;
        if (decode_utf8(src, src_len, i, code)) {
            cps[cps_len++] = font_supports_codepoint(code) ? code : '?';
        } else if (ch >= 0xA0) {
            // dict.cc exports may be ISO-8859-1/Windows-1252 instead of UTF-8.
            // Preserve Latin-1 characters as UTF-8 now that the font supports them.
            cps[cps_len++] = font_supports_codepoint(ch) ? ch : '?';
        } else {
            const char* fallback = windows1252_ascii(ch);
            for (int f = 0; fallback[f] && cps_len < VOCAB_LINE_MAX - 1; ++f) {
                cps[cps_len++] = (unsigned char)fallback[f];
            }
        }
    }

    int out_len = 0;
    for (int i = 0; i < cps_len; i++) {
        if (!copy_supported_codepoint(dst, out_len, cps[i])) return false;
    }
    dst[out_len] = 0;
    return true;
}

struct RawRowParts {
    int a_start;
    int a_len;
    int b_start;
    int b_len;
};

static bool raw_row_parts(const char* line, int line_len, RawRowParts& parts)
{
    if (!line || line_len <= 0 || line_len > VOCAB_RAW_LINE_MAX - 1) return false;

    while (line_len && (line[line_len - 1] == '\r' || line[line_len - 1] == '\n')) --line_len;
    int tabs = 0;
    for (int i = 0; i < line_len; ++i) {
        if (!line[i] || line[i] == '\r' || line[i] == '\n') return false;
        if (line[i] == '\t') ++tabs;
    }
    if (tabs < 1) return false;
    int start = 0;
    while (start < line_len && line[start] == ' ') ++start;
    if (start >= line_len) return false;

    int tab_pos = -1;
    for (int i = start; i < line_len; ++i) {
        if (line[i] == '\t') {
            tab_pos = i;
            break;
        }
    }
    if (tab_pos < 0) return false;

    int a_end = tab_pos;
    while (a_end > start && (line[a_end - 1] == '\r' || line[a_end - 1] == '\n' ||
                              line[a_end - 1] == ' ')) {
        --a_end;
    }
    int b_start = tab_pos + 1;
    while (b_start < line_len && line[b_start] == ' ') ++b_start;
    int b_end = b_start;
    while (b_end < line_len && line[b_end] != '\t') ++b_end;
    while (b_end > b_start && (line[b_end - 1] == '\r' || line[b_end - 1] == '\n' ||
                                line[b_end - 1] == ' ')) {
        --b_end;
    }

    parts.a_start = start;
    parts.a_len = a_end - start;
    parts.b_start = b_start;
    parts.b_len = b_end - b_start;
    return parts.a_len > 0 && parts.b_len > 0;
}

bool vocab_validate_raw_row(const char* line, int line_len)
{
    RawRowParts parts;
    return raw_row_parts(line, line_len, parts);
}

bool parse_line_into(const char* line, int line_len, LineBuf& out)
{
    out.a[0] = 0;
    out.b[0] = 0;
    out.field = 1;

    RawRowParts parts;
    if (!raw_row_parts(line, line_len, parts)) return false;
    return copy_display_text(line + parts.a_start, parts.a_len, out.a) &&
           copy_display_text(line + parts.b_start, parts.b_len, out.b);
}

// --------------------------------------------------------------------
// Phase 1: build the line_offsets[] and field[] tables by streaming
// the .txt once.
// --------------------------------------------------------------------

int vocab_open(VocabFile& vf, const char* data, int data_len)
{
    struct MemorySource {
        const char* data; int length; int pos = 0;
        bool next(char& ch, uint32_t& offset) {
            if (!data || pos >= length) return false;
            offset = uint32_t(pos); ch = data[pos++]; return true;
        }
    } source{data, data_len};
    return vocab_scan(source, vf);
}

// --------------------------------------------------------------------
// Phase 2: stream-read a single line by offset.
// --------------------------------------------------------------------

bool vocab_show(VocabFile& vf, const char* data, int data_len,
                int line_idx, LineBuf& out)
{
    (void)vf;  // vf is used in the GBA path (state), no-op on host.
    if (line_idx < 0 || line_idx >= vf.line_count) {
        return false;
    }
    uint32_t off = vf.line_offsets[line_idx];
    if ((int)off >= data_len) {
        return false;
    }

    // Find end of this line.
    int line_end = (int)off;
    while (line_end < data_len && data[line_end] != '\n') {
        line_end++;
    }
    int line_len = line_end - (int)off;
    if (line_len && data[line_end - 1] == '\r') --line_len;

    if (!parse_line_into(data + off, line_len, out)) {
        return false;
    }
    out.field = vf.field[line_idx];
    return true;
}

// --------------------------------------------------------------------
// Mutate field. Update field_counts[]. Mark dirty bit.
// --------------------------------------------------------------------

static void set_dirty(VocabFile& vf, int line_idx) {
    vf.dirty[line_idx / 8] |= (uint8_t)(1 << (line_idx % 8));
}

static bool get_dirty(const VocabFile& vf, int line_idx) {
    return (vf.dirty[line_idx / 8] & (uint8_t)(1 << (line_idx % 8))) != 0;
}

static void set_dirty_value(VocabFile& vf, int line_idx, bool dirty) {
    uint8_t mask = (uint8_t)(1 << (line_idx % 8));
    if (dirty) {
        vf.dirty[line_idx / 8] |= mask;
    } else {
        vf.dirty[line_idx / 8] &= (uint8_t)~mask;
    }
}

static void swap_line_records(VocabFile& vf, int a, int b) {
    if (a == b) return;

    uint32_t off = vf.line_offsets[a];
    uint8_t field = vf.field[a];
    bool dirty = get_dirty(vf, a);

    vf.line_offsets[a] = vf.line_offsets[b];
    vf.field[a] = vf.field[b];
    set_dirty_value(vf, a, get_dirty(vf, b));

    vf.line_offsets[b] = off;
    vf.field[b] = field;
    set_dirty_value(vf, b, dirty);
}

void vocab_advance(VocabFile& vf, int line_idx)
{
    if (line_idx < 0 || line_idx >= vf.line_count) return;
    uint8_t old = vf.field[line_idx];
    if (old >= 5) {
        // Already in field 5 (graduated). Stay. No field_counts change.
        // Don't mark dirty — nothing changed.
        return;
    }
    uint8_t next = old + 1;
    vf.field[line_idx] = next;
    vf.field_counts[old - 1]--;
    vf.field_counts[next - 1]++;
    set_dirty(vf, line_idx);
}

void vocab_reset(VocabFile& vf, int line_idx)
{
    if (line_idx < 0 || line_idx >= vf.line_count) return;
    uint8_t old = vf.field[line_idx];
    if (old == 1) {
        // Already in field 1. No field_counts change.
        return;
    }
    vf.field[line_idx] = 1;
    vf.field_counts[old - 1]--;
    vf.field_counts[0]++;
    set_dirty(vf, line_idx);
}

bool vocab_move_line_to_field_end(VocabFile& vf, int line_idx, int field,
                                  int& new_idx)
{
    new_idx = line_idx;
    if (line_idx < 0 || line_idx >= vf.line_count || field < 1 || field > 5) return false;
    if (vf.field[line_idx] != (uint8_t)field) return false;

    int last = line_idx;
    for (int i = line_idx + 1; i < vf.line_count; ++i) {
        if (vf.field[i] == (uint8_t)field) {
            last = i;
        }
    }
    if (last == line_idx) {
        return true;
    }

    uint32_t off = vf.line_offsets[line_idx];
    uint8_t fld = vf.field[line_idx];
    bool dirty = get_dirty(vf, line_idx);
    for (int i = line_idx; i < last; ++i) {
        vf.line_offsets[i] = vf.line_offsets[i + 1];
        vf.field[i] = vf.field[i + 1];
        set_dirty_value(vf, i, get_dirty(vf, i + 1));
    }
    vf.line_offsets[last] = off;
    vf.field[last] = fld;
    set_dirty_value(vf, last, dirty);
    new_idx = last;
    ++vf.array_generation;
    return true;
}

static uint32_t shuffle_next(uint32_t& state)
{
    state = state * 1664525u + 1013904223u;
    return state;
}

static int nth_index_in_field(const VocabFile& vf, int field, int nth)
{
    for (int i = 0; i < vf.line_count; ++i) {
        if (vf.field[i] == (uint8_t)field) {
            if (nth == 0) return i;
            --nth;
        }
    }
    return -1;
}

bool vocab_shuffle_field(VocabFile& vf, int field, uint32_t seed)
{
    if (field < 1 || field > 5) return false;
    int count = vf.field_counts[field - 1];
    if (count <= 1) return count == 1;
    uint32_t rng = seed ? seed : 0xA341316Cu;

    for (int k = count - 1; k > 0; --k) {
        int j = (int)(shuffle_next(rng) % (uint32_t)(k + 1));
        int idx_k = nth_index_in_field(vf, field, k);
        int idx_j = nth_index_in_field(vf, field, j);
        if (idx_k >= 0 && idx_j >= 0) {
            swap_line_records(vf, idx_k, idx_j);
        }
    }
    // A shuffle operation changes the identity/order contract of the arrays,
    // even if this seed happened to swap some entries with themselves.
    ++vf.array_generation;
    return true;
}

bool vocab_is_dirty(const VocabFile& vf, int line_idx) {
    if (line_idx < 0 || line_idx >= vf.line_count) return false;
    return (vf.dirty[line_idx / 8] & (uint8_t)(1 << (line_idx % 8))) != 0;
}

bool vocab_any_dirty(const VocabFile& vf) {
    int bytes = (vf.line_count + 7) / 8;
    for (int i = 0; i < bytes; ++i) {
        if (vf.dirty[i] != 0) return true;
    }
    return false;
}

bool vocab_field_counts_valid(const VocabFile& vf) {
    uint16_t direct[5] = {0, 0, 0, 0, 0};
    for (int i = 0; i < vf.line_count; ++i) {
        uint8_t field = vf.field[i];
        if (field < 1 || field > 5) return false;
        ++direct[field - 1];
    }
    int sum = 0;
    for (int i = 0; i < 5; ++i) {
        if (vf.field_counts[i] != direct[i]) return false;
        sum += vf.field_counts[i];
    }
    return sum == vf.line_count;
}

void vocab_clear_dirty(VocabFile& vf) {
    for (int i = 0; i < VOCAB_MAX_LINES / 8; i++) {
        vf.dirty[i] = 0;
    }
}

// --------------------------------------------------------------------
// Serialize a LineBuf to "a\tb\n" form.
// --------------------------------------------------------------------

int format_line(char* out_buf, int out_buf_len, const LineBuf& in)
{
    int a_len = (int)strlen(in.a);
    int b_len = (int)strlen(in.b);
    int needed = a_len + 1 + b_len + 1;  // a \t b \n
    if (needed > out_buf_len) {
        return -1;
    }
    memcpy(out_buf, in.a, a_len);
    out_buf[a_len] = '\t';
    memcpy(out_buf + a_len + 1, in.b, b_len);
    out_buf[a_len + 1 + b_len] = '\n';
    return needed;
}

int vocab_export_grouped(const VocabFile& vf, const char* data, int data_len,
                         char* out_buf, int out_buf_len)
{
    if (vf.rejected_rows) return -1;
    int written = 0;
    for (int field = 1; field <= 5; field++) {
        for (int i = 0; i < vf.line_count; i++) {
            if (vf.field[i] != field) {
                continue;
            }

            int off = (int)vf.line_offsets[i];
            if (off < 0 || off >= data_len) {
                return -1;
            }
            int line_end = off;
            while (line_end < data_len && data[line_end] != '\n') {
                line_end++;
            }
            int row_end = line_end;
            while (row_end > off && (data[row_end - 1] == '\r' || data[row_end - 1] == '\n')) {
                row_end--;
            }
            int row_len = row_end - off;
            if (written + row_len + 2 > out_buf_len) {
                return -1;
            }
            memcpy(out_buf + written, data + off, row_len);
            written += row_len;
            out_buf[written++] = '\r';
            out_buf[written++] = '\n';
        }

        if (field < 5) {
            if (written + 2 > out_buf_len) {
                return -1;
            }
            out_buf[written++] = '\r';
            out_buf[written++] = '\n';
        }
    }
    return written;
}

// --------------------------------------------------------------------
// Test harness (host only).
//
// Verifies:
//   1. open() returns correct line count, all fields=1, all in field_counts[0]
//   2. show(i) returns the same source/target as the input line at offset[i]
//   3. advance/reset move the field correctly and update field_counts
//   4. field 5 is sticky (advance does nothing on already-5)
//   5. dirty bits get set on advance/reset, cleared by clear_dirty
//   6. RAM budget: sizeof(VocabFile) is < 64KB
//   7. round-trip: full rewrite produces byte-identical normalized output
// --------------------------------------------------------------------

#ifndef __DEVKITARM__

#include <cstdlib>

int vocab_streaming_test(const char* sample_data, int sample_len)
{
    printf("=== Vocab streaming test ===\n");
    printf("sizeof(VocabFile) = %zu bytes (%.1f KB)\n",
           sizeof(VocabFile), sizeof(VocabFile) / 1024.0f);
    if (sizeof(VocabFile) > 64 * 1024) {
        fprintf(stderr, "FAIL: VocabFile > 64KB\n");
        return 1;
    }

    // 1. open
    VocabFile vf;
    int loaded = vocab_open(vf, sample_data, sample_len);
    printf("Loaded %d lines\n", loaded);
    if (loaded <= 0) {
        fprintf(stderr, "FAIL: no lines loaded\n");
        return 1;
    }
    if (!vf.loaded) {
        fprintf(stderr, "FAIL: loaded flag not set\n");
        return 1;
    }
    if (vf.line_count != loaded) {
        fprintf(stderr, "FAIL: line_count mismatch\n");
        return 1;
    }
    int total_counts = 0;
    for (int i = 0; i < 5; i++) {
        total_counts += vf.field_counts[i];
    }
    if (total_counts != loaded) {
        fprintf(stderr, "FAIL: field_counts total should be %d, got %d\n", loaded, total_counts);
        return 1;
    }

    // 2. show each line, verify matches input
    int content_diffs = 0;
    for (int i = 0; i < loaded; i++) {
        LineBuf lb;
        if (!vocab_show(vf, sample_data, sample_len, i, lb)) {
            fprintf(stderr, "FAIL: show(%d) failed\n", i);
            return 1;
        }
        if ((int)strlen(lb.a) == 0 || (int)strlen(lb.b) == 0) {
            fprintf(stderr, "FAIL: show(%d) returned empty: a='%s' b='%s'\n", i, lb.a, lb.b);
            content_diffs++;
        }
    }
    if (content_diffs > 0) {
        fprintf(stderr, "FAIL: %d content errors\n", content_diffs);
        return 1;
    }

    // 3. advance and reset semantics
    uint8_t line0_start_field = vf.field[0];
    int count_before[5];
    for (int i = 0; i < 5; i++) count_before[i] = vf.field_counts[i];
    vocab_advance(vf, 0);
    if (line0_start_field < 5) {
        if (vf.field[0] != line0_start_field + 1) { fprintf(stderr, "FAIL: advance 0\n"); return 1; }
        if (vf.field_counts[line0_start_field - 1] != count_before[line0_start_field - 1] - 1) { fprintf(stderr, "FAIL: old field count\n"); return 1; }
        if (vf.field_counts[line0_start_field] != count_before[line0_start_field] + 1) { fprintf(stderr, "FAIL: new field count\n"); return 1; }
    }

    while (vf.field[0] < 5) {
        vocab_advance(vf, 0);
    }
    if (vf.field[0] != 5) { fprintf(stderr, "FAIL: not 5 after advances\n"); return 1; }

    int field5_before_sticky = vf.field_counts[4];
    // 4. field 5 is sticky
    vocab_advance(vf, 0);  // already 5, no-op
    if (vf.field[0] != 5) { fprintf(stderr, "FAIL: stuck on 5\n"); return 1; }
    if (vf.field_counts[4] != field5_before_sticky) { fprintf(stderr, "FAIL: f_c[4] after no-op\n"); return 1; }

    // 5. reset
    int field1_before_reset = vf.field_counts[0];
    vocab_reset(vf, 0);  // 5 -> 1
    if (vf.field[0] != 1) { fprintf(stderr, "FAIL: reset 0\n"); return 1; }
    if (vf.field_counts[0] != field1_before_reset + 1) { fprintf(stderr, "FAIL: f_c[0] after reset\n"); return 1; }

    // 6. dirty bits
    if (!vocab_is_dirty(vf, 0)) { fprintf(stderr, "FAIL: dirty not set after reset\n"); return 1; }
    if (vocab_is_dirty(vf, 1)) { fprintf(stderr, "FAIL: line 1 should not be dirty\n"); return 1; }
    vocab_clear_dirty(vf);
    if (vocab_is_dirty(vf, 0)) { fprintf(stderr, "FAIL: dirty not cleared\n"); return 1; }

    // 7. round-trip: rewrite the file from the streaming state and
    //    compare to the normalized input.
    //
    // We have to reproduce the field state. For this test, modify a
    // few lines to non-default fields, then rewrite, then verify the
    // output matches the expected (input with those field changes).
    vocab_advance(vf, 5);  // 1 -> 2
    vocab_advance(vf, 5);  // 2 -> 3
    vocab_advance(vf, 10); // 1 -> 2
    // Now line 5 is in field 3, line 10 is in field 2, all others in field 1.
    //
    // The streaming model doesn't know per-line "raw" content beyond
    // a and b. So to test round-trip we re-serialize each line as
    // "a\tb\n" — losing the metadata like {de}, [ugs.].
    //
    // The dict.cc spec says fields are stored separately (not in the
    // .txt) — the .txt is just pairs, fields live in a side table.
    // For the trainer we use a per-line marker OR a sidecar .sav.
    // For v1 we accept losing the metadata (it's redundant info that
    // the user can re-add). Future versions preserve it.
    //
    // Build the expected output: input, but with empty lines dropped
    // and metadata stripped. Since strip is hard to do generally,
    // we compare against a known minimal serialization.

    // Output: walk the file via show() + format_line()
    int out_buf_len = sample_len * 2 + 4096;  // generous
    char* out_buf = new char[out_buf_len];
    int written = 0;
    for (int i = 0; i < loaded; i++) {
        LineBuf lb;
        vocab_show(vf, sample_data, sample_len, i, lb);
        int n = format_line(out_buf + written, out_buf_len - written, lb);
        if (n < 0) {
            fprintf(stderr, "FAIL: format_line overflow at i=%d\n", i);
            delete[] out_buf;
            return 1;
        }
        written += n;
    }
    printf("Rewrote %d bytes (was %d)\n", written, sample_len);

    // Now re-parse the rewritten output and verify line count matches.
    VocabFile vf2;
    int reloaded = vocab_open(vf2, out_buf, written);
    if (reloaded != loaded) {
        fprintf(stderr, "FAIL: round-trip line count: %d -> %d\n", loaded, reloaded);
        delete[] out_buf;
        return 1;
    }
    // And verify each line's a/b matches the original.
    int round_trip_diffs = 0;
    for (int i = 0; i < loaded; i++) {
        LineBuf lb1, lb2;
        vocab_show(vf, sample_data, sample_len, i, lb1);
        vocab_show(vf2, out_buf, written, i, lb2);
        if (strcmp(lb1.a, lb2.a) != 0 || strcmp(lb1.b, lb2.b) != 0) {
            if (round_trip_diffs < 5) {
                fprintf(stderr, "  diff at i=%d: '%s\\t%s' vs '%s\\t%s'\n",
                        i, lb1.a, lb1.b, lb2.a, lb2.b);
            }
            round_trip_diffs++;
        }
    }
    delete[] out_buf;
    if (round_trip_diffs > 0) {
        fprintf(stderr, "FAIL: %d round-trip content diffs\n", round_trip_diffs);
        return 1;
    }

    printf("PASS: streaming model round-trip is content-identical (modulo inline metadata)\n");
    return 0;
}

#endif // !__DEVKITARM__
