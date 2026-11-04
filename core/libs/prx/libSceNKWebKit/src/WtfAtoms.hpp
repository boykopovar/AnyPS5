#pragma once

#include "WtfStringAbi.hpp"
#include <unordered_map>

namespace NKWebKit {
void* Allocate(std::size_t size, std::size_t alignment = 16);
}  // namespace NKWebKit

namespace NKWebKit::Atoms {

using namespace WtfAbi;

struct AtomTable {
    std::unordered_multimap<unsigned, StringImpl*> entries;
    ~AtomTable();
};

AtomTable& AtomTable_nid_no_patch();
StringView View_nid_no_patch(const StringImpl* value);
unsigned Hash_nid_no_patch(StringView view);
bool EqualViews_nid_no_patch(StringView a, StringView b);
StringImpl* Retain_nid_no_patch(StringImpl* impl);
StringImpl* EmptyImpl_nid_no_patch();
StringImpl* CopyImpl_nid_no_patch(StringView view, bool narrow);
String SharedSubstring_nid_no_patch(const String* value, unsigned start, unsigned length);
String FromUTF8_nid_no_patch(const unsigned char* input, std::size_t length);
void RemoveAtom_nid_no_patch(StringImpl* impl);
StringImpl* FindAtom_nid_no_patch(StringView view, unsigned hash);
String AddAtomBuffer_nid_no_patch(StringView view);
String AddStaticAtom_nid_no_patch(StringView view);
String AddAtom_nid_no_patch(StringImpl* impl);

String CreateSharedSubstring_nid_no_patch(StringImpl* original, unsigned start, unsigned length);
String CreateSymbol_nid_no_patch(StringImpl& impl, unsigned flags);

}  // namespace NKWebKit::Atoms
