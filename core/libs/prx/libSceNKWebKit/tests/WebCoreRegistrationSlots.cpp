#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <cstdio>
#include <cstdlib>
#include <source_location>
#include <type_traits>

namespace {

using NKWebKit::WebCoreAbi::LocalLoadPolicy;
using NKWebKit::WebCoreAbi::MessagePortChannelProvider;
using NKWebKit::WebCoreAbi::PlatformStrategies;
using NKWebKit::WebCoreAbi::WebAnimationHashSet;

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "WebCore registration slots check failed at line %u\n", location.line());
        std::abort();
    }
}

using GetPlatformStrategies = PlatformStrategies* (APS5_VABI *)();
using SetPlatformStrategies = void (APS5_VABI *)(PlatformStrategies*);
using Instances = WebAnimationHashSet* (APS5_VABI *)();
using SetSharedProvider = void (APS5_VABI *)(MessagePortChannelProvider&);
using ForceContentSniffing = void (APS5_VABI *)();
using SetLocalLoadPolicy = void (APS5_VABI *)(LocalLoadPolicy);
static_assert(std::is_same_v<decltype(&_ZN7WebCore18platformStrategiesEv), GetPlatformStrategies>);
static_assert(std::is_same_v<decltype(&_ZN7WebCore21setPlatformStrategiesEPNS_18PlatformStrategiesE), SetPlatformStrategies>);
static_assert(std::is_same_v<decltype(&_ZN7WebCore12WebAnimation9instancesEv), Instances>);
static_assert(std::is_same_v<decltype(&_ZN7WebCore26MessagePortChannelProvider17setSharedProviderERS0_), SetSharedProvider>);
static_assert(std::is_same_v<decltype(&_ZN7WebCore14ResourceHandle20forceContentSniffingEv), ForceContentSniffing>);
static_assert(std::is_same_v<decltype(&_ZN7WebCore14SecurityPolicy18setLocalLoadPolicyENS0_15LocalLoadPolicyE), SetLocalLoadPolicy>);
static_assert(sizeof(WebAnimationHashSet) == 8);
static_assert(NKWebKit::WebCoreAbi::AllowLocalLoadsForAll == 0);
static_assert(NKWebKit::WebCoreAbi::AllowLocalLoadsForLocalAndSubstituteData == 1);
static_assert(NKWebKit::WebCoreAbi::AllowLocalLoadsForLocalOnly == 2);

void CheckPlatformStrategies() {
    auto* first = reinterpret_cast<PlatformStrategies*>(0x1000);
    auto* second = reinterpret_cast<PlatformStrategies*>(0x2000);
    Require(_ZN7WebCore18platformStrategiesEv() == nullptr);
    _ZN7WebCore21setPlatformStrategiesEPNS_18PlatformStrategiesE(first);
    Require(_ZN7WebCore18platformStrategiesEv() == first);
    _ZN7WebCore21setPlatformStrategiesEPNS_18PlatformStrategiesE(second);
    Require(_ZN7WebCore18platformStrategiesEv() == first);
}

void CheckWebAnimationInstances() {
    auto* instances = _ZN7WebCore12WebAnimation9instancesEv();
    Require(instances != nullptr);
    Require(instances->table == nullptr);
    Require(_ZN7WebCore12WebAnimation9instancesEv() == instances);
}

void CheckSharedProvider() {
    _ZN3WTF20initializeMainThreadEv();
    auto& provider = *reinterpret_cast<MessagePortChannelProvider*>(0x3000);
    _ZN7WebCore26MessagePortChannelProvider17setSharedProviderERS0_(provider);
}

void CheckContentSniffing() {
    Require(!NKWebKit::WebCoreAbi::ResourceHandleShouldForceContentSniffing());
    _ZN7WebCore14ResourceHandle20forceContentSniffingEv();
    Require(NKWebKit::WebCoreAbi::ResourceHandleShouldForceContentSniffing());
    _ZN7WebCore14ResourceHandle20forceContentSniffingEv();
    Require(NKWebKit::WebCoreAbi::ResourceHandleShouldForceContentSniffing());
}

void CheckLocalLoadPolicy() {
    Require(NKWebKit::WebCoreAbi::SecurityPolicyRestrictAccessToLocal());
    Require(!NKWebKit::WebCoreAbi::SecurityPolicyAllowSubstituteDataAccessToLocal());
    _ZN7WebCore14SecurityPolicy18setLocalLoadPolicyENS0_15LocalLoadPolicyE(NKWebKit::WebCoreAbi::AllowLocalLoadsForAll);
    Require(!NKWebKit::WebCoreAbi::SecurityPolicyRestrictAccessToLocal());
    Require(NKWebKit::WebCoreAbi::SecurityPolicyAllowSubstituteDataAccessToLocal());
    _ZN7WebCore14SecurityPolicy18setLocalLoadPolicyENS0_15LocalLoadPolicyE(NKWebKit::WebCoreAbi::AllowLocalLoadsForLocalAndSubstituteData);
    Require(NKWebKit::WebCoreAbi::SecurityPolicyRestrictAccessToLocal());
    Require(NKWebKit::WebCoreAbi::SecurityPolicyAllowSubstituteDataAccessToLocal());
    _ZN7WebCore14SecurityPolicy18setLocalLoadPolicyENS0_15LocalLoadPolicyE(NKWebKit::WebCoreAbi::AllowLocalLoadsForLocalOnly);
    Require(NKWebKit::WebCoreAbi::SecurityPolicyRestrictAccessToLocal());
    Require(!NKWebKit::WebCoreAbi::SecurityPolicyAllowSubstituteDataAccessToLocal());
}

}

int main() {
    CheckPlatformStrategies();
    CheckWebAnimationInstances();
    CheckSharedProvider();
    CheckContentSniffing();
    CheckLocalLoadPolicy();
    std::printf("WebCore registration slots: first platform strategies kept, one animation set, provider registered, sniffing forced, local-load policy stored\n");
}
