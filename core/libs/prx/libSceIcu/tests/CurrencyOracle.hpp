#pragma once
#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace CurrencyOracle {

struct ScalarResult {
    double value;
    std::int32_t status;
};

struct CodeResult {
    std::array<char16_t, 8> text;
    std::int32_t length;
    std::int32_t status;
};

struct NameResult {
    std::u16string text;
    bool null;
    bool alias;
    std::int32_t length;
    std::int32_t status;
    std::int8_t choice;
};

struct EnumerationResult {
    std::vector<std::string> values;
    std::vector<std::u16string> unicode;
    std::int32_t count;
    std::int32_t status;
    std::int32_t endLength;
    std::int32_t unicodeEndLength;
};

void SetDefault(const char*);
ScalarResult Scalar(unsigned, const char16_t*, const char*, std::int32_t, double, double, std::int32_t);
CodeResult Code(const char*, double, std::int32_t, std::int32_t, std::int32_t, bool);
NameResult Name(const char16_t*, const char*, std::int32_t, const char*, std::int32_t, bool);
EnumerationResult Enumeration(std::uint32_t, const char*, bool, std::int32_t, bool = true);
bool EnumerationWriteBounds(std::int32_t*, std::int32_t*);

}
