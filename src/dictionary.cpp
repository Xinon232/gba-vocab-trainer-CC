#include "dictionary.h"
struct DictionaryAnchor {char magic[16];uint32_t offset,size;};
// Volatile prevents compiler constant-folding the PC-patched offsets.
extern const volatile DictionaryAnchor g_dictionary_anchor __attribute__((used,section(".rodata"))) = {
    {'G','B','A','V','O','C','A','B','D','I','C','T','1','.','6','!'},0,0};
DictionaryCatalog dictionary_rom() {
    DictionaryCatalog catalog;
    uint32_t offset=g_dictionary_anchor.offset,size=g_dictionary_anchor.size;
    if(offset>=192 && !(offset&3) && offset<=32*1024*1024 && size<=32*1024*1024-offset)
        catalog.open(reinterpret_cast<const uint8_t*>(0x08000000+offset),size);
    return catalog;
}
