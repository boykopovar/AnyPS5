#include "prx/libSceIcu/libSceIcu.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "ICU converter helpers: %s\n", message);
    std::abort();
}

template<class TCall>
void RequiresInvalidArgument(TCall call) {
    try {
        call();
    } catch (const std::invalid_argument&) {
        return;
    }
    Require(false, "null normalization argument must throw");
}

char Ebcdic(unsigned char value) {
    if (value >= 'a' && value <= 'i') return static_cast<char>(0x81 + value - 'a');
    if (value >= 'j' && value <= 'r') return static_cast<char>(0x91 + value - 'j');
    if (value >= 's' && value <= 'z') return static_cast<char>(0xa2 + value - 's');
    if (value >= 'A' && value <= 'Z') return static_cast<char>(static_cast<unsigned char>(Ebcdic(value + 32)) + 0x40);
    if (value >= '0' && value <= '9') return static_cast<char>(0xf0 + value - '0');
    return value ? '\x40' : 0;
}

std::string ToEbcdic(std::string_view text) {
    std::string result;
    for (unsigned char value : text) result += Ebcdic(value);
    return result;
}

void CheckStrip(const std::string& source, const std::string& expected, bool ebcdic) {
    const auto call = ebcdic ? ucnv_io_stripEBCDICForCompare_nid_postfix : ucnv_io_stripASCIIForCompare_nid_postfix;
    std::string output(source.size() + 4, '@');
    const auto saved = source;
    Require(call(output.data() + 1, source.c_str()) == output.data() + 1, "strip returns destination");
    Require(std::string_view(output.data() + 1, expected.size()) == expected, "strip bytes match expected normalization");
    Require(output[expected.size() + 1] == 0, "strip terminates output");
    Require(output.front() == '@' && std::all_of(output.begin() + expected.size() + 2, output.end(), [](char value) { return value == '@'; }), "strip preserves both canaries");
    Require(source == saved, "strip leaves separate input unchanged");
    std::string inPlace = source;
    inPlace.append(3, '@');
    inPlace[source.size()] = 0;
    Require(call(inPlace.data(), inPlace.data()) == inPlace.data(), "strip supports identical source and destination");
    Require(std::string_view(inPlace.c_str()) == expected, "in-place strip bytes match");
    Require(inPlace[source.size() + 1] == '@' && inPlace[source.size() + 2] == '@', "in-place strip preserves tail");
}

void NameMatching() {
    struct Case { const char* input; const char* expected; };
    constexpr Case cases[] = {
        {"", ""}, {" -_./@!\t\r\n", ""}, {"UTF-8", "utf8"}, {"u*T@f08", "utf8"},
        {"ibm-001200", "ibm1200"}, {"0", "0"}, {"000", "0"}, {"00090", "90"},
        {"10002", "10002"}, {"10-02", "102"}, {"00-01", "01"}, {"0a01", "0a1"},
        {"A0_01Z", "a01z"}, {"AbCdEfGhIjKlMnOpQrStUvWxYz", "abcdefghijklmnopqrstuvwxyz"},
        {"09.090.0.001", "99001"}, {"utf-8,version=02", "utf8version2"},
    };
    for (const auto& test : cases) {
        CheckStrip(test.input, test.expected, false);
        CheckStrip(ToEbcdic(test.input), ToEbcdic(test.expected), true);
    }
    Require(ucnv_compareNames_nid_postfix("UTF-8", "u*T@f08") == 0, "converter aliases compare equal");
    Require(ucnv_compareNames_nid_postfix("u*T@f08", "UTF-8") == 0, "alias equality is symmetric");
    Require(ucnv_compareNames_nid_postfix("00-01", "01") == '0' - '1', "ignored separators reset numeric runs before zero stripping");
    for (const auto& left : cases) for (const auto& right : cases) {
        const auto a = reinterpret_cast<const unsigned char*>(left.expected);
        const auto b = reinterpret_cast<const unsigned char*>(right.expected);
        std::size_t index = 0;
        while (a[index] && a[index] == b[index]) ++index;
        const int expected = static_cast<int>(a[index]) - static_cast<int>(b[index]);
        Require(ucnv_compareNames_nid_postfix(left.input, right.input) == expected, "comparison returns first differing normalized-byte difference");
    }
    for (unsigned value = 1; value <= 255; ++value) {
        const std::string source(1, static_cast<char>(value));
        std::string ascii;
        if ((value >= '0' && value <= '9') || (value >= 'a' && value <= 'z')) ascii = source;
        if (value >= 'A' && value <= 'Z') ascii += static_cast<char>(value + 32);
        CheckStrip(source, ascii, false);
        std::string ebcdic;
        for (char c : std::string_view("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789"))
            if (static_cast<unsigned char>(Ebcdic(c)) == value) ebcdic.assign(1, Ebcdic(c >= 'A' && c <= 'Z' ? c + 32 : c));
        CheckStrip(source, ebcdic, true);
    }
    const std::string longName = "000" + std::string(4096, 'Q') + "-0001200";
    CheckStrip(longName, "0" + std::string(4096, 'q') + "1200", false);
    CheckStrip(ToEbcdic(longName), ToEbcdic("0" + std::string(4096, 'q') + "1200"), true);
    Require(ucnv_compareNames_nid_postfix("utf8", "utf16") == '8' - '1', "names are compared lexically, not numerically");
    char output[4] = {'@', '@', '@', 0};
    RequiresInvalidArgument([&] { ucnv_compareNames_nid_postfix(nullptr, ""); });
    RequiresInvalidArgument([&] { ucnv_compareNames_nid_postfix("", nullptr); });
    for (auto call : {ucnv_io_stripASCIIForCompare_nid_postfix, ucnv_io_stripEBCDICForCompare_nid_postfix}) {
        RequiresInvalidArgument([&] { call(nullptr, ""); });
        RequiresInvalidArgument([&] { call(output, nullptr); });
        Require(output[0] == '@' && output[1] == '@' && output[2] == '@', "invalid strip leaves output unchanged");
    }
}

