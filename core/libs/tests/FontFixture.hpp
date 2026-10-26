#ifndef CORE_LIBS_TESTS_FONTFIXTURE_HPP
#define CORE_LIBS_TESTS_FONTFIXTURE_HPP

#include <cstddef>
#include <initializer_list>
#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace FontFixture {

inline void Put16(std::vector<unsigned char>& out, int value) {
    out.push_back(static_cast<unsigned char>((value >> 8) & 0xFF));
    out.push_back(static_cast<unsigned char>(value & 0xFF));
}

inline std::vector<unsigned char> Words(std::initializer_list<int> values) {
    std::vector<unsigned char> out;
    for (const int value : values) Put16(out, value);
    return out;
}

inline std::vector<unsigned char> BitmapFont() {
    const std::string_view font =
        "STARTFONT 2.1\n"
        "FONT -anyps5-fixture-medium-r-normal--1-10-75-75-c-10-iso10646-1\n"
        "SIZE 1 75 75\n"
        "FONTBOUNDINGBOX 1 1 0 0\n"
        "STARTPROPERTIES 4\n"
        "FONT_ASCENT 1\n"
        "FONT_DESCENT 0\n"
        "CHARSET_REGISTRY \"ISO10646\"\n"
        "CHARSET_ENCODING \"1\"\n"
        "ENDPROPERTIES\n"
        "CHARS 1\n"
        "STARTCHAR A\n"
        "ENCODING 65\n"
        "SWIDTH 1000 0\n"
        "DWIDTH 1 0\n"
        "BBX 1 1 0 0\n"
        "BITMAP\n"
        "80\n"
        "ENDCHAR\n"
        "ENDFONT\n";
    return {font.begin(), font.end()};
}

inline std::vector<unsigned char> BuildFont(int glyphCount, std::map<std::string, std::vector<unsigned char>> tables) {
    tables["glyf"] = Words({0, 0});
    tables["head"] = Words({1, 0, 1, 0, 0, 0, 0x5F0F, 0x3CF5, 0, 1000, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 8, 2, 0, 0});
    tables["hhea"] = Words({1, 0, 800, -200, 0, 500, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1});
    tables["hmtx"] = Words({500});
    tables["hmtx"].resize(2 + 2 * static_cast<std::size_t>(glyphCount));
    tables["loca"].assign(2 * (static_cast<std::size_t>(glyphCount) + 1), 0);
    tables["maxp"] = Words({1, 0, glyphCount, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0});
    const int tableCount = static_cast<int>(tables.size());
    int entrySelector = 0;
    while ((2 << entrySelector) <= tableCount) ++entrySelector;
    std::vector<unsigned char> font = Words({1, 0, tableCount, 16 << entrySelector, entrySelector, 16 * tableCount - (16 << entrySelector)});
    int offset = 12 + 16 * tableCount;
    for (const auto& table : tables) {
        font.insert(font.end(), table.first.begin(), table.first.end());
        for (const int value : {0, 0, offset >> 16, offset & 0xFFFF, 0, static_cast<int>(table.second.size())}) Put16(font, value);
        offset += static_cast<int>((table.second.size() + 3) & ~std::size_t{3});
    }
    for (const auto& table : tables) {
        font.insert(font.end(), table.second.begin(), table.second.end());
        font.resize((font.size() + 3) & ~std::size_t{3});
    }
    return font;
}

}

#endif
