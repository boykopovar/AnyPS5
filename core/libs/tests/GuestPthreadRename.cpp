#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libkernel/KernelErrors.hpp"
#include "SceTypes.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#ifdef _WIN32
#include <windows.h>
#endif

static void Require(bool value) { if (!value) std::abort(); }

#ifdef _WIN32
static void CheckDescription(const std::wstring& expected) {
    PWSTR description = nullptr;
    Require(SUCCEEDED(GetThreadDescription(GetCurrentThread(), &description)) && description != nullptr);
    const std::wstring actual(description);
    LocalFree(description);
    if (actual != expected) {
        std::fprintf(stderr, "thread description units:");
        for (const wchar_t unit : actual) std::fprintf(stderr, " %04x", static_cast<unsigned>(unit));
        std::fprintf(stderr, "; expected:");
        for (const wchar_t unit : expected) std::fprintf(stderr, " %04x", static_cast<unsigned>(unit));
        std::fprintf(stderr, "\n");
        std::abort();
    }
}
#endif

static void CheckStoredName(Pthread thread, const std::string& expected) {
    std::array<unsigned char, 64> buffer;
    buffer.fill(0xaa);
    Require(scePthreadGetname(thread, reinterpret_cast<char*>(buffer.data())) == 0);
    Require(std::memcmp(buffer.data(), expected.c_str(), expected.size() + 1) == 0);
    for (std::size_t i = expected.size() + 1; i < buffer.size(); ++i) Require(buffer[i] == 0xaa);
}

static void CheckThreadRename(Pthread thread) {
    const int savedError = *__error_nid_postfix();
    const std::string names[] = {"", "A", "AnyPS5Probe", "0123456789012345678901234567890"};
    for (const auto& name : names) {
        *__error_nid_postfix() = 77;
        Require(pthread_rename_np_nid_postfix(thread, name.c_str()) == 0);
        Require(*__error_nid_postfix() == 77);
        CheckStoredName(thread, name);
    }
    for (const std::size_t length : {32u, 40u, 1024u}) {
        const std::string name(length, 'x');
        *__error_nid_postfix() = 77;
        const int error = pthread_rename_np_nid_postfix(thread, name.c_str());
        if (error != 63) {
            std::fprintf(stderr, "pthread_rename_np: %zu-byte name returned %d; expected ENAMETOOLONG=63\n", length, error);
            std::abort();
        }
        Require(*__error_nid_postfix() == 77);
        CheckStoredName(thread, names[3]);
    }
    constexpr int nameTooLong = static_cast<int>(0x8002003fu);
    for (const std::size_t length : {32u, 40u}) {
        const std::string name(length, 'x');
        Require(scePthreadRename(thread, name.c_str()) == nameTooLong);
        CheckStoredName(thread, names[3]);
    }
    for (const std::size_t length : {32u, 33u, 40u}) {
        const std::string name(length, 'x');
        *__error_nid_postfix() = 0;
        pthread_set_name_np_nid_postfix(thread, name.c_str());
        Require(*__error_nid_postfix() == 63);
        CheckStoredName(thread, names[3]);
    }
    Require(pthread_rename_np_nid_postfix(thread, "renamed") == 0);
    CheckStoredName(thread, "renamed");
    *__error_nid_postfix() = savedError;
}

static void CheckUnusableNames(Pthread thread) {
    constexpr std::size_t Page = 0x4000;
    auto* memory = static_cast<char*>(mmap_nid_postfix(nullptr, 2 * Page, 3, 0x1002, -1, 0));
    Require(memory != reinterpret_cast<char*>(static_cast<std::uintptr_t>(-1)));
    char* const end = memory + Page;
    Require(mprotect_nid_postfix(end, Page, 0) == 0);
    const std::string kept = "kept";
    Require(pthread_rename_np_nid_postfix(thread, kept.c_str()) == 0);
    const int savedError = *__error_nid_postfix();
    char* const fits = end - 32;
    std::memset(fits, 'a', 31);
    fits[31] = '\0';
    char* const noTerminator = end - 32;
    char* const unterminated = end - 31;
    char* const bad[] = {reinterpret_cast<char*>(0x10), end, unterminated};
    for (char* name : bad) {
        std::memset(memory, 'a', Page);
        Require(pthread_rename_np_nid_postfix(thread, name) == 14);
        Require(scePthreadRename(thread, name) == SCE_KERNEL_ERROR_EFAULT);
        Require(pthread_rename_np_nid_postfix(nullptr, name) == 22);
        Require(*__error_nid_postfix() == savedError);
        CheckStoredName(thread, kept);
    }
    std::memset(memory, 'a', Page);
    Require(pthread_rename_np_nid_postfix(thread, noTerminator) == 63);
    CheckStoredName(thread, kept);
    std::memset(memory, 'a', Page);
    fits[31] = '\0';
    Require(pthread_rename_np_nid_postfix(thread, fits) == 0);
    CheckStoredName(thread, std::string(31, 'a'));
    Require(scePthreadRename(thread, kept.c_str()) == 0);
    Require(scePthreadRename(thread, fits) == 0);
    CheckStoredName(thread, std::string(31, 'a'));
    Require(scePthreadRename(reinterpret_cast<Pthread>(0x10), kept.c_str()) == SCE_KERNEL_ERROR_ESRCH);
    Require(scePthreadRename(thread, nullptr) == SCE_KERNEL_ERROR_EINVAL);
    Require(pthread_rename_np_nid_postfix(thread, nullptr) == 22);
    std::memset(memory, 'b', Page);
    Require(scePthreadRename(thread, memory) == SCE_KERNEL_ERROR_ENAMETOOLONG);
    CheckStoredName(thread, std::string(31, 'a'));
    Require(mprotect_nid_postfix(end, Page, 3) == 0);
    Require(munmap_nid_postfix(memory, 2 * Page) == 0);
    Require(pthread_rename_np_nid_postfix(thread, kept.c_str()) == 0);
}

static void* APS5_VABI Worker(void*) {
    const Pthread self = scePthreadSelf();
    Require(self != nullptr);
#ifdef _WIN32
    CheckDescription(L"Z\u00e4hler");
    Require(scePthreadRename(self, "Z\xc3\xa4hler-\xe6\x97\xa5") == 0);
    CheckDescription(L"Z\u00e4hler-\u65e5");
#endif
    CheckThreadRename(self);
    return nullptr;
}

int main() {
    const Pthread self = scePthreadSelf();
    Require(self != nullptr);
    CheckThreadRename(self);
    CheckUnusableNames(self);
    Pthread thread = nullptr;
    Require(scePthreadCreate(&thread, nullptr, Worker, nullptr, "Z\xc3\xa4hler") == 0);
    Require(scePthreadJoin(thread, nullptr) == 0);
}
