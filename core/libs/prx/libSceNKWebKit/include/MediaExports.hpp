#pragma once

#include "SceTypes.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/src/MediaTypes.hpp"
#include <cstddef>
#include <cstdint>

extern "C" {

WTF::MediaTime APS5_VABI _ZN3WTF3absERKNS_9MediaTimeE(const WTF::MediaTime& rhs);
const WTF::MediaTime& APS5_VABI _ZN3WTF9MediaTime11invalidTimeEv();
void APS5_VABI _ZN3WTF9MediaTime12setTimeScaleEjNS0_13RoundingFlagsE(WTF::MediaTime* self, uint32_t scale, WTF::MediaTime::RoundingFlags flags);
const WTF::MediaTime& APS5_VABI _ZN3WTF9MediaTime14indefiniteTimeEv();
WTF::MediaTime APS5_VABI _ZN3WTF9MediaTime15createWithFloatEf(float floatTime);
WTF::MediaTime APS5_VABI _ZN3WTF9MediaTime15createWithFloatEfj(float floatTime, uint32_t timeScale);
WTF::MediaTime APS5_VABI _ZN3WTF9MediaTime16createWithDoubleEdj(double doubleTime, uint32_t timeScale);
WTF::MediaTime APS5_VABI _ZN3WTF9MediaTime16createWithDoubleEd(double doubleTime);
const WTF::MediaTime& APS5_VABI _ZN3WTF9MediaTime20negativeInfiniteTimeEv();
const WTF::MediaTime& APS5_VABI _ZN3WTF9MediaTime20positiveInfiniteTimeEv();
const WTF::MediaTime& APS5_VABI _ZN3WTF9MediaTime8zeroTimeEv();
void APS5_VABI _ZN3WTF9MediaTimeC1ERKS0_(WTF::MediaTime* self, const WTF::MediaTime& rhs);
void APS5_VABI _ZN3WTF9MediaTimeC1Eljh(WTF::MediaTime* self, int64_t value, uint32_t scale, uint8_t flags);
void APS5_VABI _ZN3WTF9MediaTimeC1Ev(WTF::MediaTime* self);
void APS5_VABI _ZN3WTF9MediaTimeC2ERKS0_(WTF::MediaTime* self, const WTF::MediaTime& rhs);
void APS5_VABI _ZN3WTF9MediaTimeC2Eljh(WTF::MediaTime* self, int64_t value, uint32_t scale, uint8_t flags);
void APS5_VABI _ZN3WTF9MediaTimeC2Ev(WTF::MediaTime* self);
WTF::MediaTime& APS5_VABI _ZN3WTF9MediaTimeaSERKS0_(WTF::MediaTime* self, const WTF::MediaTime& rhs);
WTF::MediaTime& APS5_VABI _ZN3WTF9MediaTimemIERKS0_(WTF::MediaTime* self, const WTF::MediaTime& rhs);
void* APS5_VABI _ZN3WTF9MediaTimenaEmPv(std::size_t size, void* ptr);
void* APS5_VABI _ZN3WTF9MediaTimenwEm10NotNullTagPv(std::size_t size, WTF::NotNullTag, void* ptr);
void* APS5_VABI _ZN3WTF9MediaTimenwEmPv(std::size_t size, void* ptr);
WTF::MediaTime& APS5_VABI _ZN3WTF9MediaTimepLERKS0_(WTF::MediaTime* self, const WTF::MediaTime& rhs);
void APS5_VABI _ZN7WebCore10TimeRanges13intersectWithERKS0_(WebCore::TimeRanges* self, const WebCore::TimeRanges& other);
void APS5_VABI _ZN7WebCore10TimeRanges3addEdd(WebCore::TimeRanges* self, double start, double end);
WebCore::Ref<WebCore::TimeRanges> APS5_VABI _ZN7WebCore10TimeRanges6createEdd(double start, double end);
WebCore::Ref<WebCore::TimeRanges> APS5_VABI _ZN7WebCore10TimeRanges6createEv();
void APS5_VABI _ZN7WebCore10TimeRangesC1Edd(WebCore::TimeRanges* self, double start, double end);
void APS5_VABI _ZN7WebCore10TimeRangesC1Ev(WebCore::TimeRanges* self);
void APS5_VABI _ZN7WebCore10TimeRangesC2Edd(WebCore::TimeRanges* self, double start, double end);
void APS5_VABI _ZN7WebCore10TimeRangesC2Ev(WebCore::TimeRanges* self);
void APS5_VABI _ZN7WebCore18PlatformTimeRanges13intersectWithERKS0_(WebCore::PlatformTimeRanges* self, const WebCore::PlatformTimeRanges& other);
void APS5_VABI _ZN7WebCore18PlatformTimeRanges3addERKN3WTF9MediaTimeES4_(WebCore::PlatformTimeRanges* self, const WTF::MediaTime& start, const WTF::MediaTime& end);
void APS5_VABI _ZN7WebCore18PlatformTimeRanges5clearEv(WebCore::PlatformTimeRanges* self);
void APS5_VABI _ZN7WebCore18PlatformTimeRanges6invertEv(WebCore::PlatformTimeRanges* self);
void APS5_VABI _ZN7WebCore18PlatformTimeRanges9unionWithERKS0_(WebCore::PlatformTimeRanges* self, const WebCore::PlatformTimeRanges& other);
void APS5_VABI _ZN7WebCore18PlatformTimeRangesC1EON3WTF6VectorINS0_5RangeELm0ENS1_15CrashOnOverflowELm16ENS1_10FastMallocEEE(WebCore::PlatformTimeRanges* self, WebCore::PlatformTimeRanges::VectorRange* ranges);
void APS5_VABI _ZN7WebCore18PlatformTimeRangesC1ERKN3WTF9MediaTimeES4_(WebCore::PlatformTimeRanges* self, const WTF::MediaTime& start, const WTF::MediaTime& end);
void APS5_VABI _ZN7WebCore18PlatformTimeRangesC1ERKS0_(WebCore::PlatformTimeRanges* self, const WebCore::PlatformTimeRanges& other);
void APS5_VABI _ZN7WebCore18PlatformTimeRangesC1Ev(WebCore::PlatformTimeRanges* self);
void APS5_VABI _ZN7WebCore18PlatformTimeRangesC2EON3WTF6VectorINS0_5RangeELm0ENS1_15CrashOnOverflowELm16ENS1_10FastMallocEEE(WebCore::PlatformTimeRanges* self, WebCore::PlatformTimeRanges::VectorRange* ranges);
void APS5_VABI _ZN7WebCore18PlatformTimeRangesC2ERKN3WTF9MediaTimeES4_(WebCore::PlatformTimeRanges* self, const WTF::MediaTime& start, const WTF::MediaTime& end);
void APS5_VABI _ZN7WebCore18PlatformTimeRangesC2ERKS0_(WebCore::PlatformTimeRanges* self, const WebCore::PlatformTimeRanges& other);
void APS5_VABI _ZN7WebCore18PlatformTimeRangesC2Ev(WebCore::PlatformTimeRanges* self);
void APS5_VABI _ZN7WebCore18PlatformTimeRangesD1Ev(WebCore::PlatformTimeRanges* self);
void APS5_VABI _ZN7WebCore18PlatformTimeRangesD2Ev(WebCore::PlatformTimeRanges* self);
WebCore::PlatformTimeRanges& APS5_VABI _ZN7WebCore18PlatformTimeRangesaSERKS0_(WebCore::PlatformTimeRanges* self, const WebCore::PlatformTimeRanges& other);
void* APS5_VABI _ZN7WebCore18PlatformTimeRangesnaEmPv(std::size_t size, void* ptr);
void* APS5_VABI _ZN7WebCore18PlatformTimeRangesnwEm10NotNullTagPv(std::size_t size, WTF::NotNullTag, void* ptr);
void* APS5_VABI _ZN7WebCore18PlatformTimeRangesnwEmPv(std::size_t size, void* ptr);
WTF::MediaTime APS5_VABI _ZNK3WTF9MediaTime11toTimeScaleEjNS0_13RoundingFlagsE(const WTF::MediaTime* self, uint32_t scale, WTF::MediaTime::RoundingFlags flags);
bool APS5_VABI _ZNK3WTF9MediaTime12isIndefiniteEv(const WTF::MediaTime* self);
bool APS5_VABI _ZNK3WTF9MediaTime14hasBeenRoundedEv(const WTF::MediaTime* self);
bool APS5_VABI _ZNK3WTF9MediaTime14hasDoubleValueEv(const WTF::MediaTime* self);
bool APS5_VABI _ZNK3WTF9MediaTime18isNegativeInfiniteEv(const WTF::MediaTime* self);
bool APS5_VABI _ZNK3WTF9MediaTime18isPositiveInfiniteEv(const WTF::MediaTime* self);
WTF::MediaTime::ComparisonFlags APS5_VABI _ZNK3WTF9MediaTime7compareERKS0_(const WTF::MediaTime* self, const WTF::MediaTime& rhs);
bool APS5_VABI _ZNK3WTF9MediaTime7isValidEv(const WTF::MediaTime* self);
float APS5_VABI _ZNK3WTF9MediaTime7toFloatEv(const WTF::MediaTime* self);
double APS5_VABI _ZNK3WTF9MediaTime8toDoubleEv(const WTF::MediaTime* self);
bool APS5_VABI _ZNK3WTF9MediaTime9isBetweenERKS0_S2_(const WTF::MediaTime* self, const WTF::MediaTime& a, const WTF::MediaTime& b);
bool APS5_VABI _ZNK3WTF9MediaTime9isInvalidEv(const WTF::MediaTime* self);
uint8_t APS5_VABI _ZNK3WTF9MediaTime9timeFlagsEv(const WTF::MediaTime* self);
const uint32_t& APS5_VABI _ZNK3WTF9MediaTime9timeScaleEv(const WTF::MediaTime* self);
const int64_t& APS5_VABI _ZNK3WTF9MediaTime9timeValueEv(const WTF::MediaTime* self);
bool APS5_VABI _ZNK3WTF9MediaTimecvbEv(const WTF::MediaTime* self);
bool APS5_VABI _ZNK3WTF9MediaTimeeqERKS0_(const WTF::MediaTime* self, const WTF::MediaTime& rhs);
bool APS5_VABI _ZNK3WTF9MediaTimegeERKS0_(const WTF::MediaTime* self, const WTF::MediaTime& rhs);
bool APS5_VABI _ZNK3WTF9MediaTimegtERKS0_(const WTF::MediaTime* self, const WTF::MediaTime& rhs);
bool APS5_VABI _ZNK3WTF9MediaTimeleERKS0_(const WTF::MediaTime* self, const WTF::MediaTime& rhs);
bool APS5_VABI _ZNK3WTF9MediaTimeltERKS0_(const WTF::MediaTime* self, const WTF::MediaTime& rhs);
WTF::MediaTime APS5_VABI _ZNK3WTF9MediaTimemiERKS0_(const WTF::MediaTime* self, const WTF::MediaTime& rhs);
WTF::MediaTime APS5_VABI _ZNK3WTF9MediaTimemlEi(const WTF::MediaTime* self, int32_t rhs);
WTF::MediaTime APS5_VABI _ZNK3WTF9MediaTimengEv(const WTF::MediaTime* self);
bool APS5_VABI _ZNK3WTF9MediaTimentEv(const WTF::MediaTime* self);
WTF::MediaTime APS5_VABI _ZNK3WTF9MediaTimeplERKS0_(const WTF::MediaTime* self, const WTF::MediaTime& rhs);
WebCore::ExceptionOr<double> APS5_VABI _ZNK7WebCore10TimeRanges3endEj(const WebCore::TimeRanges* self, unsigned int index);
WebCore::Ref<WebCore::TimeRanges> APS5_VABI _ZNK7WebCore10TimeRanges4copyEv(const WebCore::TimeRanges* self);
WebCore::ExceptionOr<double> APS5_VABI _ZNK7WebCore10TimeRanges5startEj(const WebCore::TimeRanges* self, unsigned int index);
unsigned int APS5_VABI _ZNK7WebCore10TimeRanges6lengthEv(const WebCore::TimeRanges* self);
double APS5_VABI _ZNK7WebCore10TimeRanges7nearestEd(const WebCore::TimeRanges* self, double time);
WTF::MediaTime APS5_VABI _ZNK7WebCore18PlatformTimeRanges13totalDurationEv(const WebCore::PlatformTimeRanges* self);
WTF::MediaTime APS5_VABI _ZNK7WebCore18PlatformTimeRanges19maximumBufferedTimeEv(const WebCore::PlatformTimeRanges* self);
WTF::MediaTime APS5_VABI _ZNK7WebCore18PlatformTimeRanges3endEj(const WebCore::PlatformTimeRanges* self, unsigned int index);
WTF::MediaTime APS5_VABI _ZNK7WebCore18PlatformTimeRanges3endEjRb(const WebCore::PlatformTimeRanges* self, unsigned int index, bool& valid);
std::size_t APS5_VABI _ZNK7WebCore18PlatformTimeRanges4findERKN3WTF9MediaTimeE(const WebCore::PlatformTimeRanges* self, const WTF::MediaTime& time);
WTF::MediaTime APS5_VABI _ZNK7WebCore18PlatformTimeRanges5startEj(const WebCore::PlatformTimeRanges* self, unsigned int index);
WTF::MediaTime APS5_VABI _ZNK7WebCore18PlatformTimeRanges5startEjRb(const WebCore::PlatformTimeRanges* self, unsigned int index, bool& valid);
unsigned int APS5_VABI _ZNK7WebCore18PlatformTimeRanges6lengthEv(const WebCore::PlatformTimeRanges* self);
bool APS5_VABI _ZNK7WebCore18PlatformTimeRanges7containERKN3WTF9MediaTimeE(const WebCore::PlatformTimeRanges* self, const WTF::MediaTime& time);
WTF::MediaTime APS5_VABI _ZNK7WebCore18PlatformTimeRanges7nearestERKN3WTF9MediaTimeE(const WebCore::PlatformTimeRanges* self, const WTF::MediaTime& time);
WTF::MediaTime APS5_VABI _ZNK7WebCore18PlatformTimeRanges8durationEj(const WebCore::PlatformTimeRanges* self, unsigned int index);

}
