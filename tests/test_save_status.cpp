#include "save_status.h"

#include <cstdio>
#include <cstring>

int main()
{
    if (std::strcmp(save_status_text(SaveStatus::IDLE), "") != 0 ||
        std::strcmp(save_status_text(SaveStatus::SAVING), "save...") != 0 ||
        std::strcmp(save_status_text(SaveStatus::FAILED), "SAVE FAILED") != 0) {
        std::puts("FAIL: save status is not visibly distinguishable");
        return 1;
    }
    std::puts("PASS: save failure has a visible status");
    return 0;
}
