#ifndef CORE_LIBS_TESTS_TYPE1FIXTURE_HPP
#define CORE_LIBS_TESTS_TYPE1FIXTURE_HPP

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace Type1Fixture {

inline std::vector<unsigned char> Encrypt(const std::vector<unsigned char>& data, std::uint16_t key) {
    std::vector<unsigned char> out;
    std::vector<unsigned char> plain(4, 0);
    plain.insert(plain.end(), data.begin(), data.end());
    for (const unsigned char byte : plain) {
        const unsigned char cipher = static_cast<unsigned char>(byte ^ (key >> 8));
        key = static_cast<std::uint16_t>((cipher + key) * 52845u + 22719u);
        out.push_back(cipher);
    }
    return out;
}

inline void Number(std::vector<unsigned char>& out, int value) {
    if (value >= -107 && value <= 107) {
        out.push_back(static_cast<unsigned char>(value + 139));
    } else if (value >= 108 && value <= 1131) {
        value -= 108;
        out.push_back(static_cast<unsigned char>(247 + (value >> 8)));
        out.push_back(static_cast<unsigned char>(value & 255));
    } else {
        value = -value - 108;
        out.push_back(static_cast<unsigned char>(251 + (value >> 8)));
        out.push_back(static_cast<unsigned char>(value & 255));
    }
}

inline std::vector<unsigned char> Square() {
    std::vector<unsigned char> out;
    Number(out, 50);
    Number(out, 600);
    out.push_back(13);
    Number(out, 0);
    Number(out, 0);
    out.push_back(21);
    Number(out, 500);
    Number(out, 0);
    out.push_back(5);
    Number(out, 0);
    Number(out, 700);
    out.push_back(5);
    Number(out, -500);
    Number(out, 0);
    out.push_back(5);
    out.push_back(9);
    out.push_back(14);
    return out;
}

inline std::vector<unsigned char> Empty() {
    std::vector<unsigned char> out;
    Number(out, 0);
    Number(out, 500);
    out.push_back(13);
    out.push_back(14);
    return out;
}

inline std::vector<unsigned char> Font() {
    const std::string header =
        "%!PS-AdobeFont-1.0: FixtureType1 001.000\n%%Title: FixtureType1\n/FontInfo 10 dict dup begin\n/version (001.000) readonly def\n"
        "/Notice (Fixture) readonly def\n/FullName (Fixture Type1 Regular) readonly def\n/FamilyName (Fixture Type1) readonly def\n"
        "/Weight (Regular) readonly def\n/ItalicAngle 0 def\n/isFixedPitch false def\n/UnderlinePosition -100 def\n/UnderlineThickness 50 def\n"
        "end readonly def\n/FontName /FixtureType1 def\n/PaintType 0 def\n/FontType 1 def\n/FontMatrix [0.001 0 0 0.001 0 0] readonly def\n"
        "/Encoding StandardEncoding def\n/FontBBox {0 0 600 700} readonly def\ncurrentdict end\ncurrentfile eexec\n";
    const std::string beginPrivate =
        "dup /Private 8 dict dup begin\n/RD {string currentfile exch readstring pop} executeonly def\n/ND {noaccess def} executeonly def\n"
        "/NP {noaccess put} executeonly def\n/BlueValues [] def\n/MinFeature{16 16} def\n/password 5839 def\n/lenIV 4 def\n/Subrs 0 array\nND\n"
        "2 index /CharStrings 2 dict dup begin\n";
    const std::string endPrivate = "end\nend\nreadonly put\nnoaccess put\ndup /FontName get exch definefont pop\nmark currentfile closefile\n";
    std::vector<unsigned char> priv(beginPrivate.begin(), beginPrivate.end());
    const std::pair<std::string, std::vector<unsigned char>> glyphs[] = {{".notdef", Empty()}, {"A", Square()}};
    for (const auto& [name, charstring] : glyphs) {
        const std::vector<unsigned char> encrypted = Encrypt(charstring, 4330);
        const std::string prefix = "/" + name + " " + std::to_string(encrypted.size()) + " RD ";
        priv.insert(priv.end(), prefix.begin(), prefix.end());
        priv.insert(priv.end(), encrypted.begin(), encrypted.end());
        const std::string suffix = " ND\n";
        priv.insert(priv.end(), suffix.begin(), suffix.end());
    }
    priv.insert(priv.end(), endPrivate.begin(), endPrivate.end());
    const std::vector<unsigned char> encryptedPrivate = Encrypt(priv, 55665);
    std::vector<unsigned char> font(header.begin(), header.end());
    constexpr char digits[] = "0123456789abcdef";
    for (const unsigned char byte : encryptedPrivate) {
        font.push_back(static_cast<unsigned char>(digits[byte >> 4]));
        font.push_back(static_cast<unsigned char>(digits[byte & 15]));
    }
    font.push_back('\n');
    for (int line = 0; line < 8; ++line) {
        font.insert(font.end(), 64, '0');
        font.push_back('\n');
    }
    const std::string trailer = "cleartomark\n";
    font.insert(font.end(), trailer.begin(), trailer.end());
    return font;
}

inline std::string Afm() {
    return "StartFontMetrics 2.0\nFontName FixtureType1\nFullName Fixture Type1 Regular\nFamilyName Fixture Type1\nWeight Regular\nItalicAngle 0\n"
           "IsFixedPitch false\nFontBBox 0 0 600 700\nUnderlinePosition -100\nUnderlineThickness 50\nAscender 700\nDescender 0\nStartCharMetrics 2\n"
           "C -1 ; WX 500 ; N .notdef ; B 0 0 0 0 ;\nC 65 ; WX 600 ; N A ; B 0 0 500 700 ;\nEndCharMetrics\nStartKernData\nStartKernPairs 1\n"
           "KPX A A -50\nEndKernPairs\nEndKernData\nEndFontMetrics\n";
}

}

#endif
