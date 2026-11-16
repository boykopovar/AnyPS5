#pragma once

#include "SceTypes.hpp"
#include "prx/libSceNKWebKit/src/WtfStringAbi.hpp"
#include "prx/libSceNKWebKit/src/WtfTimeTypes.hpp"

#include <cstddef>
#include <cstdint>

namespace NKWebKit::WebCoreAbi {

struct RegistrableDomain {
    NKWebKit::WtfAbi::String string;
};

struct ResourceLoadStatistics {
    RegistrableDomain registrableDomain;
    WTF::WallTime lastSeen;
    bool hadUserInteraction;
    WTF::WallTime mostRecentUserInteractionTime;
    bool grandfathered;
    void* storageAccessUnderTopFrameDomains;
    void* topFrameUniqueRedirectsTo;
    void* topFrameUniqueRedirectsToSinceSameSiteStrictEnforcement;
    void* topFrameUniqueRedirectsFrom;
    void* topFrameLinkDecorationsFrom;
    bool gotLinkDecorationFromPrevalentResource;
    void* topFrameLoadedThirdPartyScripts;
    void* subframeUnderTopFrameDomains;
    void* subresourceUnderTopFrameDomains;
    void* subresourceUniqueRedirectsTo;
    void* subresourceUniqueRedirectsFrom;
    bool isPrevalentResource;
    bool isVeryPrevalentResource;
    std::uint32_t dataRecordsRemoved;
    std::uint32_t timesAccessedAsFirstPartyDueToUserInteraction;
    std::uint32_t timesAccessedAsFirstPartyDueToStorageAccessAPI;
};

struct KeyedEncoderVtable {
    void (APS5_VABI* completeDestructor)(void*);
    void (APS5_VABI* deletingDestructor)(void*);
    void (APS5_VABI* encodeBytes)(void*, const NKWebKit::WtfAbi::String*, const std::uint8_t*, std::size_t);
    void (APS5_VABI* encodeBool)(void*, const NKWebKit::WtfAbi::String*, bool);
    void (APS5_VABI* encodeUInt32)(void*, const NKWebKit::WtfAbi::String*, std::uint32_t);
    void (APS5_VABI* encodeUInt64)(void*, const NKWebKit::WtfAbi::String*, std::uint64_t);
    void (APS5_VABI* encodeInt32)(void*, const NKWebKit::WtfAbi::String*, std::int32_t);
    void (APS5_VABI* encodeInt64)(void*, const NKWebKit::WtfAbi::String*, std::int64_t);
    void (APS5_VABI* encodeFloat)(void*, const NKWebKit::WtfAbi::String*, float);
    void (APS5_VABI* encodeDouble)(void*, const NKWebKit::WtfAbi::String*, double);
    void (APS5_VABI* encodeString)(void*, const NKWebKit::WtfAbi::String*, const NKWebKit::WtfAbi::String*);
    void* finishEncoding;
    void (APS5_VABI* beginObject)(void*, const NKWebKit::WtfAbi::String*);
    void (APS5_VABI* endObject)(void*);
    void (APS5_VABI* beginArray)(void*, const NKWebKit::WtfAbi::String*);
    void (APS5_VABI* beginArrayElement)(void*);
    void (APS5_VABI* endArrayElement)(void*);
    void (APS5_VABI* endArray)(void*);
};

struct KeyedEncoder {
    const KeyedEncoderVtable* vtable;
};

struct KeyedDecoderVtable {
    void (APS5_VABI* completeDestructor)(void*);
    void (APS5_VABI* deletingDestructor)(void*);
    bool (APS5_VABI* decodeBytes)(void*, const NKWebKit::WtfAbi::String*, const std::uint8_t**, std::size_t*);
    bool (APS5_VABI* decodeBool)(void*, const NKWebKit::WtfAbi::String*, bool*);
    bool (APS5_VABI* decodeUInt32)(void*, const NKWebKit::WtfAbi::String*, std::uint32_t*);
    bool (APS5_VABI* decodeUInt64)(void*, const NKWebKit::WtfAbi::String*, std::uint64_t*);
    bool (APS5_VABI* decodeInt32)(void*, const NKWebKit::WtfAbi::String*, std::int32_t*);
    bool (APS5_VABI* decodeInt64)(void*, const NKWebKit::WtfAbi::String*, std::int64_t*);
    bool (APS5_VABI* decodeFloat)(void*, const NKWebKit::WtfAbi::String*, float*);
    bool (APS5_VABI* decodeDouble)(void*, const NKWebKit::WtfAbi::String*, double*);
    bool (APS5_VABI* decodeString)(void*, const NKWebKit::WtfAbi::String*, NKWebKit::WtfAbi::String*);
    bool (APS5_VABI* beginObject)(void*, const NKWebKit::WtfAbi::String*);
    void (APS5_VABI* endObject)(void*);
    bool (APS5_VABI* beginArray)(void*, const NKWebKit::WtfAbi::String*);
    bool (APS5_VABI* beginArrayElement)(void*);
    void (APS5_VABI* endArrayElement)(void*);
    void (APS5_VABI* endArray)(void*);
};

struct KeyedDecoder {
    const KeyedDecoderVtable* vtable;
};

static_assert(sizeof(RegistrableDomain) == 8);
static_assert(alignof(RegistrableDomain) == 8);
static_assert(offsetof(RegistrableDomain, string) == 0);
static_assert(sizeof(ResourceLoadStatistics) == 144);
static_assert(alignof(ResourceLoadStatistics) == 8);
static_assert(offsetof(ResourceLoadStatistics, registrableDomain) == 0);
static_assert(offsetof(ResourceLoadStatistics, lastSeen) == 8);
static_assert(offsetof(ResourceLoadStatistics, hadUserInteraction) == 16);
static_assert(offsetof(ResourceLoadStatistics, mostRecentUserInteractionTime) == 24);
static_assert(offsetof(ResourceLoadStatistics, grandfathered) == 32);
static_assert(offsetof(ResourceLoadStatistics, storageAccessUnderTopFrameDomains) == 40);
static_assert(offsetof(ResourceLoadStatistics, topFrameUniqueRedirectsTo) == 48);
static_assert(offsetof(ResourceLoadStatistics, topFrameUniqueRedirectsToSinceSameSiteStrictEnforcement) == 56);
static_assert(offsetof(ResourceLoadStatistics, topFrameUniqueRedirectsFrom) == 64);
static_assert(offsetof(ResourceLoadStatistics, topFrameLinkDecorationsFrom) == 72);
static_assert(offsetof(ResourceLoadStatistics, gotLinkDecorationFromPrevalentResource) == 80);
static_assert(offsetof(ResourceLoadStatistics, topFrameLoadedThirdPartyScripts) == 88);
static_assert(offsetof(ResourceLoadStatistics, subframeUnderTopFrameDomains) == 96);
static_assert(offsetof(ResourceLoadStatistics, subresourceUnderTopFrameDomains) == 104);
static_assert(offsetof(ResourceLoadStatistics, subresourceUniqueRedirectsTo) == 112);
static_assert(offsetof(ResourceLoadStatistics, subresourceUniqueRedirectsFrom) == 120);
static_assert(offsetof(ResourceLoadStatistics, isPrevalentResource) == 128);
static_assert(offsetof(ResourceLoadStatistics, isVeryPrevalentResource) == 129);
static_assert(offsetof(ResourceLoadStatistics, dataRecordsRemoved) == 132);
static_assert(offsetof(ResourceLoadStatistics, timesAccessedAsFirstPartyDueToUserInteraction) == 136);
static_assert(offsetof(ResourceLoadStatistics, timesAccessedAsFirstPartyDueToStorageAccessAPI) == 140);
static_assert(sizeof(KeyedEncoder) == 8);
static_assert(offsetof(KeyedEncoder, vtable) == 0);
static_assert(sizeof(KeyedDecoder) == 8);
static_assert(offsetof(KeyedDecoder, vtable) == 0);

}

extern "C" {

void APS5_VABI _ZN7WebCore22ResourceLoadStatistics5mergeERKS0_(NKWebKit::WebCoreAbi::ResourceLoadStatistics* self, const NKWebKit::WebCoreAbi::ResourceLoadStatistics* other);
bool APS5_VABI _ZN7WebCore22ResourceLoadStatistics6decodeERNS_12KeyedDecoderEj(NKWebKit::WebCoreAbi::ResourceLoadStatistics* self, NKWebKit::WebCoreAbi::KeyedDecoder* decoder, unsigned modelVersion);
void APS5_VABI _ZNK7WebCore22ResourceLoadStatistics6encodeERNS_12KeyedEncoderE(const NKWebKit::WebCoreAbi::ResourceLoadStatistics* self, NKWebKit::WebCoreAbi::KeyedEncoder* encoder);
NKWebKit::WtfAbi::String APS5_VABI _ZNK7WebCore22ResourceLoadStatistics8toStringEv(const NKWebKit::WebCoreAbi::ResourceLoadStatistics* self);

}
