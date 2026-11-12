#include "prx/libSceIcu/libSceIcu.hpp"
#include "prx/libSceIcu/tests/RecoveredSignatureChecks.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <source_location>
#include <stdexcept>
#include <type_traits>

namespace UlistfmtClose {

using IcuRecoveredTest::Require;

static_assert(std::is_same_v<decltype(&ulistfmt_close_nid_postfix), void (APS5_VABI*)(UListFormatter*)>);

namespace {

bool ThrowsNotImplemented(UListFormatter* listfmt) {
    try {
        ulistfmt_close_nid_postfix(listfmt);
    } catch (const std::runtime_error&) {
        return true;
    }
    return false;
}

}

void Run() {
    ulistfmt_close_nid_postfix(nullptr);
    ulistfmt_close_nid_postfix(nullptr);

    alignas(8) std::array<std::uint8_t, 64> bytes{};
    bytes.fill(0xa5);
    const auto expected = bytes;
    Require(ThrowsNotImplemented(reinterpret_cast<UListFormatter*>(bytes.data())), "non-null list formatter throws");
    Require(bytes == expected, "non-null list formatter bytes stay untouched");
    Require(ThrowsNotImplemented(reinterpret_cast<UListFormatter*>(std::uintptr_t{1})), "unmapped non-null list formatter throws before dereference");
}

} // namespace UlistfmtClose

namespace UlistfmtCloseResult {

using IcuRecoveredTest::Filler;
using IcuRecoveredTest::Require;
using IcuRecoveredTest::RequireNoThrow;
using IcuRecoveredTest::RequireThrows;
using IcuRecoveredTest::Unchanged;

static_assert(std::is_same_v<decltype(&ulistfmt_closeResult_nid_postfix), void (APS5_VABI*)(UFormattedList*)>);

void Run() {
    RequireNoThrow([] { ulistfmt_closeResult_nid_postfix(nullptr); });
    RequireNoThrow([] { ulistfmt_closeResult_nid_postfix(nullptr); });

    const auto storage = Filler();
    RequireNoThrow([&] { ulistfmt_closeResult_nid_postfix(reinterpret_cast<UFormattedList*>(const_cast<std::uint32_t*>(storage.data()))); });
    Require(Unchanged(storage));
    RequireNoThrow([&] { ulistfmt_closeResult_nid_postfix(reinterpret_cast<UFormattedList*>(std::uintptr_t{1})); });

    std::int32_t status = 0;
    auto* result = ulistfmt_openResult_nid_postfix(&status);
    Require(result != nullptr && status == 0);
    ulistfmt_closeResult_nid_postfix(result);
    RequireNoThrow([&] { ulistfmt_closeResult_nid_postfix(result); });

    auto* foreign = unumf_openResult_nid_postfix(&status);
    Require(foreign != nullptr && status == 0);
    RequireNoThrow([&] { ulistfmt_closeResult_nid_postfix(reinterpret_cast<UFormattedList*>(foreign)); });
    unumf_closeResult_nid_postfix(foreign);
}

} // namespace UlistfmtCloseResult

