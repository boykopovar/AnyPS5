#include "prx/libSceIcu/libSceIcu.hpp"
#include "prx/libSceIcu/Status.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <initializer_list>
#include <source_location>
#include <type_traits>

static_assert(std::is_same_v<decltype(&ucal_setGregorianChange_nid_postfix), void (APS5_VABI*)(UCalendar*, UDate, std::int32_t*)>);

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "ICU calendar check failed at line %u\n", location.line());
    std::abort();
}

template<typename TAction>
void RequireThrows(TAction action) {
    try {
        action();
    } catch (const std::exception&) {
        return;
    }
    Require(false);
}

void NullCalendar() {
    for (double date : {0.0, -1000.0, 14400000.0, 1.0e12, -1.0e12}) {
        std::int32_t status = 0;
        ucal_setGregorianChange_nid_postfix(nullptr, date, &status);
        Require(status == Icu::IllegalArgument);
    }
}

void WarningStatus() {
    for (std::int32_t warning : {-1, -124, -128}) {
        std::int32_t status = warning;
        ucal_setGregorianChange_nid_postfix(nullptr, 0.0, &status);
        Require(status == Icu::IllegalArgument);
    }
}

void FailureShortCircuit() {
    int dummy = 0;
    auto* cal = reinterpret_cast<UCalendar*>(&dummy);
    for (std::int32_t failure : {1, 2, 7, 10, 15, 16}) {
        std::int32_t status = failure;
        ucal_setGregorianChange_nid_postfix(nullptr, 0.0, &status);
        Require(status == failure);
        status = failure;
        ucal_setGregorianChange_nid_postfix(cal, 0.0, &status);
        Require(status == failure);
    }
}

void NullStatus() {
    RequireThrows([] {
        ucal_setGregorianChange_nid_postfix(nullptr, 0.0, nullptr);
    });
}

}

int main() {
    NullCalendar();
    WarningStatus();
    FailureShortCircuit();
    NullStatus();
    std::puts("ICU calendar tests passed");
}
