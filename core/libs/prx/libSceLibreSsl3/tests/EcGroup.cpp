#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"

#include <array>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <source_location>
#include <stdexcept>

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "LibreSSL3 EC_GROUP_cmp check failed at line %u\n", location.line());
    std::abort();
}

template <typename Function>
bool RejectsNull(Function function) {
    try {
        function();
    } catch (const std::invalid_argument&) {
        return true;
    }
    return false;
}

} // namespace

int main() {
    constexpr std::array nids{409, 415, 714, 715, 716, 928, 934};
    std::array<EcGroup*, nids.size()> groups{};
    std::array<EcGroup*, nids.size()> copies{};
    for (std::size_t i = 0; i < nids.size(); ++i) {
        groups[i] = EC_GROUP_new_by_curve_name_nid_postfix(nids[i]);
        copies[i] = EC_GROUP_new_by_curve_name_nid_postfix(nids[i]);
        Require(groups[i] && copies[i] && groups[i] != copies[i]);
    }
    int context = 0;
    for (std::size_t i = 0; i < nids.size(); ++i) {
        Require(EC_GROUP_cmp_nid_postfix(groups[i], groups[i], nullptr) == 0);
        for (std::size_t j = 0; j < nids.size(); ++j) {
            const int expected = i == j ? 0 : 1;
            Require(EC_GROUP_cmp_nid_postfix(groups[i], copies[j], nullptr) == expected);
            Require(EC_GROUP_cmp_nid_postfix(copies[j], groups[i], &context) == expected);
        }
    }
    Require(EC_GROUP_get_degree_nid_postfix(groups[1]) == EC_GROUP_get_degree_nid_postfix(groups[2]));
    Require(EC_GROUP_get_degree_nid_postfix(groups[1]) == EC_GROUP_get_degree_nid_postfix(groups[5]));
    Require(context == 0);
    Require(RejectsNull([&] { EC_GROUP_cmp_nid_postfix(nullptr, groups[0], nullptr); }));
    Require(RejectsNull([&] { EC_GROUP_cmp_nid_postfix(groups[0], nullptr, nullptr); }));
    Require(RejectsNull([] { EC_GROUP_cmp_nid_postfix(nullptr, nullptr, nullptr); }));
    for (std::size_t i = 0; i < nids.size(); ++i) {
        EC_GROUP_free_nid_postfix(groups[i]);
        EC_GROUP_free_nid_postfix(copies[i]);
    }
    std::puts("LibreSSL3 EC_GROUP_cmp tests passed");
}
