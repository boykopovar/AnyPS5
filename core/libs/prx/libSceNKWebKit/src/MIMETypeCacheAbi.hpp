#pragma once

#include "SceTypes.hpp"
#include "prx/libSceNKWebKit/src/ContentTypeAbi.hpp"
#include "prx/libSceNKWebKit/src/WtfStringAbi.hpp"

#include <cstddef>
#include <cstdint>

namespace NKWebKit::WebCoreAbi {

struct MIMETypeCache;

struct MIMETypeSet {
    WtfAbi::StringImpl** table;
};

struct MIMETypeResult {
    WtfAbi::StringImpl* key;
    std::uint8_t value;
};

struct MIMETypeResultMap {
    MIMETypeResult* table;
};

struct MIMETypeCacheVtable {
    void (APS5_VABI* completeDestructor)(MIMETypeCache* self);
    void (APS5_VABI* deletingDestructor)(MIMETypeCache* self);
    bool (APS5_VABI* isAvailable)(const MIMETypeCache* self);
    std::uint8_t (APS5_VABI* canDecodeType)(MIMETypeCache* self, const WtfAbi::String* mimeType);
    MIMETypeSet* (APS5_VABI* supportedTypes)(MIMETypeCache* self);
    bool (APS5_VABI* isStaticContainerType)(MIMETypeCache* self, WtfAbi::StringView type);
    bool (APS5_VABI* isUnsupportedContainerType)(MIMETypeCache* self, const WtfAbi::String* type);
    void (APS5_VABI* initializeCache)(MIMETypeCache* self, MIMETypeSet* types);
    bool (APS5_VABI* canDecodeExtendedType)(MIMETypeCache* self, const ContentType* type);
};

struct MIMETypeCacheVtableStorage {
    std::ptrdiff_t offsetToTop;
    const void* typeInfo;
    MIMETypeCacheVtable dispatch;
};

struct MIMETypeCache {
    const MIMETypeCacheVtable* vtable;
    MIMETypeSet supportedTypes;
    bool hasSupportedTypes;
    MIMETypeResultMap cachedResults;
    bool hasCachedResults;
};

constexpr std::uint8_t kMIMETypeIsNotSupported = 0;
constexpr std::uint8_t kMIMETypeIsSupported = 1;
constexpr std::uint8_t kMIMETypeMayBeSupported = 2;

static_assert(sizeof(MIMETypeResult) == 16);
static_assert(offsetof(MIMETypeResult, value) == 8);
static_assert(sizeof(MIMETypeCacheVtable) == 9 * sizeof(void*));
static_assert(offsetof(MIMETypeCacheVtable, isAvailable) == 16);
static_assert(offsetof(MIMETypeCacheVtable, canDecodeType) == 24);
static_assert(offsetof(MIMETypeCacheVtable, supportedTypes) == 32);
static_assert(offsetof(MIMETypeCacheVtable, isStaticContainerType) == 40);
static_assert(offsetof(MIMETypeCacheVtable, isUnsupportedContainerType) == 48);
static_assert(offsetof(MIMETypeCacheVtable, initializeCache) == 56);
static_assert(offsetof(MIMETypeCacheVtable, canDecodeExtendedType) == 64);
static_assert(offsetof(MIMETypeCacheVtableStorage, dispatch) == 16);
static_assert(sizeof(MIMETypeCache) == 40);
static_assert(offsetof(MIMETypeCache, supportedTypes) == 8);
static_assert(offsetof(MIMETypeCache, hasSupportedTypes) == 16);
static_assert(offsetof(MIMETypeCache, cachedResults) == 24);
static_assert(offsetof(MIMETypeCache, hasCachedResults) == 32);

}  // namespace NKWebKit::WebCoreAbi