namespace UlistfmtFormat {

using IcuRecoveredTest::Require;
using IcuRecoveredTest::RequireThrows;

static_assert(std::is_same_v<decltype(&ulistfmt_format_nid_postfix), std::int32_t (APS5_VABI*)(const UListFormatter*, const char16_t* const*, const std::int32_t*, std::int32_t, char16_t*, std::int32_t, std::int32_t*)>);

namespace {

constexpr std::int32_t IllegalArgument = 1;

constexpr std::uint32_t FillerWord = 0xa5a5a5a5;

std::array<std::uint32_t, 4> Filler() {
    std::array<std::uint32_t, 4> storage{};
    storage.fill(FillerWord);
    return storage;
}

bool Unchanged(const std::array<std::uint32_t, 4>& storage) {
    for (const auto word : storage) {
        if (word != FillerWord) return false;
    }
    return true;
}

} // namespace

void Run() {
    const std::array<std::int32_t, 7> failing{1, 2, 7, 15, 16, 27, 0x7fffffff};
    const std::array<std::int32_t, 4> passing{0, -128, -124, -1};
    auto formatterStorage = Filler();
    const auto* formatter = reinterpret_cast<const UListFormatter*>(formatterStorage.data());
    std::array<char16_t, 4> result{u'x', u'y', u'z', 0};
    const auto probeResult = result;
    std::array<std::int32_t, 1> lengths{3};
    const char16_t* strings[1] = {result.data()};

    for (const auto incoming : failing) {
        std::int32_t status = incoming;
        Require(ulistfmt_format_nid_postfix(nullptr, nullptr, nullptr, 0, nullptr, 0, &status) == -1);
        Require(status == incoming);

        status = incoming;
        Require(ulistfmt_format_nid_postfix(formatter, strings, lengths.data(), 1, result.data(), 4, &status) == -1);
        Require(status == incoming);
        Require(result == probeResult);
        Require(Unchanged(formatterStorage));
    }

    for (const auto incoming : passing) {
        std::int32_t status = incoming;
        Require(ulistfmt_format_nid_postfix(formatter, strings, lengths.data(), 1, nullptr, 4, &status) == -1);
        Require(status == IllegalArgument);

        status = incoming;
        Require(ulistfmt_format_nid_postfix(formatter, strings, lengths.data(), 1, result.data(), -1, &status) == -1);
        Require(status == IllegalArgument);
        Require(result == probeResult);
        Require(Unchanged(formatterStorage));
    }

    for (const auto incoming : passing) {
        std::int32_t status = incoming;
        RequireThrows([&] { ulistfmt_format_nid_postfix(formatter, strings, lengths.data(), 1, result.data(), 4, &status); });
        Require(status == incoming);
        Require(result == probeResult);
        Require(Unchanged(formatterStorage));

        status = incoming;
        RequireThrows([&] { ulistfmt_format_nid_postfix(formatter, nullptr, nullptr, 0, nullptr, 0, &status); });
        Require(status == incoming);
    }

    RequireThrows([&] { ulistfmt_format_nid_postfix(formatter, strings, lengths.data(), 1, result.data(), 4, nullptr); });
    Require(result == probeResult);
}

} // namespace UlistfmtFormat

namespace UlistfmtFormatStringsToResult {

using IcuRecoveredTest::Require;
using IcuRecoveredTest::RequireThrows;

static_assert(std::is_same_v<decltype(&ulistfmt_formatStringsToResult_nid_postfix), void (APS5_VABI*)(const UListFormatter*, const char16_t* const*, const std::int32_t*, std::int32_t, UFormattedList*, std::int32_t*)>);

namespace {

constexpr std::int32_t IllegalArgument = 1;

constexpr std::uint32_t FillerWord = 0xa5a5a5a5;

std::array<std::uint32_t, 4> Filler() {
    std::array<std::uint32_t, 4> storage{};
    storage.fill(FillerWord);
    return storage;
}

bool Unchanged(const std::array<std::uint32_t, 4>& storage) {
    for (const auto word : storage) {
        if (word != FillerWord) return false;
    }
    return true;
}

} // namespace

void Run() {
    const std::array<std::int32_t, 7> failing{1, 2, 7, 15, 16, 27, 0x7fffffff};
    const std::array<std::int32_t, 4> passing{0, -128, -124, -1};
    auto formatterStorage = Filler();
    auto resultStorage = Filler();
    const auto* formatter = reinterpret_cast<const UListFormatter*>(formatterStorage.data());
    auto* result = reinterpret_cast<UFormattedList*>(resultStorage.data());
    std::array<char16_t, 4> item{u'a', u'b', u'c', 0};
    const auto probeItem = item;
    std::array<std::int32_t, 1> lengths{3};
    const char16_t* strings[1] = {item.data()};

    for (const auto incoming : failing) {
        std::int32_t status = incoming;
        ulistfmt_formatStringsToResult_nid_postfix(nullptr, nullptr, nullptr, 0, nullptr, &status);
        Require(status == incoming);

        status = incoming;
        ulistfmt_formatStringsToResult_nid_postfix(formatter, strings, lengths.data(), 1, result, &status);
        Require(status == incoming);
        Require(Unchanged(formatterStorage) && Unchanged(resultStorage));
        Require(item == probeItem);
    }

    for (const auto incoming : passing) {
        std::int32_t status = incoming;
        ulistfmt_formatStringsToResult_nid_postfix(formatter, strings, lengths.data(), 1, nullptr, &status);
        Require(status == IllegalArgument);
        Require(item == probeItem);

        status = incoming;
        ulistfmt_formatStringsToResult_nid_postfix(nullptr, nullptr, nullptr, 0, nullptr, &status);
        Require(status == IllegalArgument);
    }

    for (const auto incoming : passing) {
        std::int32_t status = incoming;
        RequireThrows([&] { ulistfmt_formatStringsToResult_nid_postfix(formatter, strings, lengths.data(), 1, result, &status); });
        Require(status == incoming);
        Require(Unchanged(formatterStorage) && Unchanged(resultStorage));
        Require(item == probeItem);
    }

    RequireThrows([&] { ulistfmt_formatStringsToResult_nid_postfix(formatter, strings, lengths.data(), 1, result, nullptr); });
    Require(Unchanged(resultStorage));
}

} // namespace UlistfmtFormatStringsToResult

