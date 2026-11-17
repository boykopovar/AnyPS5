#include "prx/libSceIcu/libSceIcu.hpp"
#include "prx/libSceIcu/tests/RecoveredSignatureChecks.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <initializer_list>
#include <source_location>
#include <stdexcept>
#include <type_traits>

namespace UfieldpositerClose {

using IcuRecoveredTest::Require;

static_assert(std::is_same_v<decltype(&ufieldpositer_close_nid_postfix), void (APS5_VABI*)(UFieldPositionIterator*)>);

namespace {

void RequireNonNullThrows(UFieldPositionIterator* iterator) {
    try {
        ufieldpositer_close_nid_postfix(iterator);
    } catch (const std::runtime_error&) {
        return;
    }
    Require(false, "non-null UFieldPositionIterator handle did not throw");
}

}

void Run() {
    ufieldpositer_close_nid_postfix(nullptr);
    UFieldPositionIterator* nullIterator = nullptr;
    ufieldpositer_close_nid_postfix(nullIterator);

    alignas(8) std::array<std::uint8_t, 64> storage{};
    storage.fill(0x5a);
    const auto expected = storage;
    RequireNonNullThrows(reinterpret_cast<UFieldPositionIterator*>(storage.data()));
    Require(storage == expected, "non-null UFieldPositionIterator handle was modified before the throw");
    RequireNonNullThrows(reinterpret_cast<UFieldPositionIterator*>(static_cast<std::uintptr_t>(1)));
}

} // namespace UfieldpositerClose

namespace UfieldpositerOpen {

using IcuRecoveredTest::Require;
using IcuRecoveredTest::RequireThrows;

static_assert(std::is_same_v<decltype(&ufieldpositer_open_nid_postfix), UFieldPositionIterator* (APS5_VABI*)(std::int32_t*)>);

namespace {

void Open() {
    for (std::int32_t incoming : {1, 2, 7, 10, 15, 16, 27}) {
        auto status = incoming;
        auto* iterator = ufieldpositer_open_nid_postfix(&status);
        Require(iterator == nullptr);
        Require(status == incoming);
    }
    RequireThrows([&] { ufieldpositer_open_nid_postfix(nullptr); });
}

}

void Run() {
    Open();
}

} // namespace UfieldpositerOpen

void CheckIcuFieldPositionIterator() {
    UfieldpositerClose::Run();
    UfieldpositerOpen::Run();
}
