#pragma once
#include "vocab.h"

// Byte spans always start/end at codepoint boundaries. Measurement callback is
// the selected production font's width(), not a byte/character estimate.
struct TextLayout {
    uint16_t start[VOCAB_LINE_MAX];
    uint16_t end[VOCAB_LINE_MAX];
    int count = 0;
    bool valid = true;
};
template<class Measure>
TextLayout layout_text(const char* text, int max_width, Measure measure)
{
    TextLayout result;
    int length = int(std::strlen(text)), pos = 0;
    if (length >= VOCAB_LINE_MAX) { result.valid = false; return result; }
    char candidate[VOCAB_LINE_MAX];
    while (pos < length) {
        while (pos < length && text[pos] == ' ') ++pos;
        if (pos == length) break;
        int end = pos, space = -1;
        while (end < length) {
            int next = end + 1;
            while (next < length && (static_cast<unsigned char>(text[next]) & 0xc0) == 0x80) ++next;
            std::memcpy(candidate, text + pos, next - pos);
            candidate[next - pos] = 0;
            if (measure(candidate) > max_width) break;
            if (text[end] == ' ') space = end;
            end = next;
        }
        if (end == pos) { result.valid = false; return result; }
        if (end < length && space > pos) end = space;
        result.start[result.count] = uint16_t(pos);
        result.end[result.count++] = uint16_t(end);
        pos = end;
    }
    return result;
}
inline int text_pages(const TextLayout& layout) { return layout.count ? (layout.count + 1) / 2 : 1; }