namespace UlistfmtOpenForType {

using IcuRecoveredTest::Require;
using IcuRecoveredTest::RequireThrows;

static_assert(std::is_same_v<decltype(&ulistfmt_openForType_nid_postfix), UListFormatter* (APS5_VABI*)(const char*, UListFormatterType, UListFormatterWidth, std::int32_t*)>);

void Run() {
    const std::array<std::int32_t, 7> failing{1, 2, 7, 15, 16, 27, 0x7fffffff};
    const std::array<std::int32_t, 4> passing{0, -128, -124, -1};
    for (const auto incoming : failing) {
        std::int32_t status = incoming;
        Require(ulistfmt_openForType_nid_postfix("en", 0, 0, &status) == nullptr);
        Require(status == incoming);

        status = incoming;
        Require(ulistfmt_openForType_nid_postfix(nullptr, 3, 2, &status) == nullptr);
        Require(status == incoming);
    }

    for (const auto incoming : passing) {
        std::int32_t status = incoming;
        RequireThrows([&] { ulistfmt_openForType_nid_postfix("en", 0, 0, &status); });
        Require(status == incoming);

        status = incoming;
        RequireThrows([&] { ulistfmt_openForType_nid_postfix(nullptr, 3, 2, &status); });
        Require(status == incoming);
    }

    RequireThrows([&] { ulistfmt_openForType_nid_postfix("en", 0, 0, nullptr); });
}

} // namespace UlistfmtOpenForType

namespace UlistfmtOpenResult {

using IcuRecoveredTest::Filler;
using IcuRecoveredTest::Require;
using IcuRecoveredTest::RequireNoThrow;
using IcuRecoveredTest::RequireThrows;
using IcuRecoveredTest::Unchanged;

static_assert(std::is_same_v<decltype(&ulistfmt_openResult_nid_postfix), UFormattedList* (APS5_VABI*)(std::int32_t*)>);

void Run() {
    const std::array<std::int32_t, 7> failing{1, 2, 7, 15, 16, 27, 0x7fffffff};
    const std::array<std::int32_t, 4> passing{0, -128, -124, -1};
    for (const auto incoming : failing) {
        std::int32_t status = incoming;
        Require(ulistfmt_openResult_nid_postfix(&status) == nullptr);
        Require(status == incoming);
    }

    for (const auto incoming : passing) {
        std::int32_t status = incoming;
        auto* result = ulistfmt_openResult_nid_postfix(&status);
        Require(result != nullptr);
        Require(status == incoming);
        ulistfmt_closeResult_nid_postfix(result);
    }

    std::int32_t status = 0;
    auto* first = ulistfmt_openResult_nid_postfix(&status);
    auto* second = ulistfmt_openResult_nid_postfix(&status);
    Require(status == 0 && first != nullptr && second != nullptr && first != second);
    ulistfmt_closeResult_nid_postfix(first);
    ulistfmt_closeResult_nid_postfix(second);

    RequireThrows([&] { ulistfmt_openResult_nid_postfix(nullptr); });
}

} // namespace UlistfmtOpenResult

void CheckIcuListFormatter() {
    UlistfmtClose::Run();
    UlistfmtCloseResult::Run();
    UlistfmtFormat::Run();
    UlistfmtFormatStringsToResult::Run();
    UlistfmtOpenForType::Run();
    UlistfmtOpenResult::Run();
}
