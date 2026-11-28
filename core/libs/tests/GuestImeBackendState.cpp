#include "prx/libSceImeBackend/include/Api.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libc/include/GuestWriteWatch.hpp"
#include <array>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace {

void Require(bool value) { if (!value) std::abort(); }

constexpr int kNotInitialized = static_cast<int>(0x80bc1002);
constexpr int kAlreadyInitialized = static_cast<int>(0x80bc1001);
constexpr int kInvalidMode = static_cast<int>(0x80bc1010);
constexpr int kInvalidLength = static_cast<int>(0x80bc1011);
constexpr int kInvalidType = static_cast<int>(0x80bc1012);
constexpr int kInvalidReservedArgument = static_cast<int>(0x80bc1014);
constexpr int kInvalidStringSelector = static_cast<int>(0x80bc1030);
constexpr int kNullArgument = static_cast<int>(0x80bc1031);
constexpr int kInvalidReservedField = static_cast<int>(0x80bc1032);
constexpr int kInitialStringTooLong = static_cast<int>(0x80bc1034);
constexpr int kInvalidField18 = static_cast<int>(0x80bc1035);

bool TextEquals(const char16_t* text, const char16_t* expected) {
    return std::char_traits<char16_t>::compare(text, expected, std::char_traits<char16_t>::length(expected) + 1) == 0;
}

SceImeBackendParam MakeParam() {
    SceImeBackendParam param;
    sceImeBackendParamInit(&param);
    param.mode = 0;
    param.maxLength = 8;
    return param;
}

