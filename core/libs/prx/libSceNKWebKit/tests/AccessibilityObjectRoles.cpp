#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <string_view>

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
        const bool expectedInput = contains(inputRoles, role);
        const bool expectedControl = expectedInput || contains(controlOnlyRoles, role);
        Require(_ZN7WebCore19AccessibilityObject11isARIAInputENS_17AccessibilityRoleE(role) == expectedInput);
        Require(_ZN7WebCore19AccessibilityObject13isARIAControlENS_17AccessibilityRoleE(role) == expectedControl);
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

}  // namespace

int main() {
    TestIsARIAInputAndControl();
    TestAriaRoleToWebCoreRole();
    return 0;
}
