#include "prx/libSceMsgDialog/include/MsgDialogParam.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace {

void Require(bool value, const char* what) {
    if (!value) {
        std::fprintf(stderr, "msg_dialog_param: %s\n", what);
        std::abort();
    }
}

struct UserMessage {
    std::int32_t buttonType;
    std::int32_t pad;
    const char* message;
};

struct Param {
    std::uint8_t bytes[MsgDialog::ParamBytes] = {};

    void setMode(std::int32_t mode) { std::memcpy(bytes + 0x38, &mode, 4); }
    void setUserMessage(const UserMessage* user) { std::memcpy(bytes + 0x40, &user, 8); }
};

std::size_t guestReadCount = 0;
std::size_t failedGuestRead = 0;

void ThrowOnGuestRead(std::uintptr_t, std::size_t) {
    if (++guestReadCount == failedGuestRead) {
        errno = 91;
        throw std::runtime_error("checked guest read failed");
    }
}

void CheckReadFailure(const Param& param) {
    for (std::size_t failedRead = 1; failedRead <= 3; ++failedRead) {
        guestReadCount = 0;
        failedGuestRead = failedRead;
        GuestArena::GuestArenaSetHostReadHook_nid_no_patch(ThrowOnGuestRead);
        errno = 71;
        MsgDialog::ParsedParam parsed;
        bool escaped = false;
        try {
            parsed = MsgDialog::ParseParam(&param);
        } catch (const std::runtime_error&) {
            escaped = true;
        }
        GuestArena::GuestArenaSetHostReadHook_nid_no_patch(nullptr);
        Require(!escaped, "checked guest read exception escaped the parser");
        Require(guestReadCount == failedRead, "checked read failure was not reached");
        Require(errno == 71, "checked read failure changed errno");
        Require(parsed.mode == MsgDialog::ModeUserMsg, "checked read failure changed default mode");
        Require(parsed.hasUserMessage == (failedRead == 3), "checked read failure changed user message presence");
        Require(!parsed.hasText && parsed.text.empty(), "checked read failure retained text");
    }
}

