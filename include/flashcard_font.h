#pragma once
#include <cstdint>
// Bank order and fallback match the legacy body sprites. No editor changes.
class FlashcardFont {
public:
    explicit constexpr FlashcardFont(int bank=0): _bank(bank) {}
    int columns(unsigned code, uint16_t out[16]) const;
    int width(const char* text) const;
    int advance(unsigned code) const;
    bool supports(unsigned code) const;
private:
    int _bank;
};
