#include "prx/libSceIcu/libSceIcu.hpp"
#include <cstdio>
#include <cstdlib>
#include <source_location>
#include <stdexcept>

namespace {

int calls = 0;

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "ICU cleanup check failed at line %u\n", location.line());
    std::exit(EXIT_FAILURE);
}

std::int8_t APS5_VABI First_nid_no_patch() {
    ++calls;
    return -128;
}

std::int8_t APS5_VABI Second_nid_no_patch() {
    calls += 10;
    return 0;
}

std::int8_t APS5_VABI Register_nid_no_patch() {
    ++calls;
    ucln_registerCleanup_nid_postfix(3, Second_nid_no_patch);
    return 1;
}

}

int main() {
    for (int type = 0; type < 8; ++type) {
        ucln_cleanupOne_nid_postfix(type);
        ucln_registerCleanup_nid_postfix(type, First_nid_no_patch);
    }
    Require(!calls);
    for (int type = 0; type < 8; ++type) {
        ucln_cleanupOne_nid_postfix(type);
        Require(calls == type + 1);
        ucln_cleanupOne_nid_postfix(type);
        Require(calls == type + 1);
    }
    ucln_registerCleanup_nid_postfix(0, First_nid_no_patch);
    ucln_registerCleanup_nid_postfix(0, Second_nid_no_patch);
    ucln_cleanupOne_nid_postfix(0);
    Require(calls == 18);
    ucln_registerCleanup_nid_postfix(1, First_nid_no_patch);
    ucln_registerCleanup_nid_postfix(1, nullptr);
    ucln_cleanupOne_nid_postfix(1);
    Require(calls == 18);
    ucln_registerCleanup_nid_postfix(-1, First_nid_no_patch);
    ucln_registerCleanup_nid_postfix(8, First_nid_no_patch);
    Require(calls == 18);
    ucln_registerCleanup_nid_postfix(3, Register_nid_no_patch);
    ucln_cleanupOne_nid_postfix(3);
    ucln_cleanupOne_nid_postfix(3);
    Require(calls == 19);
    for (const auto type : {-2147483647, -1, 8, 2147483647}) {
        bool threw = false;
        try { ucln_cleanupOne_nid_postfix(type); } catch (const std::invalid_argument&) { threw = true; }
        Require(threw);
    }
    std::puts("ICU cleanup tests passed");
}
