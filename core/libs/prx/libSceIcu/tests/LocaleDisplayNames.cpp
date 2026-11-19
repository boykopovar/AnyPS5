#include "prx/libSceIcu/libSceIcu.hpp"
#include "prx/libSceIcu/tests/RecoveredSignatureChecks.hpp"
#include <array>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <source_location>
#include <stdexcept>
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

namespace LocaleDisplayNamesGuestPointers {

void Run() {
    errno = 71;
    bool threw = false;
    try {
        uldn_openForContext_nid_postfix("en", nullptr, 0, reinterpret_cast<std::int32_t*>(std::uintptr_t{0x10}));
    } catch (const std::runtime_error&) {
        threw = true;
    }
    IcuRecoveredTest::Require(threw && errno == 71);
}

}

namespace UldnDisplayNames {

using IcuRecoveredTest::Filler;
using IcuRecoveredTest::Require;
using IcuRecoveredTest::RequireThrows;
using IcuRecoveredTest::Unchanged;

static_assert(std::is_same_v<decltype(&uldn_keyValueDisplayName_nid_postfix), std::int32_t (APS5_VABI*)(const ULocaleDisplayNames*, const char*, const char*, char16_t*, std::int32_t, std::int32_t*)>);
static_assert(std::is_same_v<decltype(&uldn_localeDisplayName_nid_postfix), std::int32_t (APS5_VABI*)(const ULocaleDisplayNames*, const char*, char16_t*, std::int32_t, std::int32_t*)>);
static_assert(std::is_same_v<decltype(&uldn_regionDisplayName_nid_postfix), std::int32_t (APS5_VABI*)(const ULocaleDisplayNames*, const char*, char16_t*, std::int32_t, std::int32_t*)>);
static_assert(std::is_same_v<decltype(&uldn_scriptDisplayName_nid_postfix), std::int32_t (APS5_VABI*)(const ULocaleDisplayNames*, const char*, char16_t*, std::int32_t, std::int32_t*)>);

namespace {

constexpr std::int32_t IllegalArgument = 1;

template<typename TCall>
void Check(TCall call) {
    const std::array<std::int32_t, 7> failing{1, 2, 7, 15, 16, 27, 0x7fffffff};
    const std::array<std::int32_t, 4> passing{0, -128, -124, -1};
    const auto storage = Filler();
    const auto names = reinterpret_cast<const ULocaleDisplayNames*>(storage.data());
    const auto destination = reinterpret_cast<char16_t*>(const_cast<std::uint32_t*>(storage.data()));

    for (const auto incoming : failing) {
        std::int32_t status = incoming;
        Require(call(names, true, destination, 8, &status) == 0);
        Require(status == incoming);

        status = incoming;
        Require(call(nullptr, false, nullptr, 4, &status) == 0);
        Require(status == incoming);

        status = incoming;
        Require(call(names, true, destination, -1, &status) == 0);
        Require(status == incoming);
    }

    for (const auto incoming : passing) {
        std::int32_t status = incoming;
        RequireThrows([&] { call(names, true, destination, 8, &status); });
        Require(status == incoming);

        status = incoming;
        RequireThrows([&] { call(names, true, nullptr, 0, &status); });
        Require(status == incoming);

        status = incoming;
        Require(call(nullptr, true, destination, 8, &status) == 0);
        Require(status == IllegalArgument);

        status = incoming;
        Require(call(names, false, destination, 8, &status) == 0);
        Require(status == IllegalArgument);

        status = incoming;
        Require(call(names, true, nullptr, 4, &status) == 0);
        Require(status == IllegalArgument);

        status = incoming;
        Require(call(names, true, destination, -2, &status) == 0);
        Require(status == IllegalArgument);
    }

    Require(Unchanged(storage));
    RequireThrows([&] { call(names, true, destination, 8, nullptr); });
    RequireThrows([&] { call(nullptr, false, nullptr, -1, nullptr); });

    errno = 71;
    bool threw = false;
    try {
        call(nullptr, true, destination, 8, reinterpret_cast<std::int32_t*>(std::uintptr_t{0x10}));
    } catch (const std::runtime_error&) {
        threw = true;
    }
    Require(threw && errno == 71);
}

} // namespace

void Run() {
    Check([](const ULocaleDisplayNames* names, bool present, char16_t* result, std::int32_t capacity, std::int32_t* status) {
        return uldn_localeDisplayName_nid_postfix(names, present ? "sr_Latn_RS" : nullptr, result, capacity, status);
    });
    Check([](const ULocaleDisplayNames* names, bool present, char16_t* result, std::int32_t capacity, std::int32_t* status) {
        return uldn_regionDisplayName_nid_postfix(names, present ? "RS" : nullptr, result, capacity, status);
    });
    Check([](const ULocaleDisplayNames* names, bool present, char16_t* result, std::int32_t capacity, std::int32_t* status) {
        return uldn_scriptDisplayName_nid_postfix(names, present ? "Latn" : nullptr, result, capacity, status);
    });
    Check([](const ULocaleDisplayNames* names, bool present, char16_t* result, std::int32_t capacity, std::int32_t* status) {
        return uldn_keyValueDisplayName_nid_postfix(names, present ? "calendar" : nullptr, present ? "gregorian" : nullptr, result, capacity, status);
    });
    Check([](const ULocaleDisplayNames* names, bool present, char16_t* result, std::int32_t capacity, std::int32_t* status) {
        return uldn_keyValueDisplayName_nid_postfix(names, "calendar", present ? "gregorian" : nullptr, result, capacity, status);
    });
    Check([](const ULocaleDisplayNames* names, bool present, char16_t* result, std::int32_t capacity, std::int32_t* status) {
        return uldn_keyValueDisplayName_nid_postfix(names, present ? "calendar" : nullptr, "gregorian", result, capacity, status);
    });
}

} // namespace UldnDisplayNames

void CheckIcuLocaleDisplayNames() {
    UldnClose::Run();
    UldnOpenForContext::Run();
    LocaleDisplayNamesGuestPointers::Run();
    UldnDisplayNames::Run();
}
