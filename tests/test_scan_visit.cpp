#include "vocab_scanner.h"
#include <cassert>
#include <cstdio>
#include <string>
struct Source {
    const std::string& text;
    uint32_t pos = 0;
    bool next(char& c, uint32_t& offset) {
        if (pos == text.size()) return false;
        offset = pos; c = text[pos++]; return true;
    }
};
int main() {
    // Empty first/middle boxes, CRLF/LF, max row, unterminated EOF.
    std::string text = "\r\na\tb\n\n\n" + std::string(189, 'x') + "\ty";
    Source source{text}; uint32_t rejected = 0; int visits = 0;
    auto visit = [&](int rank, uint32_t offset, int box, const char* row, int len) {
        assert(rank == visits++);
        assert(offset == (rank ? text.find('x') : text.find('a')));
        assert(box == (rank ? 4 : 2));
        assert(std::string(row, len) == (rank ? std::string(189, 'x') + "\ty" : "a\tb"));
        return true;
    };
    assert(vocab_scan_visit(source, visit, rejected) == 2);
    assert(visits == 2 && rejected == 0);
    Source refused{text};
    auto reject = [](int, uint32_t, int, const char*, int) { return false; };
    assert(vocab_scan_visit(refused, reject, rejected) == -1);
    // Preserve scanner grammar: bare CR inside a physical LF-delimited row
    // is not silently interpreted as another line.
    std::string invalid = "a\tb\rc\td\n" + std::string(192, 'x') + "\nvalid\trow";
    Source bad{invalid}; visits = 0;
    auto accepted = [&](int, uint32_t, int, const char* row, int len) {
        ++visits; assert(std::string(row,len) == "valid\trow"); return true;
    };
    assert(vocab_scan_visit(bad, accepted, rejected) == 1);
    assert(rejected == 2 && visits == 1);
    puts("PASS scanner visitor raw rows, offsets, empty boxes, rejection and abort");
}
