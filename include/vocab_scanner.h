#pragma once
#include "vocab.h"

// One scanner grammar for indexing and persisted-output validation. The visitor
// sees each accepted raw row while it is still in bounded scanner scratch.
// Returning false aborts with -1; I/O failure remains the Source's contract.
template<class Source, class Visitor>
int vocab_scan_visit(Source& source, Visitor&& visit, uint32_t& rejected, PairMetadata* languages = nullptr)
{
    rejected = 0;
    int count = 0, box = 1;
    bool footer = false;
    char row[VOCAB_RAW_LINE_MAX + 1];
    while (true) {
        int length = 0;
        bool any = false, overflow = false;
        uint32_t start = 0, offset = 0;
        char ch;
        while (source.next(ch, offset)) {
            if (!any) { start = offset; any = true; }
            if (ch == '\n') break;
            if (length < VOCAB_RAW_LINE_MAX) row[length++] = ch;
            else overflow = true;
        }
        if (!any) break;
        if (length && row[length - 1] == '\r') --length;
        if (overflow || length >= VOCAB_RAW_LINE_MAX) { ++rejected; continue; }
        row[length] = 0;
        if (footer) { if(length) ++rejected; continue; }
        if (PairMetadata::starts(row,"# gbavocab:")) {
            PairMetadata parsed;
            if (!parsed.parse(row)) { ++rejected; continue; }
            if(languages) *languages = parsed;
            footer = true; continue;
        }
        if (!length) { if (box < 5) ++box; continue; }
        if (!vocab_validate_raw_row(row, length)) { ++rejected; continue; }
        if (count == VOCAB_MAX_LINES) { ++rejected; continue; }
        if (!visit(count, start, box, row, length)) return -1;
        ++count;
    }
    return count;
}

template<class Source>
int vocab_scan(Source& source, VocabFile& vf)
{
    vf.reset();
    vf.line_count = vocab_scan_visit(source,
        [&vf](int i, uint32_t start, int box, const char*, int) {
            vf.line_offsets[i] = start;
            vf.field[i] = uint8_t(box);
            ++vf.field_counts[box - 1];
            return true;
        }, vf.rejected_rows, &vf.languages);
    vf.loaded = vf.line_count > 0;
    return vf.line_count;
}
