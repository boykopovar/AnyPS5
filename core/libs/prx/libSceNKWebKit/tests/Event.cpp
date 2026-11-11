#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"
#include "prx/libSceNKWebKit/src/Event.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <source_location>
#include <type_traits>

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "WebKit event check failed at line %u\n", location.line());
        std::abort();
    }
}

bool Same(const char* actual, const char* expected) {
    return actual && std::strcmp(actual, expected) == 0;
}

using KeyboardEventMake = NKWebKit::KeyboardEvent (APS5_VABI *)(std::int32_t, std::uint8_t, const char*, std::uint32_t, const char*, std::int32_t, const char*, const char*, std::int32_t, std::uint32_t, std::uint32_t);
using MouseEventMake = NKWebKit::MouseEvent (APS5_VABI *)(std::int32_t, std::int32_t, NKWebKit::PointValue, std::uint32_t);
using WheelEventMake = NKWebKit::WheelEvent (APS5_VABI *)(std::int32_t, NKWebKit::PointValue, NKWebKit::SizeValue, NKWebKit::SizeValue, std::uint32_t);
static_assert(std::is_same_v<decltype(&WKKeyboardEventMake), KeyboardEventMake>);
static_assert(std::is_same_v<decltype(&WKMouseEventMake), MouseEventMake>);
static_assert(std::is_same_v<decltype(&WKWheelEventMake), WheelEventMake>);

void TestKeyboardEventNormalInput() {
    const char text[] = "a";
    const char identifier[] = "U+0041";
    const char key[] = "a";
    const char code[] = "KeyA";
    const auto event = WKKeyboardEventMake(7, NKWebKit::InputTypeNormal, text, 1, identifier, 65, key, code, 3, 0xdead, 0x10);
    Require(event.type == 7);
    Require(event.text == text);
    Require(event.length == 1);
    Require(event.keyIdentifier == identifier);
    Require(event.key == key);
    Require(event.code == code);
    Require(event.modifiers == 0x10);
    Require(event.virtualKeyCode == 65);
    Require(event.caretOffset == 3);
    std::printf("WKKeyboardEventMake: normal input keeps every argument\n");
}

void TestKeyboardEventEmptyText() {
    const char text[] = "ignored";
    const auto event = WKKeyboardEventMake(8, NKWebKit::InputTypeNormal, text, 0, "Enter", 13, "Enter", "Enter", -1, 0, 0);
    Require(event.text == nullptr);
    Require(event.length == 0);
    Require(event.type == 8);
    Require(event.caretOffset == -1);
    Require(Same(event.keyIdentifier, "Enter"));
    std::printf("WKKeyboardEventMake: zero length drops the text pointer\n");
}

void TestKeyboardEventCompositionIdentifiers() {
    const char identifier[] = "Process";
    Require(Same(WKKeyboardEventMake(7, NKWebKit::InputTypeSetComposition, "x", 1, identifier, 229, "x", "KeyX", 1, 0, 0).keyIdentifier, "Convert"));
    Require(Same(WKKeyboardEventMake(7, NKWebKit::InputTypeConfirmComposition, "x", 1, identifier, 229, "x", "KeyX", 1, 0, 0).keyIdentifier, "Accept"));
    Require(Same(WKKeyboardEventMake(7, NKWebKit::InputTypeCancelComposition, "x", 1, identifier, 229, "x", "KeyX", 1, 0, 0).keyIdentifier, "Cancel"));
    Require(WKKeyboardEventMake(7, 4, "x", 1, identifier, 229, "x", "KeyX", 1, 0, 0).keyIdentifier == identifier);
    std::printf("WKKeyboardEventMake: composition input types replace the key identifier\n");
}

void TestMouseEvent() {
    const auto event = WKMouseEventMake(2, -2, {12.5, -4.25}, 1u << 20);
    Require(event.type == 2);
    Require(event.button == -2);
    Require(event.position.x == 12.5);
    Require(event.position.y == -4.25);
    Require(event.modifiers == 1u << 20);
    std::printf("WKMouseEventMake: fields copied\n");
}

void TestWheelEvent() {
    const auto event = WKWheelEventMake(6, {1.0, 2.0}, {-3.5, 4.5}, {-1.0, 0.25}, 9);
    Require(event.type == 6);
    Require(event.position.x == 1.0);
    Require(event.position.y == 2.0);
    Require(event.delta.width == -3.5);
    Require(event.delta.height == 4.5);
    Require(event.wheelTicks.width == -1.0);
    Require(event.wheelTicks.height == 0.25);
    Require(event.modifiers == 9);
    std::printf("WKWheelEventMake: fields copied\n");
}

}

int main() {
    TestKeyboardEventNormalInput();
    TestKeyboardEventEmptyText();
    TestKeyboardEventCompositionIdentifiers();
    TestMouseEvent();
    TestWheelEvent();
}
