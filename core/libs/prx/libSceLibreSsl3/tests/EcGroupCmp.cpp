#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"

#include <cstdio>
#include <cstdlib>
#include <stdexcept>

namespace {

constexpr int Secp256r1 = 415;
constexpr int Secp384r1 = 715;

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "%s\n", message);
    std::abort();
}

template <typename Function>
void RequireInvalidArgument(Function function, const char* message) {
    try {
        function();
    } catch (const std::invalid_argument&) {
        return;
    }
    Require(false, message);
}

void ComparesNamedCurves() {
    EcGroup* first = EC_GROUP_new_by_curve_name_nid_postfix(Secp256r1);
    EcGroup* second = EC_GROUP_new_by_curve_name_nid_postfix(Secp256r1);
    EcGroup* other = EC_GROUP_new_by_curve_name_nid_postfix(Secp384r1);
    Require(first && second && other, "groups are created");
    Require(EC_GROUP_cmp_nid_postfix(first, second, nullptr) == 0, "equal NIDs compare equal");
    Require(EC_GROUP_cmp_nid_postfix(first, first, nullptr) == 0, "a group compares equal to itself");
    Require(EC_GROUP_cmp_nid_postfix(first, other, nullptr) == 1, "different NIDs compare different");
    Require(EC_GROUP_cmp_nid_postfix(other, first, nullptr) == 1, "the comparison is symmetric");
    EC_GROUP_free_nid_postfix(other);
    EC_GROUP_free_nid_postfix(second);
    EC_GROUP_free_nid_postfix(first);
}

void RejectsNullGroups() {
    EcGroup* group = EC_GROUP_new_by_curve_name_nid_postfix(Secp256r1);
    Require(group != nullptr, "group is created");
    RequireInvalidArgument([&] { EC_GROUP_cmp_nid_postfix(nullptr, group, nullptr); }, "a null first group throws");
    RequireInvalidArgument([&] { EC_GROUP_cmp_nid_postfix(group, nullptr, nullptr); }, "a null second group throws");
    EC_GROUP_free_nid_postfix(group);
}

}

int main() {
    ComparesNamedCurves();
    RejectsNullGroups();
    std::puts("libSceLibreSsl3 EC_GROUP_cmp tests passed");
}
