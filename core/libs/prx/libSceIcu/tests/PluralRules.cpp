#include "prx/libSceIcu/libSceIcu.hpp"
#include "prx/libSceIcu/tests/RecoveredSignatureChecks.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <source_location>
#include <stdexcept>
#include <string_view>
#include <type_traits>

namespace UplrulesClose {

using IcuRecoveredTest::Require;

static_assert(std::is_same_v<decltype(&uplrules_close_nid_postfix), void (APS5_VABI*)(UPluralRules*)>);

namespace {

void CheckNonNullThrows(UPluralRules* handle) {
    try {
        uplrules_close_nid_postfix(handle);
    } catch (const std::runtime_error& error) {
        Require(std::string_view(error.what()) == "uplrules_close_nid_postfix not implemented", "unexpected runtime_error message");
        return;
    }
    Require(false, "non-null handle did not throw");
}

}

void Run() {
    uplrules_close_nid_postfix(nullptr);
    uplrules_close_nid_postfix(nullptr);

    alignas(8) std::array<std::uint8_t, 64> storage;
    storage.fill(0xa5);
    CheckNonNullThrows(reinterpret_cast<UPluralRules*>(storage.data()));
    for (const std::uint8_t byte : storage) {
        Require(byte == 0xa5, "non-null close modified caller storage");
    }
    CheckNonNullThrows(reinterpret_cast<UPluralRules*>(static_cast<std::uintptr_t>(1)));
}

} // namespace UplrulesClose

namespace UplrulesOpenForType {

using IcuRecoveredTest::Filler;
using IcuRecoveredTest::Require;
using IcuRecoveredTest::RequireNoThrow;
using IcuRecoveredTest::RequireThrows;
using IcuRecoveredTest::Unchanged;

static_assert(std::is_same_v<decltype(&uplrules_openForType_nid_postfix), UPluralRules* (APS5_VABI*)(const char*, std::int32_t, std::int32_t*)>);

namespace {

constexpr std::int32_t IllegalArgument = 1;
constexpr std::int32_t UnsupportedError = 16;

} // namespace

void Run() {
    const std::array<std::int32_t, 7> failing{1, 2, 7, 15, 16, 27, 0x7fffffff};
    const std::array<std::int32_t, 4> passing{0, -128, -124, -1};
    const std::array<std::int32_t, 3> usable{0, 1, -1};
    const std::array<std::int32_t, 3> beyond{2, 3, 0x7fffffff};

    for (const auto incoming : failing) {
        for (const auto type : usable) {
            std::int32_t status = incoming;
            Require(uplrules_openForType_nid_postfix("en", type, &status) == nullptr);
            Require(status == incoming);
        }

        for (const auto type : beyond) {
            std::int32_t status = incoming;
            Require(uplrules_openForType_nid_postfix("en", type, &status) == nullptr);
            Require(status == incoming);
        }
    }

    for (const auto incoming : passing) {
        for (const auto type : usable) {
            std::int32_t status = incoming;
            RequireThrows([&] { uplrules_openForType_nid_postfix("en", type, &status); });
            Require(status == incoming);
        }

        for (const auto type : beyond) {
            std::int32_t status = incoming;
            Require(uplrules_openForType_nid_postfix("en", type, &status) == nullptr);
            Require(status == IllegalArgument);
        }
    }

    RequireThrows([&] { uplrules_openForType_nid_postfix("en", 0, nullptr); });
}

} // namespace UplrulesOpenForType

void CheckIcuPluralRules() {
    UplrulesClose::Run();
    UplrulesOpenForType::Run();
}
