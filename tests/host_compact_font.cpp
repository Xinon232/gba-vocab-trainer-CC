// Host-only actual ROM-pack loader; no fake glyph pixels.
#include <fstream>
#include <vector>
#include <iterator>
#include <cassert>
extern "C" { void *font_base_addr; void *reader_font_base_addr; }
namespace {
std::vector<char> load(const char* path) {
    std::ifstream f(path,std::ios::binary);assert(f);
    return {std::istreambuf_iterator<char>(f),std::istreambuf_iterator<char>()};
}
auto base=load("references/gbawriter/res/fonts.pack");
auto symbols=load("references/gbawriter/res/reader-symbols.pack");
struct Init { Init(){font_base_addr=base.data();reader_font_base_addr=symbols.data();} } init;
}
