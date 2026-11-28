#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

using NKWebKit::WebCoreAbi::AuthenticationChallenge;
using NKWebKit::WtfAbi::StringImpl;

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "AuthenticationChallenge check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

static StringImpl Make8(const char* text) {
    return StringImpl{2, static_cast<unsigned>(std::strlen(text)), text, 4};
}

static StringImpl Make16(const char16_t* text) {
    unsigned length = 0;
    while (text[length]) ++length;
    return StringImpl{2, length, text, 0};
}

static void TestIsNull() {
    alignas(16) unsigned char storage[0x200] = {};
    auto* challenge = reinterpret_cast<AuthenticationChallenge*>(storage);

    Require(!_ZNK7WebCore27AuthenticationChallengeBase6isNullEv(challenge));
    challenge->isNull = 1;
    Require(_ZNK7WebCore27AuthenticationChallengeBase6isNullEv(challenge));

    bool threw = false;
    try {
        _ZNK7WebCore27AuthenticationChallengeBase6isNullEv(nullptr);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);
}

class GuardedMapping {
    unsigned char* base;
    std::size_t pageSize;

public:
    GuardedMapping() {
#ifdef _WIN32
        SYSTEM_INFO info{};
        GetSystemInfo(&info);
        pageSize = info.dwPageSize;
        base = static_cast<unsigned char*>(VirtualAlloc(nullptr, pageSize * 2, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
        Require(base != nullptr);
        DWORD previous = 0;
        Require(VirtualProtect(base + pageSize, pageSize, PAGE_NOACCESS, &previous) != 0);
#else
        const long size = sysconf(_SC_PAGESIZE);
        Require(size > 0);
        pageSize = static_cast<std::size_t>(size);
        void* mapped = mmap(nullptr, pageSize * 2, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, -1, 0);
        Require(mapped != MAP_FAILED);
        base = static_cast<unsigned char*>(mapped);
        Require(mprotect(base + pageSize, pageSize, PROT_NONE) == 0);
        Require(pageSize >= 0x200u);
#endif
    }

    ~GuardedMapping() {
#ifdef _WIN32
        Require(VirtualFree(base, 0, MEM_RELEASE) != 0);
#else
        Require(munmap(base, pageSize * 2) == 0);
#endif
    }

    GuardedMapping(const GuardedMapping&) = delete;
    GuardedMapping& operator=(const GuardedMapping&) = delete;

    unsigned char* GuardPage() const { return base + pageSize; }

    unsigned char* ReadOnlyPage() const { return base; }

    void MakeReadOnly() {
#ifdef _WIN32
        DWORD previous = 0;
        Require(VirtualProtect(base, pageSize, PAGE_READONLY, &previous) != 0);
#else
        Require(mprotect(base, pageSize, PROT_READ) == 0);
#endif
    }
};

static void TestNullify() {
    alignas(16) unsigned char storage[0x200] = {};
    auto* challenge = reinterpret_cast<AuthenticationChallenge*>(storage);

    challenge->isNull = 0;
    _ZN7WebCore27AuthenticationChallengeBase7nullifyEv(challenge);
    Require(challenge->isNull == 1);
    Require(_ZNK7WebCore27AuthenticationChallengeBase6isNullEv(challenge));

    bool threw = false;
    try {
        _ZN7WebCore27AuthenticationChallengeBase7nullifyEv(nullptr);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);

    {
        GuardedMapping mapping;
        unsigned char* guardPage = mapping.GuardPage();
        auto* guarded = reinterpret_cast<AuthenticationChallenge*>(guardPage);

        bool caught = false;
        int savedErrno = errno;
        errno = 0x42;
        try {
            _ZN7WebCore27AuthenticationChallengeBase7nullifyEv(guarded);
        } catch (const std::invalid_argument&) {
            caught = true;
        }
        Require(caught);
        Require(errno == 0x42);
    }

    {
        GuardedMapping mapping;
        unsigned char* page = mapping.ReadOnlyPage();
        auto* readOnly = reinterpret_cast<AuthenticationChallenge*>(page);
        std::memset(page, 0x5A, sizeof(AuthenticationChallenge));
        mapping.MakeReadOnly();

        bool caught = false;
        try {
            _ZN7WebCore27AuthenticationChallengeBase7nullifyEv(readOnly);
        } catch (const std::invalid_argument&) {
            caught = true;
        }
        Require(caught);
        for (std::size_t i = 0; i < sizeof(AuthenticationChallenge); ++i) Require(page[i] == 0x5A);
    }
}

static void TestAuthenticationClient() {
    alignas(16) unsigned char storage[0x200] = {};
    std::memset(storage, 0xAB, sizeof(storage));
    auto* challenge = reinterpret_cast<AuthenticationChallenge*>(storage);

    Require(_ZNK7WebCore23AuthenticationChallenge20authenticationClientEv(challenge) == nullptr);
    Require(_ZNK7WebCore23AuthenticationChallenge20authenticationClientEv(nullptr) == nullptr);
}

static void TestPlatformCompare() {
    alignas(16) unsigned char left[0x200] = {};
    alignas(16) unsigned char right[0x200] = {};
    std::memset(left, 0xAB, sizeof(left));
    std::memset(right, 0xAB, sizeof(right));
    auto* a = reinterpret_cast<AuthenticationChallenge*>(left);
    auto* b = reinterpret_cast<AuthenticationChallenge*>(right);

    a->requestURL.impl = nullptr;
    b->requestURL.impl = nullptr;
    Require(_ZN7WebCore23AuthenticationChallenge15platformCompareERKS0_S2_(a, b));

    a->isForMainResource = 1;
    Require(!_ZN7WebCore23AuthenticationChallenge15platformCompareERKS0_S2_(a, b));
    b->isForMainResource = 1;
    Require(_ZN7WebCore23AuthenticationChallenge15platformCompareERKS0_S2_(a, b));

    b->isInMainFrame = 1;
    Require(!_ZN7WebCore23AuthenticationChallenge15platformCompareERKS0_S2_(a, b));
    a->isInMainFrame = 1;
    Require(_ZN7WebCore23AuthenticationChallenge15platformCompareERKS0_S2_(a, b));

    a->isForMainResource = 0;
    b->isForMainResource = 0;
    a->isInMainFrame = 0;
    b->isInMainFrame = 0;

    static const char text[] = "https://example.com/path";
    auto sameImpl = Make8(text);
    auto other8 = Make8(text);
    static const char16_t wide[] = u"https://example.com/path";
    auto other16 = Make16(wide);
    auto different = Make8("https://example.org/path");

    a->requestURL.impl = &sameImpl;
    b->requestURL.impl = &sameImpl;
    Require(_ZN7WebCore23AuthenticationChallenge15platformCompareERKS0_S2_(a, b));

    b->requestURL.impl = &other8;
    Require(_ZN7WebCore23AuthenticationChallenge15platformCompareERKS0_S2_(a, b));

    b->requestURL.impl = &other16;
    Require(_ZN7WebCore23AuthenticationChallenge15platformCompareERKS0_S2_(a, b));

    a->requestURL.impl = &other16;
    b->requestURL.impl = &sameImpl;
    Require(_ZN7WebCore23AuthenticationChallenge15platformCompareERKS0_S2_(a, b));
    a->requestURL.impl = &sameImpl;

    b->requestURL.impl = &different;
    Require(!_ZN7WebCore23AuthenticationChallenge15platformCompareERKS0_S2_(a, b));

    auto shorter = Make8("https://example.com/pat");
    b->requestURL.impl = &shorter;
    Require(!_ZN7WebCore23AuthenticationChallenge15platformCompareERKS0_S2_(a, b));

    auto empty = Make8("");
    a->requestURL.impl = &empty;
    b->requestURL.impl = &empty;
    Require(_ZN7WebCore23AuthenticationChallenge15platformCompareERKS0_S2_(a, b));

    auto otherEmpty = Make8("");
    b->requestURL.impl = &otherEmpty;
    Require(_ZN7WebCore23AuthenticationChallenge15platformCompareERKS0_S2_(a, b));

    b->requestURL.impl = nullptr;
    Require(!_ZN7WebCore23AuthenticationChallenge15platformCompareERKS0_S2_(a, b));

    a->requestURL.impl = nullptr;
    Require(_ZN7WebCore23AuthenticationChallenge15platformCompareERKS0_S2_(a, b));

    bool threw = false;
    try {
        _ZN7WebCore23AuthenticationChallenge15platformCompareERKS0_S2_(nullptr, b);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);

    threw = false;
    try {
        _ZN7WebCore23AuthenticationChallenge15platformCompareERKS0_S2_(a, nullptr);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);
}

int main() {
    TestIsNull();
    TestNullify();
    TestAuthenticationClient();
    TestPlatformCompare();
    return 0;
}
