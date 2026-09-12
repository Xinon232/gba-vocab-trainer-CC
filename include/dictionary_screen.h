#pragma once
#include "vocab.h"
class Renderer;
struct DictionaryResult {
    PairMetadata languages;
    char front[VOCAB_RAW_LINE_MAX]={},back[VOCAB_RAW_LINE_MAX]={};
};
bool run_dictionary_screen(Renderer&,VocabFile* target,DictionaryResult&);
// Ask missing FRONT/BACK orientation, or verify/map existing footer. No writes.
bool dictionary_accept_pair(Renderer&,VocabFile&,DictionaryResult&);
