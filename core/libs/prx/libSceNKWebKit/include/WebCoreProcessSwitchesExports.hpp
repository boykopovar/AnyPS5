#pragma once

#include "prx/libc/include/general/VabiMacros.hpp"

#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace NKWebKit::WebCoreAbi {

struct ResourceLoadObserver;

struct OptionalUInt64 {
    bool hasValue{};
    std::uint64_t value{};

    OptionalUInt64() = default;
    OptionalUInt64(bool engaged, std::uint64_t contents) : hasValue(engaged), value(contents) {}
    OptionalUInt64(const OptionalUInt64& other) : hasValue(other.hasValue), value(other.value) {}
    OptionalUInt64(OptionalUInt64&& other) noexcept : OptionalUInt64(other) {}
};

static_assert(sizeof(OptionalUInt64) == 16);
static_assert(alignof(OptionalUInt64) == 8);
static_assert(offsetof(OptionalUInt64, hasValue) == 0);
static_assert(offsetof(OptionalUInt64, value) == 8);
static_assert(!std::is_trivially_copy_constructible_v<OptionalUInt64>);
static_assert(!std::is_trivially_move_constructible_v<OptionalUInt64>);
static_assert(std::is_trivially_destructible_v<OptionalUInt64>);

bool MediaEngineConfigurationMockEnabled();

}

extern "C" {

bool APS5_VABI _ZN7WebCore21NetworkStorageSession22processMayUseCookieAPIEv();
void APS5_VABI _ZN7WebCore21NetworkStorageSession27permitProcessToUseCookieAPIEb(bool value);
void APS5_VABI _ZN7WebCore20ResourceLoadObserver9setSharedERS0_(NKWebKit::WebCoreAbi::ResourceLoadObserver& observer);
NKWebKit::WebCoreAbi::ResourceLoadObserver* APS5_VABI _ZN7WebCore20ResourceLoadObserver14sharedIfExistsEv();
void APS5_VABI _ZN7WebCore31MediaEngineConfigurationFactory10enableMockEv();
void APS5_VABI _ZN7WebCore31MediaEngineConfigurationFactory11disableMockEv();
NKWebKit::WebCoreAbi::OptionalUInt64 APS5_VABI _ZN7WebCore18PerformanceLogging17physicalFootprintEv();

}
