#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdio>
#include <cstdlib>
#include <stdexcept>

extern "C" {
void* APS5_VABI EC_GROUP_new_by_curve_name_nid_postfix(int);
void APS5_VABI EC_GROUP_free_nid_postfix(void*);
int APS5_VABI EC_GROUP_get_degree_nid_postfix(const void*);
void* APS5_VABI EC_KEY_new_nid_postfix(void);
void* APS5_VABI EC_KEY_new_by_curve_name_nid_postfix(int);
void APS5_VABI EC_KEY_free_nid_postfix(void*);
const void* APS5_VABI EC_KEY_get0_group_nid_postfix(const void*);
int APS5_VABI EC_KEY_set_group_nid_postfix(void*, const void*);
int APS5_VABI ECDSA_size_nid_postfix(const void*);
}

struct Curve {
    int nid;
    int degree;
    int signatureSize;
};

static constexpr Curve Curves[] = {
    {409, 192, 56},
    {415, 256, 72},
    {711, 192, 56},
    {712, 224, 66},
    {713, 224, 64},
    {714, 256, 72},
    {715, 384, 104},
    {716, 521, 141},
    {928, 256, 72},
    {932, 384, 104},
    {934, 512, 137},
};

static constexpr int UnknownNids[] = {0, -1, 1, 406, 691, 746, 921, 937, 948, 100000};
static constexpr int CurvesMissingFromMbedtls[] = {410, 704, 721, 735, 749, 840, 929, 935, 936, 944};

static void Require(bool value, const char* what) {
    if (value) return;
    std::fprintf(stderr, "failed: %s\n", what);
    std::abort();
}

template<typename TAction> static void RequireThrows(TAction action, const char* what) {
    try { action(); } catch (const std::exception&) { return; }
    Require(false, what);
}

static void CheckNamedCurves() {
    for (const Curve& curve : Curves) {
        void* group = EC_GROUP_new_by_curve_name_nid_postfix(curve.nid);
        Require(group != nullptr, "EC_GROUP_new_by_curve_name for a named curve");
        Require(EC_GROUP_get_degree_nid_postfix(group) == curve.degree, "EC_GROUP_get_degree");
        void* key = EC_KEY_new_by_curve_name_nid_postfix(curve.nid);
        Require(key != nullptr, "EC_KEY_new_by_curve_name for a named curve");
        const void* keyGroup = EC_KEY_get0_group_nid_postfix(key);
        Require(keyGroup != nullptr && keyGroup != group, "EC_KEY_get0_group of a key made from a curve name");
        Require(EC_GROUP_get_degree_nid_postfix(keyGroup) == curve.degree, "degree of the key's group");
        Require(ECDSA_size_nid_postfix(key) == curve.signatureSize, "ECDSA_size");
        EC_KEY_free_nid_postfix(key);
        EC_GROUP_free_nid_postfix(group);
    }
}

static void CheckOtherNids() {
    for (const int nid : UnknownNids) {
        Require(EC_GROUP_new_by_curve_name_nid_postfix(nid) == nullptr, "EC_GROUP_new_by_curve_name for an unknown curve");
        Require(EC_KEY_new_by_curve_name_nid_postfix(nid) == nullptr, "EC_KEY_new_by_curve_name for an unknown curve");
    }
    for (const int nid : CurvesMissingFromMbedtls) {
        RequireThrows([&] { EC_GROUP_new_by_curve_name_nid_postfix(nid); }, "EC_GROUP_new_by_curve_name for a curve mbedtls lacks");
        RequireThrows([&] { EC_KEY_new_by_curve_name_nid_postfix(nid); }, "EC_KEY_new_by_curve_name for a curve mbedtls lacks");
    }
}

static void CheckSetGroup() {
    void* key = EC_KEY_new_nid_postfix();
    Require(key != nullptr, "EC_KEY_new");
    Require(EC_KEY_get0_group_nid_postfix(key) == nullptr, "EC_KEY_new has no group");
    Require(ECDSA_size_nid_postfix(key) == 0, "ECDSA_size of a key without a group");

    void* group = EC_GROUP_new_by_curve_name_nid_postfix(715);
    Require(EC_KEY_set_group_nid_postfix(key, group) == 1, "EC_KEY_set_group");
    const void* copy = EC_KEY_get0_group_nid_postfix(key);
    Require(copy != nullptr && copy != group, "EC_KEY_set_group stores a copy");
    EC_GROUP_free_nid_postfix(group);
    Require(EC_GROUP_get_degree_nid_postfix(EC_KEY_get0_group_nid_postfix(key)) == 384, "the copy outlives the original group");
    Require(ECDSA_size_nid_postfix(key) == 104, "ECDSA_size after EC_KEY_set_group");

    Require(EC_KEY_set_group_nid_postfix(key, EC_KEY_get0_group_nid_postfix(key)) == 1, "EC_KEY_set_group with the key's own group");
    Require(EC_GROUP_get_degree_nid_postfix(EC_KEY_get0_group_nid_postfix(key)) == 384, "the key's own group set again");

    void* other = EC_GROUP_new_by_curve_name_nid_postfix(409);
    Require(EC_KEY_set_group_nid_postfix(key, other) == 1, "EC_KEY_set_group replacing a group");
    Require(EC_GROUP_get_degree_nid_postfix(EC_KEY_get0_group_nid_postfix(key)) == 192, "the replaced group");
    EC_GROUP_free_nid_postfix(other);

    Require(EC_KEY_set_group_nid_postfix(key, nullptr) == 0, "EC_KEY_set_group with a null group");
    Require(EC_KEY_get0_group_nid_postfix(key) == nullptr, "a null group drops the previous one");
    Require(ECDSA_size_nid_postfix(key) == 0, "ECDSA_size after a null group");
    EC_KEY_free_nid_postfix(key);
}

static void CheckNullArguments() {
    EC_GROUP_free_nid_postfix(nullptr);
    EC_KEY_free_nid_postfix(nullptr);
    Require(ECDSA_size_nid_postfix(nullptr) == 0, "ECDSA_size of a null key");
    RequireThrows([] { EC_GROUP_get_degree_nid_postfix(nullptr); }, "EC_GROUP_get_degree of a null group");
    RequireThrows([] { EC_KEY_get0_group_nid_postfix(nullptr); }, "EC_KEY_get0_group of a null key");
    void* group = EC_GROUP_new_by_curve_name_nid_postfix(415);
    RequireThrows([&] { EC_KEY_set_group_nid_postfix(nullptr, group); }, "EC_KEY_set_group of a null key");
    EC_GROUP_free_nid_postfix(group);
}

int main() {
    CheckNamedCurves();
    CheckOtherNids();
    CheckSetGroup();
    CheckNullArguments();
    return 0;
}
