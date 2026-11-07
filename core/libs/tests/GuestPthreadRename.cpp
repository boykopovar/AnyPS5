#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "SceTypes.hpp"
#include <array>
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
    Require(pthread_rename_np_nid_postfix(thread, "renamed") == 0);
    CheckStoredName(thread, "renamed");
    *__error_nid_postfix() = savedError;
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
    Pthread thread = nullptr;
    Require(scePthreadCreate(&thread, nullptr, Worker, nullptr, "Z\xc3\xa4hler") == 0);
    Require(scePthreadJoin(thread, nullptr) == 0);
}
