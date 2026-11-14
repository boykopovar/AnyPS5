#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/include/InventoryExports.hpp"
#include "prx/libSceNKWebKit/include/LegacyExports.hpp"
#include "prx/libSceNKWebKit/src/DeprecatedGlobalSettings.hpp"

#include <cstdio>
#include <cstdlib>
#include <source_location>

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "DeprecatedGlobalSettings check failed at line %u\n", location.line());
        std::abort();
    }
}

using Setter = void (APS5_VABI*)(bool);

struct Flag {
    Setter set;
    bool NKWebKit::DeprecatedGlobalSettings::* field;
};

bool NoFlagSet(const NKWebKit::DeprecatedGlobalSettings& settings) {
    return !settings.mockScrollbarsEnabled && !settings.usesOverlayScrollbars && !settings.mockScrollAnimatorEnabled &&
        !settings.shouldRespectPriorityInCSSAttributeSetters && !settings.lowPowerVideoAudioBufferSizeEnabled &&
        !settings.resourceLoadStatisticsEnabled && !settings.allowsAnySSLCertificate;
}

}

int main() {
    auto& settings = NKWebKit::SharedDeprecatedGlobalSettings();
    Require(NoFlagSet(settings));
    Require(!_ZN7WebCore24DeprecatedGlobalSettings21mockScrollbarsEnabledEv());

    _ZN7WebCore24DeprecatedGlobalSettings24setMockScrollbarsEnabledEb(true);
    Require(_ZN7WebCore24DeprecatedGlobalSettings21mockScrollbarsEnabledEv());
    _ZN7WebCore24DeprecatedGlobalSettings24setMockScrollbarsEnabledEb(false);
    Require(!_ZN7WebCore24DeprecatedGlobalSettings21mockScrollbarsEnabledEv());

    const Flag flags[] = {
        {_ZN7WebCore24DeprecatedGlobalSettings24setMockScrollbarsEnabledEb,
            &NKWebKit::DeprecatedGlobalSettings::mockScrollbarsEnabled},
        {_ZN7WebCore24DeprecatedGlobalSettings24setUsesOverlayScrollbarsEb,
            &NKWebKit::DeprecatedGlobalSettings::usesOverlayScrollbars},
        {_ZN7WebCore24DeprecatedGlobalSettings25setUsesMockScrollAnimatorEb,
            &NKWebKit::DeprecatedGlobalSettings::mockScrollAnimatorEnabled},
        {_ZN7WebCore24DeprecatedGlobalSettings45setShouldRespectPriorityInCSSAttributeSettersEb,
            &NKWebKit::DeprecatedGlobalSettings::shouldRespectPriorityInCSSAttributeSetters},
        {_ZN7WebCore24DeprecatedGlobalSettings38setLowPowerVideoAudioBufferSizeEnabledEb,
            &NKWebKit::DeprecatedGlobalSettings::lowPowerVideoAudioBufferSizeEnabled},
        {_ZN7WebCore24DeprecatedGlobalSettings32setResourceLoadStatisticsEnabledEb,
            &NKWebKit::DeprecatedGlobalSettings::resourceLoadStatisticsEnabled},
        {_ZN7WebCore24DeprecatedGlobalSettings26setAllowsAnySSLCertificateEb,
            &NKWebKit::DeprecatedGlobalSettings::allowsAnySSLCertificate},
    };

    for (const auto& flag : flags) {
        flag.set(true);
        Require(settings.*flag.field);
        for (const auto& other : flags) {
            if (&other != &flag) Require(!(settings.*other.field));
        }
        Require(_ZN7WebCore24DeprecatedGlobalSettings21mockScrollbarsEnabledEv() ==
            (flag.field == &NKWebKit::DeprecatedGlobalSettings::mockScrollbarsEnabled));
        flag.set(true);
        Require(settings.*flag.field);
        flag.set(false);
        Require(NoFlagSet(settings));
    }

    std::puts("DeprecatedGlobalSettings tests passed");
    return 0;
}
