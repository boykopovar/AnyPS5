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
        if (auto* names = uldn_openForContext_nid_postfix("en", contexts, 2, &status)) uldn_close_nid_postfix(names);

        status = incoming;
        if (auto* names = uldn_openForContext_nid_postfix(nullptr, nullptr, 7, &status)) uldn_close_nid_postfix(names);
    }

    {
        std::int32_t status = 0;
        if (auto* names = uldn_openForContext_nid_postfix("en", nullptr, 0, &status)) {
            std::array<char16_t, 64> out{};
            const std::int32_t locLen = uldn_localeDisplayName_nid_postfix(names, "en_US", out.data(), 64, &status);
            Require(locLen >= 0);
            status = 0;
            const std::int32_t scrLen = uldn_scriptDisplayName_nid_postfix(names, "Latn", out.data(), 64, &status);
            Require(scrLen >= 0);
            status = 0;
            const std::int32_t regLen = uldn_regionDisplayName_nid_postfix(names, "US", out.data(), 64, &status);
            Require(regLen >= 0);
            status = 0;
            const std::int32_t kvLen = uldn_keyValueDisplayName_nid_postfix(names, "calendar", "gregorian", out.data(), 64, &status);
            Require(kvLen >= 0);
            uldn_close_nid_postfix(names);
        }
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

using SingleCode = std::int32_t (APS5_VABI*)(const ULocaleDisplayNames*, const char*, char16_t*, std::int32_t, std::int32_t*);

static_assert(std::is_same_v<decltype(&uldn_localeDisplayName_nid_postfix), SingleCode>);
static_assert(std::is_same_v<decltype(&uldn_regionDisplayName_nid_postfix), SingleCode>);
static_assert(std::is_same_v<decltype(&uldn_scriptDisplayName_nid_postfix), SingleCode>);
static_assert(std::is_same_v<decltype(&uldn_keyValueDisplayName_nid_postfix),
                             std::int32_t (APS5_VABI*)(const ULocaleDisplayNames*, const char*, const char*, char16_t*, std::int32_t, std::int32_t*)>);

namespace {

constexpr std::int32_t IllegalArgument = 1;
constexpr std::array<std::int32_t, 7> Failing{1, 2, 7, 15, 16, 27, 0x7fffffff};
constexpr std::array<std::int32_t, 4> Passing{0, -128, -124, -1};

std::int32_t APS5_VABI KeyValue(const ULocaleDisplayNames* names, const char* code, char16_t* result, std::int32_t size, std::int32_t* status) {
    return uldn_keyValueDisplayName_nid_postfix(names, code, code, result, size, status);
}

void Check(SingleCode call, const ULocaleDisplayNames* names) {
    char16_t buffer[8]{};
    for (const auto incoming : Failing) {
        std::int32_t status = incoming;
        Require(call(names, "en", buffer, 8, &status) == 0);
        Require(call(nullptr, nullptr, nullptr, -1, &status) == 0);
        Require(status == incoming);
    }
    for (const auto incoming : Passing) {
        const auto illegal = [&](const ULocaleDisplayNames* target, const char* code, char16_t* result, std::int32_t size) {
            std::int32_t status = incoming;
            Require(call(target, code, result, size, &status) == 0);
            Require(status == IllegalArgument);
        };
        illegal(nullptr, "en", buffer, 8);
        illegal(names, nullptr, buffer, 8);
        illegal(names, "en", nullptr, 1);
        illegal(names, "en", buffer, -1);
        illegal(names, "en", nullptr, -1);
        std::int32_t status = incoming;
        RequireThrows([&] { call(names, "en", buffer, 8, &status); });
        RequireThrows([&] { call(names, "en", nullptr, 0, &status); });
        RequireThrows([&] { call(names, "en", buffer, 0, &status); });
        Require(status == incoming);
    }
    RequireThrows([&] { call(names, "en", buffer, 8, nullptr); });
    RequireThrows([&] { call(nullptr, nullptr, nullptr, -1, nullptr); });
    for (const auto unit : buffer) Require(unit == 0);
}

} // namespace

void Run() {
    const auto storage = Filler();
    const auto names = reinterpret_cast<const ULocaleDisplayNames*>(storage.data());
    Check(uldn_localeDisplayName_nid_postfix, names);
    Check(uldn_regionDisplayName_nid_postfix, names);
    Check(uldn_scriptDisplayName_nid_postfix, names);
    Check(KeyValue, names);
    char16_t buffer[8]{};
    for (const auto incoming : Passing) {
        std::int32_t status = incoming;
        Require(uldn_keyValueDisplayName_nid_postfix(names, "calendar", nullptr, buffer, 8, &status) == 0);
        Require(status == IllegalArgument);
        status = incoming;
        Require(uldn_keyValueDisplayName_nid_postfix(names, nullptr, "gregorian", buffer, 8, &status) == 0);
        Require(status == IllegalArgument);
        status = incoming;
        RequireThrows([&] { uldn_keyValueDisplayName_nid_postfix(names, "calendar", "gregorian", buffer, 8, &status); });
        Require(status == incoming);
    }
    Require(Unchanged(storage));
}

} // namespace UldnDisplayNames

void CheckIcuLocaleDisplayNames() {
    UldnClose::Run();
    UldnDisplayNames::Run();
    UldnOpenForContext::Run();
    LocaleDisplayNamesGuestPointers::Run();
}
