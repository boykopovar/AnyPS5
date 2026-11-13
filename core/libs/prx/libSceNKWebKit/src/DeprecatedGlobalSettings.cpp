#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/include/InventoryExports.hpp"
#include "prx/libSceNKWebKit/include/LegacyExports.hpp"
#include "prx/libSceNKWebKit/src/DeprecatedGlobalSettings.hpp"

namespace NKWebKit {

DeprecatedGlobalSettings& SharedDeprecatedGlobalSettings() {
    static DeprecatedGlobalSettings settings;
    return settings;
}

}  // namespace NKWebKit

extern "C" {

void APS5_VABI _ZN7WebCore24DeprecatedGlobalSettings24setMockScrollbarsEnabledEb(bool flag) {
    NKWebKit::SharedDeprecatedGlobalSettings().mockScrollbarsEnabled = flag;
}

bool APS5_VABI _ZN7WebCore24DeprecatedGlobalSettings21mockScrollbarsEnabledEv() {
    return NKWebKit::SharedDeprecatedGlobalSettings().mockScrollbarsEnabled;
}

void APS5_VABI _ZN7WebCore24DeprecatedGlobalSettings24setUsesOverlayScrollbarsEb(bool flag) {
    NKWebKit::SharedDeprecatedGlobalSettings().usesOverlayScrollbars = flag;
}

void APS5_VABI _ZN7WebCore24DeprecatedGlobalSettings25setUsesMockScrollAnimatorEb(bool flag) {
    NKWebKit::SharedDeprecatedGlobalSettings().mockScrollAnimatorEnabled = flag;
}

void APS5_VABI _ZN7WebCore24DeprecatedGlobalSettings45setShouldRespectPriorityInCSSAttributeSettersEb(bool flag) {
    NKWebKit::SharedDeprecatedGlobalSettings().shouldRespectPriorityInCSSAttributeSetters = flag;
}

void APS5_VABI _ZN7WebCore24DeprecatedGlobalSettings38setLowPowerVideoAudioBufferSizeEnabledEb(bool flag) {
    NKWebKit::SharedDeprecatedGlobalSettings().lowPowerVideoAudioBufferSizeEnabled = flag;
}

void APS5_VABI _ZN7WebCore24DeprecatedGlobalSettings32setResourceLoadStatisticsEnabledEb(bool flag) {
    NKWebKit::SharedDeprecatedGlobalSettings().resourceLoadStatisticsEnabled = flag;
}

void APS5_VABI _ZN7WebCore24DeprecatedGlobalSettings26setAllowsAnySSLCertificateEb(bool flag) {
    NKWebKit::SharedDeprecatedGlobalSettings().allowsAnySSLCertificate = flag;
}

}
