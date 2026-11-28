#include "prx/libSceLibreSsl/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <source_location>
#include <stdexcept>

struct TestUi {
    const void* meth;
    void* strings;
    void* userData;
    void* exData;
    int flags;
};

struct TestUiString {
    int type;
    const char* outString;
    int inputFlags;
    char* resultBuf;
    unsigned char data[24];
    int flags;
};

static_assert(sizeof(TestUi) == 40 && offsetof(TestUi, userData) == 16);
static_assert(sizeof(TestUiString) == 64 && offsetof(TestUiString, inputFlags) == 16);

static const LibreSslUi* Ui(const TestUi* ui) {
    return reinterpret_cast<const LibreSslUi*>(ui);
}

static const LibreSslUiString* UiString(const TestUiString* string) {
    return reinterpret_cast<const LibreSslUiString*>(string);
}

static void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "LibreSSL UI string check failed at line %u\n", location.line());
    std::abort();
}

template<typename TAction> static void RequireThrows(TAction action, std::source_location location = std::source_location::current()) {
    try { action(); } catch (const std::exception&) { return; }
    Require(false, location);
}

static void TestUserData() {
    int data = 0;
    int strings = 0;
    TestUi ui {};
    RequireThrows([] { UI_get0_user_data_nid_postfix(nullptr); });
    Require(UI_get0_user_data_nid_postfix(Ui(&ui)) == nullptr);
    ui.strings = &strings;
    ui.userData = &data;
    ui.flags = 0x0101;
    Require(UI_get0_user_data_nid_postfix(Ui(&ui)) == &data);
}

static void TestStringType() {
    TestUiString string {};
    Require(UI_get_string_type_nid_postfix(nullptr) == 0);
    Require(UI_get_string_type_nid_postfix(UiString(&string)) == 0);
    string.inputFlags = 1;
    string.flags = 1;
    for (int type = 1; type <= 5; ++type) {
        string.type = type;
        Require(UI_get_string_type_nid_postfix(UiString(&string)) == type);
    }
}

static void TestInputFlags() {
    TestUiString string {};
    Require(UI_get_input_flags_nid_postfix(nullptr) == 0);
    Require(UI_get_input_flags_nid_postfix(UiString(&string)) == 0);
    string.type = 2;
    string.flags = 1;
    string.inputFlags = 0x01;
    Require(UI_get_input_flags_nid_postfix(UiString(&string)) == 0x01);
    string.inputFlags = 0x8001;
    Require(UI_get_input_flags_nid_postfix(UiString(&string)) == 0x8001);
    string.inputFlags = -1;
    Require(UI_get_input_flags_nid_postfix(UiString(&string)) == -1);
}

static void TestGuestPointers() {
    struct GuardedPages {
        unsigned char* base = nullptr;
        std::size_t pageSize = 4096;

        GuardedPages() {
#ifdef _WIN32
            SYSTEM_INFO info{};
            GetSystemInfo(&info);
            pageSize = info.dwPageSize ? static_cast<std::size_t>(info.dwPageSize) : 4096;
            base = static_cast<unsigned char*>(VirtualAlloc(nullptr, pageSize * 2, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
            Require(base != nullptr);
            DWORD oldProtect = 0;
            Require(VirtualProtect(base + pageSize, pageSize, PAGE_NOACCESS, &oldProtect) != 0);
#else
            const long size = sysconf(_SC_PAGESIZE);
            pageSize = size > 0 ? static_cast<std::size_t>(size) : 4096;
            void* mapped = mmap(nullptr, pageSize * 2, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
            Require(mapped != MAP_FAILED);
            base = static_cast<unsigned char*>(mapped);
            Require(mprotect(base + pageSize, pageSize, PROT_NONE) == 0);
#endif
        }

        ~GuardedPages() {
            if (!base) return;
#ifdef _WIN32
            VirtualFree(base, 0, MEM_RELEASE);
#else
            munmap(base, pageSize * 2);
#endif
        }

        unsigned char* Guard() const { return base + pageSize; }
    };

    GuardedPages pages;
    errno = 71;
    RequireThrows([] { UI_get0_user_data_nid_postfix(reinterpret_cast<const LibreSslUi*>(0x10)); });
    RequireThrows([&] { UI_get0_user_data_nid_postfix(reinterpret_cast<const LibreSslUi*>(pages.Guard())); });
    RequireThrows([&] { UI_get0_user_data_nid_postfix(reinterpret_cast<const LibreSslUi*>(pages.Guard() - offsetof(TestUi, userData))); });
    RequireThrows([&] { UI_get0_user_data_nid_postfix(reinterpret_cast<const LibreSslUi*>(pages.Guard() - offsetof(TestUi, userData) - 4)); });
    RequireThrows([] { UI_get0_user_data_nid_postfix(reinterpret_cast<const LibreSslUi*>(~std::uintptr_t{0} - 8)); });

    const auto* const unmapped = reinterpret_cast<const LibreSslUiString*>(0x10);
    const auto* const guarded = reinterpret_cast<const LibreSslUiString*>(pages.Guard());
    const auto* const straddleType = reinterpret_cast<const LibreSslUiString*>(pages.Guard() - 2);
    const auto* const straddleFlags = reinterpret_cast<const LibreSslUiString*>(pages.Guard() - offsetof(TestUiString, inputFlags) - 2);
    const auto* const wrap = reinterpret_cast<const LibreSslUiString*>(~std::uintptr_t{0} - 8);
    Require(UI_get_string_type_nid_postfix(unmapped) == 0 && UI_get_input_flags_nid_postfix(unmapped) == 0);
    Require(UI_get_string_type_nid_postfix(guarded) == 0 && UI_get_input_flags_nid_postfix(guarded) == 0);
    Require(UI_get_string_type_nid_postfix(straddleType) == 0);
    Require(UI_get_input_flags_nid_postfix(straddleFlags) == 0);
    Require(UI_get_string_type_nid_postfix(wrap) == 0 && UI_get_input_flags_nid_postfix(wrap) == 0);
    Require(errno == 71);
}

int main() {
    TestUserData();
    TestStringType();
    TestInputFlags();
    TestGuestPointers();
    return 0;
}
