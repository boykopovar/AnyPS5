#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "WTF number parsing check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

static bool Same(double first, double second) {
    return std::memcmp(&first, &second, sizeof(double)) == 0;
}

static bool Same(float first, float second) {
    return std::memcmp(&first, &second, sizeof(float)) == 0;
}

static std::u16string Wide(const std::string& text) {
    return std::u16string(text.begin(), text.end());
}

static void Double(const std::string& text, double expected, bool expectedOk) {
    const auto* latin1 = reinterpret_cast<const unsigned char*>(text.data());
    bool ok = !expectedOk;
    Require(Same(_ZN3WTF18charactersToDoubleEPKhmPb(latin1, text.size(), &ok), expected));
    Require(ok == expectedOk);
    const auto wide = Wide(text);
    ok = !expectedOk;
    Require(Same(_ZN3WTF18charactersToDoubleEPKDsmPb(wide.data(), wide.size(), &ok), expected));
    Require(ok == expectedOk);
    Require(Same(_ZN3WTF18charactersToDoubleEPKhmPb(latin1, text.size(), nullptr), expected));
}

static void Float(const std::string& text, float expected, bool expectedOk, std::size_t expectedLength) {
    const auto* latin1 = reinterpret_cast<const unsigned char*>(text.data());
    const auto wide = Wide(text);
    bool ok = !expectedOk;
    Require(Same(_ZN3WTF17charactersToFloatEPKhmPb(latin1, text.size(), &ok), expected));
    Require(ok == expectedOk);
    ok = !expectedOk;
    Require(Same(_ZN3WTF17charactersToFloatEPKDsmPb(wide.data(), wide.size(), &ok), expected));
    Require(ok == expectedOk);
    std::size_t parsedLength = 12345;
    Require(Same(_ZN3WTF17charactersToFloatEPKhmRm(latin1, text.size(), &parsedLength), expected));
    Require(parsedLength == expectedLength);
    parsedLength = 12345;
    Require(Same(_ZN3WTF17charactersToFloatEPKDsmRm(wide.data(), wide.size(), &parsedLength), expected));
    Require(parsedLength == expectedLength);
}

static void Grammar() {
    Double("1.5", 1.5, true);
    Double(" \t\n\v\f\r-2.5e3", -2500.0, true);
    Double("+5", 5.0, true);
    Double("5.", 5.0, true);
    Double(".5", 0.5, true);
    Double("-0", -0.0, true);
    Double("-.0e5", -0.0, true);
    Double("2E-3", 0.002, true);
    Double("1.5 ", 1.5, false);
    Double("", 0.0, false);
    Double("   ", 0.0, false);
    Double("-", 0.0, false);
    Double("- 5", 0.0, false);
    Double(".", 0.0, false);
    Double("-.e1", 0.0, false);
    Double("Infinity", 0.0, false);
    Double("NaN", 0.0, false);
    Double("0x10", 0.0, false);
    Float("1.5x", 1.5f, false, 3);
    Float("  12abc", 12.0f, false, 4);
    Float("1e", 1.0f, false, 1);
    Float("1e+", 1.0f, false, 1);
    Float("1e-x", 1.0f, false, 1);
    Float("7e2.", 700.0f, false, 3);
    Float("0x10", 0.0f, false, 1);
    Float("-", 0.0f, false, 0);
    Float("  ", 0.0f, false, 0);
    Float("-00.", -0.0f, true, 4);
}

static void Range() {
    const double infinity = std::numeric_limits<double>::infinity();
    Double("1e400", infinity, true);
    Double("-1e400", -infinity, true);
    Double("1e-400", 0.0, true);
    Double("1e99999999999999999999", infinity, true);
    Double("0e99999999999", 0.0, true);
    Double("1.7976931348623157e308", std::numeric_limits<double>::max(), true);
    Double("1.7976931348623158e308", std::numeric_limits<double>::max(), true);
    Double("1.7976931348623159e308", infinity, true);
    Double("4.9406564584124654e-324", std::numeric_limits<double>::denorm_min(), true);
    Double("2.4703282292062328e-324", std::numeric_limits<double>::denorm_min(), true);
    Double("2.4703282292062327e-324", 0.0, true);
    Float("3.4028235e38", std::numeric_limits<float>::max(), true, 12);
    Float("3.4028236e38", std::numeric_limits<float>::infinity(), true, 12);
    Float("1e-50", 0.0f, true, 5);
}

