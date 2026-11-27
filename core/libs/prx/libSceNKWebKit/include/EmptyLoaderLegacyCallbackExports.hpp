#pragma once

#include "SceTypes.hpp"
#include "prx/libSceNKWebKit/src/WtfStringAbi.hpp"

#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace NKWebKit::WebCoreAbi {

struct WidgetRefPtr {
    void* ptr = nullptr;

    WidgetRefPtr() = default;
    WidgetRefPtr(const WidgetRefPtr& other) : ptr(other.ptr) {}
    ~WidgetRefPtr() {}
};

static_assert(sizeof(WidgetRefPtr) == 8);
static_assert(!std::is_trivially_copyable_v<WidgetRefPtr>);

struct OptionalIdentifier {
    bool engaged = false;
    std::uint64_t value = 0;

    OptionalIdentifier() = default;
    OptionalIdentifier(const OptionalIdentifier& other) : engaged(other.engaged), value(other.value) {}
};

static_assert(sizeof(OptionalIdentifier) == 16);
static_assert(offsetof(OptionalIdentifier, engaged) == 0);
static_assert(offsetof(OptionalIdentifier, value) == 8);
static_assert(!std::is_trivially_copyable_v<OptionalIdentifier>);

}

extern "C" {

void APS5_VABI _ZN7WebCore22EmptyFrameLoaderClient13committedLoadEPNS_14DocumentLoaderEPKci(void* self, void* loader, const char* data, int length);
void APS5_VABI _ZN7WebCore22EmptyFrameLoaderClient21dispatchDidCommitLoadEN3WTF8OptionalINS_18HasInsecureContentEEENS2_INS_13UsedLegacyTLSEEE(void* self);
void* APS5_VABI _ZN7WebCore22EmptyFrameLoaderClient18dispatchCreatePageERKNS_16NavigationActionE(void* self, const void* action);
NKWebKit::WtfAbi::String APS5_VABI _ZNK7WebCore22EmptyFrameLoaderClient29generatedMIMETypeForURLSchemeERKN3WTF6StringE(const void* self, const NKWebKit::WtfAbi::String* scheme);
NKWebKit::WebCoreAbi::OptionalIdentifier APS5_VABI _ZNK7WebCore22EmptyFrameLoaderClient6pageIDEv(const void* self);
NKWebKit::WebCoreAbi::OptionalIdentifier APS5_VABI _ZNK7WebCore22EmptyFrameLoaderClient7frameIDEv(const void* self);
NKWebKit::WebCoreAbi::WidgetRefPtr APS5_VABI _ZN7WebCore22EmptyFrameLoaderClient22createJavaAppletWidgetERKNS_7IntSizeERNS_17HTMLAppletElementERKN3WTF3URLERKNS6_6VectorINS6_6StringELm0ENS6_15CrashOnOverflowELm16ENS6_10FastMallocEEESG_(void* self, const void* size, void* element, const void* baseURL, const void* paramNames, const void* paramValues);

}