void CheckSignature(const char* source, std::int32_t length, const char* expected, std::int32_t expectedLength) {
    for (std::int32_t initial : {0, -1, -128}) {
        std::int32_t status = initial;
        std::array<std::int32_t, 3> output{91, 92, 93};
        const char* result = ucnv_detectUnicodeSignature_nid_postfix(source, length, &output[1], &status);
        Require((!expected && !result) || (expected && result && std::strcmp(expected, result) == 0), "detected encoding matches");
        Require(output[1] == expectedLength && output[0] == 91 && output[2] == 93, "signature length and canaries match");
        Require(status == initial, "successful detection preserves existing success or warning status");
        result = ucnv_detectUnicodeSignature_nid_postfix(source, length, nullptr, &status);
        Require((!expected && !result) || (expected && result && std::strcmp(expected, result) == 0), "optional length output is supported");
    }
}

void Signatures() {
    struct Case { std::array<unsigned char, 6> bytes; const char* name; int length; };
    constexpr Case cases[] = {
        {{0xfe, 0xff}, "UTF-16BE", 2}, {{0xff, 0xfe}, "UTF-16LE", 2},
        {{0xef, 0xbb, 0xbf}, "UTF-8", 3}, {{0xff, 0xfe, 0, 0}, "UTF-32LE", 4},
        {{0, 0, 0xfe, 0xff}, "UTF-32BE", 4}, {{0x0e, 0xfe, 0xff}, "SCSU", 3},
        {{0xfb, 0xee, 0x28}, "BOCU-1", 3}, {{0x2b, 0x2f, 0x76, 0x38, 0x2d}, "UTF-7", 5},
        {{0x2b, 0x2f, 0x76, 0x38}, "UTF-7", 4}, {{0x2b, 0x2f, 0x76, 0x39}, "UTF-7", 4},
        {{0x2b, 0x2f, 0x76, 0x2b}, "UTF-7", 4}, {{0x2b, 0x2f, 0x76, 0x2f}, "UTF-7", 4},
        {{0xdd, 0x73, 0x66, 0x73}, "UTF-EBCDIC", 4},
    };
    for (const auto& test : cases) {
        const char* bytes = reinterpret_cast<const char*>(test.bytes.data());
        for (int length = 0; length <= test.length; ++length) {
            const char* expected = nullptr;
            int expectedLength = 0;
            if (length == test.length) { expected = test.name; expectedLength = test.length; }
            else if (test.bytes[0] == 0xff && length >= 2) { expected = "UTF-16LE"; expectedLength = 2; }
            else if (test.length == 5 && length == 4) { expected = "UTF-7"; expectedLength = 4; }
            CheckSignature(bytes, length, expected, expectedLength);
        }
        std::array<char, 12> padded;
        padded.fill('@');
        std::copy_n(bytes, test.length, padded.begin());
        CheckSignature(padded.data(), static_cast<std::int32_t>(padded.size()), test.name, test.length);
        padded[test.length] = 0;
        if (test.bytes[0] == 0) CheckSignature(padded.data(), -1, nullptr, 0);
        else if (test.length == 4 && test.bytes[0] == 0xff) CheckSignature(padded.data(), -1, "UTF-16LE", 2);
        else CheckSignature(padded.data(), -1, test.name, test.length);
    }
    CheckSignature("+/v7-", 5, nullptr, 0);
    CheckSignature("ordinary ASCII", -1, nullptr, 0);
    CheckSignature("\xef\xbb\xbf", 0, nullptr, 0);
    CheckSignature("", 0, nullptr, 0);
    std::int32_t length = 123;
    for (std::int32_t initial : {1, 15, INT32_MAX}) {
        std::int32_t status = initial;
        Require(!ucnv_detectUnicodeSignature_nid_postfix(nullptr, -2, &length, &status), "preexisting failure is returned immediately");
        Require(status == initial && length == 123, "preexisting failure preserves all outputs");
    }
    Require(!ucnv_detectUnicodeSignature_nid_postfix(nullptr, -2, &length, nullptr) && length == 123, "null status suppresses input access and output mutation");
    for (std::int32_t invalid : {-2, INT32_MIN}) {
        std::int32_t status = -1;
        Require(!ucnv_detectUnicodeSignature_nid_postfix("x", invalid, &length, &status), "invalid length is rejected");
        Require(status == 1 && length == 123, "invalid length changes only status");
    }
    for (std::int32_t size : {-1, 0, 1}) {
        std::int32_t status = 0;
        Require(!ucnv_detectUnicodeSignature_nid_postfix(nullptr, size, &length, &status), "null source is rejected even when empty");
        Require(status == 1 && length == 123, "null source changes only status");
    }
}

}

int main() {
    NameMatching();
    Signatures();
}
