#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <source_location>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {

using namespace NKWebKit::WtfAbi;
using WebCore::LegacyOptionalVPCodecConfigurationRecord;

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "VP codec check failed at line %u\n", location.line());
        std::abort();
    }
}

LegacyOptionalVPCodecConfigurationRecord Parse(std::string_view text, bool bytes) {
    if (bytes)
        return _ZN7WebCore22parseVPCodecParametersEN3WTF10StringViewE({text.data(), static_cast<unsigned>(text.size()), true});
    std::u16string wide(text.begin(), text.end());
    return _ZN7WebCore22parseVPCodecParametersEN3WTF10StringViewE({wide.data(), static_cast<unsigned>(wide.size()), false});
}

LegacyOptionalVPCodecConfigurationRecord ParseWide(const std::u16string& wide) {
    return _ZN7WebCore22parseVPCodecParametersEN3WTF10StringViewE({wide.data(), static_cast<unsigned>(wide.size()), false});
}

void CheckRecord(const LegacyOptionalVPCodecConfigurationRecord& result, std::string_view name, const std::vector<unsigned>& fields) {
    Require(result.engaged);
    const auto* impl = result.value.codecName.impl;
    Require(impl && impl->length == name.size() && impl->refCount == 2);
    for (unsigned i = 0; i < name.size(); ++i)
        Require((impl->hashAndFlags & 4 ? static_cast<const unsigned char*>(impl->data)[i] : static_cast<const char16_t*>(impl->data)[i]) == name[i]);
    unsigned char storage[24];
    std::memcpy(storage, static_cast<const void*>(&result), sizeof(result));
    Require(storage[0] == 1);
    const StringImpl* stored = nullptr;
    std::memcpy(&stored, storage + 8, sizeof(stored));
    Require(stored == impl);
    const unsigned order[8] = {0, 1, 2, 3, 7, 4, 5, 6};
    for (unsigned i = 0; i < 8; ++i)
        Require(storage[16 + i] == fields[order[i]]);
    {
        auto copy = result;
        Require(copy.value.codecName.impl == impl && impl->refCount == 4);
    }
    Require(impl->refCount == 2);
}

void Check(std::string_view text, bool bytes, std::string_view name, const std::vector<unsigned>& fields) {
    CheckRecord(Parse(text, bytes), name, fields);
}

bool Rejected(std::string_view text, bool bytes) {
    const auto result = Parse(text, bytes);
    unsigned char storage[24];
    std::memcpy(storage, static_cast<const void*>(&result), sizeof(result));
    return !result.engaged && storage[0] == 0 && !result.value.codecName.impl;
}

bool Unknown(std::string_view text, bool bytes) {
    try {
        Parse(text, bytes);
    } catch (const std::logic_error&) {
        return true;
    } catch (...) {
        return false;
    }
    return false;
}

bool In(unsigned value, std::initializer_list<unsigned> values) {
    for (auto candidate : values)
        if (candidate == value)
            return true;
    return false;
}

void TestNames(bool bytes) {
    for (auto name : {"vp08", "vp09"}) {
        Check(std::string(name) + ".02.31.10", bytes, name, {2, 31, 10, 1, 1, 1, 1, 0});
        Check(std::string(name) + ".03.62.12.03.22.18.00.01", bytes, name, {3, 62, 12, 3, 22, 18, 0, 1});
    }
    for (auto text : {"vp8", "vp9", "vp8.0", "vp9.0", "vp8...", "vp9.0.", "vp8.02.31.10", "vp9.02.31.10", "VP09.0.10.8", "vp10.0.10.8", "vp0.0.10.8", "vp090.0.10.8"})
        Require(Rejected(text, bytes));
}

void TestShape(bool bytes) {
    Check("..vp09..+0002. 31\v.\t08\r...", bytes, "vp09", {2, 31, 8, 1, 1, 1, 1, 0});
    Check("vp09.00000000000000000000000000000002.31.08", bytes, "vp09", {2, 31, 8, 1, 1, 1, 1, 0});
    for (auto text : {"", ".", "...", "vp09", "vp09.0", "vp09.0.10", "vp09..10.8", "vp09.0..8", "vp09.0.10..", "vp09.0.10.8.1", "vp09.0.10.8.1.1",
                      "vp09.0.10.8.1.1.1", "vp09.0.10.8.1.1.1.1.0.0", "vp09.-0.10.8", "vp09.0x0.10.8", "vp09.0.10.8a", "vp09.256.10.8", "vp09.0.10.+", "vp09.0.10. "})
        Require(Rejected(text, bytes));
}

