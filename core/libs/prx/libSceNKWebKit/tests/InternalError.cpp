#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>
#include <cerrno>
#include <cstdint>
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#endif

using NKWebKit::WebCoreAbi::ResourceError;
using NKWebKit::WebCoreAbi::ResourceErrorType;
using NKWebKit::WtfAbi::StringImpl;
using NKWebKit::WtfAbi::URL;

namespace {

void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "InternalError check failed at line %d\n", line);
        std::abort();
    }
}

#define Require(value) Check((value), __LINE__)

std::string Read(const StringImpl* impl) {
    if (!impl) return "<null>";
    Require((impl->hashAndFlags & 4) != 0);
    return std::string(static_cast<const char*>(impl->data), impl->length);
}

void TestInternalError() {
    const char text[] = "https://user@example.com/path?q#f";
    StringImpl urlImpl{0x40000002, static_cast<unsigned>(sizeof(text) - 1), text, 4};
    URL url{};
    url.impl = &urlImpl;
    url.bits = 0x12345678;
    url.userStart = 8;
    url.userEnd = 12;
    url.passwordEnd = 12;
    url.hostEnd = 24;
    url.pathAfterLastSlash = 25;
    url.pathEnd = 29;
    url.queryEnd = 31;

    ResourceError first = _ZN7WebCore13internalErrorERKN3WTF3URLE(&url);
    Require(Read(first.domain) == "WebKitErrorDomain");
    Require(first.domain->refCount == 2);
    Require(Read(first.localizedDescription) == "WebKit encountered an internal error");
    Require(first.errorCode == 300);
    Require(first.type == ResourceErrorType::General);
    Require(!first.isSanitized);
    Require(first.tlsVerificationError == 0 && first.tlsVerificationErrorDescription == nullptr);
    Require(std::memcmp(&first.failingURL, &url, sizeof(URL)) == 0);
    Require(urlImpl.refCount == 0x40000004);

    unsigned char bytes[sizeof(ResourceError)];
    std::memcpy(bytes, &first, sizeof(first));
    Require(bytes[56] == 0x2C && bytes[57] == 0x01 && bytes[58] == 0 && bytes[59] == 0);
    Require(bytes[60] == 1 && bytes[61] == 0 && bytes[62] == 0 && bytes[63] == 0);

    ResourceError second = _ZN7WebCore13internalErrorERKN3WTF3URLE(&url);
    Require(second.domain != first.domain);
    Require(urlImpl.refCount == 0x40000006);

    URL null{};
    ResourceError withoutURL = _ZN7WebCore13internalErrorERKN3WTF3URLE(&null);
    Require(withoutURL.failingURL.impl == nullptr);
    Require(Read(withoutURL.domain) == "WebKitErrorDomain" && withoutURL.errorCode == 300);

    bool threw = false;
    try {
        _ZN7WebCore13internalErrorERKN3WTF3URLE(nullptr);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);
    Require(urlImpl.refCount == 0x40000006);

    unsigned char savedBytes[sizeof(URL)];
    std::memcpy(savedBytes, &url, sizeof(URL));
    for (auto bad : {static_cast<const URL*>(nullptr), reinterpret_cast<const URL*>(std::uintptr_t{0x10}), reinterpret_cast<const URL*>(~std::uintptr_t{0})}) {
        if (!bad) continue;
        threw = false;
        int savedErrno = errno;
        try {
            _ZN7WebCore13internalErrorERKN3WTF3URLE(bad);
        } catch (const std::invalid_argument&) {
            threw = true;
        }
        Require(threw);
        Require(errno == savedErrno);
        Require(std::memcmp(&url, savedBytes, sizeof(URL)) == 0);
        Require(urlImpl.refCount == 0x40000006);
    }

    URL badImplGuest{};
    badImplGuest.impl = reinterpret_cast<StringImpl*>(std::uintptr_t{0x10});
    threw = false;
    try {
        _ZN7WebCore13internalErrorERKN3WTF3URLE(&badImplGuest);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);
    Require(urlImpl.refCount == 0x40000006);

    unsigned char implBefore[sizeof(StringImpl)];
    std::memcpy(implBefore, &urlImpl, sizeof(StringImpl));
#ifdef _WIN32
    void* readOnlyPage = VirtualAlloc(nullptr, 4096, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
#else
    void* readOnlyPage = mmap(nullptr, 4096, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, -1, 0);
#endif
#ifdef _WIN32
    Require(readOnlyPage != nullptr);
#else
    Require(readOnlyPage != MAP_FAILED);
#endif
    std::memcpy(readOnlyPage, &urlImpl, sizeof(StringImpl));
#ifdef _WIN32
    DWORD previous = 0;
    Require(VirtualProtect(readOnlyPage, 4096, PAGE_READONLY, &previous) != 0);
#else
    Require(mprotect(readOnlyPage, 4096, PROT_READ) == 0);
#endif
    URL readOnlyImplGuest{};
    readOnlyImplGuest.impl = static_cast<StringImpl*>(readOnlyPage);
    threw = false;
    try {
        _ZN7WebCore13internalErrorERKN3WTF3URLE(&readOnlyImplGuest);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);
    unsigned char implAfter[sizeof(StringImpl)];
    std::memcpy(implAfter, readOnlyPage, sizeof(StringImpl));
    Require(std::memcmp(implBefore, implAfter, sizeof(StringImpl)) == 0);
    Require(urlImpl.refCount == 0x40000006);
#ifdef _WIN32
    Require(VirtualFree(readOnlyPage, 0, MEM_RELEASE) != 0);
#else
    Require(munmap(readOnlyPage, 4096) == 0);
#endif

    for (ResourceError* error : {&first, &second, &withoutURL}) {
        NKWebKit::WtfAbi::Release(error->domain);
        NKWebKit::WtfAbi::Release(error->localizedDescription);
        NKWebKit::WtfAbi::Release(error->failingURL.impl);
    }
    Require(urlImpl.refCount == 0x40000002);
}

}

int main() {
    TestInternalError();
    return 0;
}
