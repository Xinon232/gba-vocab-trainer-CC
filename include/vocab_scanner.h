#pragma once
#include "vocab.h"

// One scanner for memory and FatFS. Limit counts content, not CRLF/LF.
template<class Source>
int vocab_scan(Source& source, VocabFile& vf)
{
    vf.reset();
    int box = 1;
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
        if (overflow || length >= VOCAB_RAW_LINE_MAX) { ++vf.rejected_rows; continue; }
        row[length] = 0;
        if (!length) { if (box < 5) ++box; continue; }
        if (!vocab_validate_raw_row(row, length)) { ++vf.rejected_rows; continue; }
        if (vf.line_count == VOCAB_MAX_LINES) { ++vf.rejected_rows; continue; }
        int i = vf.line_count++;
        vf.line_offsets[i] = start;
        vf.field[i] = uint8_t(box);
        ++vf.field_counts[box - 1];
    }
    vf.loaded = vf.line_count > 0;
    return vf.line_count;
}
