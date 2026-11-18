#pragma once

#include "SceTypes.hpp"

#include <cstddef>

namespace NKWebKit::WebCoreAbi {

struct PlatformStrategies;
struct MessagePortChannelProvider;
struct WebAnimation;

struct WebAnimationHashSet {
    WebAnimation** table;
};

static_assert(sizeof(WebAnimationHashSet) == 8);
static_assert(alignof(WebAnimationHashSet) == 8);
static_assert(offsetof(WebAnimationHashSet, table) == 0);

bool ResourceHandleShouldForceContentSniffing();

}

extern "C" {

NKWebKit::WebCoreAbi::PlatformStrategies* APS5_VABI _ZN7WebCore18platformStrategiesEv();
void APS5_VABI _ZN7WebCore21setPlatformStrategiesEPNS_18PlatformStrategiesE(NKWebKit::WebCoreAbi::PlatformStrategies* platformStrategies);
NKWebKit::WebCoreAbi::WebAnimationHashSet* APS5_VABI _ZN7WebCore12WebAnimation9instancesEv();
void APS5_VABI _ZN7WebCore26MessagePortChannelProvider17setSharedProviderERS0_(NKWebKit::WebCoreAbi::MessagePortChannelProvider& provider);
void APS5_VABI _ZN7WebCore14ResourceHandle20forceContentSniffingEv();

}
