#pragma once
#include "render.h"
void run_entry_screen(Renderer& renderer, State& state, VocabFile& vf,
                      const char* fallback, int used, const char* front=nullptr,const char* back=nullptr);
