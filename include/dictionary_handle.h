#pragma once
#include "fatfs/ff.h"
// Exclusive dictionary-screen lease of the inactive, persistent candidate FIL.
// No TXT load/save may run until the screen's catalog has been destroyed.
// Failed closes retain the pool's existing quarantine flag across screen exits.
FIL& vocab_file_dictionary_fil();
bool& vocab_file_dictionary_opened();
