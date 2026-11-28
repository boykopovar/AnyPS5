#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>
#include <string_view>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#else
#include <sys/mman.h>
#endif

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "AccessibilityObjectRoles check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

namespace {

using NKWebKit::WebCoreAbi::AccessibilityRole;

constexpr unsigned kIs8BitFlag = 4u;

struct StackString {
    NKWebKit::WtfAbi::StringImpl impl;
    NKWebKit::WtfAbi::String value;

    StackString(unsigned length, const void* data, unsigned flags)
        : impl{2u, length, data, flags}, value{&impl} {}
    ~StackString() { value.impl = nullptr; }
};

AccessibilityRole RoleFromAscii(std::string_view text) {
    StackString str{static_cast<unsigned>(text.size()), text.data(), kIs8BitFlag};
    return _ZN7WebCore19AccessibilityObject21ariaRoleToWebCoreRoleERKN3WTF6StringE(&str.value);
}

AccessibilityRole RoleFromUtf16(std::u16string_view text) {
    StackString str{static_cast<unsigned>(text.size()), text.data(), 0u};
    return _ZN7WebCore19AccessibilityObject21ariaRoleToWebCoreRoleERKN3WTF6StringE(&str.value);
}

void TestIsARIAInputAndControl() {
    const AccessibilityRole inputRoles[] = {
        AccessibilityRole::CheckBox,
        AccessibilityRole::RadioButton,
        AccessibilityRole::SearchField,
        AccessibilityRole::Switch,
        AccessibilityRole::TextField,
    };
    const AccessibilityRole controlOnlyRoles[] = {
        AccessibilityRole::Button,
        AccessibilityRole::ComboBox,
        AccessibilityRole::ListBox,
        AccessibilityRole::PopUpButton,
        AccessibilityRole::Slider,
        AccessibilityRole::TextArea,
        AccessibilityRole::ToggleButton,
    };

    auto contains = [](const auto& arr, AccessibilityRole role) {
        for (AccessibilityRole item : arr) {
            if (item == role)
                return true;
        }
        return false;
    };

    for (int raw = 0; raw <= 200; ++raw) {
        const auto role = static_cast<AccessibilityRole>(raw);
        const bool expectedControl = contains(inputRoles, role) || contains(controlOnlyRoles, role);
        const bool expectedLegacyInput = raw == 20 || raw == 96 || raw == 110 || raw == 122 || raw == 136;
        Require(_ZN7WebCore19AccessibilityObject11isARIAInputENS_17AccessibilityRoleE(static_cast<NKWebKit::WebCoreAbi::LegacyAccessibilityRole>(raw)) == expectedLegacyInput);
        Require(_ZN7WebCore19AccessibilityObject13isARIAControlENS_17AccessibilityRoleE(role) == expectedControl);
    }

    for (const int raw : {-1, 256 + 20, 65536 + 96, -2147483647 - 1, 2147483647}) {
        Require(!_ZN7WebCore19AccessibilityObject11isARIAInputENS_17AccessibilityRoleE(static_cast<NKWebKit::WebCoreAbi::LegacyAccessibilityRole>(raw)));
    }
}

void TestAriaRoleToWebCoreRole() {
    bool threwNullString = false;
    try {
        (void)_ZN7WebCore19AccessibilityObject21ariaRoleToWebCoreRoleERKN3WTF6StringE(nullptr);
    } catch (const std::invalid_argument&) {
        threwNullString = true;
    }
    Require(threwNullString);

    bool threwNullData = false;
    try {
        StackString brokenStr{4u, nullptr, kIs8BitFlag};
        (void)_ZN7WebCore19AccessibilityObject21ariaRoleToWebCoreRoleERKN3WTF6StringE(&brokenStr.value);
    } catch (const std::invalid_argument&) {
        threwNullData = true;
    }
    Require(threwNullData);

    NKWebKit::WtfAbi::String nullString{nullptr};
    Require(_ZN7WebCore19AccessibilityObject21ariaRoleToWebCoreRoleERKN3WTF6StringE(&nullString) == AccessibilityRole::Unknown);
    Require(RoleFromAscii("") == AccessibilityRole::Unknown);
    Require(RoleFromUtf16(u"") == AccessibilityRole::Unknown);
    Require(RoleFromAscii("   ") == AccessibilityRole::Unknown);
    Require(RoleFromAscii("nonexistentrole anotherunknown") == AccessibilityRole::Unknown);

    Require(RoleFromAscii("button") == AccessibilityRole::Button);
    Require(RoleFromAscii("BUTTON") == AccessibilityRole::Button);
    Require(RoleFromUtf16(u"TeXtBoX") == AccessibilityRole::TextField);
    Require(RoleFromAscii("  unknownRole   searchbox  button ") == AccessibilityRole::SearchField);
    Require(RoleFromUtf16(u"invalid   doc-cover  region") == AccessibilityRole::Image);
    Require(RoleFromAscii("\tbutton") == AccessibilityRole::Unknown);
    Require(RoleFromUtf16(u"\tbutton menu") == AccessibilityRole::Menu);

    Require(RoleFromAscii("img") == AccessibilityRole::Image);
    Require(RoleFromAscii("image") == AccessibilityRole::Image);
    Require(RoleFromAscii("none") == AccessibilityRole::Presentational);
    Require(RoleFromAscii("presentation") == AccessibilityRole::Presentational);
    Require(RoleFromAscii("separator") == AccessibilityRole::Splitter);
    Require(RoleFromAscii("doc-pagebreak") == AccessibilityRole::Splitter);
    Require(RoleFromAscii("doc-subtitle") == AccessibilityRole::Heading);
    Require(RoleFromAscii("doc-biblioref") == AccessibilityRole::WebCoreLink);
    Require(RoleFromAscii("graphics-symbol") == AccessibilityRole::GraphicsSymbol);
    Require(RoleFromAscii("menuitemradio") == AccessibilityRole::MenuItemRadio);
    Require(RoleFromAscii("treeitem") == AccessibilityRole::TreeItem);
    Require(RoleFromUtf16(u"b\u00fctton") == AccessibilityRole::Unknown);
}

bool Rejected(const NKWebKit::WtfAbi::String* value) {
    errno = 71;
    bool rejected = false;
    try {
        (void)_ZN7WebCore19AccessibilityObject21ariaRoleToWebCoreRoleERKN3WTF6StringE(value);
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    return rejected && errno == 71;
}

unsigned char* MapGuardedPage() {
#ifdef _WIN32
    auto* pages = static_cast<unsigned char*>(VirtualAlloc(nullptr, 8192, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
    Require(pages != nullptr);
    DWORD previous = 0;
    Require(VirtualProtect(pages + 4096, 4096, PAGE_NOACCESS, &previous) != 0);
#else
    void* mapped = mmap(nullptr, 8192, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, -1, 0);
    Require(mapped != MAP_FAILED);
    auto* pages = static_cast<unsigned char*>(mapped);
    Require(mprotect(pages + 4096, 4096, PROT_NONE) == 0);
#endif
    return pages;
}

void UnmapGuardedPage(unsigned char* pages) {
#ifdef _WIN32
    Require(VirtualFree(pages, 0, MEM_RELEASE) != 0);
#else
    Require(munmap(pages, 8192) == 0);
#endif
}

void TestGuestPointers() {
    Require(Rejected(reinterpret_cast<const NKWebKit::WtfAbi::String*>(std::uintptr_t{0x10})));
    {
        NKWebKit::WtfAbi::String unmapped(reinterpret_cast<NKWebKit::WtfAbi::StringImpl*>(std::uintptr_t{0x10}));
        Require(Rejected(&unmapped));
        unmapped.impl = nullptr;
    }
    StackString str{INT32_MAX, reinterpret_cast<const void*>(std::uintptr_t{0x10}), kIs8BitFlag};
    Require(Rejected(&str.value));
    str.impl.hashAndFlags = 0;
    Require(Rejected(&str.value));

    unsigned char* pages = MapGuardedPage();
    std::memset(pages, ' ', 4096);
    str.impl.data = pages;
    str.impl.length = 8192;
    str.impl.hashAndFlags = kIs8BitFlag;
    Require(Rejected(&str.value));

    std::memcpy(pages + 4096 - 7, "button ", 7);
    str.impl.data = pages + 4096 - 7;
    str.impl.length = 100;
    Require(_ZN7WebCore19AccessibilityObject21ariaRoleToWebCoreRoleERKN3WTF6StringE(&str.value) == AccessibilityRole::Button);
    const char16_t wide[] = u"menu ";
    std::memcpy(pages + 4096 - 10, wide, 10);
    str.impl.data = pages + 4096 - 10;
    str.impl.hashAndFlags = 0;
    Require(_ZN7WebCore19AccessibilityObject21ariaRoleToWebCoreRoleERKN3WTF6StringE(&str.value) == AccessibilityRole::Menu);
    UnmapGuardedPage(pages);

    const std::string longToken = std::string(5000, 'a') + " treeitem";
    Require(RoleFromAscii(longToken) == AccessibilityRole::TreeItem);
    const std::u16string longWide = std::u16string(5000, u'b') + u"button" + std::u16string(5000, u' ') + u"tree";
    Require(RoleFromUtf16(longWide) == AccessibilityRole::Tree);
    std::puts("ariaRoleToWebCoreRole: unreadable strings throw and keep errno, reads stop at the first role");
}

}  // namespace

int main() {
    TestIsARIAInputAndControl();
    TestAriaRoleToWebCoreRole();
    TestGuestPointers();
    return 0;
}
