#include "prx/libSceIcu/libSceIcu.hpp"
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <initializer_list>
#include <source_location>

namespace {

void Require(bool value, std::source_location location = std::source_location::current()) {
    if (value) return;
    std::fprintf(stderr, "ICU collation check failed at line %u\n", location.line());
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

}

int main() {
    for (std::int32_t attr : {0, 1, 2, 3, 4, 5, 6, 7, 8, 100, -1}) {
        std::int32_t status = 0;
        const auto result = ucol_getAttribute_nid_postfix(nullptr, attr, &status);
        Require(result == -1);
        Require(status == 0);
    }
    for (std::int32_t warning : {-1, -124, -128}) {
        std::int32_t status = warning;
        const auto result = ucol_getAttribute_nid_postfix(nullptr, 0, &status);
        Require(result == -1);
        Require(status == warning);
    }
    for (std::int32_t error : {1, 2, 10, 15, 16, 65535, 0x7fffffff}) {
        std::int32_t status = error;
        const auto result = ucol_getAttribute_nid_postfix(nullptr, 0, &status);
        Require(result == -1);
        Require(status == error);
    }
    const auto nonNull = reinterpret_cast<const UCollator*>(0x1000);
    for (std::int32_t error : {1, 15}) {
        std::int32_t status = error;
        const auto result = ucol_getAttribute_nid_postfix(nonNull, 0, &status);
        Require(result == -1);
        Require(status == error);
    }
    RequireThrows([] {
        ucol_getAttribute_nid_postfix(nullptr, 0, nullptr);
    });
    RequireThrows([&] {
        ucol_getAttribute_nid_postfix(nonNull, 0, nullptr);
    });
    RequireThrows([&] {
        std::int32_t status = 0;
        ucol_getAttribute_nid_postfix(nonNull, 0, &status);
    });
    RequireThrows([&] {
        std::int32_t status = -124;
        ucol_getAttribute_nid_postfix(nonNull, 0, &status);
    });
}