char* GuardedPageEnd() {
#ifdef _WIN32
    SYSTEM_INFO info{};
    GetSystemInfo(&info);
    const std::size_t pageSize = info.dwPageSize;
    auto* memory = static_cast<char*>(VirtualAlloc(nullptr, pageSize * 2, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
    Require(memory != nullptr, "the guarded memory was not allocated");
    DWORD previous = 0;
    Require(VirtualProtect(memory + pageSize, pageSize, PAGE_NOACCESS, &previous) != 0, "the guard page was not protected");
#else
    const long nativePageSize = sysconf(_SC_PAGESIZE);
    Require(nativePageSize > 0, "the page size is unknown");
    const std::size_t pageSize = static_cast<std::size_t>(nativePageSize);
    void* allocation = mmap(nullptr, pageSize * 2, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    Require(allocation != MAP_FAILED, "the guarded memory was not allocated");
    auto* memory = static_cast<char*>(allocation);
    Require(mprotect(memory + pageSize, pageSize, PROT_NONE) == 0, "the guard page was not protected");
#endif
    return memory + pageSize;
}

}

int main() {
    Require(!MsgDialog::ParseParam(nullptr).hasUserMessage, "a null parameter has a user message");
    Require(MsgDialog::ParseParam(nullptr).mode == MsgDialog::ModeUserMsg, "a null parameter is not a user message");
    errno = 71;
    Require(!MsgDialog::ParseParam(reinterpret_cast<const void*>(0x10)).hasUserMessage, "an unreadable parameter was read");
    Require(errno == 71, "reading an unreadable parameter changed errno");

    Param empty;
    auto parsed = MsgDialog::ParseParam(&empty);
    Require(parsed.mode == MsgDialog::ModeUserMsg, "mode 0 is not treated as a user message");
    Require(!parsed.hasUserMessage && !parsed.hasText, "a null user message pointer was read");

    Param outOfRange;
    outOfRange.setMode(9);
    Require(MsgDialog::ParseParam(&outOfRange).mode == MsgDialog::ModeUserMsg, "mode 9 is not treated as a user message");

    for (const int mode : {MsgDialog::ModeProgressBar, MsgDialog::ModeSystemMsg}) {
        UserMessage user{MsgDialog::ButtonTypeYesNo, 0, "ignored"};
        Param other;
        other.setMode(mode);
        other.setUserMessage(&user);
        parsed = MsgDialog::ParseParam(&other);
        Require(parsed.mode == mode, "the mode was not kept");
        Require(!parsed.hasUserMessage && !parsed.hasText, "a user message was read for another mode");
    }

    UserMessage user{MsgDialog::ButtonTypeYesNoFocusNo, 0, "Overwrite the save?\nThis cannot be undone."};
    Param param;
    param.setMode(MsgDialog::ModeUserMsg);
    param.setUserMessage(&user);
    parsed = MsgDialog::ParseParam(&param);
    Require(parsed.mode == MsgDialog::ModeUserMsg, "user message mode");
    Require(parsed.hasUserMessage && parsed.hasText, "user message not read");
    Require(parsed.buttonType == MsgDialog::ButtonTypeYesNoFocusNo, "button type");
    Require(parsed.text == "Overwrite the save?\nThis cannot be undone.", "message text");
    CheckReadFailure(param);
    const auto* unreadableUser = reinterpret_cast<const UserMessage*>(0x10);
    param.setUserMessage(unreadableUser);
    errno = 71;
    parsed = MsgDialog::ParseParam(&param);
    Require(!parsed.hasUserMessage && !parsed.hasText, "an unreadable user message was read");
    Require(errno == 71, "reading an unreadable user message changed errno");
    UserMessage unreadableText{MsgDialog::ButtonTypeYesNo, 0, reinterpret_cast<const char*>(0x10)};
    param.setUserMessage(&unreadableText);
    errno = 71;
    parsed = MsgDialog::ParseParam(&param);
    Require(parsed.hasUserMessage && !parsed.hasText, "an unreadable text pointer was read");
    Require(errno == 71, "reading unreadable text changed errno");
    char* guard = GuardedPageEnd();
    std::memcpy(guard - 4, "ABCD", 4);
    UserMessage boundaryText{MsgDialog::ButtonTypeOk, 0, guard - 4};
    param.setUserMessage(&boundaryText);
    parsed = MsgDialog::ParseParam(&param);
    Require(parsed.hasText && parsed.text == "ABCD", "text crossing into a guard page was not bounded");
    guard[-1] = '\0';
    UserMessage oneByteText{MsgDialog::ButtonTypeOk, 0, guard - 2};
    param.setUserMessage(&oneByteText);
    parsed = MsgDialog::ParseParam(&param);
    Require(parsed.hasText && parsed.text == "C", "short NUL-terminated text at the readable page end was rejected");
    UserMessage emptyBoundaryText{MsgDialog::ButtonTypeOk, 0, guard - 1};
    param.setUserMessage(&emptyBoundaryText);
    parsed = MsgDialog::ParseParam(&param);
    Require(parsed.hasText && parsed.text.empty(), "empty text at the readable page end was rejected");

    UserMessage noText{MsgDialog::ButtonTypeOkCancel, 0, nullptr};
    param.setUserMessage(&noText);
    parsed = MsgDialog::ParseParam(&param);
    Require(parsed.hasUserMessage && !parsed.hasText, "a null message pointer was read");
    Require(parsed.buttonType == MsgDialog::ButtonTypeOkCancel, "button type without a message");

    static char longText[MsgDialog::MaxMessageBytes + 64];
    std::memset(longText, 'a', sizeof(longText) - 1);
    UserMessage longMessage{MsgDialog::ButtonTypeOk, 0, longText};
    param.setUserMessage(&longMessage);
    Require(MsgDialog::ParseParam(&param).text.size() == MsgDialog::MaxMessageBytes, "a long message was not capped");

    const std::string logged = MsgDialog::LogText(std::string("a\tb\nc") + std::string(200, 'x'));
    Require(logged.size() == 160, "the logged text is not capped at 160");
    Require(logged.compare(0, 5, "a?b\nc") == 0, "control characters are not masked in the log");
}
