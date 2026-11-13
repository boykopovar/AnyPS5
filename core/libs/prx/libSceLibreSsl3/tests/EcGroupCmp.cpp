#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"

#include <cstdio>
#include <cstdlib>
#include <source_location>
#include <stdexcept>

struct EcGroup;

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "EC_GROUP_cmp check failed at line %u\n", location.line());
    std::abort();
}

template <typename Function>
void RequireThrows(Function function) {
    bool threw = false;
    try {
        function();
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);
}

}

int main() {
    ApplicationHeapInitialize_nid_no_patch(ApplicationEmptyProcessParameters_nid_no_patch());
    EcGroup* first = EC_GROUP_new_by_curve_name_nid_postfix(415);
    EcGroup* second = EC_GROUP_new_by_curve_name_nid_postfix(415);
    EcGroup* other = EC_GROUP_new_by_curve_name_nid_postfix(409);
    Require(first != nullptr && second != nullptr && other != nullptr);

    Require(EC_GROUP_cmp_nid_postfix(first, first, nullptr) == 0);
    Require(EC_GROUP_cmp_nid_postfix(first, second, nullptr) == 0);
    Require(EC_GROUP_cmp_nid_postfix(first, other, nullptr) == 1);
    Require(EC_GROUP_cmp_nid_postfix(other, first, nullptr) == 1);
    RequireThrows([&] { EC_GROUP_cmp_nid_postfix(nullptr, first, nullptr); });
    RequireThrows([&] { EC_GROUP_cmp_nid_postfix(first, nullptr, nullptr); });

    EC_GROUP_free_nid_postfix(first);
    EC_GROUP_free_nid_postfix(second);
    EC_GROUP_free_nid_postfix(other);
    std::puts("EC_GROUP_cmp tests passed");
}
