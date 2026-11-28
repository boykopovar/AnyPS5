// Derived from neonmodder123/PS5-WebKit-11.x/Source/WebCore/accessibility/AccessibilityObject.cpp@80f77e94c63ee7202eec3d4b5b1a021dd89011ba
// Derived from neonmodder123/PS5-WebKit-11.x/Source/WebCore/accessibility/AccessibilityObjectInterface.h@80f77e94c63ee7202eec3d4b5b1a021dd89011ba
/*
 * Copyright (C) 2008-2022 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "prx/libSceNKWebKit/include/AccessibilityObjectExports.hpp"
#include "prx/libc/include/General.hpp"

#include <algorithm>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>
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

bool equalsAsciiIgnoreCase(const char16_t* data, std::size_t length, std::string_view expectedLowerAscii) {
    if (length != expectedLowerAscii.size())
        return false;
    for (std::size_t i = 0; i < length; ++i) {
        if (toAsciiLower(data[i]) != static_cast<char16_t>(static_cast<unsigned char>(expectedLowerAscii[i])))
            return false;
    }
    return true;
}

constexpr std::size_t longestAriaRoleName() {
    std::size_t longest = 0;
    for (const auto& entry : kAriaRoleEntries)
        longest = std::max(longest, entry.name.size());
    return longest;
}

constexpr std::size_t kLongestAriaRoleName = longestAriaRoleName();
constexpr std::size_t kGuestPageSize = 4096;

class AriaRoleTokens {
    char16_t token[kLongestAriaRoleName]{};
    std::size_t length = 0;

public:
    bool push(char16_t unit, AccessibilityRole& role) {
        if (unit == u' ')
            return finish(role);
        if (length < kLongestAriaRoleName)
            token[length] = unit;
        ++length;
        return false;
    }

    bool finish(AccessibilityRole& role) {
        const std::size_t tokenLength = length;
        length = 0;
        if (tokenLength == 0 || tokenLength > kLongestAriaRoleName)
            return false;
        for (const auto& entry : kAriaRoleEntries) {
            if (equalsAsciiIgnoreCase(token, tokenLength, entry.name)) {
                role = entry.role;
                return true;
            }
        }
        return false;
    }
};

void requireCopied(bool copied, int savedErrno, const char* what) {
    errno = savedErrno;
    if (!copied)
        throw std::invalid_argument(std::string("ariaRoleToWebCoreRole: unreadable ") + what);
}

bool IsARIAInputRole(AccessibilityRole ariaRole) {
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

}  // namespace

extern "C" {

bool APS5_VABI _ZN7WebCore19AccessibilityObject11isARIAInputENS_17AccessibilityRoleE(NKWebKit::WebCoreAbi::LegacyAccessibilityRole ariaRole) {
    using NKWebKit::WebCoreAbi::LegacyAccessibilityRole;
    switch (ariaRole) {
    case LegacyAccessibilityRole::CheckBox:
    case LegacyAccessibilityRole::RadioButton:
    case LegacyAccessibilityRole::SearchField:
    case LegacyAccessibilityRole::Switch:
    case LegacyAccessibilityRole::TextField:
        return true;
    default:
        return false;
    }
}

bool APS5_VABI _ZN7WebCore19AccessibilityObject13isARIAControlENS_17AccessibilityRoleE(AccessibilityRole ariaRole) {
    if (IsARIAInputRole(ariaRole))
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

    const int savedErrno = errno;
    const NKWebKit::WtfAbi::StringImpl* guestImpl = nullptr;
    requireCopied(GuestCopyIn_nid_no_patch(&guestImpl, &value->impl, sizeof(guestImpl)), savedErrno, "string");
    if (!guestImpl)
        return AccessibilityRole::Unknown;
    NKWebKit::WtfAbi::StringImpl impl{};
    requireCopied(GuestCopyIn_nid_no_patch(&impl, guestImpl, sizeof(impl)), savedErrno, "string implementation");
    if (impl.length == 0)
        return AccessibilityRole::Unknown;
    if (!impl.data)
        throw std::invalid_argument("ariaRoleToWebCoreRole: null string data");

    const std::size_t width = (impl.hashAndFlags & kStringImplIs8BitFlag) != 0 ? 1 : 2;
    const auto* cursor = static_cast<const unsigned char*>(impl.data);
    std::uint64_t remaining = std::uint64_t{impl.length} * width;
    unsigned char chunk[kGuestPageSize];
    AriaRoleTokens tokens;
    AccessibilityRole role = AccessibilityRole::Unknown;
    while (remaining) {
        std::size_t size = kGuestPageSize - reinterpret_cast<std::uintptr_t>(cursor) % kGuestPageSize;
        size = static_cast<std::size_t>(std::min<std::uint64_t>(size, remaining));
        size -= size % width;
        if (!size)
            size = width;
        requireCopied(GuestCopyIn_nid_no_patch(chunk, cursor, size), savedErrno, "string characters");
        for (std::size_t offset = 0; offset < size; offset += width) {
            char16_t unit = chunk[offset];
            if (width == 2)
                std::memcpy(&unit, chunk + offset, sizeof(unit));
            if (tokens.push(unit, role))
                return role;
        }
        cursor += size;
        remaining -= size;
    }
    return tokens.finish(role) ? role : AccessibilityRole::Unknown;
}

}
