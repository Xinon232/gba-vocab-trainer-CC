#include "flashcard_font.h"
#include "flashcard_overrides.h"
#ifdef __arm__
#include "bn_assert.h"
#endif
extern "C" unsigned entry_font_columns(uint32_t, uint16_t[16]);
extern "C" unsigned entry_font_advance(uint32_t);
namespace {
const FlashcardOverride* override_for(int bank,unsigned cp) {
    if(cp>=33 && cp<=126)return &flashcard_overrides[cp-33];
    if(bank==1)for(unsigned i=94;i<sizeof(flashcard_overrides)/sizeof(*flashcard_overrides);++i)
        if(flashcard_overrides[i].code==cp)return &flashcard_overrides[i];
    return nullptr;
}
unsigned mapped(const FlashcardFont& font,int bank,unsigned cp) {
    static constexpr unsigned first[]={0x80,0x370,0x3000,0x4e00,0xac00};
    if(font.supports(cp))return cp;
#ifdef __arm__
    // Preserve the legacy Butano map's debug diagnostic, and its index-zero
    // fallback when assertions are disabled. Parser/bank policy is unchanged.
    BN_ERROR("UTF-8 character not found: ",cp);
#endif
    return first[bank>=0 && bank<5?bank:0];
}
}
int FlashcardFont::advance(unsigned cp) const {
    if(cp==32)return 8;
    cp=mapped(*this,_bank,cp);
    if(const auto* o=override_for(_bank,cp))return o->advance;
    unsigned width=entry_font_advance(cp),cell=_bank<2?8:16;
    return int(width>cell?cell:width);
}
bool FlashcardFont::supports(unsigned cp) const {
    if(cp>=32 && cp<=126) return true;
    switch(_bank) {
    case 0:return cp>=0x80 && cp<=0x24f;
    case 1:return cp>=0x370 && cp<=0x4ff;
    case 2:return (cp>=0x3000 && cp<=0x3009)||(cp>=0x3040 && cp<=0x30ff);
    case 3:return (cp>=0x4e00 && cp<=0x9fef)||(cp>=0x20000 && cp<=0x200cc);
    case 4:return cp>=0xac00 && cp<=0xd7a3;
    default:return false;
    }
}
int FlashcardFont::columns(unsigned cp,uint16_t out[16]) const {
    for(int x=0;x<16;++x)out[x]=0;
    if(cp==32)return 8;
    // Legacy release map miss returned index zero (first non-ASCII glyph).
    cp=mapped(*this,_bank,cp);
    if(const auto* o=override_for(_bank,cp)){
        for(int x=0;x<16;++x)out[x]=o->columns[x];
        return o->advance;
    }
    unsigned width=entry_font_columns(cp,out);
    unsigned cell=_bank<2?8:16;
    return int(width>cell?cell:width);
}
int FlashcardFont::width(const char* s) const {
    int total=0;
    while(*s){
        unsigned cp=static_cast<unsigned char>(*s++);
        unsigned n=cp<128?0:cp<224?1:cp<240?2:3;
        if(n)cp&=(1u<<(6-n))-1;
        while(n-- && *s)cp=(cp<<6)|(static_cast<unsigned char>(*s++)&63);
        total+=advance(cp);
    }
    return total;
}
