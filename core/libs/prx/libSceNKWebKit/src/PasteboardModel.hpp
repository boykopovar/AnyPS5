#pragma once

#include "prx/libSceNKWebKit/include/WtfExports.hpp"
#include "prx/libSceNKWebKit/src/PasteboardAbi.hpp"
#include "prx/libSceNKWebKit/src/WtfStringAbi.hpp"

#include <cstddef>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace NKWebKit::WebCoreAbi {

struct PasteboardEntry {
    std::u16string type;
    std::u16string value;
};

struct PasteboardStore {
    std::vector<PasteboardEntry> entries;
};

inline PasteboardStore& ModulePasteboardStore() {
    static PasteboardStore store;
    return store;
}

inline std::u16string CopyGuestString(const WtfAbi::String& string) {
    if (!string.impl) return {};
    const WtfAbi::StringView view = WtfAbi::ViewOf(string.impl);
    std::u16string copy;
    copy.reserve(view.length);
    for (unsigned index = 0; index < view.length; ++index) copy.push_back(WtfAbi::CodeUnitAt(view, index));
    return copy;
}

inline WtfAbi::String CreateGuestString(const std::u16string& text) {
    return _ZN3WTF10StringImpl6createEPKDsj(text.data(), static_cast<unsigned>(text.size()));
}

inline PasteboardEntry* FindPasteboardEntry(std::vector<PasteboardEntry>& entries, std::u16string_view type) {
    for (auto& entry : entries) {
        if (std::u16string_view(entry.type) == type) return &entry;
    }
    return nullptr;
}

inline void SetPasteboardEntry(std::vector<PasteboardEntry>& entries, std::u16string type, std::u16string value) {
    if (auto* entry = FindPasteboardEntry(entries, type)) {
        entry->value = std::move(value);
        return;
    }
    entries.push_back({std::move(type), std::move(value)});
}

}  // namespace NKWebKit::WebCoreAbi
