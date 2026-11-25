#pragma once

#include "SceTypes.hpp"
#include "prx/libSceNKWebKit/src/ResourceErrorAbi.hpp"

extern "C" {

NKWebKit::WebCoreAbi::ResourceError APS5_VABI _ZNK7WebCore22EmptyFrameLoaderClient12blockedErrorERKNS_15ResourceRequestE(const void* self, const void* request);
NKWebKit::WebCoreAbi::ResourceError APS5_VABI _ZNK7WebCore22EmptyFrameLoaderClient14cancelledErrorERKNS_15ResourceRequestE(const void* self, const void* request);
NKWebKit::WebCoreAbi::ResourceError APS5_VABI _ZNK7WebCore22EmptyFrameLoaderClient18cannotShowURLErrorERKNS_15ResourceRequestE(const void* self, const void* request);
NKWebKit::WebCoreAbi::ResourceError APS5_VABI _ZNK7WebCore22EmptyFrameLoaderClient21fileDoesNotExistErrorERKNS_16ResourceResponseE(const void* self, const void* response);
NKWebKit::WebCoreAbi::ResourceError APS5_VABI _ZNK7WebCore22EmptyFrameLoaderClient23cannotShowMIMETypeErrorERKNS_16ResourceResponseE(const void* self, const void* response);
NKWebKit::WebCoreAbi::ResourceError APS5_VABI _ZNK7WebCore22EmptyFrameLoaderClient25pluginWillHandleLoadErrorERKNS_16ResourceResponseE(const void* self, const void* response);
NKWebKit::WebCoreAbi::ResourceError APS5_VABI _ZNK7WebCore22EmptyFrameLoaderClient28blockedByContentBlockerErrorERKNS_15ResourceRequestE(const void* self, const void* request);
NKWebKit::WebCoreAbi::ResourceError APS5_VABI _ZNK7WebCore22EmptyFrameLoaderClient31interruptedForPolicyChangeErrorERKNS_15ResourceRequestE(const void* self, const void* request);

}
