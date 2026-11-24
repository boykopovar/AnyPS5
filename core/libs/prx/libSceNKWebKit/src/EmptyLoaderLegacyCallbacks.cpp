#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/include/EmptyLoaderLegacyCallbackExports.hpp"

using NKWebKit::WebCoreAbi::OptionalIdentifier;
using NKWebKit::WebCoreAbi::WidgetRefPtr;
using NKWebKit::WtfAbi::String;

extern "C" {

void APS5_VABI _ZN7WebCore22EmptyFrameLoaderClient13committedLoadEPNS_14DocumentLoaderEPKci(void*, void*, const char*, int) {
}

void APS5_VABI _ZN7WebCore22EmptyFrameLoaderClient21dispatchDidCommitLoadEN3WTF8OptionalINS_18HasInsecureContentEEENS2_INS_13UsedLegacyTLSEEE(void*) {
}

void* APS5_VABI _ZN7WebCore22EmptyFrameLoaderClient18dispatchCreatePageERKNS_16NavigationActionE(void*, const void*) {
    return nullptr;
}

String APS5_VABI _ZNK7WebCore22EmptyFrameLoaderClient29generatedMIMETypeForURLSchemeERKN3WTF6StringE(const void*, const String*) {
    return String(NKWebKit::WtfAbi::EmptyStringImpl());
}

OptionalIdentifier APS5_VABI _ZNK7WebCore22EmptyFrameLoaderClient6pageIDEv(const void*) {
    return {};
}

OptionalIdentifier APS5_VABI _ZNK7WebCore22EmptyFrameLoaderClient7frameIDEv(const void*) {
    return {};
}

WidgetRefPtr APS5_VABI _ZN7WebCore22EmptyFrameLoaderClient22createJavaAppletWidgetERKNS_7IntSizeERNS_17HTMLAppletElementERKN3WTF3URLERKNS6_6VectorINS6_6StringELm0ENS6_15CrashOnOverflowELm16ENS6_10FastMallocEEESG_(void*, const void*, void*, const void*, const void*, const void*) {
    return {};
}

}
