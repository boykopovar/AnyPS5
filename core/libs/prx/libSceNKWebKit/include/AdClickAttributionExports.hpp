#pragma once

#include "SceTypes.hpp"
#include "prx/libSceNKWebKit/src/WtfStringAbi.hpp"
#include "prx/libSceNKWebKit/src/WtfTimeTypes.hpp"

#include <cstddef>
#include <cstdint>

namespace NKWebKit::WebCoreAbi {

struct AdClickAttributionConversion {
    std::uint32_t data;
    std::uint32_t priority;
    bool wasSent;
};

static_assert(sizeof(AdClickAttributionConversion) == 12);
static_assert(offsetof(AdClickAttributionConversion, wasSent) == 8);

struct AdClickAttributionOptionalConversion {
    bool hasValue;
    AdClickAttributionConversion value;
};

static_assert(sizeof(AdClickAttributionOptionalConversion) == 16);
static_assert(offsetof(AdClickAttributionOptionalConversion, value) == 4);

struct AdClickAttributionOptionalWallTime {
    bool hasValue;
    WTF::WallTime value;
};

static_assert(sizeof(AdClickAttributionOptionalWallTime) == 16);
static_assert(offsetof(AdClickAttributionOptionalWallTime, value) == 8);

struct AdClickAttribution {
    std::uint32_t campaign;
    WtfAbi::StringImpl* source;
    WtfAbi::StringImpl* destination;
    WTF::WallTime timeOfAdClick;
    AdClickAttributionOptionalConversion conversion;
    AdClickAttributionOptionalWallTime earliestTimeToSend;
};

static_assert(sizeof(AdClickAttribution) == 64);
static_assert(offsetof(AdClickAttribution, source) == 8);
static_assert(offsetof(AdClickAttribution, destination) == 16);
static_assert(offsetof(AdClickAttribution, timeOfAdClick) == 24);
static_assert(offsetof(AdClickAttribution, conversion) == 32);
static_assert(offsetof(AdClickAttribution, earliestTimeToSend) == 48);

}  // namespace NKWebKit::WebCoreAbi

extern "C" {

void APS5_VABI _ZN7WebCore18AdClickAttribution13markAsExpiredEv(NKWebKit::WebCoreAbi::AdClickAttribution* self);
bool APS5_VABI _ZNK7WebCore18AdClickAttribution10hasExpiredEv(const NKWebKit::WebCoreAbi::AdClickAttribution* self);
bool APS5_VABI _ZNK7WebCore18AdClickAttribution21hasHigherPriorityThanERKS0_(const NKWebKit::WebCoreAbi::AdClickAttribution* self, const NKWebKit::WebCoreAbi::AdClickAttribution* other);
void APS5_VABI _ZN7WebCore18AdClickAttribution20markConversionAsSentEv(NKWebKit::WebCoreAbi::AdClickAttribution* self);
bool APS5_VABI _ZNK7WebCore18AdClickAttribution17wasConversionSentEv(const NKWebKit::WebCoreAbi::AdClickAttribution* self);
NKWebKit::WtfAbi::String APS5_VABI _ZNK7WebCore18AdClickAttribution8toStringEv(const NKWebKit::WebCoreAbi::AdClickAttribution* self);

}