void CheckValidation() {
    SceImeBackendParam param = MakeParam();
    Require(sceImeBackendOpen(nullptr, nullptr) == kNullArgument);
    errno = 71;
    Require(sceImeBackendOpen(reinterpret_cast<const SceImeBackendParam*>(0x10), nullptr) == kNullArgument);
    Require(errno == 71);
    Require(sceImeBackendOpen(&param, &param) == kInvalidReservedArgument);

    param.mode = -1;
    Require(sceImeBackendOpen(&param, nullptr) == kInvalidMode);
    param = MakeParam();
    param.maxLength = 0;
    Require(sceImeBackendOpen(&param, nullptr) == kInvalidLength);
    param = MakeParam();
    param.type = 4;
    Require(sceImeBackendOpen(&param, nullptr) == kInvalidType);
    param = MakeParam();
    param.field18 = 2;
    Require(sceImeBackendOpen(&param, nullptr) == kInvalidField18);
    param = MakeParam();
    param.reserved[0] = 1;
    Require(sceImeBackendOpen(&param, nullptr) == kInvalidReservedField);

    const std::u16string longText(0x400, u'A');
    param = MakeParam();
    param.type = 2;
    param.initialString = longText.c_str();
    Require(sceImeBackendOpen(&param, nullptr) == kInitialStringTooLong);

    param = MakeParam();
    param.type = 2;
    param.initialString = reinterpret_cast<const char16_t*>(0x10);
    errno = 71;
    Require(sceImeBackendOpen(&param, nullptr) == kNullArgument);
    Require(errno == 71);
#ifdef _WIN32
    SYSTEM_INFO sysInfo{};
    GetSystemInfo(&sysInfo);
    const std::size_t pageSize = sysInfo.dwPageSize;
    auto* guardPages = static_cast<unsigned char*>(VirtualAlloc(nullptr, pageSize * 2, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
    Require(guardPages != nullptr);
    DWORD prevProt = 0;
    Require(VirtualProtect(guardPages + pageSize, pageSize, PAGE_NOACCESS, &prevProt) != 0);
#else
    const std::size_t pageSize = static_cast<std::size_t>(sysconf(_SC_PAGESIZE));
    auto* guardPages = static_cast<unsigned char*>(mmap(nullptr, pageSize * 2, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
    Require(guardPages != MAP_FAILED);
    Require(mprotect(guardPages + pageSize, pageSize, PROT_NONE) == 0);
#endif
    auto* unterminated = reinterpret_cast<char16_t*>(guardPages + pageSize - 4);
    unterminated[0] = u'A';
    unterminated[1] = u'B';
    param = MakeParam();
    param.type = 2;
    param.initialString = unterminated;
    errno = 71;
    Require(sceImeBackendOpen(&param, nullptr) == kNullArgument);
    Require(errno == 71);
#ifdef _WIN32
    Require(VirtualFree(guardPages, 0, MEM_RELEASE) != 0);
#else
    Require(munmap(guardPages, pageSize * 2) == 0);
#endif
}

void CheckLifecycle() {
    SceImeBackendParam param = MakeParam();
    const char16_t initial[] = u"Hi";
    param.type = 2;
    param.initialString = initial;
    Require(sceImeBackendOpen(&param, nullptr) == 0);
    Require(sceImeBackendOpen(&param, nullptr) == kAlreadyInitialized);
    Require(sceImeBackendGetStatus() == 1);
    Require(sceImeBackendGetCaretIndex() == 2);

    char16_t text[0x20] = {};
    Require(sceImeBackendGetConfirmString(text) == 0 && TextEquals(text, u"Hi"));
    Require(sceImeBackendGetConvertString(text) == 0 && TextEquals(text, u"Hi"));
    Require(sceImeBackendGetString(0, text, 0x20) == 0 && TextEquals(text, u"Hi"));
    Require(sceImeBackendGetString(1, text, 0x20) == 0 && TextEquals(text, u"Hi"));
    Require(sceImeBackendGetString(2, text, 0x20) == kInvalidStringSelector);
    Require(sceImeBackendGetString(0, nullptr, 0x20) == kNullArgument);

    int mode = -1;
    Require(sceImeBackendGetConvertMode(&mode) == 0 && mode == 0);
    Require(sceImeBackendSetConvertMode(1) == 0);
    Require(sceImeBackendGetConvertMode(&mode) == 0 && mode == 1);

    const std::uint16_t noCandidates = 1;
    Require(sceImeBackendGetCandidateListSize() == 0);
    std::uint16_t selected = noCandidates;
    Require(sceImeBackendGetCandidateList(&selected, text) == 0 && selected == 0 && text[0] == 0);
    Require(sceImeBackendGetCandidateSelect(&selected) == 0 && selected == 0);
    errno = 71;
    Require(sceImeBackendGetString(0, reinterpret_cast<char16_t*>(0x10), 0x20) == kNullArgument);
    Require(errno == 71);
    Require(sceImeBackendGetConvertString(reinterpret_cast<char16_t*>(0x10)) == kNullArgument);
    Require(errno == 71);
    Require(sceImeBackendGetConfirmString(reinterpret_cast<char16_t*>(0x10)) == kNullArgument);
    Require(errno == 71);
    Require(sceImeBackendGetConvertMode(reinterpret_cast<int*>(0x10)) == kNullArgument);
    Require(errno == 71);
    Require(sceImeBackendGetCandidateSelect(reinterpret_cast<std::uint16_t*>(0x10)) == kNullArgument);
    Require(errno == 71);
    Require(sceImeBackendGetCandidateList(reinterpret_cast<std::uint16_t*>(0x10), text) == kNullArgument);
    Require(errno == 71);
    Require(sceImeBackendGetCandidateList(&selected, reinterpret_cast<char16_t*>(0x10)) == kNullArgument);
    Require(errno == 71);
    Require(sceImeBackendEnterCharacter('x', reinterpret_cast<char16_t*>(0x10)) == kNullArgument);
    Require(errno == 71);
    Require(sceImeBackendGetCaretIndex() == 2);
    Require(sceImeBackendGetConvertString(text) == 0 && TextEquals(text, u"Hi"));

    if (GuestArena::GuestArenaAvailable_nid_postfix()) {
        constexpr std::size_t kArenaBytes = 0x10000;
        auto* const mapped = static_cast<unsigned char*>(GuestArena::GuestArenaAllocate_nid_postfix(kArenaBytes, kArenaBytes));
        Require(mapped != nullptr);
#ifdef _WIN32
        const HANDLE section = CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, PAGE_EXECUTE_READWRITE, 0, kArenaBytes, nullptr);
        Require(section != nullptr);
        GuestArena::GuestArenaMap_nid_postfix(mapped, kArenaBytes, section, 0, PAGE_READWRITE);
        CloseHandle(section);
        const bool watchedWrites = GuestArena::GuestArenaWriteWatched_nid_postfix();
#else
        GuestArena::GuestArenaCommit_nid_postfix(mapped, kArenaBytes, 0x04, kArenaBytes);
        const bool watchedWrites = GuestWriteWatch::GuestWriteWatchAvailable_nid_postfix();
        if (watchedWrites) {
            GuestWriteWatch::GuestWriteWatchRegister_nid_postfix(mapped, kArenaBytes);
        }
#endif
        const auto collectDirty = [&]() -> bool {
            if (!watchedWrites) {
                return true;
            }
#ifdef _WIN32
            std::array<void*, 16> pages{};
            std::size_t count = pages.size();
            Require(GuestArena::GuestArenaCollectWrites_nid_postfix(reinterpret_cast<std::uintptr_t>(mapped), kArenaBytes, pages.data(), &count, true));
            return count != 0;
#else
            bool dirty = false;
            Require(GuestWriteWatch::GuestWriteWatchCollect_nid_postfix(reinterpret_cast<std::uintptr_t>(mapped), kArenaBytes,
                [](void* context, std::uintptr_t, std::uintptr_t) { *static_cast<bool*>(context) = true; }, &dirty));
            return dirty;
#endif
        };
        const auto armWatch = [&](void* target) {
            if (!watchedWrites) {
                return;
            }
            static_cast<void>(collectDirty());
            Require(!collectDirty());
#ifdef _WIN32
            MEMORY_BASIC_INFORMATION mbi{};
            Require(VirtualQuery(target, &mbi, sizeof(mbi)) == sizeof(mbi) && mbi.Protect == PAGE_READONLY);
#else
            static_cast<void>(target);
#endif
        };
        auto* watchedText = reinterpret_cast<char16_t*>(mapped);
        auto* watchedSel = reinterpret_cast<std::uint16_t*>(mapped + 256);
        auto* watchedMode = reinterpret_cast<int*>(mapped + 512);
        armWatch(watchedText);
        errno = 71;
        Require(sceImeBackendGetString(0, watchedText, 0x20) == 0 && TextEquals(watchedText, u"Hi"));
        Require(errno == 71);
        Require(collectDirty());
        *watchedText = u'~';
        armWatch(watchedText);
        errno = 71;
        Require(sceImeBackendGetConvertString(watchedText) == 0 && TextEquals(watchedText, u"Hi"));
        Require(errno == 71);
        Require(collectDirty());
        *watchedText = u'~';
        armWatch(watchedText);
        errno = 71;
        Require(sceImeBackendGetConfirmString(watchedText) == 0 && TextEquals(watchedText, u"Hi"));
        Require(errno == 71);
        Require(collectDirty());
        *watchedText = u'~';
        *watchedSel = 99;
        armWatch(watchedText);
        errno = 71;
        Require(sceImeBackendGetCandidateList(watchedSel, watchedText) == 0 && *watchedSel == 0 && watchedText[0] == 0);
        Require(errno == 71);
        Require(collectDirty());
        *watchedSel = 99;
        armWatch(watchedSel);
        errno = 71;
        Require(sceImeBackendGetCandidateSelect(watchedSel) == 0 && *watchedSel == 0);
        Require(errno == 71);
        Require(collectDirty());
        *watchedMode = 99;
        armWatch(watchedMode);
        errno = 71;
        Require(sceImeBackendGetConvertMode(watchedMode) == 0 && *watchedMode == 1);
        Require(errno == 71);
        Require(collectDirty());
        armWatch(watchedText);
        errno = 71;
        Require(sceImeBackendEnterCharacter(u'Z', watchedText) == 0 && TextEquals(watchedText, u"HiZ"));
        Require(sceImeBackendGetCaretIndex() == 3);
        Require(errno == 71);
        Require(collectDirty());
        GuestArena::GuestArenaSetProtection_nid_postfix(reinterpret_cast<std::uintptr_t>(mapped), kArenaBytes, 0x02);
        errno = 71;
        Require(sceImeBackendGetString(0, watchedText, 0x20) == kNullArgument && TextEquals(watchedText, u"HiZ"));
        Require(sceImeBackendGetConvertString(watchedText) == kNullArgument && TextEquals(watchedText, u"HiZ"));
        Require(sceImeBackendGetConfirmString(watchedText) == kNullArgument && TextEquals(watchedText, u"HiZ"));
        Require(sceImeBackendGetCandidateList(watchedSel, watchedText) == kNullArgument && *watchedSel == 0);
        Require(sceImeBackendGetCandidateSelect(watchedSel) == kNullArgument && *watchedSel == 0);
        Require(sceImeBackendGetConvertMode(watchedMode) == kNullArgument && *watchedMode == 1);
        Require(sceImeBackendEnterCharacter(u'Q', watchedText) == kNullArgument && TextEquals(watchedText, u"HiZ"));
        Require(sceImeBackendGetCaretIndex() == 3);
        Require(errno == 71);
        GuestArena::GuestArenaSetProtection_nid_postfix(reinterpret_cast<std::uintptr_t>(mapped), kArenaBytes, 0x04);
#ifndef _WIN32
        if (watchedWrites) {
            GuestWriteWatch::GuestWriteWatchUnregister_nid_postfix(mapped, kArenaBytes);
        }
#endif
        GuestArena::GuestArenaReset_nid_postfix(mapped, kArenaBytes);
        GuestArena::GuestArenaRelease_nid_postfix(mapped, kArenaBytes);
    }

    Require(sceImeBackendClose() == 0);
    Require(sceImeBackendGetStatus() == 0);
    Require(sceImeBackendClose() == kNotInitialized);
    Require(sceImeBackendGetCaretIndex() == kNotInitialized);
}

void CheckEditing() {
    SceImeBackendParam param = MakeParam();
    const char16_t initial[] = u"Hi";
    param.type = 2;
    param.initialString = initial;
    Require(sceImeBackendOpen(&param, nullptr) == 0);

    char16_t text[0x20] = {};
    Require(sceImeBackendEnterCharacter(u'A', text) == 0 && TextEquals(text, u"HiA"));
    Require(sceImeBackendGetConvertString(text) == 0 && TextEquals(text, u"HiA"));
    Require(sceImeBackendGetString(0, text, 0x20) == 0 && TextEquals(text, u"HiA"));
    Require(sceImeBackendGetCaretIndex() == 3);

    Require(sceImeBackendModeCaretPrevious() == 0);
    Require(sceImeBackendGetCaretIndex() == 2);
    Require(sceImeBackendModeCaretNext() == 0);
    Require(sceImeBackendGetCaretIndex() == 3);
    Require(sceImeBackendModeCaretPrevious() == 0);
    Require(sceImeBackendGetCaretIndex() == 2);
    Require(sceImeBackendEnterCharacter(u'B', text) == 0 && TextEquals(text, u"HiBA"));
    Require(sceImeBackendBackspaceCharacter() == 0);
    Require(sceImeBackendGetConvertString(text) == 0 && TextEquals(text, u"HiA"));
    Require(sceImeBackendModeCaretPrevious() == 0);
    Require(sceImeBackendDeleteCharacter() == 0);
    Require(sceImeBackendGetConvertString(text) == 0 && TextEquals(text, u"HA"));

    Require(sceImeBackendAllDeleteConvertString() == 0);
    Require(sceImeBackendGetConvertString(text) == 0 && TextEquals(text, u""));
    Require(sceImeBackendGetConfirmString(text) == 0 && TextEquals(text, u""));

    Require(sceImeBackendReset() == 0);
    Require(sceImeBackendGetConfirmString(text) == 0 && TextEquals(text, u""));
    Require(sceImeBackendGetCaretIndex() == 0);

    Require(sceImeBackendClose() == 0);
}

void CheckStringCapacity() {
    SceImeBackendParam param = MakeParam();
    const char16_t initial[] = u"Abcd";
    param.type = 2;
    param.initialString = initial;
    Require(sceImeBackendOpen(&param, nullptr) == 0);

    char16_t text[0x20];
    for (char16_t& character : text) character = u'~';
    Require(sceImeBackendGetString(0, text, 4) == 0);
    Require(text[0] == u'A' && text[1] == u'b' && text[2] == u'c' && text[3] == u'd' && text[4] == 0 && text[5] == u'~');

    for (char16_t& character : text) character = u'~';
    Require(sceImeBackendGetString(0, text, 2) == 0);
    Require(text[0] == u'A' && text[1] == u'b' && text[2] == 0 && text[3] == u'~');

    for (char16_t& character : text) character = u'~';
    Require(sceImeBackendGetConvertString(text) == 0 && text[4] == 0 && text[5] == u'~');
    Require(sceImeBackendGetConfirmString(text) == 0 && text[4] == 0 && text[5] == u'~');

    Require(sceImeBackendClose() == 0);
}

}

int main() {
    CheckValidation();
    CheckLifecycle();
    CheckEditing();
    CheckStringCapacity();
    return 0;
}
