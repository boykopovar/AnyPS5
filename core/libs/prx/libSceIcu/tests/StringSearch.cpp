#include "prx/libSceIcu/libSceIcu.hpp"
#include "prx/libSceIcu/tests/RecoveredSignatureChecks.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <source_location>
#include <type_traits>

namespace UsearchOpen {

using IcuRecoveredTest::Filler;
using IcuRecoveredTest::Require;
using IcuRecoveredTest::RequireNoThrow;
using IcuRecoveredTest::RequireThrows;
using IcuRecoveredTest::Unchanged;

static_assert(std::is_same_v<decltype(&usearch_open_nid_postfix), UStringSearch* (APS5_VABI*)(const char16_t*, std::int32_t, const char16_t*, std::int32_t, const char*, UBreakIterator*, std::int32_t*)>);

namespace {

constexpr std::int32_t IllegalArgument = 1;
constexpr std::int32_t UnsupportedError = 16;

} // namespace

void Run() {
    const std::array<std::int32_t, 7> failing{1, 2, 7, 15, 16, 27, 0x7fffffff};
    const std::array<std::int32_t, 4> passing{0, -128, -124, -1};
    const auto storage = Filler();
    const auto pattern = reinterpret_cast<const char16_t*>(storage.data());
    const auto text = reinterpret_cast<const char16_t*>(storage.data());
    const auto iterator = reinterpret_cast<UBreakIterator*>(const_cast<std::uint32_t*>(storage.data()));

    for (const auto incoming : failing) {
        std::int32_t status = incoming;
        Require(usearch_open_nid_postfix(pattern, 4, text, 8, "en", iterator, &status) == nullptr);
        Require(status == incoming);

        status = incoming;
        Require(usearch_open_nid_postfix(nullptr, -1, nullptr, -1, nullptr, nullptr, &status) == nullptr);
        Require(status == incoming);
    }

    for (const auto incoming : passing) {
        std::int32_t status = incoming;
        RequireThrows([&] { usearch_open_nid_postfix(pattern, 4, text, 8, "en", iterator, &status); });
        Require(status == incoming);

        status = incoming;
        RequireThrows([&] { usearch_open_nid_postfix(nullptr, 0, nullptr, 0, "de", nullptr, &status); });
        Require(status == incoming);

        status = incoming;
        Require(usearch_open_nid_postfix(pattern, 4, text, 8, nullptr, iterator, &status) == nullptr);
        Require(status == IllegalArgument);
    }

    Require(Unchanged(storage));
    RequireThrows([&] { usearch_open_nid_postfix(pattern, 4, text, 8, "en", iterator, nullptr); });
}

} // namespace UsearchOpen

void CheckIcuStringSearch() {
    UsearchOpen::Run();
}
