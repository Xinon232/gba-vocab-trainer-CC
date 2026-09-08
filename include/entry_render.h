#pragma once
#include "entry_editor.h"
using EntryUiLine = void (*)(void*, int, int, const char*);
int entry_glyph_width(const char* text);
void render_entry(EntryEditor& editor, uint8_t* pixels, EntryUiLine ui, void* context);