static void Rounding() {
    Float("1.0000000596046447755", 1.0f, true, 21);
    Float("1.00000006", std::nextafter(1.0f, 2.0f), true, 10);
    const std::string midpoint = "1.00000000000000011102230246251565404236316680908203125";
    Double(midpoint, 1.0, true);
    Double(midpoint + std::string(800, '0'), 1.0, true);
    Double(midpoint + std::string(800, '0') + "1", std::nextafter(1.0, 2.0), true);
    Double("0." + std::string(400, '0') + "100000000000000011102230246251565404236316680908203125" + std::string(800, '0') + "1e401", std::nextafter(1.0, 2.0), true);
    Double(std::string(900, '9') + "e-900", 1.0, true);
}

static void Utf16() {
    const std::u16string dotted = u"ı1.5";
    bool ok = true;
    Require(Same(_ZN3WTF18charactersToDoubleEPKDsmPb(dotted.data(), dotted.size(), &ok), 0.0));
    Require(!ok);
    const std::u16string arabic = u"2١";
    std::size_t parsedLength = 0;
    Require(Same(_ZN3WTF17charactersToFloatEPKDsmRm(arabic.data(), arabic.size(), &parsedLength), 2.0f));
    Require(parsedLength == 1);
    const std::u16string space = u" 1";
    ok = true;
    Require(Same(_ZN3WTF17charactersToFloatEPKDsmPb(space.data(), space.size(), &ok), 0.0f));
    Require(!ok);
    const std::u16string shifted = u"1ĵ";
    parsedLength = 0;
    Require(Same(_ZN3WTF17charactersToFloatEPKDsmRm(shifted.data(), shifted.size(), &parsedLength), 1.0f));
    Require(parsedLength == 1);
    std::u16string longer(70, u'0');
    longer += u"1.25";
    ok = false;
    Require(Same(_ZN3WTF18charactersToDoubleEPKDsmPb(longer.data(), longer.size(), &ok), 1.25));
    Require(ok);
    longer += u"ĵ";
    longer += u"5";
    parsedLength = 0;
    Require(Same(_ZN3WTF17charactersToFloatEPKDsmRm(longer.data(), longer.size(), &parsedLength), 1.25f));
    Require(parsedLength == 74);
}

static std::vector<std::uint16_t> Units(const std::u16string& text) {
    return std::vector<std::uint16_t>(text.begin(), text.end());
}

static void Converter(const std::u16string& text, double expectedDouble, float expectedFloat, std::size_t expectedLength) {
    const auto units = Units(text);
    std::size_t processed = 12345;
    Require(Same(_ZN3WTF17double_conversion23StringToDoubleConverter14StringToDoubleEPKtmPm(units.data(), units.size(), &processed), expectedDouble));
    Require(processed == expectedLength);
    processed = 12345;
    Require(Same(_ZN3WTF17double_conversion23StringToDoubleConverter13StringToFloatEPKtmPm(units.data(), units.size(), &processed), expectedFloat));
    Require(processed == expectedLength);
    for (const auto unit : text) {
        if (unit > 0xFF) return;
    }
    const std::string bytes(text.begin(), text.end());
    processed = 12345;
    Require(Same(_ZN3WTF17double_conversion23StringToDoubleConverter14StringToDoubleEPKcmPm(bytes.data(), bytes.size(), &processed), expectedDouble));
    Require(processed == expectedLength);
    processed = 12345;
    Require(Same(_ZN3WTF17double_conversion23StringToDoubleConverter13StringToFloatEPKcmPm(bytes.data(), bytes.size(), &processed), expectedFloat));
    Require(processed == expectedLength);
}

