#pragma once

#include "WtfStringAbi.hpp"
#include <string>
#include <string_view>

namespace NKWebKit::URLSetters {

using namespace WtfAbi;

StringView View_nid_no_patch(const StringImpl* value);
StringView Slice_nid_no_patch(StringView view, unsigned start, unsigned length);
std::u16string URLUnits_nid_no_patch(StringView view);
std::string URLBytes_nid_no_patch(std::u16string_view text, unsigned mode);
unsigned URLDefaultPort_nid_no_patch(std::string_view scheme);
StringImpl* CopyImpl_nid_no_patch(StringView view);
char16_t Unit_nid_no_patch(StringView view, unsigned index);
unsigned HostStart_nid_no_patch(const URL& url);
unsigned PathStart_nid_no_patch(const URL& url);
char16_t AsciiCase_nid_no_patch(char16_t value, bool lower);
std::string EncodeURL_nid_no_patch(std::u16string_view text, unsigned component, unsigned mode);

}  // namespace NKWebKit::URLSetters
