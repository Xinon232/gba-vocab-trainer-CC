// test_integration.cpp — end-to-end TXT-backed learning-state persistence.
#include "vocab.h"

#include <cstdio>
#include <cstring>
#include <fstream>
#include <vector>

int main(int argc, char* argv[])
{
    const char* path = argc > 1 ? argv[1] : "tests/legacy_sample.txt";
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        std::fprintf(stderr, "Cannot open %s\n", path);
        return 1;
    }
    std::vector<char> source((std::istreambuf_iterator<char>(input)),
                             std::istreambuf_iterator<char>());

    VocabFile before;
    int loaded = vocab_open(before, source.data(), (int)source.size());
    if (loaded < 3) {
        std::fprintf(stderr, "FAIL: need at least three source rows\n");
        return 1;
    }

    vocab_advance(before, 0);  // field 2
    vocab_advance(before, 1);
    vocab_advance(before, 1);  // field 3
    if (!vocab_any_dirty(before)) {
        std::fprintf(stderr, "FAIL: learning changes were not dirty\n");
        return 1;
    }

    std::vector<char> grouped(source.size() + 32);
    int written = vocab_export_grouped(before, source.data(), (int)source.size(),
                                       grouped.data(), (int)grouped.size());
    if (written <= 0) {
        std::fprintf(stderr, "FAIL: grouped TXT export failed\n");
        return 1;
    }

    VocabFile after;
    if (vocab_open(after, grouped.data(), written) != loaded) {
        std::fprintf(stderr, "FAIL: grouped TXT changed row count\n");
        return 1;
    }
    for (int field = 0; field < 5; ++field) {
        if (after.field_counts[field] != before.field_counts[field]) {
            std::fprintf(stderr, "FAIL: grouped TXT did not restore field %d\n", field + 1);
            return 1;
        }
    }
    for (int i = 0; i < loaded; ++i) {
        LineBuf restored;
        if (!vocab_show(after, grouped.data(), written, i, restored)) {
            std::fprintf(stderr, "FAIL: restored grouped row %d could not be shown\n", i);
            return 1;
        }
    }

    std::puts("PASS: learning state round-trips entirely through grouped TXT");
    return 0;
}