static void StringToDoubleConverter() {
    Converter(u"1.5", 1.5, 1.5f, 3);
    Converter(u"00012.50e-1z", 1.25, 1.25f, 11);
    Converter(u" 1", 0.0, 0.0f, 0);
    Converter(u"-", 0.0, 0.0f, 0);
    Converter(u"-0", -0.0, -0.0f, 2);
    Converter(u"-.0e5", -0.0, -0.0f, 5);
    Converter(u"1e+x", 1.0, 1.0f, 1);
    Converter(u"0x10", 0.0, 0.0f, 1);
    Converter(u"1\u00B1", 1.0, 1.0f, 1);
    Converter(u"1\u01315", 1.0, 1.0f, 1);
    Converter(u"1e99999999999999", std::numeric_limits<double>::infinity(), std::numeric_limits<float>::infinity(), 16);
    Converter(u"1.0000000596046447755", 1.0000000596046447755, std::nextafter(1.0f, 2.0f), 21);
    Converter(u"1.000000059604644775390625", 1.000000059604644775390625, 1.0f, 26);
    Converter(u"3.4028235677973366e38", 3.4028235677973366e38, std::numeric_limits<float>::max(), 21);
    Converter(u"3.4028235677973367e38", 3.4028235677973367e38, std::numeric_limits<float>::infinity(), 21);
    Converter(u"7.0064923216240854e-46", 7.0064923216240854e-46, std::numeric_limits<float>::denorm_min(), 22);
    Converter(u"7.006492321624085e-46", 7.006492321624085e-46, 0.0f, 21);
    std::u16string longer(70, u'0');
    longer += u"1.25\u0135";
    Converter(longer, 1.25, 1.25f, 74);
}

template<typename TCall>
static void RequireInvalidArgument(TCall call, int line) {
    try {
        call();
    } catch (const std::invalid_argument&) {
        return;
    }
    Check(false, line);
}

static void NullPointers() {
    bool ok = true;
    Require(Same(_ZN3WTF18charactersToDoubleEPKhmPb(nullptr, 0, &ok), 0.0));
    Require(!ok);
    ok = true;
    Require(Same(_ZN3WTF17charactersToFloatEPKDsmPb(nullptr, 0, &ok), 0.0f));
    Require(!ok);
    std::size_t parsedLength = 12345;
    Require(Same(_ZN3WTF17charactersToFloatEPKhmRm(nullptr, 0, &parsedLength), 0.0f));
    Require(parsedLength == 0);
    RequireInvalidArgument([] { _ZN3WTF18charactersToDoubleEPKhmPb(nullptr, 1, nullptr); }, __LINE__);
    RequireInvalidArgument([] { _ZN3WTF18charactersToDoubleEPKDsmPb(nullptr, 1, nullptr); }, __LINE__);
    RequireInvalidArgument([] { _ZN3WTF17charactersToFloatEPKhmPb(nullptr, 1, nullptr); }, __LINE__);
    RequireInvalidArgument([] { _ZN3WTF17charactersToFloatEPKDsmPb(nullptr, 1, nullptr); }, __LINE__);
    const unsigned char latin1[] = {'1'};
    const char16_t wide[] = {u'1'};
    RequireInvalidArgument([&] { _ZN3WTF17charactersToFloatEPKhmRm(latin1, 1, nullptr); }, __LINE__);
    RequireInvalidArgument([&] { _ZN3WTF17charactersToFloatEPKDsmRm(wide, 1, nullptr); }, __LINE__);
    RequireInvalidArgument([] { _ZN3WTF17charactersToFloatEPKhmRm(nullptr, 0, nullptr); }, __LINE__);
    parsedLength = 12345;
    Require(Same(_ZN3WTF17double_conversion23StringToDoubleConverter14StringToDoubleEPKcmPm(nullptr, 0, &parsedLength), 0.0));
    Require(parsedLength == 0);
    parsedLength = 12345;
    RequireInvalidArgument([&] { _ZN3WTF17double_conversion23StringToDoubleConverter13StringToFloatEPKtmPm(nullptr, 1, &parsedLength); }, __LINE__);
    Require(parsedLength == 0);
    const char bytes[] = {'1'};
    const std::uint16_t units[] = {u'1'};
    RequireInvalidArgument([&] { _ZN3WTF17double_conversion23StringToDoubleConverter14StringToDoubleEPKcmPm(bytes, 1, nullptr); }, __LINE__);
    RequireInvalidArgument([&] { _ZN3WTF17double_conversion23StringToDoubleConverter14StringToDoubleEPKtmPm(units, 1, nullptr); }, __LINE__);
    RequireInvalidArgument([&] { _ZN3WTF17double_conversion23StringToDoubleConverter13StringToFloatEPKcmPm(bytes, 1, nullptr); }, __LINE__);
    RequireInvalidArgument([] { _ZN3WTF17double_conversion23StringToDoubleConverter13StringToFloatEPKtmPm(nullptr, 0, nullptr); }, __LINE__);
}

int main() {
    NullPointers();
    Grammar();
    Range();
    Rounding();
    Utf16();
    StringToDoubleConverter();
}
