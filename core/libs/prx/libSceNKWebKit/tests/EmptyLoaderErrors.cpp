#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <array>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <source_location>

namespace {

using NKWebKit::WebCoreAbi::ResourceError;
using NKWebKit::WebCoreAbi::ResourceErrorType;

using ErrorFunction = ResourceError (APS5_VABI*)(const void*, const void*);

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "empty loader error check failed at line %u\n", location.line());
        std::abort();
    }
}

void CheckEmpty(const ResourceError& error, ResourceErrorType type) {
    Require(error.type == type);
    std::array<unsigned char, sizeof(ResourceError)> bytes {};
    std::memcpy(bytes.data(), &error, sizeof(error));
    for (std::size_t i = 0; i < bytes.size(); ++i) {
        if (i == offsetof(ResourceError, type)) {
            Require(bytes[i] == static_cast<unsigned char>(type));
            continue;
        }
        if (i > offsetof(ResourceError, isSanitized) && i < offsetof(ResourceError, tlsVerificationError)) continue;
        if (i >= offsetof(ResourceError, tlsVerificationError) + sizeof(int) && i < offsetof(ResourceError, tlsVerificationErrorDescription)) continue;
        Require(bytes[i] == 0);
    }
    Require(!error.domain && !error.localizedDescription && !error.tlsVerificationErrorDescription);
    Require(!error.failingURL.impl && !error.failingURL.bits && !error.failingURL.queryEnd);
    Require(!error.errorCode && !error.isSanitized && !error.tlsVerificationError);
}

void Check(ErrorFunction function, ResourceErrorType type) {
    int client = 0;
    int argument = 0;
    ResourceError error;
    std::memset(&error, 0xa5, sizeof(error));
    error = function(&client, &argument);
    CheckEmpty(error, type);
    CheckEmpty(function(nullptr, nullptr), type);
}

}

int main() {
    Require(static_cast<int>(ResourceErrorType::Null) == 0);
    Require(static_cast<int>(ResourceErrorType::General) == 1);
    Require(static_cast<int>(ResourceErrorType::AccessControl) == 2);
    Require(static_cast<int>(ResourceErrorType::Cancellation) == 3);
    Require(static_cast<int>(ResourceErrorType::Timeout) == 4);
    Check(_ZNK7WebCore22EmptyFrameLoaderClient14cancelledErrorERKNS_15ResourceRequestE, ResourceErrorType::Cancellation);
    Check(_ZNK7WebCore22EmptyFrameLoaderClient12blockedErrorERKNS_15ResourceRequestE, ResourceErrorType::Null);
    Check(_ZNK7WebCore22EmptyFrameLoaderClient18cannotShowURLErrorERKNS_15ResourceRequestE, ResourceErrorType::Null);
    Check(_ZNK7WebCore22EmptyFrameLoaderClient21fileDoesNotExistErrorERKNS_16ResourceResponseE, ResourceErrorType::Null);
    Check(_ZNK7WebCore22EmptyFrameLoaderClient23cannotShowMIMETypeErrorERKNS_16ResourceResponseE, ResourceErrorType::Null);
    Check(_ZNK7WebCore22EmptyFrameLoaderClient25pluginWillHandleLoadErrorERKNS_16ResourceResponseE, ResourceErrorType::Null);
    Check(_ZNK7WebCore22EmptyFrameLoaderClient28blockedByContentBlockerErrorERKNS_15ResourceRequestE, ResourceErrorType::Null);
    Check(_ZNK7WebCore22EmptyFrameLoaderClient31interruptedForPolicyChangeErrorERKNS_15ResourceRequestE, ResourceErrorType::Null);
    return 0;
}
