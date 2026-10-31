#ifndef CORE_LIBS_PRX_LIBSCEIME_INCLUDE_IMEKEYBOARDVIEW_HPP
#define CORE_LIBS_PRX_LIBSCEIME_INCLUDE_IMEKEYBOARDVIEW_HPP

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

enum class ImeKeySymbol : std::uint8_t { None, ShiftOutline, ShiftFilled, CapsLock, Backspace, ArrowLeft, ArrowRight, ArrowUp, ArrowDown };

struct ImeKeyCell {
    std::uint8_t row = 0;
    std::uint8_t column = 0;
    std::uint8_t columnSpan = 1;
    std::uint8_t rowSpan = 1;
    std::u16string label;
    ImeKeySymbol symbol = ImeKeySymbol::None;
    std::u16string badge;
    bool selected = false;
};

struct ImeKeyboardView {
    std::u16string text;
    std::size_t caret = 0;
    std::uint8_t rows = 0;
    std::uint8_t columns = 0;
    std::vector<ImeKeyCell> keys;
};

using ImeKeyboardViewProvider = bool (*)(ImeKeyboardView& view);

extern "C" void ImeSetKeyboardViewProvider_nid_no_patch(ImeKeyboardViewProvider provider);
extern "C" bool ImeReadKeyboardView_nid_postfix(ImeKeyboardView& view);

#endif
