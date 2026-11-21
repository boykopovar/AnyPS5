#ifndef CORE_LIBS_PRX_LIBSCEIME_INCLUDE_IMECOMPOSITION_HPP
#define CORE_LIBS_PRX_LIBSCEIME_INCLUDE_IMECOMPOSITION_HPP

#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>

#include "prx/libSceIme/include/ImeInput.hpp"

struct ImeComposition {
    std::u16string text;
    std::size_t cursor = 0;
    std::size_t selection = 0;

    bool Active() const { return !text.empty(); }

    void Clear() {
        text.clear();
        cursor = 0;
        selection = 0;
    }

    void Set(std::u16string units, std::int32_t startCharacters, std::int32_t lengthCharacters) {
        text = std::move(units);
        const std::int64_t start = startCharacters > 0 ? startCharacters : 0;
        const std::int64_t length = lengthCharacters > 0 ? lengthCharacters : 0;
        const std::size_t begin = Offset(start);
        cursor = begin;
        selection = Offset(start + length) - begin;
    }

private:
    std::size_t Offset(std::int64_t characters) const {
        std::size_t unit = 0;
        for (std::int64_t seen = 0; seen < characters && unit < text.size(); ++seen) {
            const bool pair = unit + 1 < text.size() && text[unit] >= 0xd800 && text[unit] <= 0xdbff && text[unit + 1] >= 0xdc00 && text[unit + 1] <= 0xdfff;
            unit += pair ? 2 : 1;
        }
        return unit;
    }
};

enum class ImeCompositionRoute { Apply, Swallow };

inline ImeCompositionRoute ImeRouteWhileComposing(ImeComposition& composition, ImeHostInputKind kind) {
    switch (kind) {
        case ImeHostInputKind::Text:
            composition.Clear();
            return ImeCompositionRoute::Apply;
        case ImeHostInputKind::Enter:
        case ImeHostInputKind::Escape:
            if (!composition.Active()) return ImeCompositionRoute::Apply;
            composition.Clear();
            return ImeCompositionRoute::Swallow;
        case ImeHostInputKind::Preedit:
            return ImeCompositionRoute::Swallow;
        case ImeHostInputKind::Backspace:
        case ImeHostInputKind::Delete:
        case ImeHostInputKind::Left:
        case ImeHostInputKind::Right:
        case ImeHostInputKind::Home:
        case ImeHostInputKind::End:
        case ImeHostInputKind::Paste:
            return composition.Active() ? ImeCompositionRoute::Swallow : ImeCompositionRoute::Apply;
    }
    return ImeCompositionRoute::Apply;
}

#endif
