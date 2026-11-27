#ifndef CORE_LIBS_PRX_LIBSCEMSGDIALOG_INCLUDE_MSGDIALOGPARAM_HPP
#define CORE_LIBS_PRX_LIBSCEMSGDIALOG_INCLUDE_MSGDIALOGPARAM_HPP

#include "prx/libc/include/General.hpp"
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>

namespace MsgDialog {

inline constexpr int ModeUserMsg = 1;
inline constexpr int ModeProgressBar = 2;
inline constexpr int ModeSystemMsg = 3;

inline constexpr int ButtonTypeOk = 0;
inline constexpr int ButtonTypeYesNo = 1;
inline constexpr int ButtonTypeNone = 2;
inline constexpr int ButtonTypeOkCancel = 3;
inline constexpr int ButtonTypeYesNoFocusNo = 4;

inline constexpr int ButtonIdOk = 1;
inline constexpr int ButtonIdNo = 2;

inline constexpr std::size_t ParamBytes = 0x60;
inline constexpr std::size_t MaxMessageBytes = 1024;

struct ParsedParam {
    int mode = ModeUserMsg;
    bool hasUserMessage = false;
    bool hasText = false;
    int buttonType = ButtonTypeOk;
    std::string text;
};

#if defined(_WIN32) || defined(__linux__) || defined(__APPLE__)
inline bool ReadGuest(void* destination, const void* source, std::size_t bytes) {
    const int savedErrno = errno;
    bool read = false;
    try {
        read = GuestCopyIn_nid_no_patch(destination, source, bytes);
    } catch (const std::runtime_error&) {
        errno = savedErrno;
        return false;
    }
    errno = savedErrno;
    return read;
}
#else
inline bool ReadGuest(void*, const void*, std::size_t) {
    return false;
}
#endif

inline ParsedParam ParseParam(const void* param) {
    ParsedParam parsed;
    std::uint8_t bytes[ParamBytes]{};
    if (!ReadGuest(bytes, param, sizeof(bytes))) return parsed;
    std::int32_t mode = 0;
    std::memcpy(&mode, bytes + 0x38, 4);
    if (mode >= ModeUserMsg && mode <= ModeSystemMsg) parsed.mode = mode;
    if (parsed.mode != ModeUserMsg) return parsed;
    const std::uint8_t* userMsg = nullptr;
    std::memcpy(&userMsg, bytes + 0x40, 8);
    std::uint8_t userBytes[16]{};
    if (!ReadGuest(userBytes, userMsg, sizeof(userBytes))) return parsed;
    std::int32_t buttonType = 0;
    std::memcpy(&buttonType, userBytes, 4);
    const char* msg = nullptr;
    std::memcpy(&msg, userBytes + 8, 8);
    parsed.hasUserMessage = true;
    parsed.buttonType = buttonType;
    const auto address = reinterpret_cast<std::uintptr_t>(msg);
    for (std::size_t i = 0; i < MaxMessageBytes && i <= UINTPTR_MAX - address; ++i) {
        char character = 0;
        if (!ReadGuest(&character, reinterpret_cast<const void*>(address + i), sizeof(character))) break;
        parsed.hasText = true;
        if (character == '\0') break;
        parsed.text.push_back(character);
    }
    return parsed;
}

inline std::string LogText(const std::string& text) {
    std::string shown;
    for (std::size_t i = 0; i < text.size() && i < 160; ++i) shown.push_back((text[i] >= 0x20 || text[i] == '\n') ? text[i] : '?');
    return shown;
}

}

#endif
