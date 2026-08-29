#pragma once

enum class SaveStatus {
    IDLE,
    SAVING,
    FAILED
};

constexpr const char* save_status_text(SaveStatus status)
{
    switch (status) {
        case SaveStatus::SAVING: return "save...";
        case SaveStatus::FAILED: return "SAVE FAILED";
        case SaveStatus::IDLE:
        default: return "";
    }
}
