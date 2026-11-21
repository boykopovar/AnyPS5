#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <source_location>
#include <type_traits>

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "WebCore static flags check failed at line %u\n", location.line());
        std::abort();
    }
}

using Flag = bool (APS5_VABI *)();
using SetFlag = void (APS5_VABI *)(bool);
using GetModifiers = void (APS5_VABI *)(bool&, bool&, bool&, bool&);
using SetModifiers = void (APS5_VABI *)(WebCore::PlatformEventModifierSet);
using SetIdentifier = void (APS5_VABI *)(std::uint64_t);
static_assert(std::is_same_v<decltype(&_ZN7WebCore17PageConsoleClient21shouldPrintExceptionsEv), Flag>);
static_assert(std::is_same_v<decltype(&_ZN7WebCore17PageConsoleClient24setShouldPrintExceptionsEb), SetFlag>);
static_assert(std::is_same_v<decltype(&_ZN7WebCore27PlatformMediaSessionManager28shouldDeactivateAudioSessionEv), Flag>);
static_assert(std::is_same_v<decltype(&_ZN7WebCore27PlatformMediaSessionManager31setShouldDeactivateAudioSessionEb), SetFlag>);
static_assert(std::is_same_v<decltype(&_ZN7WebCore21PlatformKeyboardEvent23getCurrentModifierStateERbS1_S1_S1_), GetModifiers>);
static_assert(std::is_same_v<decltype(&_ZN7WebCore21PlatformKeyboardEvent23setCurrentModifierStateEN3WTF9OptionSetINS_13PlatformEvent8ModifierEEE), SetModifiers>);
static_assert(std::is_same_v<decltype(&_ZN7WebCore7Process13setIdentifierEN3WTF16ObjectIdentifierINS_21ProcessIdentifierTypeEEE), SetIdentifier>);

void CheckFlag(Flag get, SetFlag set) {
    Require(!get());
    set(true);
    Require(get());
    set(true);
    Require(get());
    set(false);
    Require(!get());
}

void CheckModifierState() {
    bool shiftKey = true;
    bool ctrlKey = true;
    bool altKey = true;
    bool metaKey = true;
    _ZN7WebCore21PlatformKeyboardEvent23getCurrentModifierStateERbS1_S1_S1_(shiftKey, ctrlKey, altKey, metaKey);
    Require(!shiftKey && !ctrlKey && !altKey && !metaKey);
    _ZN7WebCore21PlatformKeyboardEvent23setCurrentModifierStateEN3WTF9OptionSetINS_13PlatformEvent8ModifierEEE(WebCore::PlatformEventModifierSet{0x3f});
    shiftKey = ctrlKey = altKey = metaKey = true;
    _ZN7WebCore21PlatformKeyboardEvent23getCurrentModifierStateERbS1_S1_S1_(shiftKey, ctrlKey, altKey, metaKey);
    Require(!shiftKey && !ctrlKey && !altKey && !metaKey);
    Require(_ZN7WebCore21PlatformKeyboardEvent20currentCapsLockStateEv() == 0);
}

void CheckLegacyProcessIdentifier() {
    _ZN7WebCore7Process13setIdentifierEN3WTF16ObjectIdentifierINS_21ProcessIdentifierTypeEEE(42);
    Require(_ZN7WebCore7Process10identifierEv() == 42);
    Require(_ZN7WebCore7Process10identifierEv() == 42);
    _ZN7WebCore7Process13setIdentifierEN3WTF16ObjectIdentifierINS_21ProcessIdentifierTypeEEE(0x123456789abcdef0);
    Require(_ZN7WebCore7Process10identifierEv() == 0x123456789abcdef0);
}

}

int main() {
    CheckFlag(_ZN7WebCore17PageConsoleClient21shouldPrintExceptionsEv, _ZN7WebCore17PageConsoleClient24setShouldPrintExceptionsEb);
    CheckFlag(_ZN7WebCore27PlatformMediaSessionManager28shouldDeactivateAudioSessionEv, _ZN7WebCore27PlatformMediaSessionManager31setShouldDeactivateAudioSessionEb);
    CheckModifierState();
    CheckLegacyProcessIdentifier();
    std::printf("WebCore static flags: console exceptions and audio session deactivation start off and keep the last value set, keyboard modifiers stay released, and the legacy Process::setIdentifier sets the shared identifier\n");
}
