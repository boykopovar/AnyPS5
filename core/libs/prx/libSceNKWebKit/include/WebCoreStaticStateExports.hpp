#pragma once

#include "prx/libc/include/general/VabiMacros.hpp"

#include <cstdint>
#include <type_traits>

namespace WebCore {

struct PlatformEventModifierSet {
    std::uint8_t m_storage{0};
};

static_assert(sizeof(PlatformEventModifierSet) == 1);
static_assert(alignof(PlatformEventModifierSet) == 1);
static_assert(std::is_trivially_copyable_v<PlatformEventModifierSet>);

}

extern "C" {

bool APS5_VABI _ZN7WebCore17PageConsoleClient21shouldPrintExceptionsEv();
void APS5_VABI _ZN7WebCore17PageConsoleClient24setShouldPrintExceptionsEb(bool print);
bool APS5_VABI _ZN7WebCore27PlatformMediaSessionManager28shouldDeactivateAudioSessionEv();
void APS5_VABI _ZN7WebCore27PlatformMediaSessionManager31setShouldDeactivateAudioSessionEb(bool deactivate);
void APS5_VABI _ZN7WebCore21PlatformKeyboardEvent23getCurrentModifierStateERbS1_S1_S1_(bool& shiftKey, bool& ctrlKey, bool& altKey, bool& metaKey);
void APS5_VABI _ZN7WebCore21PlatformKeyboardEvent23setCurrentModifierStateEN3WTF9OptionSetINS_13PlatformEvent8ModifierEEE(WebCore::PlatformEventModifierSet modifiers);

}
