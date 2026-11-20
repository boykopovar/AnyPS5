#include "prx/libSceIcu/libSceIcu.hpp"
#include "prx/libSceIcu/tests/RecoveredSignatureChecks.hpp"
#include <array>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>
#include <type_traits>

namespace {

using IcuRecoveredTest::Require;

constexpr std::int32_t IllegalArgument = 1;
constexpr std::int32_t BufferOverflow = 15;
constexpr std::int32_t RelativeDay = 4;
constexpr std::int32_t LongStyle = 0;
constexpr std::int32_t CapitalizationNone = 256;

std::u16string Text(const char16_t* text) {
    return std::u16string(text);
}

template <typename TCall>
void RequireThrows(TCall call) {
    bool threw = false;
    try {
        call();
    } catch (const std::runtime_error&) {
        threw = true;
    }
    Require(threw);
}

URelativeDateTimeFormatter* Open() {
    std::int32_t status = 0;
    auto* const formatter = ureldatefmt_open_nid_postfix("en", nullptr, LongStyle, CapitalizationNone, &status);
    Require(formatter != nullptr && status <= 0);
    return formatter;
}

void OpenAndClose() {
    std::int32_t status = 0;
    auto* const formatter = ureldatefmt_open_nid_postfix("en", nullptr, LongStyle, CapitalizationNone, &status);
    Require(formatter != nullptr && status <= 0);
    ureldatefmt_close_nid_postfix(formatter);
    ureldatefmt_close_nid_postfix(nullptr);

    status = 1;
    Require(ureldatefmt_open_nid_postfix("en", nullptr, LongStyle, CapitalizationNone, &status) == nullptr);
    Require(status == 1);

    status = 0;
    Require(ureldatefmt_open_nid_postfix("en", nullptr, 99, CapitalizationNone, &status) == nullptr);
    Require(status == IllegalArgument);

    status = 0;
    Require(ureldatefmt_open_nid_postfix("en", nullptr, LongStyle, 0, &status) == nullptr);
    Require(status == IllegalArgument);

    RequireThrows([&] { ureldatefmt_open_nid_postfix("en", nullptr, LongStyle, CapitalizationNone, nullptr); });
}

void FormatRelative() {
    auto* const formatter = Open();
    std::array<char16_t, 32> output;
    output.fill(u'x');
    std::int32_t status = 0;
    const auto length = ureldatefmt_format_nid_postfix(formatter, -1.0, RelativeDay, output.data(), output.size(), &status);
    Require(length == 9 && status <= 0);
    Require(Text(output.data()) == u"yesterday");

    status = 0;
    Require(ureldatefmt_format_nid_postfix(formatter, -1.0, RelativeDay, nullptr, 0, &status) == 9);
    Require(status == BufferOverflow);

    std::array<char16_t, 5> shortOutput;
    shortOutput.fill(u'x');
    status = 0;
    Require(ureldatefmt_format_nid_postfix(formatter, -1.0, RelativeDay, shortOutput.data(), shortOutput.size(), &status) == 9);
    Require(status == BufferOverflow);
    Require(std::u16string(shortOutput.begin(), shortOutput.end()) == u"xxxxx");

    status = 1;
    output.fill(u'x');
    Require(ureldatefmt_format_nid_postfix(formatter, -1.0, RelativeDay, output.data(), output.size(), &status) == 0);
    Require(status == 1 && output[0] == u'x');

    status = 0;
    Require(ureldatefmt_format_nid_postfix(formatter, -1.0, RelativeDay, nullptr, 1, &status) == 0);
    Require(status == IllegalArgument);

    status = 0;
    RequireThrows([&] {
        ureldatefmt_format_nid_postfix(formatter, -1.0, RelativeDay, reinterpret_cast<char16_t*>(std::uintptr_t{0x10}), 32, &status);
    });
    ureldatefmt_close_nid_postfix(formatter);
}

void FormatNumeric() {
    auto* const formatter = Open();
    std::array<char16_t, 32> output{};
    std::int32_t status = 0;
    const auto length = ureldatefmt_formatNumeric_nid_postfix(formatter, -3.0, RelativeDay, output.data(), output.size(), &status);
    Require(length == 10 && status <= 0);
    Require(Text(output.data()) == u"3 days ago");

    status = 0;
    Require(ureldatefmt_formatNumeric_nid_postfix(formatter, -3.0, RelativeDay, nullptr, 0, &status) == 10);
    Require(status == BufferOverflow);

    status = 0;
    Require(ureldatefmt_formatNumeric_nid_postfix(formatter, -3.0, RelativeDay, nullptr, 4, &status) == 0);
    Require(status == IllegalArgument);

    ureldatefmt_close_nid_postfix(formatter);
}

void InvalidStatusPointers() {
    const auto* const badStatus = reinterpret_cast<const std::int32_t*>(std::uintptr_t{0x10});
    RequireThrows([&] { ureldatefmt_open_nid_postfix("en", nullptr, LongStyle, CapitalizationNone, const_cast<std::int32_t*>(badStatus)); });
    RequireThrows([&] { ureldatefmt_format_nid_postfix(nullptr, 0.0, RelativeDay, nullptr, 1, const_cast<std::int32_t*>(badStatus)); });
    RequireThrows([&] { ureldatefmt_formatNumeric_nid_postfix(nullptr, 0.0, RelativeDay, nullptr, 1, const_cast<std::int32_t*>(badStatus)); });
}

}

static_assert(std::is_same_v<decltype(&ureldatefmt_close_nid_postfix), void (APS5_VABI*)(URelativeDateTimeFormatter*)>);
static_assert(std::is_same_v<decltype(&ureldatefmt_format_nid_postfix), std::int32_t (APS5_VABI*)(const URelativeDateTimeFormatter*, double, std::int32_t, char16_t*, std::int32_t, std::int32_t*)>);
static_assert(std::is_same_v<decltype(&ureldatefmt_formatNumeric_nid_postfix), std::int32_t (APS5_VABI*)(const URelativeDateTimeFormatter*, double, std::int32_t, char16_t*, std::int32_t, std::int32_t*)>);
static_assert(std::is_same_v<decltype(&ureldatefmt_open_nid_postfix), URelativeDateTimeFormatter* (APS5_VABI*)(const char*, UNumberFormat*, std::int32_t, std::int32_t, std::int32_t*)>);

void CheckIcuRelativeDateTimeFormatter() {
    OpenAndClose();
    FormatRelative();
    FormatNumeric();
    InvalidStatusPointers();
}
