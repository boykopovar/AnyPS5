#include "prx/libSceNKWebKit/include/AccessibilityObjectExports.hpp"

#include <cstdint>
#include <stdexcept>
#include <string_view>

namespace {

using NKWebKit::WebCoreAbi::AccessibilityRole;

constexpr unsigned kStringImplIs8BitFlag = 4u;

struct AriaRoleEntry {
    std::string_view name;
    AccessibilityRole role;
};

constexpr AriaRoleEntry kAriaRoleEntries[] = {
    {"alert", AccessibilityRole::ApplicationAlert},
    {"alertdialog", AccessibilityRole::ApplicationAlertDialog},
    {"application", AccessibilityRole::WebApplication},
    {"article", AccessibilityRole::DocumentArticle},
    {"banner", AccessibilityRole::LandmarkBanner},
    {"blockquote", AccessibilityRole::Blockquote},
    {"button", AccessibilityRole::Button},
    {"caption", AccessibilityRole::Caption},
    {"code", AccessibilityRole::Code},
    {"checkbox", AccessibilityRole::CheckBox},
    {"complementary", AccessibilityRole::LandmarkComplementary},
    {"contentinfo", AccessibilityRole::LandmarkContentInfo},
    {"deletion", AccessibilityRole::Deletion},
    {"dialog", AccessibilityRole::ApplicationDialog},
    {"directory", AccessibilityRole::Directory},
    {"doc-abstract", AccessibilityRole::ApplicationTextGroup},
    {"doc-acknowledgments", AccessibilityRole::LandmarkDocRegion},
    {"doc-afterword", AccessibilityRole::LandmarkDocRegion},
    {"doc-appendix", AccessibilityRole::LandmarkDocRegion},
    {"doc-backlink", AccessibilityRole::WebCoreLink},
    {"doc-biblioentry", AccessibilityRole::ListItem},
    {"doc-bibliography", AccessibilityRole::LandmarkDocRegion},
    {"doc-biblioref", AccessibilityRole::WebCoreLink},
    {"doc-chapter", AccessibilityRole::LandmarkDocRegion},
    {"doc-colophon", AccessibilityRole::ApplicationTextGroup},
    {"doc-conclusion", AccessibilityRole::LandmarkDocRegion},
    {"doc-cover", AccessibilityRole::Image},
    {"doc-credit", AccessibilityRole::ApplicationTextGroup},
    {"doc-credits", AccessibilityRole::LandmarkDocRegion},
    {"doc-dedication", AccessibilityRole::ApplicationTextGroup},
    {"doc-endnote", AccessibilityRole::ListItem},
    {"doc-endnotes", AccessibilityRole::LandmarkDocRegion},
    {"doc-epigraph", AccessibilityRole::ApplicationTextGroup},
    {"doc-epilogue", AccessibilityRole::LandmarkDocRegion},
    {"doc-errata", AccessibilityRole::LandmarkDocRegion},
    {"doc-example", AccessibilityRole::ApplicationTextGroup},
    {"doc-footnote", AccessibilityRole::Footnote},
    {"doc-foreword", AccessibilityRole::LandmarkDocRegion},
    {"doc-glossary", AccessibilityRole::LandmarkDocRegion},
    {"doc-glossref", AccessibilityRole::WebCoreLink},
    {"doc-index", AccessibilityRole::LandmarkNavigation},
    {"doc-introduction", AccessibilityRole::LandmarkDocRegion},
    {"doc-noteref", AccessibilityRole::WebCoreLink},
    {"doc-notice", AccessibilityRole::DocumentNote},
    {"doc-pagebreak", AccessibilityRole::Splitter},
    {"doc-pagelist", AccessibilityRole::LandmarkNavigation},
    {"doc-part", AccessibilityRole::LandmarkDocRegion},
    {"doc-preface", AccessibilityRole::LandmarkDocRegion},
    {"doc-prologue", AccessibilityRole::LandmarkDocRegion},
    {"doc-pullquote", AccessibilityRole::ApplicationTextGroup},
    {"doc-qna", AccessibilityRole::ApplicationTextGroup},
    {"doc-subtitle", AccessibilityRole::Heading},
    {"doc-tip", AccessibilityRole::DocumentNote},
    {"doc-toc", AccessibilityRole::LandmarkNavigation},
    {"figure", AccessibilityRole::Figure},
    {"generic", AccessibilityRole::Generic},
    {"graphics-document", AccessibilityRole::GraphicsDocument},
    {"graphics-object", AccessibilityRole::GraphicsObject},
    {"graphics-symbol", AccessibilityRole::GraphicsSymbol},
    {"grid", AccessibilityRole::Grid},
    {"gridcell", AccessibilityRole::GridCell},
    {"table", AccessibilityRole::Table},
    {"cell", AccessibilityRole::Cell},
    {"columnheader", AccessibilityRole::ColumnHeader},
    {"combobox", AccessibilityRole::ComboBox},
    {"definition", AccessibilityRole::Definition},
    {"document", AccessibilityRole::Document},
    {"feed", AccessibilityRole::Feed},
    {"form", AccessibilityRole::Form},
    {"rowheader", AccessibilityRole::RowHeader},
    {"group", AccessibilityRole::ApplicationGroup},
    {"heading", AccessibilityRole::Heading},
    {"image", AccessibilityRole::Image},
    {"img", AccessibilityRole::Image},
    {"insertion", AccessibilityRole::Insertion},
    {"link", AccessibilityRole::WebCoreLink},
    {"list", AccessibilityRole::List},
    {"listitem", AccessibilityRole::ListItem},
    {"listbox", AccessibilityRole::ListBox},
    {"log", AccessibilityRole::ApplicationLog},
    {"main", AccessibilityRole::LandmarkMain},
    {"marquee", AccessibilityRole::ApplicationMarquee},
    {"math", AccessibilityRole::DocumentMath},
    {"mark", AccessibilityRole::Mark},
    {"menu", AccessibilityRole::Menu},
    {"menubar", AccessibilityRole::MenuBar},
    {"menuitem", AccessibilityRole::MenuItem},
    {"menuitemcheckbox", AccessibilityRole::MenuItemCheckbox},
    {"menuitemradio", AccessibilityRole::MenuItemRadio},
    {"meter", AccessibilityRole::Meter},
    {"none", AccessibilityRole::Presentational},
    {"note", AccessibilityRole::DocumentNote},
    {"navigation", AccessibilityRole::LandmarkNavigation},
    {"option", AccessibilityRole::ListBoxOption},
    {"paragraph", AccessibilityRole::Paragraph},
    {"presentation", AccessibilityRole::Presentational},
    {"progressbar", AccessibilityRole::ProgressIndicator},
    {"radio", AccessibilityRole::RadioButton},
    {"radiogroup", AccessibilityRole::RadioGroup},
    {"region", AccessibilityRole::LandmarkRegion},
    {"row", AccessibilityRole::Row},
    {"rowgroup", AccessibilityRole::RowGroup},
    {"scrollbar", AccessibilityRole::ScrollBar},
    {"search", AccessibilityRole::LandmarkSearch},
    {"searchbox", AccessibilityRole::SearchField},
    {"separator", AccessibilityRole::Splitter},
    {"slider", AccessibilityRole::Slider},
    {"spinbutton", AccessibilityRole::SpinButton},
    {"status", AccessibilityRole::ApplicationStatus},
    {"subscript", AccessibilityRole::Subscript},
    {"suggestion", AccessibilityRole::Suggestion},
    {"superscript", AccessibilityRole::Superscript},
    {"switch", AccessibilityRole::Switch},
    {"tab", AccessibilityRole::Tab},
    {"tablist", AccessibilityRole::TabList},
    {"tabpanel", AccessibilityRole::TabPanel},
    {"text", AccessibilityRole::StaticText},
    {"textbox", AccessibilityRole::TextField},
    {"term", AccessibilityRole::Term},
    {"time", AccessibilityRole::Time},
    {"timer", AccessibilityRole::ApplicationTimer},
    {"toolbar", AccessibilityRole::Toolbar},
    {"tooltip", AccessibilityRole::UserInterfaceTooltip},
    {"tree", AccessibilityRole::Tree},
    {"treegrid", AccessibilityRole::TreeGrid},
    {"treeitem", AccessibilityRole::TreeItem},
};

constexpr char16_t toAsciiLower(char16_t ch) {
    if (ch >= u'A' && ch <= u'Z')
        return static_cast<char16_t>(ch + (u'a' - u'A'));
    return ch;
}

template <typename CharT>
bool equalsAsciiIgnoreCase(const CharT* data, std::size_t length, std::string_view expectedLowerAscii) {
    if (length != expectedLowerAscii.size())
        return false;
    for (std::size_t i = 0; i < length; ++i) {
        if (toAsciiLower(static_cast<char16_t>(data[i])) != static_cast<char16_t>(static_cast<unsigned char>(expectedLowerAscii[i])))
            return false;
    }
    return true;
}

template <typename CharT>
AccessibilityRole lookupAriaRoleTokens(const CharT* data, std::uint32_t length) {
    std::uint32_t index = 0;
    while (index < length) {
        while (index < length && data[index] == static_cast<CharT>(' '))
            ++index;
        if (index >= length)
            break;
        const std::uint32_t tokenStart = index;
        while (index < length && data[index] != static_cast<CharT>(' '))
            ++index;
        const std::size_t tokenLength = index - tokenStart;
        for (const auto& entry : kAriaRoleEntries) {
            if (equalsAsciiIgnoreCase(data + tokenStart, tokenLength, entry.name))
                return entry.role;
        }
    }
    return AccessibilityRole::Unknown;
}

}  // namespace

