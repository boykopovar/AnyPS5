#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include "prx/libSceLibreSsl/include/Ec.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdio>
#include <cstdlib>
#include <stdexcept>

namespace {

constexpr int Secp256r1 = 415;
constexpr int NamedCurveFlag = 1;

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "%s\n", message);
    std::abort();
}

int FlagOf(const EcKey* key) {
    const EcGroup* group = EC_KEY_get0_group_nid_postfix(key);
    Require(group != nullptr, "key has a group");
    return group->asn1Flag;
}

void StoresTheFlagOnTheGroup() {
    EcKey* key = EC_KEY_new_by_curve_name_nid_postfix(Secp256r1);
    Require(key != nullptr, "EC_KEY_new_by_curve_name");
    Require(FlagOf(key) == 0, "a new group has no asn1 flag");
    EC_KEY_set_asn1_flag_nid_postfix(key, NamedCurveFlag);
    Require(FlagOf(key) == NamedCurveFlag, "flag is stored");
    EC_KEY_set_asn1_flag_nid_postfix(key, 0);
    Require(FlagOf(key) == 0, "flag is cleared");
    EC_KEY_set_asn1_flag_nid_postfix(key, -7);
    Require(FlagOf(key) == -7, "flag is not validated");
    EC_KEY_free_nid_postfix(key);
}

void IgnoresKeyWithoutGroup() {
    EcKey* key = EC_KEY_new_nid_postfix();
    Require(key != nullptr, "EC_KEY_new");
    EC_KEY_set_asn1_flag_nid_postfix(key, NamedCurveFlag);
    Require(EC_KEY_get0_group_nid_postfix(key) == nullptr, "no group is created");
    EC_KEY_free_nid_postfix(key);
}

void SetGroupCopiesTheFlag() {
    EcKey* source = EC_KEY_new_by_curve_name_nid_postfix(Secp256r1);
    EcKey* target = EC_KEY_new_nid_postfix();
    Require(source != nullptr && target != nullptr, "keys");
    EC_KEY_set_asn1_flag_nid_postfix(source, NamedCurveFlag);
    Require(EC_KEY_set_group_nid_postfix(target, EC_KEY_get0_group_nid_postfix(source)) == 1, "EC_KEY_set_group");
    Require(FlagOf(target) == NamedCurveFlag, "copy keeps the flag");
    EC_KEY_set_asn1_flag_nid_postfix(target, 0);
    Require(FlagOf(source) == NamedCurveFlag, "copy does not share the flag");
    EC_KEY_set_asn1_flag_nid_postfix(target, NamedCurveFlag);
    Require(EC_KEY_set_group_nid_postfix(target, EC_KEY_get0_group_nid_postfix(target)) == 1, "EC_KEY_set_group with its own group");
    Require(FlagOf(target) == NamedCurveFlag, "self copy keeps the flag");
    EC_KEY_free_nid_postfix(target);
    EC_KEY_free_nid_postfix(source);
}

void RejectsNullKey() {
    try {
        EC_KEY_set_asn1_flag_nid_postfix(nullptr, NamedCurveFlag);
    } catch (const std::invalid_argument&) {
        return;
    }
    Require(false, "null key throws");
}

}

int main() {
    StoresTheFlagOnTheGroup();
    IgnoresKeyWithoutGroup();
    SetGroupCopiesTheFlag();
    RejectsNullKey();
    std::puts("libSceLibreSsl3 EC key asn1 flag tests passed");
}
