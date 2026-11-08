#include "prx/libSceIcu/libSceIcu.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <initializer_list>
#include <source_location>
#include <type_traits>

static_assert(std::is_same_v<decltype(&ucol_getTailoredSet_nid_postfix), USet* (APS5_VABI*)(const UCollator*, std::int32_t*)>);

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "ICU collation tailored set check failed at line %u\n", location.line());
    std::abort();
}

void TestFailingStatus() {
    for (std::int32_t errorCode : {1, 2, 7, 15, 16, 100}) {
        std::int32_t status = errorCode;
        USet* result = ucol_getTailoredSet_nid_postfix(nullptr, &status);
        Require(result == nullptr);
        Require(status == errorCode);
    }

    const UCollator* dummyCollator = reinterpret_cast<const UCollator*>(0x12340);
    for (std::int32_t errorCode : {1, 2, 7, 15, 16, 100}) {
        std::int32_t status = errorCode;
        USet* result = ucol_getTailoredSet_nid_postfix(dummyCollator, &status);
        Require(result == nullptr);
        Require(status == errorCode);
    }
}

}

int main() {
    TestFailingStatus();
    std::puts("ICU collation tailored set tests passed");
}