extern "C" {

bool APS5_VABI _ZN7WebCore19AccessibilityObject11isARIAInputENS_17AccessibilityRoleE(AccessibilityRole ariaRole) {
    switch (ariaRole) {
    case AccessibilityRole::CheckBox:
    case AccessibilityRole::RadioButton:
    case AccessibilityRole::SearchField:
    case AccessibilityRole::Switch:
    case AccessibilityRole::TextField:
        return true;
    default:
        return false;
    }
}

bool APS5_VABI _ZN7WebCore19AccessibilityObject13isARIAControlENS_17AccessibilityRoleE(AccessibilityRole ariaRole) {
    if (_ZN7WebCore19AccessibilityObject11isARIAInputENS_17AccessibilityRoleE(ariaRole))
        return true;

    switch (ariaRole) {
    case AccessibilityRole::Button:
    case AccessibilityRole::ComboBox:
    case AccessibilityRole::ListBox:
    case AccessibilityRole::PopUpButton:
    case AccessibilityRole::Slider:
    case AccessibilityRole::TextArea:
    case AccessibilityRole::ToggleButton:
        return true;
    default:
        return false;
    }
}

AccessibilityRole APS5_VABI _ZN7WebCore19AccessibilityObject21ariaRoleToWebCoreRoleERKN3WTF6StringE(const NKWebKit::WtfAbi::String* value) {
    if (!value)
        throw std::invalid_argument("ariaRoleToWebCoreRole: null string pointer");

    const NKWebKit::WtfAbi::StringImpl* impl = value->impl;
    if (!impl || impl->length == 0)
        return AccessibilityRole::Unknown;
    if (!impl->data)
        throw std::invalid_argument("ariaRoleToWebCoreRole: null string data");

    if ((impl->hashAndFlags & kStringImplIs8BitFlag) != 0)
        return lookupAriaRoleTokens(static_cast<const std::uint8_t*>(impl->data), impl->length);
    return lookupAriaRoleTokens(static_cast<const char16_t*>(impl->data), impl->length);
}

}
