#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <source_location>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {

using namespace NKWebKit;
using WebCoreAbi::ResourceError;
using WebCoreAbi::ResourceErrorType;
using WtfAbi::String;
using WtfAbi::StringImpl;
using WtfAbi::URL;

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "ResourceError check failed at line %u\n", location.line());
        std::abort();
    }
}

void Text(const StringImpl* impl, std::u16string_view expected, std::source_location location = std::source_location::current()) {
    if (expected.empty() && !impl) return;
    Require(impl && impl->length == expected.size(), location);
    for (unsigned i = 0; i < expected.size(); ++i) {
        char16_t character = impl->hashAndFlags & 4
            ? static_cast<const unsigned char*>(impl->data)[i]
            : static_cast<const char16_t*>(impl->data)[i];
        Require(character == expected[i], location);
    }
}

String Wide(std::u16string_view text) {
    return _ZN3WTF10StringImpl6createEPKDsj(text.data(), static_cast<unsigned>(text.size()));
}

String Narrow(std::string_view text) {
    return _ZN3WTF10StringImpl6createEPKhj(reinterpret_cast<const unsigned char*>(text.data()), static_cast<unsigned>(text.size()));
}

URL MakeURL(const String& text) {
    URL url{};
    url.impl = text.impl;
    url.bits = (6U << 6) | 1U;
    url.userStart = 8;
    url.userEnd = 8;
    url.passwordEnd = 8;
    url.hostEnd = 19;
    url.pathAfterLastSlash = 20;
    url.pathEnd = 22;
    url.queryEnd = 26;
    return url;
}

struct OwnedError {
    ResourceError value;
    explicit OwnedError(const ResourceError& error) : value(error) {}
    OwnedError(const OwnedError&) = delete;
    OwnedError& operator=(const OwnedError&) = delete;
    ~OwnedError() {
        WtfAbi::Release(value.domain);
        WtfAbi::Release(value.failingURL.impl);
        WtfAbi::Release(value.localizedDescription);
        WtfAbi::Release(value.tlsVerificationErrorDescription);
    }
};

template<typename Operation>
bool Throws(Operation&& operation) {
    try {
        operation();
    } catch (const std::invalid_argument&) {
        return true;
    }
    return false;
}

void CheckSetType() {
    ResourceError error{};
    error.errorCode = -7;
    error.isSanitized = 1;
    error.tlsVerificationError = 35;
    Require(error.type == ResourceErrorType::Null);
    _ZN7WebCore17ResourceErrorBase7setTypeENS0_4TypeE(&error, ResourceErrorType::General);
    Require(error.type == ResourceErrorType::General);
    _ZN7WebCore17ResourceErrorBase7setTypeENS0_4TypeE(&error, ResourceErrorType::AccessControl);
    Require(error.type == ResourceErrorType::AccessControl);
    _ZN7WebCore17ResourceErrorBase7setTypeENS0_4TypeE(&error, ResourceErrorType::Cancellation);
    Require(error.type == ResourceErrorType::Cancellation);
    _ZN7WebCore17ResourceErrorBase7setTypeENS0_4TypeE(&error, ResourceErrorType::Timeout);
    Require(error.type == ResourceErrorType::Timeout);
    _ZN7WebCore17ResourceErrorBase7setTypeENS0_4TypeE(&error, ResourceErrorType::Null);
    Require(error.type == ResourceErrorType::Null);
    _ZN7WebCore17ResourceErrorBase7setTypeENS0_4TypeE(&error, static_cast<ResourceErrorType>(200));
    Require(error.type == static_cast<ResourceErrorType>(200));
    _ZN7WebCore17ResourceErrorBase7setTypeENS0_4TypeE(&error, ResourceErrorType::General);
    Require(error.errorCode == -7);
    Require(error.isSanitized == 1);
    Require(error.tlsVerificationError == 35);
    Require(error.domain == nullptr);
    Require(error.failingURL.impl == nullptr);
    Require(error.localizedDescription == nullptr);
    Require(error.tlsVerificationErrorDescription == nullptr);
    Require(Throws([] { _ZN7WebCore17ResourceErrorBase7setTypeENS0_4TypeE(nullptr, ResourceErrorType::General); }));
}

void CheckLazyInit() {
    ResourceError error{};
    error.type = ResourceErrorType::Cancellation;
    error.isSanitized = 1;
    error.errorCode = -3;
    unsigned char before[sizeof(ResourceError)];
    std::memcpy(before, &error, sizeof(before));
    _ZNK7WebCore17ResourceErrorBase8lazyInitEv(&error);
    Require(std::memcmp(before, &error, sizeof(before)) == 0);
    error.type = ResourceErrorType::General;
    std::memcpy(before, &error, sizeof(before));
    _ZNK7WebCore17ResourceErrorBase8lazyInitEv(&error);
    Require(std::memcmp(before, &error, sizeof(before)) == 0);
    Require(Throws([] { _ZNK7WebCore17ResourceErrorBase8lazyInitEv(nullptr); }));
}

