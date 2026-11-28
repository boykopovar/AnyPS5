#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"

#include <array>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <source_location>
#include <stdexcept>
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "LibreSSL3 X509_VERIFY_PARAM check failed at line %u\n", location.line());
    std::abort();
}

constexpr std::uint64_t PolicyCheck = 0x80;
constexpr std::uint64_t ExplicitPolicy = 0x100;
constexpr std::uint64_t InhibitAny = 0x200;
constexpr std::uint64_t InhibitMap = 0x400;
constexpr std::uint64_t PartialChain = 0x80000;

struct GuardedParam {
    std::array<std::uint64_t, 2> before;
    LibreSslX509VerifyParam param;
    std::array<std::uint64_t, 2> after;
};

void CheckSetFlagsResult(std::uint64_t initial, std::uint64_t flags, std::uint64_t result) {
    GuardedParam guarded;
    std::memset(&guarded, 0x5A, sizeof(guarded));
    guarded.param.flags = initial;
    GuardedParam expected;
    std::memcpy(&expected, &guarded, sizeof(guarded));
    expected.param.flags = result;
    Require(X509_VERIFY_PARAM_set_flags_nid_postfix(&guarded.param, flags) == 1);
    Require(std::memcmp(&guarded, &expected, sizeof(guarded)) == 0);
}

void CheckSetFlags(std::uint64_t initial, std::uint64_t flags) {
    CheckSetFlagsResult(initial, flags, initial | flags);
}

void TestSetFlags() {
    CheckSetFlags(0, 0);
    CheckSetFlags(0, PartialChain);
    CheckSetFlags(PartialChain, PartialChain);
    CheckSetFlags(0x8000, 0x4);
    CheckSetFlags(~std::uint64_t {0}, 0x1);
    CheckSetFlags(0x1, ~std::uint64_t {0});
    CheckSetFlags(0, PolicyCheck);
    CheckSetFlags(0, PolicyCheck | ExplicitPolicy);
    CheckSetFlags(PolicyCheck, ExplicitPolicy);
    CheckSetFlags(PolicyCheck | PartialChain, InhibitAny | InhibitMap);
    CheckSetFlags(0, 0x1);
    CheckSetFlagsResult(0, ExplicitPolicy, ExplicitPolicy | PolicyCheck);
    CheckSetFlagsResult(0, InhibitAny, InhibitAny | PolicyCheck);
    CheckSetFlagsResult(0, InhibitMap, InhibitMap | PolicyCheck);
    CheckSetFlagsResult(ExplicitPolicy | PolicyCheck, 0x1, ExplicitPolicy | PolicyCheck | 0x1);
    bool thrown = false;
    try {
        X509_VERIFY_PARAM_set_flags_nid_postfix(nullptr, PartialChain);
    } catch (const std::invalid_argument&) {
        thrown = true;
    }
    Require(thrown);
}

class ReadOnlyPage {
public:
    ReadOnlyPage() {
#ifdef _WIN32
        SYSTEM_INFO info{};
        GetSystemInfo(&info);
        bytes = info.dwPageSize;
        data = VirtualAlloc(nullptr, bytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
        Require(data != nullptr);
#else
        const long page = sysconf(_SC_PAGESIZE);
        Require(page > 0);
        bytes = static_cast<std::size_t>(page);
        data = mmap(nullptr, bytes, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        Require(data != nullptr && data != MAP_FAILED);
#endif
    }

    ~ReadOnlyPage() {
#ifdef _WIN32
        VirtualFree(data, 0, MEM_RELEASE);
#else
        munmap(data, bytes);
#endif
    }

    template <typename T>
    T* Set(const T& value) {
        Require(sizeof(value) <= bytes);
        std::memcpy(data, &value, sizeof(value));
        return static_cast<T*>(data);
    }

    void Protect() {
#ifdef _WIN32
        DWORD previous{};
        Require(VirtualProtect(data, bytes, PAGE_READONLY, &previous) != 0);
#else
        Require(mprotect(data, bytes, PROT_READ) == 0);
#endif
    }

private:
    void* data = nullptr;
    std::size_t bytes = 0;
};

template <typename TCall>
void Rejects(TCall call) {
    errno = EINVAL;
    bool thrown = false;
    try {
        call();
    } catch (const std::runtime_error&) {
        thrown = true;
    }
    Require(thrown);
    Require(errno == EINVAL);
}

void GuestPointers() {
    auto* badParam = reinterpret_cast<LibreSslX509VerifyParam*>(0x10);
    Rejects([&] { X509_VERIFY_PARAM_set_flags_nid_postfix(badParam, PartialChain); });

    ReadOnlyPage page;
    LibreSslX509VerifyParam initial{};
    initial.flags = PolicyCheck;
    auto* roParam = page.Set(initial);
    page.Protect();

    Rejects([&] { X509_VERIFY_PARAM_set_flags_nid_postfix(roParam, PartialChain); });
    Rejects([&] { X509_VERIFY_PARAM_set_flags_nid_postfix(roParam, 0); });
    Require(roParam->flags == PolicyCheck);
}

}

int main() {
    TestSetFlags();
    GuestPointers();
    std::puts("LibreSSL3 X509_VERIFY_PARAM tests passed");
}
