#include "prx/libSceIcu/libSceIcu.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <initializer_list>
#include <source_location>
#include <type_traits>

static_assert(std::is_same_v<decltype(&ufieldpositer_open_nid_postfix), UFieldPositionIterator* (APS5_VABI*)(std::int32_t*)>);

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "ICU field position iterator check failed at line %u\n", location.line());
    std::abort();
}

template<typename TAction>
void RequireThrows(TAction action) {
    try { action(); } catch (const std::exception&) { return; }
    Require(false);
}

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

int main() {
    Open();
    std::puts("ICU field position iterator tests passed");
}
