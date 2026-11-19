#pragma once

#include "SceTypes.hpp"
#include "prx/libSceNKWebKit/src/WtfStringAbi.hpp"

#include <cstddef>
#include <cstdint>

namespace NKWebKit::MimeTypeCacheAbi {

enum class SupportsType : std::uint8_t { IsNotSupported, IsSupported, MayBeSupported };

struct SupportsTypeBucket {
    WtfAbi::StringImpl* key;
    SupportsType value;
};

struct MIMETypeCache {
    const void* const* vtable;
    bool hasSupportedTypes;
    WtfAbi::StringImpl** supportedTypes;
    bool hasCachedResults;
    SupportsTypeBucket* cachedResults;
};

static_assert(sizeof(SupportsTypeBucket) == 16);
static_assert(sizeof(MIMETypeCache) == 0x28);
static_assert(offsetof(MIMETypeCache, hasSupportedTypes) == 0x8);
static_assert(offsetof(MIMETypeCache, supportedTypes) == 0x10);
static_assert(offsetof(MIMETypeCache, hasCachedResults) == 0x18);
static_assert(offsetof(MIMETypeCache, cachedResults) == 0x20);

}

extern "C" {

void APS5_VABI _ZN7WebCore13MIMETypeCacheD2Ev(NKWebKit::MimeTypeCacheAbi::MIMETypeCache* self);
void APS5_VABI _ZN7WebCore13MIMETypeCacheD0Ev(NKWebKit::MimeTypeCacheAbi::MIMETypeCache* self);
bool APS5_VABI _ZNK7WebCore13MIMETypeCache7isEmptyEv(const NKWebKit::MimeTypeCacheAbi::MIMETypeCache* self);

}
