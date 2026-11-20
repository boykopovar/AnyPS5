#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/include/EmptyLoaderErrorExports.hpp"

using NKWebKit::WebCoreAbi::EmptyResourceError;
using NKWebKit::WebCoreAbi::ResourceError;
using NKWebKit::WebCoreAbi::ResourceErrorType;

extern "C" {

ResourceError APS5_VABI _ZNK7WebCore22EmptyFrameLoaderClient12blockedErrorERKNS_15ResourceRequestE(const void*, const void*) {
    return EmptyResourceError(ResourceErrorType::Null);
}

ResourceError APS5_VABI _ZNK7WebCore22EmptyFrameLoaderClient14cancelledErrorERKNS_15ResourceRequestE(const void*, const void*) {
    return EmptyResourceError(ResourceErrorType::Cancellation);
}

ResourceError APS5_VABI _ZNK7WebCore22EmptyFrameLoaderClient18cannotShowURLErrorERKNS_15ResourceRequestE(const void*, const void*) {
    return EmptyResourceError(ResourceErrorType::Null);
}

ResourceError APS5_VABI _ZNK7WebCore22EmptyFrameLoaderClient21fileDoesNotExistErrorERKNS_16ResourceResponseE(const void*, const void*) {
    return EmptyResourceError(ResourceErrorType::Null);
}

ResourceError APS5_VABI _ZNK7WebCore22EmptyFrameLoaderClient23cannotShowMIMETypeErrorERKNS_16ResourceResponseE(const void*, const void*) {
    return EmptyResourceError(ResourceErrorType::Null);
}

ResourceError APS5_VABI _ZNK7WebCore22EmptyFrameLoaderClient25pluginWillHandleLoadErrorERKNS_16ResourceResponseE(const void*, const void*) {
    return EmptyResourceError(ResourceErrorType::Null);
}

ResourceError APS5_VABI _ZNK7WebCore22EmptyFrameLoaderClient28blockedByContentBlockerErrorERKNS_15ResourceRequestE(const void*, const void*) {
    return EmptyResourceError(ResourceErrorType::Null);
}

ResourceError APS5_VABI _ZNK7WebCore22EmptyFrameLoaderClient31interruptedForPolicyChangeErrorERKNS_15ResourceRequestE(const void*, const void*) {
    return EmptyResourceError(ResourceErrorType::Null);
}

}
