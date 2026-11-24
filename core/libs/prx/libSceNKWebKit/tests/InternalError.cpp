#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>

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
