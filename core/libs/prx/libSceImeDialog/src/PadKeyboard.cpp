#include "prx/libSceImeDialog/include/PadKeyboard.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

namespace ImeDialog {

namespace {

constexpr PadKey LatinLowerKeys[] = {
    {0, 0, 1, 1, u"1", PadKeyAction::Character, PadKeyGlyph::None, nullptr},
    {0, 1, 1, 1, u"2", PadKeyAction::Character, PadKeyGlyph::None, nullptr},
    {0, 2, 1, 1, u"3", PadKeyAction::Character, PadKeyGlyph::None, nullptr},
    {0, 3, 1, 1, u"4", PadKeyAction::Character, PadKeyGlyph::None, nullptr},
    {0, 4, 1, 1, u"5", PadKeyAction::Character, PadKeyGlyph::None, nullptr},
    {0, 5, 1, 1, u"6", PadKeyAction::Character, PadKeyGlyph::None, nullptr},
    {0, 6, 1, 1, u"7", PadKeyAction::Character, PadKeyGlyph::None, nullptr},
    {0, 7, 1, 1, u"8", PadKeyAction::Character, PadKeyGlyph::None, nullptr},
    {0, 8, 1, 1, u"9", PadKeyAction::Character, PadKeyGlyph::None, nullptr},
    {0, 9, 1, 1, u"0", PadKeyAction::Character, PadKeyGlyph::None, nullptr},
    {1, 0, 1, 1, u"q", PadKeyAction::Character, PadKeyGlyph::None, nullptr},
    {1, 1, 1, 1, u"w", PadKeyAction::Character, PadKeyGlyph::None, nullptr},
    {1, 2, 1, 1, u"e", PadKeyAction::Character, PadKeyGlyph::None, nullptr},
    {1, 3, 1, 1, u"r", PadKeyAction::Character, PadKeyGlyph::None, nullptr},
    {1, 4, 1, 1, u"t", PadKeyAction::Character, PadKeyGlyph::None, nullptr},
    {1, 5, 1, 1, u"y", PadKeyAction::Character, PadKeyGlyph::None, nullptr},
    {1, 6, 1, 1, u"u", PadKeyAction::Character, PadKeyGlyph::None, nullptr},
    {1, 7, 1, 1, u"i", PadKeyAction::Character, PadKeyGlyph::None, nullptr},
    {1, 8, 1, 1, u"o", PadKeyAction::Character, PadKeyGlyph::None, nullptr},
    {1, 9, 1, 1, u"p", PadKeyAction::Character, PadKeyGlyph::None, nullptr},
    {2, 0, 1, 1, u"a", PadKeyAction::Character, PadKeyGlyph::None, nullptr},
    {2, 1, 1, 1, u"s", PadKeyAction::Character, PadKeyGlyph::None, nullptr},
    {2, 2, 1, 1, u"d", PadKeyAction::Character, PadKeyGlyph::None, nullptr},
    {2, 3, 1, 1, u"f", PadKeyAction::Character, PadKeyGlyph::None, nullptr},
    {2, 4, 1, 1, u"g", PadKeyAction::Character, PadKeyGlyph::None, nullptr},
    {2, 5, 1, 1, u"h", PadKeyAction::Character, PadKeyGlyph::None, nullptr},
    {2, 6, 1, 1, u"j", PadKeyAction::Character, PadKeyGlyph::None, nullptr},
    {2, 7, 1, 1, u"k", PadKeyAction::Character, PadKeyGlyph::None, nullptr},
    {2, 8, 1, 1, u"l", PadKeyAction::Character, PadKeyGlyph::None, nullptr},
    {2, 9, 1, 1, u"'", PadKeyAction::Character, PadKeyGlyph::None, nullptr},
    {3, 0, 1, 1, u"z", PadKeyAction::Character, PadKeyGlyph::None, nullptr},
    {3, 1, 1, 1, u"x", PadKeyAction::Character, PadKeyGlyph::None, nullptr},
    {3, 2, 1, 1, u"c", PadKeyAction::Character, PadKeyGlyph::None, nullptr},
    {3, 3, 1, 1, u"v", PadKeyAction::Character, PadKeyGlyph::None, nullptr},
    {3, 4, 1, 1, u"b", PadKeyAction::Character, PadKeyGlyph::None, nullptr},
    {3, 5, 1, 1, u"n", PadKeyAction::Character, PadKeyGlyph::None, nullptr},
    {3, 6, 1, 1, u"m", PadKeyAction::Character, PadKeyGlyph::None, nullptr},
    {3, 7, 1, 1, u",", PadKeyAction::Character, PadKeyGlyph::None, nullptr},
    {3, 8, 1, 1, u".", PadKeyAction::Character, PadKeyGlyph::None, nullptr},
    {3, 9, 1, 1, u"?", PadKeyAction::Character, PadKeyGlyph::None, nullptr},
    {4, 0, 1, 1, nullptr, PadKeyAction::Shift, PadKeyGlyph::Shift, u"L2"},
    {4, 1, 1, 1, u"@#:", PadKeyAction::Symbols, PadKeyGlyph::None, u"L2+\u25B3"},
    {4, 2, 1, 1, u"\u00E0", PadKeyAction::Specials, PadKeyGlyph::None, u"L3"},
    {4, 3, 4, 1, u"Space", PadKeyAction::Space, PadKeyGlyph::None, u"\u25B3"},
    {4, 8, 2, 1, nullptr, PadKeyAction::Backspace, PadKeyGlyph::Backspace, u"\u25A1"},
    {5, 0, 1, 1, nullptr, PadKeyAction::ArrowDown, PadKeyGlyph::ArrowDown, nullptr},
    {5, 1, 1, 1, nullptr, PadKeyAction::ArrowUp, PadKeyGlyph::ArrowUp, nullptr},
    {5, 2, 1, 1, nullptr, PadKeyAction::ArrowLeft, PadKeyGlyph::ArrowLeft, u"L1"},
    {5, 3, 1, 1, nullptr, PadKeyAction::ArrowRight, PadKeyGlyph::ArrowRight, u"R1"},
    {5, 8, 2, 1, nullptr, PadKeyAction::Done, PadKeyGlyph::None, u"R2"},
};

constexpr PadKey LatinUpperKeys[] = {
    {0, 0, 1, 1, u"1", PadKeyAction::Character, PadKeyGlyph::None, nullptr},
    {0, 1, 1, 1, u"2", PadKeyAction::Character, PadKeyGlyph::None, nullptr},
    {0, 2, 1, 1, u"3", PadKeyAction::Character, PadKeyGlyph::None, nullptr},
    {0, 3, 1, 1, u"4", PadKeyAction::Character, PadKeyGlyph::None, nullptr},
    {0, 4, 1, 1, u"5", PadKeyAction::Character, PadKeyGlyph::None, nullptr},
    {0, 5, 1, 1, u"6", PadKeyAction::Character, PadKeyGlyph::None, nullptr},
    {0, 6, 1, 1, u"7", PadKeyAction::Character, PadKeyGlyph::None, nullptr},
    {0, 7, 1, 1, u"8", PadKeyAction::Character, PadKeyGlyph::None, nullptr},
    {0, 8, 1, 1, u"9", PadKeyAction::Character, PadKeyGlyph::None, nullptr},
    {0, 9, 1, 1, u"0", PadKeyAction::Character, PadKeyGlyph::None, nullptr},
    {1, 0, 1, 1, u"Q", PadKeyAction::Character, PadKeyGlyph::None, nullptr},
    {1, 1, 1, 1, u"W", PadKeyAction::Character, PadKeyGlyph::None, nullptr},
    {1, 2, 1, 1, u"E", PadKeyAction::Character, PadKeyGlyph::None, nullptr},
    {1, 3, 1, 1, u"R", PadKeyAction::Character, PadKeyGlyph::None, nullptr},
    {1, 4, 1, 1, u"T", PadKeyAction::Character, PadKeyGlyph::None, nullptr},
    {1, 5, 1, 1, u"Y", PadKeyAction::Character, PadKeyGlyph::None, nullptr},
    {1, 6, 1, 1, u"U", PadKeyAction::Character, PadKeyGlyph::None, nullptr},
    {1, 7, 1, 1, u"I", PadKeyAction::Character, PadKeyGlyph::None, nullptr},
    {1, 8, 1, 1, u"O", PadKeyAction::Character, PadKeyGlyph::None, nullptr},
    {1, 9, 1, 1, u"P", PadKeyAction::Character, PadKeyGlyph::None, nullptr},
    {2, 0, 1, 1, u"A", PadKeyAction::Character, PadKeyGlyph::None, nullptr},
    {2, 1, 1, 1, u"S", PadKeyAction::Character, PadKeyGlyph::None, nullptr},
    {2, 2, 1, 1, u"D", PadKeyAction::Character, PadKeyGlyph::None, nullptr},
    {2, 3, 1, 1, u"F", PadKeyAction::Character, PadKeyGlyph::None, nullptr},
    {2, 4, 1, 1, u"G", PadKeyAction::Character, PadKeyGlyph::None, nullptr},
    {2, 5, 1, 1, u"H", PadKeyAction::Character, PadKeyGlyph::None, nullptr},
    {2, 6, 1, 1, u"J", PadKeyAction::Character, PadKeyGlyph::None, nullptr},
    {2, 7, 1, 1, u"K", PadKeyAction::Character, PadKeyGlyph::None, nullptr},
    {2, 8, 1, 1, u"L", PadKeyAction::Character, PadKeyGlyph::None, nullptr},
    {2, 9, 1, 1, u"\"", PadKeyAction::Character, PadKeyGlyph::None, nullptr},
    {3, 0, 1, 1, u"Z", PadKeyAction::Character, PadKeyGlyph::None, nullptr},
    {3, 1, 1, 1, u"X", PadKeyAction::Character, PadKeyGlyph::None, nullptr},
    {3, 2, 1, 1, u"C", PadKeyAction::Character, PadKeyGlyph::None, nullptr},
    {3, 3, 1, 1, u"V", PadKeyAction::Character, PadKeyGlyph::None, nullptr},
    {3, 4, 1, 1, u"B", PadKeyAction::Character, PadKeyGlyph::None, nullptr},
    {3, 5, 1, 1, u"N", PadKeyAction::Character, PadKeyGlyph::None, nullptr},
    {3, 6, 1, 1, u"M", PadKeyAction::Character, PadKeyGlyph::None, nullptr},
    {3, 7, 1, 1, u"-", PadKeyAction::Character, PadKeyGlyph::None, nullptr},
    {3, 8, 1, 1, u"_", PadKeyAction::Character, PadKeyGlyph::None, nullptr},
    {3, 9, 1, 1, u"/", PadKeyAction::Character, PadKeyGlyph::None, nullptr},
    {4, 0, 1, 1, nullptr, PadKeyAction::Shift, PadKeyGlyph::Shift, u"L2"},
    {4, 1, 1, 1, u"@#:", PadKeyAction::Symbols, PadKeyGlyph::None, u"L2+\u25B3"},
    {4, 2, 1, 1, u"\u00E0", PadKeyAction::Specials, PadKeyGlyph::None, u"L3"},
    {4, 3, 4, 1, u"Space", PadKeyAction::Space, PadKeyGlyph::None, u"\u25B3"},
    {4, 8, 2, 1, nullptr, PadKeyAction::Backspace, PadKeyGlyph::Backspace, u"\u25A1"},
    {5, 0, 1, 1, nullptr, PadKeyAction::ArrowDown, PadKeyGlyph::ArrowDown, nullptr},
    {5, 1, 1, 1, nullptr, PadKeyAction::ArrowUp, PadKeyGlyph::ArrowUp, nullptr},
    {5, 2, 1, 1, nullptr, PadKeyAction::ArrowLeft, PadKeyGlyph::ArrowLeft, u"L1"},
    {5, 3, 1, 1, nullptr, PadKeyAction::ArrowRight, PadKeyGlyph::ArrowRight, u"R1"},
    {5, 8, 2, 1, nullptr, PadKeyAction::Done, PadKeyGlyph::None, u"R2"},
};

constexpr PadKey SymbolsFirstKeys[] = {
    {0, 0, 1, 1, u"!", PadKeyAction::Character, PadKeyGlyph::None, nullptr},
    {0, 1, 1, 1, u"?", PadKeyAction::Character, PadKeyGlyph::None, nullptr},
    {0, 2, 1, 1, u"\"", PadKeyAction::Character, PadKeyGlyph::None, nullptr},
    {0, 3, 1, 1, u"'", PadKeyAction::Character, PadKeyGlyph::None, nullptr},
    {0, 4, 1, 1, u"#", PadKeyAction::Character, PadKeyGlyph::None, nullptr},
    {0, 5, 1, 1, u"%", PadKeyAction::Character, PadKeyGlyph::None, nullptr},
    {0, 6, 1, 1, u"(", PadKeyAction::Character, PadKeyGlyph::None, nullptr},
    {0, 7, 1, 1, u")", PadKeyAction::Character, PadKeyGlyph::None, nullptr},
    {0, 8, 1, 1, u"()", PadKeyAction::Character, PadKeyGlyph::None, nullptr},
    {0, 9, 1, 1, u"/", PadKeyAction::Character, PadKeyGlyph::None, nullptr},
    {1, 0, 1, 1, u"-", PadKeyAction::Character, PadKeyGlyph::None, nullptr},
    {1, 1, 1, 1, u"_", PadKeyAction::Character, PadKeyGlyph::None, nullptr},
    {1, 2, 1, 1, u",", PadKeyAction::Character, PadKeyGlyph::None, nullptr},
    {1, 3, 1, 1, u".", PadKeyAction::Character, PadKeyGlyph::None, nullptr},
    {1, 4, 1, 1, u":", PadKeyAction::Character, PadKeyGlyph::None, nullptr},
    {1, 5, 1, 1, u";", PadKeyAction::Character, PadKeyGlyph::None, nullptr},
    {1, 6, 1, 1, u"*", PadKeyAction::Character, PadKeyGlyph::None, nullptr},
    {1, 7, 1, 1, u"+", PadKeyAction::Character, PadKeyGlyph::None, nullptr},
    {1, 8, 1, 1, u"=", PadKeyAction::Character, PadKeyGlyph::None, nullptr},
    {1, 9, 1, 1, u"&", PadKeyAction::Character, PadKeyGlyph::None, nullptr},
    {2, 0, 1, 1, u"<", PadKeyAction::Character, PadKeyGlyph::None, nullptr},
    {2, 1, 1, 1, u">", PadKeyAction::Character, PadKeyGlyph::None, nullptr},
    {2, 2, 1, 1, u"@", PadKeyAction::Character, PadKeyGlyph::None, nullptr},
    {2, 3, 1, 1, u"[", PadKeyAction::Character, PadKeyGlyph::None, nullptr},
    {2, 4, 1, 1, u"]", PadKeyAction::Character, PadKeyGlyph::None, nullptr},
    {2, 5, 1, 1, u"[]", PadKeyAction::Character, PadKeyGlyph::None, nullptr},
    {2, 6, 1, 1, u"{", PadKeyAction::Character, PadKeyGlyph::None, nullptr},
    {2, 7, 1, 1, u"}", PadKeyAction::Character, PadKeyGlyph::None, nullptr},
    {2, 8, 1, 1, u"{}", PadKeyAction::Character, PadKeyGlyph::None, nullptr},
    {2, 9, 1, 2, u">", PadKeyAction::PageNext, PadKeyGlyph::None, nullptr},
    {3, 0, 1, 1, u"\\", PadKeyAction::Character, PadKeyGlyph::None, nullptr},
    {3, 1, 1, 1, u"|", PadKeyAction::Character, PadKeyGlyph::None, nullptr},
    {3, 2, 1, 1, u"^", PadKeyAction::Character, PadKeyGlyph::None, nullptr},
    {3, 3, 1, 1, u"`", PadKeyAction::Character, PadKeyGlyph::None, nullptr},
    {3, 4, 1, 1, u"$", PadKeyAction::Character, PadKeyGlyph::None, nullptr},
    {3, 5, 1, 1, u"\u20AC", PadKeyAction::Character, PadKeyGlyph::None, nullptr},
    {3, 6, 1, 1, u"\u00B4", PadKeyAction::Character, PadKeyGlyph::None, nullptr},
    {3, 7, 1, 1, u"\u2018", PadKeyAction::Character, PadKeyGlyph::None, nullptr},
    {3, 8, 1, 1, u"\u2019", PadKeyAction::Character, PadKeyGlyph::None, nullptr},
    {4, 1, 1, 1, u"ABC", PadKeyAction::Symbols, PadKeyGlyph::None, u"L2+\u25B3"},
    {4, 3, 4, 1, u"Space", PadKeyAction::Space, PadKeyGlyph::None, u"\u25B3"},
    {4, 8, 2, 1, nullptr, PadKeyAction::Backspace, PadKeyGlyph::Backspace, u"\u25A1"},
    {5, 0, 1, 1, nullptr, PadKeyAction::ArrowDown, PadKeyGlyph::ArrowDown, nullptr},
    {5, 1, 1, 1, nullptr, PadKeyAction::ArrowUp, PadKeyGlyph::ArrowUp, nullptr},
    {5, 2, 1, 1, nullptr, PadKeyAction::ArrowLeft, PadKeyGlyph::ArrowLeft, u"L1"},
    {5, 3, 1, 1, nullptr, PadKeyAction::ArrowRight, PadKeyGlyph::ArrowRight, u"R1"},
    {5, 8, 2, 1, nullptr, PadKeyAction::Done, PadKeyGlyph::None, u"R2"},
};

constexpr PadKey SymbolsSecondKeys[] = {
    {0, 0, 1, 1, u"\u201A", PadKeyAction::Character, PadKeyGlyph::None, nullptr},
    {0, 1, 1, 1, u"\u201C", PadKeyAction::Character, PadKeyGlyph::None, nullptr},
    {0, 2, 1, 1, u"\u201D", PadKeyAction::Character, PadKeyGlyph::None, nullptr},
    {0, 3, 1, 1, u"\u201E", PadKeyAction::Character, PadKeyGlyph::None, nullptr},
    {0, 4, 1, 1, u"~", PadKeyAction::Character, PadKeyGlyph::None, nullptr},
    {0, 5, 1, 1, u"\u00A1", PadKeyAction::Character, PadKeyGlyph::None, nullptr},
    {0, 6, 1, 1, u"\u00A1!", PadKeyAction::Character, PadKeyGlyph::None, nullptr},
    {0, 7, 1, 1, u"\u00BF", PadKeyAction::Character, PadKeyGlyph::None, nullptr},
    {0, 8, 1, 1, u"\u00BF?", PadKeyAction::Character, PadKeyGlyph::None, nullptr},
    {0, 9, 1, 1, u"\u2039", PadKeyAction::Character, PadKeyGlyph::None, nullptr},
    {1, 0, 1, 1, u"\u203A", PadKeyAction::Character, PadKeyGlyph::None, nullptr},
    {1, 1, 1, 1, u"\u00AB", PadKeyAction::Character, PadKeyGlyph::None, nullptr},
    {1, 2, 1, 1, u"\u00BB", PadKeyAction::Character, PadKeyGlyph::None, nullptr},
    {1, 3, 1, 1, u"\u00B0", PadKeyAction::Character, PadKeyGlyph::None, nullptr},
    {1, 4, 1, 1, u"\u00AA", PadKeyAction::Character, PadKeyGlyph::None, nullptr},
    {1, 5, 1, 1, u"\u00BA", PadKeyAction::Character, PadKeyGlyph::None, nullptr},
    {1, 6, 1, 1, u"\u00D7", PadKeyAction::Character, PadKeyGlyph::None, nullptr},
    {1, 7, 1, 1, u"\u00F7", PadKeyAction::Character, PadKeyGlyph::None, nullptr},
    {1, 8, 1, 1, u"\u00A4", PadKeyAction::Character, PadKeyGlyph::None, nullptr},
    {1, 9, 1, 1, u"\u00A2", PadKeyAction::Character, PadKeyGlyph::None, nullptr},
    {2, 0, 1, 1, u"\u00A5", PadKeyAction::Character, PadKeyGlyph::None, nullptr},
    {2, 1, 1, 1, u"\u00A3", PadKeyAction::Character, PadKeyGlyph::None, nullptr},
    {2, 2, 1, 1, u"\u20A9", PadKeyAction::Character, PadKeyGlyph::None, nullptr},
    {2, 3, 1, 1, u"\u00A7", PadKeyAction::Character, PadKeyGlyph::None, nullptr},
    {2, 4, 1, 1, u"\u00A6", PadKeyAction::Character, PadKeyGlyph::None, nullptr},
    {2, 5, 1, 1, u"\u00B5", PadKeyAction::Character, PadKeyGlyph::None, nullptr},
    {2, 6, 1, 1, u"\u00AC", PadKeyAction::Character, PadKeyGlyph::None, nullptr},
    {2, 7, 1, 1, u"\u00B9", PadKeyAction::Character, PadKeyGlyph::None, nullptr},
    {2, 8, 1, 1, u"\u00B2", PadKeyAction::Character, PadKeyGlyph::None, nullptr},
    {2, 9, 1, 2, u"<", PadKeyAction::PagePrev, PadKeyGlyph::None, nullptr},
    {3, 0, 1, 1, u"\u00B3", PadKeyAction::Character, PadKeyGlyph::None, nullptr},
    {3, 1, 1, 1, u"\u00BC", PadKeyAction::Character, PadKeyGlyph::None, nullptr},
    {3, 2, 1, 1, u"\u00BD", PadKeyAction::Character, PadKeyGlyph::None, nullptr},
    {3, 3, 1, 1, u"\u00BE", PadKeyAction::Character, PadKeyGlyph::None, nullptr},
    {3, 4, 1, 1, u"\u2116", PadKeyAction::Character, PadKeyGlyph::None, nullptr},
    {3, 5, 1, 1, u"\u00B7", PadKeyAction::Character, PadKeyGlyph::None, nullptr},
    {4, 1, 1, 1, u"ABC", PadKeyAction::Symbols, PadKeyGlyph::None, u"L2+\u25B3"},
    {4, 3, 4, 1, u"Space", PadKeyAction::Space, PadKeyGlyph::None, u"\u25B3"},
    {4, 8, 2, 1, nullptr, PadKeyAction::Backspace, PadKeyGlyph::Backspace, u"\u25A1"},
    {5, 0, 1, 1, nullptr, PadKeyAction::ArrowDown, PadKeyGlyph::ArrowDown, nullptr},
    {5, 1, 1, 1, nullptr, PadKeyAction::ArrowUp, PadKeyGlyph::ArrowUp, nullptr},
    {5, 2, 1, 1, nullptr, PadKeyAction::ArrowLeft, PadKeyGlyph::ArrowLeft, u"L1"},
    {5, 3, 1, 1, nullptr, PadKeyAction::ArrowRight, PadKeyGlyph::ArrowRight, u"R1"},
    {5, 8, 2, 1, nullptr, PadKeyAction::Done, PadKeyGlyph::None, u"R2"},
};

constexpr PadKey SpecialsLowerKeys[] = {
    {0, 0, 1, 1, u"\u00E0", PadKeyAction::Character, PadKeyGlyph::None, nullptr},
    {0, 1, 1, 1, u"\u00E1", PadKeyAction::Character, PadKeyGlyph::None, nullptr},
    {0, 2, 1, 1, u"\u00E2", PadKeyAction::Character, PadKeyGlyph::None, nullptr},
    {0, 3, 1, 1, u"\u00E3", PadKeyAction::Character, PadKeyGlyph::None, nullptr},
    {0, 4, 1, 1, u"\u00E4", PadKeyAction::Character, PadKeyGlyph::None, nullptr},
    {0, 5, 1, 1, u"\u00E5", PadKeyAction::Character, PadKeyGlyph::None, nullptr},
    {0, 6, 1, 1, u"\u0105", PadKeyAction::Character, PadKeyGlyph::None, nullptr},
    {0, 7, 1, 1, u"\u00E6", PadKeyAction::Character, PadKeyGlyph::None, nullptr},
    {0, 8, 1, 1, u"\u00E7", PadKeyAction::Character, PadKeyGlyph::None, nullptr},
    {0, 9, 1, 1, u"\u0107", PadKeyAction::Character, PadKeyGlyph::None, nullptr},
    {1, 0, 1, 1, u"\u00E8", PadKeyAction::Character, PadKeyGlyph::None, nullptr},
    {1, 1, 1, 1, u"\u00E9", PadKeyAction::Character, PadKeyGlyph::None, nullptr},
    {1, 2, 1, 1, u"\u00EA", PadKeyAction::Character, PadKeyGlyph::None, nullptr},
    {1, 3, 1, 1, u"\u00EB", PadKeyAction::Character, PadKeyGlyph::None, nullptr},
    {1, 4, 1, 1, u"\u0119", PadKeyAction::Character, PadKeyGlyph::None, nullptr},
    {1, 5, 1, 1, u"\u011F", PadKeyAction::Character, PadKeyGlyph::None, nullptr},
    {1, 6, 1, 1, u"\u00EC", PadKeyAction::Character, PadKeyGlyph::None, nullptr},
    {1, 7, 1, 1, u"\u00ED", PadKeyAction::Character, PadKeyGlyph::None, nullptr},
    {1, 8, 1, 1, u"\u00EE", PadKeyAction::Character, PadKeyGlyph::None, nullptr},
    {1, 9, 1, 1, u"\u00EF", PadKeyAction::Character, PadKeyGlyph::None, nullptr},
    {2, 0, 1, 1, u"\u0131", PadKeyAction::Character, PadKeyGlyph::None, nullptr},
    {2, 1, 1, 1, u"\u0142", PadKeyAction::Character, PadKeyGlyph::None, nullptr},
    {2, 2, 1, 1, u"\u00F1", PadKeyAction::Character, PadKeyGlyph::None, nullptr},
    {2, 3, 1, 1, u"\u0144", PadKeyAction::Character, PadKeyGlyph::None, nullptr},
    {2, 4, 1, 1, u"\u00F2", PadKeyAction::Character, PadKeyGlyph::None, nullptr},
    {2, 5, 1, 1, u"\u00F3", PadKeyAction::Character, PadKeyGlyph::None, nullptr},
    {2, 6, 1, 1, u"\u00F4", PadKeyAction::Character, PadKeyGlyph::None, nullptr},
    {2, 7, 1, 1, u"\u00F5", PadKeyAction::Character, PadKeyGlyph::None, nullptr},
    {2, 8, 1, 1, u"\u00F6", PadKeyAction::Character, PadKeyGlyph::None, nullptr},
    {2, 9, 1, 1, u"\u00F8", PadKeyAction::Character, PadKeyGlyph::None, nullptr},
    {3, 0, 1, 1, u"\u0153", PadKeyAction::Character, PadKeyGlyph::None, nullptr},
    {3, 1, 1, 1, u"\u015B", PadKeyAction::Character, PadKeyGlyph::None, nullptr},
    {3, 2, 1, 1, u"\u015F", PadKeyAction::Character, PadKeyGlyph::None, nullptr},
    {3, 3, 1, 1, u"\u0161", PadKeyAction::Character, PadKeyGlyph::None, nullptr},
    {3, 4, 1, 1, u"\u00DF", PadKeyAction::Character, PadKeyGlyph::None, nullptr},
    {3, 5, 1, 1, u"\u00F9", PadKeyAction::Character, PadKeyGlyph::None, nullptr},
    {3, 6, 1, 1, u"\u00FA", PadKeyAction::Character, PadKeyGlyph::None, nullptr},
    {3, 7, 1, 1, u"\u00FB", PadKeyAction::Character, PadKeyGlyph::None, nullptr},
    {3, 8, 1, 1, u"\u00FC", PadKeyAction::Character, PadKeyGlyph::None, nullptr},
    {3, 9, 1, 1, u"\u00FD", PadKeyAction::Character, PadKeyGlyph::None, nullptr},
    {4, 0, 1, 1, u"\u00FF", PadKeyAction::Character, PadKeyGlyph::None, nullptr},
    {4, 1, 1, 1, u"\u017A", PadKeyAction::Character, PadKeyGlyph::None, nullptr},
    {4, 2, 1, 1, u"\u017C", PadKeyAction::Character, PadKeyGlyph::None, nullptr},
    {4, 3, 1, 1, u"\u017E", PadKeyAction::Character, PadKeyGlyph::None, nullptr},
    {4, 4, 1, 1, u"\u00F0", PadKeyAction::Character, PadKeyGlyph::None, nullptr},
    {4, 5, 1, 1, u"\u00FE", PadKeyAction::Character, PadKeyGlyph::None, nullptr},
    {5, 0, 1, 1, nullptr, PadKeyAction::Shift, PadKeyGlyph::Shift, u"L2"},
    {5, 1, 1, 1, u"@#:", PadKeyAction::Symbols, PadKeyGlyph::None, u"L2+\u25B3"},
    {5, 2, 1, 1, u"ABC", PadKeyAction::Specials, PadKeyGlyph::None, u"L3"},
    {5, 3, 4, 1, u"Space", PadKeyAction::Space, PadKeyGlyph::None, u"\u25B3"},
    {5, 8, 2, 1, nullptr, PadKeyAction::Backspace, PadKeyGlyph::Backspace, u"\u25A1"},
    {6, 0, 1, 1, nullptr, PadKeyAction::ArrowDown, PadKeyGlyph::ArrowDown, nullptr},
    {6, 1, 1, 1, nullptr, PadKeyAction::ArrowUp, PadKeyGlyph::ArrowUp, nullptr},
    {6, 2, 1, 1, nullptr, PadKeyAction::ArrowLeft, PadKeyGlyph::ArrowLeft, u"L1"},
    {6, 3, 1, 1, nullptr, PadKeyAction::ArrowRight, PadKeyGlyph::ArrowRight, u"R1"},
    {6, 8, 2, 1, nullptr, PadKeyAction::Done, PadKeyGlyph::None, u"R2"},
};

constexpr PadKey SpecialsUpperKeys[] = {
    {0, 0, 1, 1, u"\u00C0", PadKeyAction::Character, PadKeyGlyph::None, nullptr},
    {0, 1, 1, 1, u"\u00C1", PadKeyAction::Character, PadKeyGlyph::None, nullptr},
    {0, 2, 1, 1, u"\u00C2", PadKeyAction::Character, PadKeyGlyph::None, nullptr},
    {0, 3, 1, 1, u"\u00C3", PadKeyAction::Character, PadKeyGlyph::None, nullptr},
    {0, 4, 1, 1, u"\u00C4", PadKeyAction::Character, PadKeyGlyph::None, nullptr},
    {0, 5, 1, 1, u"\u00C5", PadKeyAction::Character, PadKeyGlyph::None, nullptr},
    {0, 6, 1, 1, u"\u0104", PadKeyAction::Character, PadKeyGlyph::None, nullptr},
    {0, 7, 1, 1, u"\u00C6", PadKeyAction::Character, PadKeyGlyph::None, nullptr},
    {0, 8, 1, 1, u"\u00C7", PadKeyAction::Character, PadKeyGlyph::None, nullptr},
    {0, 9, 1, 1, u"\u0106", PadKeyAction::Character, PadKeyGlyph::None, nullptr},
    {1, 0, 1, 1, u"\u00C8", PadKeyAction::Character, PadKeyGlyph::None, nullptr},
    {1, 1, 1, 1, u"\u00C9", PadKeyAction::Character, PadKeyGlyph::None, nullptr},
    {1, 2, 1, 1, u"\u00CA", PadKeyAction::Character, PadKeyGlyph::None, nullptr},
    {1, 3, 1, 1, u"\u00CB", PadKeyAction::Character, PadKeyGlyph::None, nullptr},
    {1, 4, 1, 1, u"\u0118", PadKeyAction::Character, PadKeyGlyph::None, nullptr},
    {1, 5, 1, 1, u"\u011E", PadKeyAction::Character, PadKeyGlyph::None, nullptr},
    {1, 6, 1, 1, u"\u00CC", PadKeyAction::Character, PadKeyGlyph::None, nullptr},
    {1, 7, 1, 1, u"\u00CD", PadKeyAction::Character, PadKeyGlyph::None, nullptr},
    {1, 8, 1, 1, u"\u00CE", PadKeyAction::Character, PadKeyGlyph::None, nullptr},
    {1, 9, 1, 1, u"\u00CF", PadKeyAction::Character, PadKeyGlyph::None, nullptr},
    {2, 0, 1, 1, u"\u0130", PadKeyAction::Character, PadKeyGlyph::None, nullptr},
    {2, 1, 1, 1, u"\u0141", PadKeyAction::Character, PadKeyGlyph::None, nullptr},
    {2, 2, 1, 1, u"\u00D1", PadKeyAction::Character, PadKeyGlyph::None, nullptr},
    {2, 3, 1, 1, u"\u0143", PadKeyAction::Character, PadKeyGlyph::None, nullptr},
    {2, 4, 1, 1, u"\u00D2", PadKeyAction::Character, PadKeyGlyph::None, nullptr},
    {2, 5, 1, 1, u"\u00D3", PadKeyAction::Character, PadKeyGlyph::None, nullptr},
    {2, 6, 1, 1, u"\u00D4", PadKeyAction::Character, PadKeyGlyph::None, nullptr},
    {2, 7, 1, 1, u"\u00D5", PadKeyAction::Character, PadKeyGlyph::None, nullptr},
    {2, 8, 1, 1, u"\u00D6", PadKeyAction::Character, PadKeyGlyph::None, nullptr},
    {2, 9, 1, 1, u"\u00D8", PadKeyAction::Character, PadKeyGlyph::None, nullptr},
    {3, 0, 1, 1, u"\u0152", PadKeyAction::Character, PadKeyGlyph::None, nullptr},
    {3, 1, 1, 1, u"\u015A", PadKeyAction::Character, PadKeyGlyph::None, nullptr},
    {3, 2, 1, 1, u"\u015E", PadKeyAction::Character, PadKeyGlyph::None, nullptr},
    {3, 3, 1, 1, u"\u0160", PadKeyAction::Character, PadKeyGlyph::None, nullptr},
    {3, 4, 1, 1, u"\u00DF", PadKeyAction::Character, PadKeyGlyph::None, nullptr},
    {3, 5, 1, 1, u"\u00D9", PadKeyAction::Character, PadKeyGlyph::None, nullptr},
    {3, 6, 1, 1, u"\u00DA", PadKeyAction::Character, PadKeyGlyph::None, nullptr},
    {3, 7, 1, 1, u"\u00DB", PadKeyAction::Character, PadKeyGlyph::None, nullptr},
    {3, 8, 1, 1, u"\u00DC", PadKeyAction::Character, PadKeyGlyph::None, nullptr},
    {3, 9, 1, 1, u"\u00DD", PadKeyAction::Character, PadKeyGlyph::None, nullptr},
    {4, 0, 1, 1, u"\u0178", PadKeyAction::Character, PadKeyGlyph::None, nullptr},
    {4, 1, 1, 1, u"\u0179", PadKeyAction::Character, PadKeyGlyph::None, nullptr},
    {4, 2, 1, 1, u"\u017B", PadKeyAction::Character, PadKeyGlyph::None, nullptr},
    {4, 3, 1, 1, u"\u017D", PadKeyAction::Character, PadKeyGlyph::None, nullptr},
    {4, 4, 1, 1, u"\u00D0", PadKeyAction::Character, PadKeyGlyph::None, nullptr},
    {4, 5, 1, 1, u"\u00DE", PadKeyAction::Character, PadKeyGlyph::None, nullptr},
    {5, 0, 1, 1, nullptr, PadKeyAction::Shift, PadKeyGlyph::Shift, u"L2"},
    {5, 1, 1, 1, u"@#:", PadKeyAction::Symbols, PadKeyGlyph::None, u"L2+\u25B3"},
    {5, 2, 1, 1, u"ABC", PadKeyAction::Specials, PadKeyGlyph::None, u"L3"},
    {5, 3, 4, 1, u"Space", PadKeyAction::Space, PadKeyGlyph::None, u"\u25B3"},
    {5, 8, 2, 1, nullptr, PadKeyAction::Backspace, PadKeyGlyph::Backspace, u"\u25A1"},
    {6, 0, 1, 1, nullptr, PadKeyAction::ArrowDown, PadKeyGlyph::ArrowDown, nullptr},
    {6, 1, 1, 1, nullptr, PadKeyAction::ArrowUp, PadKeyGlyph::ArrowUp, nullptr},
    {6, 2, 1, 1, nullptr, PadKeyAction::ArrowLeft, PadKeyGlyph::ArrowLeft, u"L1"},
    {6, 3, 1, 1, nullptr, PadKeyAction::ArrowRight, PadKeyGlyph::ArrowRight, u"R1"},
    {6, 8, 2, 1, nullptr, PadKeyAction::Done, PadKeyGlyph::None, u"R2"},
};

constexpr std::uint8_t Columns = 10;
constexpr std::uint64_t LanguageEnglishUs = 0x0000000000000004;
constexpr std::uint64_t LanguageJapanese = 0x0000000000002000;
constexpr std::uint64_t LanguageKorean = 0x0000000000004000;
constexpr std::uint64_t LanguagePortugueseBr = 0x0000000000020000;
constexpr std::uint64_t LanguageEnglishGb = 0x0000000000040000;
constexpr std::uint64_t LanguageSpanishLa = 0x0000000000100000;

struct Layout {
    std::span<const PadKey> keys;
    std::uint8_t rows;
};

struct Repeated {
    bool pressed = false;
    bool repeat = false;
};

struct StickPulse {
    bool left = false;
    bool right = false;
    bool up = false;
    bool down = false;
    bool repeat = false;
};

Layout layoutOf(PadKeyboardFamily family, PadKeyboardCase caseState, std::uint8_t page) {
    switch (family) {
        case PadKeyboardFamily::Symbols:
            return page == 0 ? Layout{SymbolsFirstKeys, 6} : Layout{SymbolsSecondKeys, 6};
        case PadKeyboardFamily::Specials:
            return caseState == PadKeyboardCase::Lower ? Layout{SpecialsLowerKeys, 7} : Layout{SpecialsUpperKeys, 7};
        case PadKeyboardFamily::Latin:
            break;
    }
    return caseState == PadKeyboardCase::Lower ? Layout{LatinLowerKeys, 6} : Layout{LatinUpperKeys, 6};
}

Layout layoutOf(const PadKeyboardState& state) {
    return layoutOf(state.family, state.caseState, state.page);
}

const PadKey* keyAt(const Layout& layout, int row, int column) {
    if (row < 0 || column < 0 || row >= layout.rows || column >= Columns) return nullptr;
    for (const auto& key : layout.keys) {
        if (row >= key.row && row < key.row + key.rowSpan && column >= key.column && column < key.column + key.columnSpan) return &key;
    }
    return nullptr;
}

std::pair<int, int> nearestOnRow(const Layout& layout, int row, int column, int prefer) {
    if (row < 0 || row >= layout.rows) return {-1, -1};
    int best = -1;
    int bestDistance = std::numeric_limits<int>::max();
    bool bestInDirection = false;
    for (int candidate = 0; candidate < Columns; ++candidate) {
        if (keyAt(layout, row, candidate) == nullptr) continue;
        const int distance = std::abs(candidate - column);
        const bool inDirection = (prefer > 0 && candidate > column) || (prefer < 0 && candidate < column);
        const bool betterTie = prefer != 0 && inDirection != bestInDirection ? inDirection : candidate < best;
        if (distance < bestDistance || (distance == bestDistance && (best < 0 || betterTie))) {
            bestDistance = distance;
            best = candidate;
            bestInDirection = inDirection;
        }
    }
    return best >= 0 ? std::pair{row, best} : std::pair{-1, -1};
}

std::pair<int, int> nearest(const Layout& layout, int row, int column) {
    std::pair<int, int> best{-1, -1};
    int bestDistance = std::numeric_limits<int>::max();
    for (int candidateRow = 0; candidateRow < layout.rows; ++candidateRow) {
        for (int candidate = 0; candidate < Columns; ++candidate) {
            if (keyAt(layout, candidateRow, candidate) == nullptr) continue;
            const int distance = std::abs(candidateRow - row) + std::abs(candidate - column);
            if (distance < bestDistance) {
                bestDistance = distance;
                best = {candidateRow, candidate};
            }
        }
    }
    return best;
}

std::pair<int, int> visibleCell(const PadKeyboardState& state) {
    const auto layout = layoutOf(state);
    if (keyAt(layout, state.cursorRow, state.cursorColumn) != nullptr) return {state.cursorRow, state.cursorColumn};
    const auto onRow = nearestOnRow(layout, state.cursorRow, state.cursorColumn, state.preferColumn);
    if (onRow.first >= 0) return onRow;
    return nearest(layout, std::clamp(state.cursorRow, 0, layout.rows - 1), std::clamp(state.cursorColumn, 0, Columns - 1));
}

Repeated repeatButton(bool down, bool edge, double now, PadRepeat& repeat) {
    Repeated result;
    if (!down) {
        repeat = {};
        return result;
    }
    if (!repeat.down) {
        repeat.down = true;
        repeat.next = now + PadKeyRepeatDelay;
        result.pressed = edge;
        return result;
    }
    if (edge) {
        repeat.next = now + PadKeyRepeatDelay;
        result.pressed = true;
        return result;
    }
    if (now >= repeat.next) {
        repeat.next = now + PadKeyRepeatRate;
        result.pressed = true;
        result.repeat = true;
    }
    return result;
}

double axis(std::uint8_t value) {
    const double centred = std::clamp((static_cast<double>(value) - 128.0) / 127.0, -1.0, 1.0);
    return std::abs(centred) < PadStickDeadzone ? 0.0 : centred;
}

std::array<bool, 4> stickDirections(double x, double y) {
    return {x <= -PadStickThreshold, x >= PadStickThreshold, y <= -PadStickThreshold, y >= PadStickThreshold};
}

StickPulse stickPulse(PadKeyboardState& state, double x, double y, const std::array<bool, 4>& edges, double now) {
    StickPulse result;
    const double absX = std::abs(x);
    const double absY = std::abs(y);
    int direction = 0;
    double strength = 0.0;
    if (absX >= PadStickThreshold || absY >= PadStickThreshold) {
        if (absX >= absY) {
            strength = absX;
            direction = x < 0.0 ? 1 : 2;
        } else {
            strength = absY;
            direction = y < 0.0 ? 3 : 4;
        }
    }
    if (direction == 0) {
        state.stickDirection = 0;
        state.stickNext = 0.0;
        return result;
    }
    const double tilt = std::clamp((strength - PadStickThreshold) / (1.0 - PadStickThreshold), 0.0, 1.0);
    const double initialDelay = std::lerp(0.26, 0.16, tilt);
    const double repeatInterval = std::lerp(0.20, 0.11, tilt);
    const bool edge = edges[static_cast<std::size_t>(direction - 1)];
    bool pulse = false;
    if (state.stickDirection != direction) {
        const bool fastRotate = state.stickDirection != 0 && now < state.stickNext;
        pulse = state.stickDirection == 0 || edge || fastRotate;
        state.stickDirection = direction;
        state.stickNext = now + initialDelay;
    } else if (edge) {
        pulse = true;
        state.stickNext = now + initialDelay;
    } else if (now >= state.stickNext) {
        pulse = true;
        result.repeat = true;
        state.stickNext = now + repeatInterval;
    }
    if (!pulse) return result;
    result.left = direction == 1;
    result.right = direction == 2;
    result.up = direction == 3;
    result.down = direction == 4;
    return result;
}

bool delayWrap(PadKeyboardState& state, int stepRow, int stepColumn, bool repeatHint, bool wraps, double now) {
    if (state.wrapHold && (state.wrapRow != stepRow || state.wrapColumn != stepColumn)) state.wrapHold = false;
    if (!wraps) return false;
    const bool sameDirection = state.wrapHold && state.wrapRow == stepRow && state.wrapColumn == stepColumn;
    if (!repeatHint && !sameDirection) return false;
    if (!sameDirection) {
        state.wrapHold = true;
        state.wrapRow = stepRow;
        state.wrapColumn = stepColumn;
        state.wrapRelease = now + PadEdgeWrapHold;
    }
    return now < state.wrapRelease;
}

void move(PadKeyboardState& state, int stepRow, int stepColumn, bool repeatHint, double now) {
    const auto layout = layoutOf(state);
    int row = state.cursorRow;
    int column = state.cursorColumn;
    const PadKey* origin = keyAt(layout, row, column);
    bool wrapped = false;
    for (int step = 0; step < layout.rows * Columns; ++step) {
        const int nextRow = row + stepRow;
        const int nextColumn = column + stepColumn;
        wrapped = wrapped || nextRow < 0 || nextRow >= layout.rows || nextColumn < 0 || nextColumn >= Columns;
        row = (nextRow + layout.rows) % layout.rows;
        column = (nextColumn + Columns) % Columns;
        const PadKey* candidate = keyAt(layout, row, column);
        if (candidate == nullptr || candidate == origin) continue;
        if (delayWrap(state, stepRow, stepColumn, repeatHint, wrapped, now)) return;
        state.cursorRow = row;
        state.cursorColumn = column;
        state.preferColumn = stepColumn;
        return;
    }
}

void changeLayout(PadKeyboardState& state, PadKeyboardFamily family, PadKeyboardCase caseState, std::uint8_t page) {
    if (state.family == family && state.caseState == caseState && state.page == page) return;
    state.family = family;
    state.caseState = caseState;
    state.page = page;
    state.wrapHold = false;
    const auto layout = layoutOf(state);
    if (state.cursorRow < 0 || state.cursorRow >= layout.rows || state.cursorColumn < 0 || state.cursorColumn >= Columns) {
        const auto anchor = nearest(layout, std::clamp(state.cursorRow, 0, layout.rows - 1), std::clamp(state.cursorColumn, 0, Columns - 1));
        state.cursorRow = anchor.first;
        state.cursorColumn = anchor.second;
    }
}

void cycleCase(PadKeyboardState& state) {
    const auto next = state.caseState == PadKeyboardCase::Lower ? PadKeyboardCase::Upper : state.caseState == PadKeyboardCase::Upper ? PadKeyboardCase::CapsLock : PadKeyboardCase::Lower;
    changeLayout(state, state.family, next, state.page);
}

void toggleFamily(PadKeyboardState& state, PadKeyboardFamily target) {
    const auto set = [&](PadKeyboardFamily family) {
        if (family == PadKeyboardFamily::Latin || family == PadKeyboardFamily::Specials) state.alphaFamily = family;
        changeLayout(state, family, state.caseState, 0);
    };
    if (target == PadKeyboardFamily::Symbols) {
        if (state.family == PadKeyboardFamily::Symbols) {
            set(state.alphaFamily);
        } else {
            state.alphaFamily = state.family;
            set(PadKeyboardFamily::Symbols);
        }
        return;
    }
    set(state.family == target ? PadKeyboardFamily::Latin : target);
}

void focusAction(PadKeyboardState& state, PadKeyAction action) {
    const auto layout = layoutOf(state);
    for (const auto& key : layout.keys) {
        if (key.action != action) continue;
        auto target = nearestOnRow(layout, key.row, key.column, 0);
        if (target.first < 0) target = nearest(layout, key.row, key.column);
        if (target.first < 0) return;
        state.cursorRow = target.first;
        state.cursorColumn = target.second;
        state.preferColumn = 0;
        state.wrapHold = false;
        return;
    }
}

void flipPage(PadKeyboardState& state, int direction) {
    if (state.family != PadKeyboardFamily::Symbols && state.family != PadKeyboardFamily::Specials) return;
    changeLayout(state, state.family, state.caseState, static_cast<std::uint8_t>((state.page + direction + 2) % 2));
}

bool allowsRepeat(PadKeyAction action) {
    switch (action) {
        case PadKeyAction::Character:
        case PadKeyAction::Space:
        case PadKeyAction::Backspace:
        case PadKeyAction::ArrowLeft:
        case PadKeyAction::ArrowRight:
        case PadKeyAction::ArrowUp:
        case PadKeyAction::ArrowDown:
            return true;
        default:
            return false;
    }
}

const char16_t* currencyLabel(PadKeyboardCurrency currency) {
    switch (currency) {
        case PadKeyboardCurrency::Dollar: return u"$";
        case PadKeyboardCurrency::Pound: return u"£";
        case PadKeyboardCurrency::Yen: return u"¥";
        case PadKeyboardCurrency::Won: return u"₩";
        case PadKeyboardCurrency::Euro: break;
    }
    return u"€";
}

const char16_t* enterLabel(std::uint32_t label) {
    switch (label) {
        case 1: return u"Send";
        case 2: return u"Search";
        case 3: return u"Go";
        default: return u"Done";
    }
}

void applyKey(PadKeyboardState& state, const PadKey& key, PadKeyboardResult& result) {
    switch (key.action) {
        case PadKeyAction::Character:
            result.edit = PadEdit::Insert;
            result.text = PadKeyboardLabel(state, key);
            state.awaitingInsert = true;
            return;
        case PadKeyAction::Shift:
            cycleCase(state);
            return;
        case PadKeyAction::Symbols:
            toggleFamily(state, PadKeyboardFamily::Symbols);
            focusAction(state, PadKeyAction::Symbols);
            return;
        case PadKeyAction::Specials:
            toggleFamily(state, PadKeyboardFamily::Specials);
            focusAction(state, PadKeyAction::Specials);
            return;
        case PadKeyAction::Space:
            result.edit = PadEdit::Insert;
            result.text = u" ";
            return;
        case PadKeyAction::Backspace:
            result.edit = PadEdit::Backspace;
            return;
        case PadKeyAction::ArrowLeft:
            result.edit = PadEdit::CaretLeft;
            return;
        case PadKeyAction::ArrowRight:
            result.edit = PadEdit::CaretRight;
            return;
        case PadKeyAction::ArrowUp:
            if (!state.options.multiline) result.edit = PadEdit::CaretHome;
            return;
        case PadKeyAction::ArrowDown:
            if (!state.options.multiline) result.edit = PadEdit::CaretEnd;
            return;
        case PadKeyAction::PageNext:
            flipPage(state, 1);
            return;
        case PadKeyAction::PagePrev:
            flipPage(state, -1);
            return;
        case PadKeyAction::Done:
            result.finish = PadFinish::Done;
            return;
    }
}

}

PadKeyboardState PadKeyboardOpen(const PadKeyboardOptions& options) {
    PadKeyboardState state;
    state.options = options;
    state.family = options.family;
    state.alphaFamily = options.family == PadKeyboardFamily::Symbols ? PadKeyboardFamily::Latin : options.family;
    state.caseState = options.family == PadKeyboardFamily::Symbols ? PadKeyboardCase::Lower : options.caseState;
    const bool latinLower = state.family == PadKeyboardFamily::Latin && state.caseState == PadKeyboardCase::Lower;
    const auto layout = layoutOf(state);
    auto start = latinLower ? nearestOnRow(layout, 2, 4, 0) : nearest(layout, 0, 0);
    state.cursorRow = start.first;
    state.cursorColumn = start.second;
    return state;
}

PadKeyboardResult PadKeyboardStep(PadKeyboardState& state, const PadSample& sample, PadKeyboardState::Clock::time_point now) {
    PadKeyboardResult result;
    const double x = axis(sample.leftX);
    const double y = axis(sample.leftY);
    auto directions = stickDirections(x, y);
    const bool deflected = directions[0] || directions[1] || directions[2] || directions[3];
    if (!state.started) {
        state.started = true;
        state.origin = now;
        state.heldAtOpen = sample.buttons;
        state.stickHeldAtOpen = deflected;
        state.previous = 0;
        state.previousStick = {};
    }
    const double time = std::chrono::duration<double>(now - state.origin).count();
    state.heldAtOpen &= sample.buttons;
    const std::uint32_t buttons = sample.buttons & ~state.heldAtOpen;
    if (state.stickHeldAtOpen && !deflected) state.stickHeldAtOpen = false;
    const bool stickUsable = !state.stickHeldAtOpen;
    if (!stickUsable) directions = {};
    const auto down = [&](std::uint32_t mask) { return (buttons & mask) != 0; };
    const auto pressed = [&](std::uint32_t mask) { return (buttons & mask) != 0 && (state.previous & mask) == 0; };
    const auto commit = [&] {
        state.previous = buttons;
        state.previousStick = directions;
    };
    if (!state.crossArmed && !down(PadButtonCross)) state.crossArmed = true;
    if (!state.options.controller) {
        state.square = state.l1 = state.r1 = {};
        state.l2Armed = true;
        state.triangleNext = 0.0;
        state.stickDirection = 0;
        state.stickNext = 0.0;
        commit();
        return result;
    }
    const auto crossRepeat = repeatButton(down(PadButtonCross), pressed(PadButtonCross), time, state.cross);
    const auto left = repeatButton(down(PadButtonLeft), pressed(PadButtonLeft), time, state.left);
    const auto right = repeatButton(down(PadButtonRight), pressed(PadButtonRight), time, state.right);
    const auto upward = repeatButton(down(PadButtonUp), pressed(PadButtonUp), time, state.up);
    const auto downward = repeatButton(down(PadButtonDown), pressed(PadButtonDown), time, state.down);
    std::array<bool, 4> stickEdges{};
    for (std::size_t i = 0; i < 4; ++i) stickEdges[i] = directions[i] && !state.previousStick[i];
    StickPulse stick;
    if (stickUsable) {
        stick = stickPulse(state, x, y, stickEdges, time);
    } else {
        state.stickDirection = 0;
        state.stickNext = 0.0;
    }
    const bool navLeft = left.pressed || stick.left;
    const bool navRight = right.pressed || stick.right;
    const bool navUp = upward.pressed || stick.up;
    const bool navDown = downward.pressed || stick.down;
    if (navLeft) {
        move(state, 0, -1, left.repeat || (stick.left && stick.repeat), time);
    } else if (navRight) {
        move(state, 0, 1, right.repeat || (stick.right && stick.repeat), time);
    } else if (navUp) {
        move(state, -1, 0, upward.repeat || (stick.up && stick.repeat), time);
    } else if (navDown) {
        move(state, 1, 0, downward.repeat || (stick.down && stick.repeat), time);
    }
    const bool activatePressed = state.crossArmed && (pressed(PadButtonCross) || crossRepeat.repeat);
    bool activated = false;
    if (activatePressed) {
        const auto [row, column] = visibleCell(state);
        if (const PadKey* key = keyAt(layoutOf(state), row, column); key != nullptr) {
            const bool repeat = crossRepeat.repeat;
            if (!repeat || allowsRepeat(key->action)) {
                applyKey(state, *key, result);
                activated = true;
            }
        }
    }
    if (!activated) {
        const bool l1Down = down(PadButtonL1);
        const bool l1Edge = pressed(PadButtonL1);
        const bool l1Repeat = repeatButton(l1Down, l1Edge, time, state.l1).pressed;
        const bool squareDown = down(PadButtonSquare);
        const bool squareEdge = pressed(PadButtonSquare);
        const bool squareRepeat = repeatButton(squareDown, squareEdge, time, state.square).pressed;
        const bool r1Repeat = repeatButton(down(PadButtonR1), pressed(PadButtonR1), time, state.r1).pressed;
        const bool clearAll = (l1Down && squareEdge) || (squareDown && l1Edge);
        const bool l2Down = down(PadButtonL2);
        const bool l2Edge = pressed(PadButtonL2);
        const double l2Analog = std::max(static_cast<double>(sample.l2) / 255.0, l2Down ? 1.0 : 0.0);
        if (!l2Down && l2Analog <= PadTriggerRelease) state.l2Armed = true;
        bool l2Pressed = false;
        if (state.l2Armed && l2Edge) {
            state.l2Armed = false;
            l2Pressed = true;
        }
        const bool triangleDown = down(PadButtonTriangle);
        const bool triangleEdge = pressed(PadButtonTriangle);
        PadRepeat triangle{(state.previous & PadButtonTriangle) != 0, state.triangleNext};
        const bool trianglePressed = repeatButton(triangleDown, triangleEdge, time, triangle).pressed;
        state.triangleNext = triangle.next;
        const bool space = trianglePressed && !l2Down;
        const bool symbols = (l2Down && triangleEdge) || (triangleDown && l2Pressed);
        if (symbols) {
            toggleFamily(state, PadKeyboardFamily::Symbols);
        } else if (space) {
            result.edit = PadEdit::Insert;
            result.text = u" ";
        } else if (state.family != PadKeyboardFamily::Symbols && pressed(PadButtonL3)) {
            toggleFamily(state, PadKeyboardFamily::Specials);
        } else if (pressed(PadButtonR2)) {
            result.finish = PadFinish::Done;
        } else if (clearAll) {
            result.edit = PadEdit::Clear;
        } else if (squareRepeat) {
            result.edit = PadEdit::Backspace;
        } else if (l1Repeat) {
            result.edit = PadEdit::CaretLeft;
        } else if (r1Repeat) {
            result.edit = PadEdit::CaretRight;
        } else if (!pressed(PadButtonR3) && state.family != PadKeyboardFamily::Symbols && l2Pressed) {
            cycleCase(state);
        }
    }
    if (result.finish == PadFinish::None && pressed(PadButtonCircle)) result.finish = PadFinish::Cancel;
    commit();
    return result;
}

void PadKeyboardNoteInsert(PadKeyboardState& state, bool inserted) {
    const bool typed = std::exchange(state.awaitingInsert, false);
    if (typed && inserted && state.caseState == PadKeyboardCase::Upper) changeLayout(state, state.family, PadKeyboardCase::Lower, state.page);
}

std::span<const PadKey> PadKeyboardKeys(const PadKeyboardState& state) {
    return layoutOf(state).keys;
}

std::uint8_t PadKeyboardRows(const PadKeyboardState& state) {
    return layoutOf(state).rows;
}

std::uint8_t PadKeyboardColumns() {
    return Columns;
}

const PadKey* PadKeyboardSelected(const PadKeyboardState& state) {
    const auto [row, column] = visibleCell(state);
    return keyAt(layoutOf(state), row, column);
}

std::u16string PadKeyboardLabel(const PadKeyboardState& state, const PadKey& key) {
    if (key.action == PadKeyAction::Done) return enterLabel(state.options.enterLabel);
    if (key.action == PadKeyAction::Character && state.family == PadKeyboardFamily::Symbols && state.page == 0 && key.row == 3 && key.column == 5) return currencyLabel(state.options.currency);
    return key.label != nullptr ? std::u16string(key.label) : std::u16string();
}

PadKeyboardCurrency PadKeyboardCurrencyFor(std::uint64_t supportedLanguages) {
    if ((supportedLanguages & LanguageKorean) != 0) return PadKeyboardCurrency::Won;
    if ((supportedLanguages & LanguageJapanese) != 0) return PadKeyboardCurrency::Yen;
    if ((supportedLanguages & LanguageEnglishGb) != 0) return PadKeyboardCurrency::Pound;
    if ((supportedLanguages & (LanguageEnglishUs | LanguageSpanishLa | LanguagePortugueseBr)) != 0) return PadKeyboardCurrency::Dollar;
    return PadKeyboardCurrency::Euro;
}

}
