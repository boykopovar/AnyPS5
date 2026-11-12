#include "prx/libSceIcu/libSceIcu.hpp"
#include "prx/libSceIcu/tests/RecoveredSignatureChecks.hpp"
#include "prx/libc/include/General.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <source_location>
#include <stdexcept>
#include <type_traits>

namespace UreldatefmtClose {

using IcuRecoveredTest::Require;

static_assert(std::is_same_v<decltype(&ureldatefmt_close_nid_postfix), void (APS5_VABI*)(URelativeDateTimeFormatter*)>);

void Run() {
    const auto before = NotImplementedCalls_nid_no_patch();
    ureldatefmt_close_nid_postfix(nullptr);
    ureldatefmt_close_nid_postfix(nullptr);
    Require(NotImplementedCalls_nid_no_patch() == before, "null disposal does not hit unimplemented stub");

    alignas(8) std::array<std::uint8_t, 64> storage;
    storage.fill(0xa5);
    auto* foreign = reinterpret_cast<URelativeDateTimeFormatter*>(storage.data());
    bool threw = false;
    try {
        ureldatefmt_close_nid_postfix(foreign);
    } catch (const std::runtime_error&) {
        threw = true;
    }
    Require(threw && NotImplementedCalls_nid_no_patch() == before + 1, "non-null handle throws NotImplemented_nid_no_patch");
    for (const auto byte : storage) {
        Require(byte == 0xa5, "non-null handle memory is untouched");
    }
}

} // namespace UreldatefmtClose

namespace UreldatefmtFormat {

using IcuRecoveredTest::Filler;
using IcuRecoveredTest::Require;
using IcuRecoveredTest::RequireNoThrow;
using IcuRecoveredTest::RequireThrows;
using IcuRecoveredTest::Unchanged;

static_assert(std::is_same_v<decltype(&ureldatefmt_format_nid_postfix), std::int32_t (APS5_VABI*)(const URelativeDateTimeFormatter*, double, std::int32_t, char16_t*, std::int32_t, std::int32_t*)>);

namespace {

constexpr std::int32_t IllegalArgument = 1;
constexpr std::int32_t UnsupportedError = 16;

} // namespace

void Run() {
    const std::array<std::int32_t, 7> failing{1, 2, 7, 15, 16, 27, 0x7fffffff};
    const std::array<std::int32_t, 4> passing{0, -128, -124, -1};
    const auto storage = Filler();
    const auto destination = reinterpret_cast<char16_t*>(const_cast<std::uint32_t*>(storage.data()));
    const auto formatter = reinterpret_cast<URelativeDateTimeFormatter*>(const_cast<std::uint32_t*>(storage.data()));

    for (const auto incoming : failing) {
        std::int32_t status = incoming;
        Require(ureldatefmt_format_nid_postfix(formatter, -3.5, 0, destination, 8, &status) == 0);
        Require(status == incoming);

        status = incoming;
        Require(ureldatefmt_format_nid_postfix(formatter, 0.0, 7, nullptr, 4, &status) == 0);
        Require(status == incoming);

        status = incoming;
        Require(ureldatefmt_format_nid_postfix(formatter, 0.0, 7, destination, -1, &status) == 0);
        Require(status == incoming);
    }

    for (const auto incoming : passing) {
        std::int32_t status = incoming;
        RequireThrows([&] { ureldatefmt_format_nid_postfix(formatter, -3.5, 0, destination, 8, &status); });
        Require(status == incoming);

        status = incoming;
        RequireThrows([&] { ureldatefmt_format_nid_postfix(formatter, 2.0, 5, nullptr, 0, &status); });
        Require(status == incoming);

        status = incoming;
        Require(ureldatefmt_format_nid_postfix(formatter, 2.0, 5, nullptr, 4, &status) == 0);
        Require(status == IllegalArgument);

        status = incoming;
        Require(ureldatefmt_format_nid_postfix(formatter, 2.0, 5, destination, -2, &status) == 0);
        Require(status == IllegalArgument);
    }

    Require(Unchanged(storage));
    RequireThrows([&] { ureldatefmt_format_nid_postfix(formatter, 0.0, 0, destination, 8, nullptr); });
}

} // namespace UreldatefmtFormat

namespace UreldatefmtOpen {

using IcuRecoveredTest::Filler;
using IcuRecoveredTest::Require;
using IcuRecoveredTest::RequireNoThrow;
using IcuRecoveredTest::RequireThrows;
using IcuRecoveredTest::Unchanged;

static_assert(std::is_same_v<decltype(&ureldatefmt_open_nid_postfix), URelativeDateTimeFormatter* (APS5_VABI*)(const char*, UNumberFormat*, std::int32_t, std::int32_t, std::int32_t*)>);

void Run() {
    const std::array<std::int32_t, 7> failing{1, 2, 7, 15, 16, 27, 0x7fffffff};
    const std::array<std::int32_t, 4> passing{0, -128, -124, -1};
    const auto storage = Filler();
    const auto adopted = reinterpret_cast<UNumberFormat*>(const_cast<std::uint32_t*>(storage.data()));

    for (const auto incoming : failing) {
        std::int32_t status = incoming;
        Require(ureldatefmt_open_nid_postfix("en", adopted, 1, 0, &status) == nullptr);
        Require(status == incoming);

        status = incoming;
        Require(ureldatefmt_open_nid_postfix(nullptr, nullptr, 3, 301, &status) == nullptr);
        Require(status == incoming);
    }

    for (const auto incoming : passing) {
        std::int32_t status = incoming;
        RequireThrows([&] { ureldatefmt_open_nid_postfix("en", adopted, 1, 0, &status); });
        Require(status == incoming);

        status = incoming;
        RequireThrows([&] { ureldatefmt_open_nid_postfix(nullptr, nullptr, 3, 301, &status); });
        Require(status == incoming);
    }

    Require(Unchanged(storage));
    RequireThrows([&] { ureldatefmt_open_nid_postfix("en", adopted, 1, 0, nullptr); });
}

} // namespace UreldatefmtOpen

void CheckIcuRelativeDateTimeFormatter() {
    UreldatefmtClose::Run();
    UreldatefmtFormat::Run();
    UreldatefmtOpen::Run();
}
