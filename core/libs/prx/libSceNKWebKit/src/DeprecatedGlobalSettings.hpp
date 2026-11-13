#pragma once

namespace NKWebKit {

struct DeprecatedGlobalSettings {
    bool mockScrollbarsEnabled = false;
    bool usesOverlayScrollbars = false;
    bool mockScrollAnimatorEnabled = false;
    bool shouldRespectPriorityInCSSAttributeSetters = false;
    bool lowPowerVideoAudioBufferSizeEnabled = false;
    bool resourceLoadStatisticsEnabled = false;
    bool allowsAnySSLCertificate = false;
};

DeprecatedGlobalSettings& SharedDeprecatedGlobalSettings();

}  // namespace NKWebKit