void CheckCopyOfPopulatedError() {
    String domain = Narrow("CurlErrorDomain");
    String description = Wide(u"SSL peer failed verification");
    String tlsDescription = Narrow("certificate verify failed");
    String urlText = Narrow("https://example.com/pa?q=1");

    {
        ResourceError error{};
        error.domain = domain.impl;
        error.failingURL = MakeURL(urlText);
        error.localizedDescription = description.impl;
        error.errorCode = 60;
        error.type = ResourceErrorType::Cancellation;
        error.isSanitized = 1;
        error.tlsVerificationError = 60;
        error.tlsVerificationErrorDescription = tlsDescription.impl;
        unsigned domainRefs = domain.impl->refCount;
        unsigned descriptionRefs = description.impl->refCount;
        unsigned tlsRefs = tlsDescription.impl->refCount;
        unsigned urlRefs = urlText.impl->refCount;

        {
            OwnedError copy{_ZNK7WebCore17ResourceErrorBase12isolatedCopyEv(&error)};
            Require(copy.value.errorCode == 60);
            Require(copy.value.type == ResourceErrorType::Cancellation);
            Require(copy.value.isSanitized == 0);
            Require(copy.value.tlsVerificationError == 60);
            Require(copy.value.domain && copy.value.domain != domain.impl);
            Require(copy.value.domain->refCount == 2);
            Require(!!(copy.value.domain->hashAndFlags & 4));
            Require(copy.value.localizedDescription && copy.value.localizedDescription != description.impl);
            Require(copy.value.localizedDescription->refCount == 2);
            Require(!(copy.value.localizedDescription->hashAndFlags & 4));
            Require(copy.value.tlsVerificationErrorDescription && copy.value.tlsVerificationErrorDescription != tlsDescription.impl);
            Require(copy.value.tlsVerificationErrorDescription->refCount == 2);
            Require(copy.value.failingURL.impl && copy.value.failingURL.impl != urlText.impl);
            Require(copy.value.failingURL.impl->refCount == 2);
            Require(copy.value.failingURL.bits == error.failingURL.bits);
            Require(copy.value.failingURL.userStart == 8);
            Require(copy.value.failingURL.userEnd == 8);
            Require(copy.value.failingURL.passwordEnd == 8);
            Require(copy.value.failingURL.hostEnd == 19);
            Require(copy.value.failingURL.pathAfterLastSlash == 20);
            Require(copy.value.failingURL.pathEnd == 22);
            Require(copy.value.failingURL.queryEnd == 26);
            Text(copy.value.domain, u"CurlErrorDomain");
            Text(copy.value.localizedDescription, u"SSL peer failed verification");
            Text(copy.value.tlsVerificationErrorDescription, u"certificate verify failed");
            Text(copy.value.failingURL.impl, u"https://example.com/pa?q=1");
            Require(domain.impl->refCount == domainRefs);
            Require(description.impl->refCount == descriptionRefs);
            Require(tlsDescription.impl->refCount == tlsRefs);
            Require(urlText.impl->refCount == urlRefs);
            Require(error.failingURL.impl == urlText.impl);
            Require(error.errorCode == 60);
            Require(error.type == ResourceErrorType::Cancellation);
            Require(error.isSanitized == 1);
        }
        Require(domain.impl->refCount == domainRefs);
        Require(description.impl->refCount == descriptionRefs);
        Require(tlsDescription.impl->refCount == tlsRefs);
        Require(urlText.impl->refCount == urlRefs);
    }
    Require(domain.impl->refCount == 2);
    Require(description.impl->refCount == 2);
    Require(tlsDescription.impl->refCount == 2);
    Require(urlText.impl->refCount == 2);
}

void CheckCopyOfNullError() {
    ResourceError error{};
    OwnedError copy{_ZNK7WebCore17ResourceErrorBase12isolatedCopyEv(&error)};
    Require(copy.value.domain == nullptr);
    Require(copy.value.failingURL.impl == nullptr);
    Require(copy.value.localizedDescription == nullptr);
    Require(copy.value.errorCode == 0);
    Require(copy.value.type == ResourceErrorType::Null);
    Require(copy.value.isSanitized == 0);
    Require(copy.value.tlsVerificationError == 0);
    Require(copy.value.tlsVerificationErrorDescription == nullptr);
    Require(Throws([] { _ZNK7WebCore17ResourceErrorBase12isolatedCopyEv(nullptr); }));
}

void CheckCopyOfEmptyStrings() {
    String empty = Wide(u"");
    ResourceError error{};
    error.domain = empty.impl;
    error.failingURL = MakeURL(empty);
    error.localizedDescription = empty.impl;
    error.tlsVerificationErrorDescription = empty.impl;
    OwnedError copy{_ZNK7WebCore17ResourceErrorBase12isolatedCopyEv(&error)};
    Require(copy.value.domain == empty.impl);
    Require(copy.value.domain->length == 0);
    Require(copy.value.failingURL.impl == empty.impl);
    Require(copy.value.failingURL.impl->length == 0);
    Require(copy.value.localizedDescription == empty.impl);
    Require(copy.value.tlsVerificationErrorDescription == empty.impl);
    Text(copy.value.domain, u"");
    Text(copy.value.failingURL.impl, u"");
}

void Run() {
    CheckSetType();
    CheckLazyInit();
    CheckCopyOfPopulatedError();
    CheckCopyOfNullError();
    CheckCopyOfEmptyStrings();
}

}  // namespace

int main() {
    try {
        Run();
        std::puts("ResourceErrorBase checks passed");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "ResourceError exception: %s\n", error.what());
        return 1;
    }
}