void TestFields(bool bytes) {
    for (unsigned field = 0; field < 8; ++field) {
        for (unsigned value = 0; value <= 256; ++value) {
            std::vector<unsigned> fields{0, 10, 8, 3, 1, 1, 1, 0};
            fields[field] = value;
            const bool valid = field == 0 ? value <= 3
                : field == 1 ? In(value, {10, 11, 20, 21, 30, 31, 40, 41, 50, 51, 52, 60, 61, 62})
                : field == 2 ? In(value, {8, 10, 12})
                : field == 3 ? value <= 3
                : field == 4 ? In(value, {1, 2, 4, 5, 6, 7, 8, 9, 10, 11, 12, 22})
                : field == 5 ? value >= 1 && value <= 18 && value != 3
                : field == 6 ? value <= 14 && value != 3 : value <= 1;
            std::string text = "vp09";
            for (auto number : fields)
                text += "." + std::to_string(number);
            if (valid && value == 2 && field >= 4 && field <= 6)
                Require(Unknown(text, bytes));
            else if (valid)
                Check(text, bytes, "vp09", fields);
            else
                Require(Rejected(text, bytes));
        }
    }
    for (unsigned chroma = 0; chroma < 4; ++chroma)
        Require(Parse("vp09.0.10.8." + std::to_string(chroma) + ".1.1.0.0", bytes).engaged == (chroma == 3));
}

void TestUnspecified(bool bytes) {
    Require(Unknown("vp09.00.10.08.00.02.02.02.00", bytes));
    Require(Unknown("vp09.0.10.8.1.2.1.1.0", bytes));
    Require(Unknown("vp09.0.10.8.1.1.2.1.0", bytes));
    Require(Unknown("vp09.0.10.8.1.1.1.2.0", bytes));
    Require(Rejected("vp09.0.10.8.1.2.1.1.7", bytes));
    Require(Rejected("vp09.0.10.8.1.2.3.1.0", bytes));
    Require(Rejected("vp09.0.10.8.1.2.1.1", bytes));
    Require(Rejected("vp09.0.10.8.1.2.1.1.0.0", bytes));
    Require(Rejected("vp09.0.10.8.1.1.2.0.0", bytes));
    Check("vp09.0.10.8.2.1.1.1.0", bytes, "vp09", {0, 10, 8, 2, 1, 1, 1, 0});
    Check("vp09.2.20.8", bytes, "vp09", {2, 20, 8, 1, 1, 1, 1, 0});
}

void TestWhitespace() {
    for (const char16_t space : {char16_t(0x1680), char16_t(0x2000), char16_t(0x2003), char16_t(0x200A), char16_t(0x2028), char16_t(0x205F), char16_t(0x3000)}) {
        std::u16string text = u"vp09.";
        text += space;
        text += u"1";
        text += space;
        text += u".10.8";
        CheckRecord(ParseWide(text), "vp09", {1, 10, 8, 1, 1, 1, 1, 0});
    }
    for (const char16_t other : {char16_t(0x0085), char16_t(0x00A0), char16_t(0x1FFF), char16_t(0x200B), char16_t(0x2029), char16_t(0x202F), char16_t(0x2060), char16_t(0xFEFF), char16_t(0x0100)}) {
        std::u16string text = u"vp09.";
        text += other;
        text += u"1.10.8";
        Require(!ParseWide(text).engaged);
    }
    const std::string latin1 = std::string("vp09.") + char(0xA0) + "1.10.8";
    Require(Rejected(latin1, true));
    const std::string nextLine = std::string("vp09.") + char(0x85) + "1.10.8";
    Require(Rejected(nextLine, true));
}

void TestNullStorage() {
    const auto empty = _ZN7WebCore22parseVPCodecParametersEN3WTF10StringViewE({nullptr, 0, true});
    Require(!empty.engaged);
    for (bool bytes : {true, false}) {
        bool threw = false;
        try {
            _ZN7WebCore22parseVPCodecParametersEN3WTF10StringViewE({nullptr, 1, bytes});
        } catch (const std::invalid_argument&) {
            threw = true;
        }
        Require(threw);
    }
}

}  // namespace

int main() {
    try {
        for (bool bytes : {true, false}) {
            TestNames(bytes);
            TestShape(bytes);
            TestFields(bytes);
            TestUnspecified(bytes);
        }
        TestWhitespace();
        TestNullStorage();
    } catch (const std::exception& error) {
        std::fprintf(stderr, "VP codec exception: %s\n", error.what());
        return 1;
    }
    return 0;
}
