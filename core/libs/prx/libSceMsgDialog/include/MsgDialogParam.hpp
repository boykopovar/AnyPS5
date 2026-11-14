#ifndef CORE_LIBS_PRX_LIBSCEMSGDIALOG_INCLUDE_MSGDIALOGPARAM_HPP
#define CORE_LIBS_PRX_LIBSCEMSGDIALOG_INCLUDE_MSGDIALOGPARAM_HPP

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>

#ifdef _WIN32
#include <windows.h>
#endif

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

inline bool Readable(const void* p, std::size_t n) {
#ifdef _WIN32
    if (p == nullptr) return false;
    MEMORY_BASIC_INFORMATION mbi;
    if (VirtualQuery(p, &mbi, sizeof(mbi)) == 0 || mbi.State != MEM_COMMIT) return false;
    if ((mbi.Protect & (PAGE_NOACCESS | PAGE_GUARD)) != 0) return false;
    const auto end = reinterpret_cast<std::uintptr_t>(mbi.BaseAddress) + mbi.RegionSize;
    return reinterpret_cast<std::uintptr_t>(p) + n <= end;
#else
    return p != nullptr;
#endif
}

inline ParsedParam ParseParam(const void* param) {
    ParsedParam parsed;
    const std::uint8_t* bytes = static_cast<const std::uint8_t*>(param);
    if (!Readable(bytes, ParamBytes)) return parsed;
    std::int32_t mode = 0;
    std::memcpy(&mode, bytes + 0x38, 4);
    if (mode >= ModeUserMsg && mode <= ModeSystemMsg) parsed.mode = mode;
    if (parsed.mode != ModeUserMsg) return parsed;
    const std::uint8_t* userMsg = nullptr;
    std::memcpy(&userMsg, bytes + 0x40, 8);
    if (!Readable(userMsg, 16)) return parsed;
    std::int32_t buttonType = 0;
    std::memcpy(&buttonType, userMsg, 4);
    const char* msg = nullptr;
    std::memcpy(&msg, userMsg + 8, 8);
    parsed.hasUserMessage = true;
    parsed.buttonType = buttonType;
    if (!Readable(msg, 4)) return parsed;
    parsed.hasText = true;
    for (std::size_t i = 0; i < MaxMessageBytes && Readable(msg + i, 1) && msg[i] != '\0'; ++i) parsed.text.push_back(msg[i]);
    return parsed;
}

inline std::string LogText(const std::string& text) {
    std::string shown;
    for (std::size_t i = 0; i < text.size() && i < 160; ++i) shown.push_back((text[i] >= 0x20 || text[i] == '\n') ? text[i] : '?');
    return shown;
}

}

#endif
