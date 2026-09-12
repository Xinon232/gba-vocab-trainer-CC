// Content-neutral cross-reader verifier: never prints dictionary text.
#include "dictionary.h"
#include <fstream>
#include <vector>
#include <string>
#include <cassert>
#include <cstdio>
#include <cstdlib>
struct Memory : DictionarySource {
    std::vector<unsigned char> data;
    bool read(int, uint32_t at, void* out, unsigned n) override {
        if(at > data.size() || n > data.size()-at) return false;
        std::memcpy(out, data.data()+at, n); return true;
    }
    uint32_t size(int) override { return data.size(); }
};
std::string key(const char* text) {
    std::string result(text);
    for(char& c : result) c = Dictionary::fold(static_cast<unsigned char>(c));
    return result;
}
int main(int argc, char** argv) {
    assert(argc == 3);
    Memory source;
    std::ifstream input(argv[1], std::ios::binary);
    source.data.assign(std::istreambuf_iterator<char>(input), {});
    Dictionary dictionary(&source, 0);
    assert(dictionary.valid());
    assert(dictionary.count() == std::strtoul(argv[2], nullptr, 10));
    for(int side=0; side<2; ++side) {
        std::vector<bool> seen(dictionary.count());
        std::string previous;
        unsigned previous_row=0;
        for(unsigned pos=0; pos<dictionary.count(); ++pos) {
            auto row = dictionary.row_at(side, pos);
            assert(row < dictionary.count() && !seen[row]); seen[row]=true;
            char a[192], b[192]; assert(dictionary.read(row, a, b));
            auto current = key(side ? b : a);
            assert(pos == 0 || previous < current || (previous == current && previous_row < row));
            previous=current; previous_row=row;
            if(pos % 997 == 0 || pos+1 == dictionary.count()) {
                auto range=dictionary.prefix(side, side ? b : a);
                assert(!dictionary.failed() && range.begin <= pos && pos < range.end);
            }
        }
        printf("PASS production C++ reader side=%d rows=%u sorted permutation and prefix samples\n", side, dictionary.count());
    }
}
