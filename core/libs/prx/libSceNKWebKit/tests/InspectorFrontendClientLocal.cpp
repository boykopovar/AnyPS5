#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <source_location>
#include <type_traits>

namespace {

using NKWebKit::WtfAbi::String;
using NKWebKit::WtfAbi::StringImpl;

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "InspectorFrontendClientLocal check failed at line %u\n", location.line());
        std::abort();
    }
}

using ConstrainedAttachedWindowSize = unsigned int (APS5_VABI *)(unsigned int, unsigned int);
static_assert(std::is_same_v<decltype(&_ZN7WebCore28InspectorFrontendClientLocal31constrainedAttachedWindowHeightEjj), ConstrainedAttachedWindowSize>);
static_assert(std::is_same_v<decltype(&_ZN7WebCore28InspectorFrontendClientLocal30constrainedAttachedWindowWidthEjj), ConstrainedAttachedWindowSize>);

unsigned int Height(unsigned int preferred, unsigned int total) {
    return _ZN7WebCore28InspectorFrontendClientLocal31constrainedAttachedWindowHeightEjj(preferred, total);
}

unsigned int Width(unsigned int preferred, unsigned int total) {
    return _ZN7WebCore28InspectorFrontendClientLocal30constrainedAttachedWindowWidthEjj(preferred, total);
}

void TestHeight() {
    Require(Height(100, 1000) == 250);
    Require(Height(400, 1000) == 400);
    Require(Height(900, 1000) == 750);
    Require(Height(0, 0) == 250);
    Require(Height(333, 1001) == 333);
    Require(Height(1000, 1001) == 751);
    Require(Height(1000, 1002) == 752);
    Require(Height(0xFFFFFFFFu, 0xFFFFFFFFu) == 3221225472u);
    Require(Height(0xFFFFFF80u, 0xFFFFFFFFu) == 3221225472u);
    Require(Height(0xC0000000u, 0xFFFFFF7Fu) == 3221225216u);
    std::printf("InspectorFrontendClientLocal::constrainedAttachedWindowHeight: minimum, ratio cap and rounding passed\n");
}

void TestWidth() {
    Require(Width(100, 2000) == 500);
    Require(Width(900, 2000) == 900);
    Require(Width(1900, 2000) == 1680);
    Require(Width(600, 700) == 500);
    Require(Width(1000, 0) == 500);
    Require(Width(0xFFFFFFFFu, 0xFFFFFFFFu) == 4294967040u);
    Require(Width(0xFFFFFF00u, 0xFFFFFFFFu) == 4294967040u);
    Require(Width(0xFFFFFF7Fu, 0xFFFFFFFFu) == 4294967040u);
    Require(Width(0xFFFFFF80u, 0xFFFFFFFFu) == 4294967040u);
    Require(Width(0xFFFFFFFFu, 0xFFFFFF80u) == 4294967040u);
    Require(Width(0xFFFFFFFFu, 0xFFFFFF7Fu) == 4294966784u);
    Require(Width(0xFFFFFFFFu, 0xFFFFFEC0u) == 4294966784u);
    std::printf("InspectorFrontendClientLocal::constrainedAttachedWindowWidth: minimum, inspected-width cap, small windows and 2^32 rounding boundary passed\n");
}

using SettingsPropertyGetter = String (APS5_VABI *)(void*, const String*);
using SettingsPropertySetter = void (APS5_VABI *)(void*, const String*, const String*);
using SettingsPropertyDeleter = void (APS5_VABI *)(void*, const String*);
static_assert(std::is_same_v<decltype(&_ZN7WebCore28InspectorFrontendClientLocal8Settings11getPropertyERKN3WTF6StringE), SettingsPropertyGetter>);
static_assert(std::is_same_v<decltype(&_ZN7WebCore28InspectorFrontendClientLocal8Settings11setPropertyERKN3WTF6StringES5_), SettingsPropertySetter>);
static_assert(std::is_same_v<decltype(&_ZN7WebCore28InspectorFrontendClientLocal8Settings14deletePropertyERKN3WTF6StringE), SettingsPropertyDeleter>);
static_assert(sizeof(String) == 8);

constexpr unsigned Sentinels[] = { 0x11223344u, 0x55667788u };

struct SentinelObject {
    unsigned first;
    unsigned second;
};

SentinelObject Object() {
    return SentinelObject{ Sentinels[0], Sentinels[1] };
}

void RequireUntouched(const SentinelObject& object) {
    Require(object.first == Sentinels[0]);
    Require(object.second == Sentinels[1]);
}

bool IsNull(const String& string) {
    return string.impl == nullptr;
}

String GetProperty(void* self, const String* name) {
    return _ZN7WebCore28InspectorFrontendClientLocal8Settings11getPropertyERKN3WTF6StringE(self, name);
}

void SetProperty(void* self, const String* name, const String* value) {
    _ZN7WebCore28InspectorFrontendClientLocal8Settings11setPropertyERKN3WTF6StringES5_(self, name, value);
}

void DeleteProperty(void* self, const String* name) {
    _ZN7WebCore28InspectorFrontendClientLocal8Settings14deletePropertyERKN3WTF6StringE(self, name);
}

void TestSettingsProperties() {
    StringImpl nameImpl{ 4, 23, "inspectorAttachedHeight", 4 };
    const String name(&nameImpl);
    StringImpl valueImpl{ 4, 3, "300", 4 };
    const String value(&valueImpl);
    const String nullName;
    SentinelObject object = Object();

    Require(IsNull(GetProperty(&object, &nullName)));
    RequireUntouched(object);
    Require(IsNull(GetProperty(&object, &name)));
    RequireUntouched(object);

    SetProperty(&object, &name, &value);
    RequireUntouched(object);
    Require(IsNull(GetProperty(&object, &name)));
    RequireUntouched(object);

    SetProperty(&object, &name, &value);
    RequireUntouched(object);

    DeleteProperty(&object, &name);
    RequireUntouched(object);
    Require(IsNull(GetProperty(&object, &name)));
    RequireUntouched(object);

    DeleteProperty(&object, &name);
    Require(IsNull(GetProperty(&object, &name)));
    RequireUntouched(object);

    Require(IsNull(GetProperty(nullptr, &name)));
    Require(IsNull(GetProperty(nullptr, nullptr)));
    SetProperty(nullptr, &name, &value);
    DeleteProperty(nullptr, &name);
    std::printf("InspectorFrontendClientLocal::Settings: property get, set and delete leave the object and return a null string\n");
}

}

int main() {
    TestHeight();
    TestWidth();
    TestSettingsProperties();
    return 0;
}
