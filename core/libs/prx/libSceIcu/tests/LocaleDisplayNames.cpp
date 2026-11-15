#include "prx/libSceIcu/libSceIcu.hpp"
#include "prx/libSceIcu/tests/RecoveredSignatureChecks.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <source_location>
#include <type_traits>

namespace UldnClose {

using IcuRecoveredTest::Filler;
using IcuRecoveredTest::Require;
using IcuRecoveredTest::RequireNoThrow;
using IcuRecoveredTest::RequireThrows;
using IcuRecoveredTest::Unchanged;

static_assert(std::is_same_v<decltype(&uldn_close_nid_postfix), void (APS5_VABI*)(ULocaleDisplayNames*)>);

void Run() {
    RequireNoThrow([] { uldn_close_nid_postfix(nullptr); });
    RequireNoThrow([] { uldn_close_nid_postfix(nullptr); });

    const auto storage = Filler();
    RequireThrows([&] { uldn_close_nid_postfix(reinterpret_cast<ULocaleDisplayNames*>(const_cast<std::uint32_t*>(storage.data()))); });
    Require(Unchanged(storage));
    RequireThrows([&] { uldn_close_nid_postfix(reinterpret_cast<ULocaleDisplayNames*>(std::uintptr_t{1})); });
}

} // namespace UldnClose

namespace UldnOpenForContext {

using IcuRecoveredTest::Filler;
using IcuRecoveredTest::Require;
using IcuRecoveredTest::RequireNoThrow;
using IcuRecoveredTest::RequireThrows;
using IcuRecoveredTest::Unchanged;

static_assert(std::is_same_v<decltype(&uldn_openForContext_nid_postfix), ULocaleDisplayNames* (APS5_VABI*)(const char*, std::int32_t*, std::int32_t, std::int32_t*)>);

void Run() {
    const std::array<std::int32_t, 7> failing{1, 2, 7, 15, 16, 27, 0x7fffffff};
    const std::array<std::int32_t, 4> passing{0, -128, -124, -1};
    const auto storage = Filler();
    const auto contexts = reinterpret_cast<std::int32_t*>(const_cast<std::uint32_t*>(storage.data()));

    for (const auto incoming : failing) {
        std::int32_t status = incoming;
        Require(uldn_openForContext_nid_postfix("en", contexts, 2, &status) == nullptr);
        Require(status == incoming);

        status = incoming;
        Require(uldn_openForContext_nid_postfix(nullptr, nullptr, 0, &status) == nullptr);
        Require(status == incoming);
    }

    for (const auto incoming : passing) {
        std::int32_t status = incoming;
        RequireThrows([&] { uldn_openForContext_nid_postfix("en", contexts, 2, &status); });
        Require(status == incoming);

        status = incoming;
        RequireThrows([&] { uldn_openForContext_nid_postfix(nullptr, nullptr, 7, &status); });
        Require(status == incoming);
    }

    Require(Unchanged(storage));
    RequireThrows([&] { uldn_openForContext_nid_postfix("en", contexts, 2, nullptr); });
}

} // namespace UldnOpenForContext

void CheckIcuLocaleDisplayNames() {
    UldnClose::Run();
    UldnOpenForContext::Run();
}
