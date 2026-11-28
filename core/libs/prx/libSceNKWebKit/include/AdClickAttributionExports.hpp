#pragma once

#include "SceTypes.hpp"
#include "prx/libSceNKWebKit/src/WtfStringAbi.hpp"

#include <cstddef>
#include <cstdint>

namespace NKWebKit::AdClickAbi {

struct Conversion {
    std::uint32_t data;
    std::uint32_t priority;
    bool wasSent;
};

struct OptionalConversion {
    bool engaged;
    Conversion value;
};

struct OptionalWallTime {
    bool engaged;
    double value;
};

struct OptionalSeconds {
    bool engaged;
    double value;
};

struct ExpectedConversion {
    union {
        Conversion value;
        NKWebKit::WtfAbi::StringImpl* error;
    };
    bool has;
};

struct AdClickAttribution {
    std::uint32_t campaignId;
    NKWebKit::WtfAbi::StringImpl* source;
    NKWebKit::WtfAbi::StringImpl* destination;
    double timeOfAdClick;
    OptionalConversion conversion;
    OptionalWallTime earliestTimeToSend;
};

static_assert(sizeof(Conversion) == 12);
static_assert(offsetof(Conversion, priority) == 4);
static_assert(offsetof(Conversion, wasSent) == 8);
static_assert(sizeof(OptionalConversion) == 16);
static_assert(offsetof(OptionalConversion, value) == 4);
static_assert(sizeof(OptionalWallTime) == 16);
static_assert(offsetof(OptionalWallTime, value) == 8);
static_assert(sizeof(OptionalSeconds) == 16);
static_assert(offsetof(OptionalSeconds, value) == 8);
static_assert(sizeof(ExpectedConversion) == 24);
static_assert(offsetof(ExpectedConversion, value) == 0);
static_assert(offsetof(ExpectedConversion, error) == 0);
static_assert(offsetof(ExpectedConversion, has) == 16);
static_assert(sizeof(AdClickAttribution) == 64);
static_assert(offsetof(AdClickAttribution, source) == 8);
static_assert(offsetof(AdClickAttribution, destination) == 16);
static_assert(offsetof(AdClickAttribution, timeOfAdClick) == 24);
static_assert(offsetof(AdClickAttribution, conversion) == 32);
static_assert(offsetof(AdClickAttribution, earliestTimeToSend) == 48);

}

extern "C" {

NKWebKit::AdClickAbi::ExpectedConversion* APS5_VABI _ZN7WebCore18AdClickAttribution22parseConversionRequestERKN3WTF3URLE(NKWebKit::AdClickAbi::ExpectedConversion* result, const NKWebKit::WtfAbi::URL* redirectURL);
NKWebKit::AdClickAbi::OptionalSeconds* APS5_VABI _ZN7WebCore18AdClickAttribution31convertAndGetEarliestTimeToSendEONS0_10ConversionE(NKWebKit::AdClickAbi::OptionalSeconds* result, NKWebKit::AdClickAbi::AdClickAttribution* self, const NKWebKit::AdClickAbi::Conversion* conversion);
bool APS5_VABI _ZNK7WebCore18AdClickAttribution21hasHigherPriorityThanERKS0_(const NKWebKit::AdClickAbi::AdClickAttribution* self, const NKWebKit::AdClickAbi::AdClickAttribution* other);
NKWebKit::WtfAbi::URL APS5_VABI _ZNK7WebCore18AdClickAttribution3urlEv(const NKWebKit::AdClickAbi::AdClickAttribution* self);
NKWebKit::WtfAbi::URL APS5_VABI _ZNK7WebCore18AdClickAttribution13urlForTestingERKN3WTF3URLE(const NKWebKit::AdClickAbi::AdClickAttribution* self, const NKWebKit::WtfAbi::URL* baseURL);
NKWebKit::WtfAbi::URL APS5_VABI _ZNK7WebCore18AdClickAttribution8referrerEv(const NKWebKit::AdClickAbi::AdClickAttribution* self);
void APS5_VABI _ZN7WebCore18AdClickAttribution13markAsExpiredEv(NKWebKit::AdClickAbi::AdClickAttribution* self);
bool APS5_VABI _ZNK7WebCore18AdClickAttribution10hasExpiredEv(const NKWebKit::AdClickAbi::AdClickAttribution* self);
void APS5_VABI _ZN7WebCore18AdClickAttribution20markConversionAsSentEv(NKWebKit::AdClickAbi::AdClickAttribution* self);
bool APS5_VABI _ZNK7WebCore18AdClickAttribution17wasConversionSentEv(const NKWebKit::AdClickAbi::AdClickAttribution* self);
NKWebKit::WtfAbi::String APS5_VABI _ZNK7WebCore18AdClickAttribution8toStringEv(const NKWebKit::AdClickAbi::AdClickAttribution* self);

}
