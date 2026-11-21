#include "prx/libSceLibcInternal/include/TimeGetFields.hpp"
#include "prx/libc/include/GuestLocale.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <source_location>
#include <stdexcept>

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "libc internal wide time_put base-object constructor check failed at line %u\n", location.line());
    std::abort();
}

void CheckMatchesTheCompleteObjectConstructor() {
    GuestLocale::Facet complete{};
    GuestLocale::Facet base{};
    _ZNSt8time_putIwSt19ostreambuf_iteratorIwSt11char_traitsIwEEEC1Em_nid_postfix(&complete, 7);
    _ZNSt8time_putIwSt19ostreambuf_iteratorIwSt11char_traitsIwEEEC2Em_nid_postfix(&base, 7);
    Require(complete.vtable != nullptr);
    Require(std::memcmp(&complete, &base, sizeof(complete)) == 0);
    Require(base.references == 7);
}

void CheckKeepsTheReferenceCount() {
    GuestLocale::Facet facet{};
    _ZNSt8time_putIwSt19ostreambuf_iteratorIwSt11char_traitsIwEEEC2Em_nid_postfix(&facet, 0);
    Require(facet.vtable != nullptr);
    Require(facet.references == 0);
    Require(facet.reserved == 0);
}

void CheckRejectsNull() {
    bool threw = false;
    try {
        _ZNSt8time_putIwSt19ostreambuf_iteratorIwSt11char_traitsIwEEEC2Em_nid_postfix(nullptr, 1);
    } catch (...) {
        threw = true;
    }
    Require(threw);
}

}

int main() {
    CheckMatchesTheCompleteObjectConstructor();
    CheckKeepsTheReferenceCount();
    CheckRejectsNull();
    std::printf("libc internal wide time_put base-object constructor checks passed\n");
    return 0;
}
