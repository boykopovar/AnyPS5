#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include "prx/libSceLibreSsl/include/Ec.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <new>
#include <stdexcept>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#else
#include <sys/mman.h>
#endif

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

template <typename TAction>
bool Rejected(TAction action) {
    errno = 71;
    bool rejected = false;
    try {
        action();
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    return rejected && errno == 71;
}

constexpr std::size_t ReadOnlyBytes = (sizeof(EcGroup) + 4095) & ~std::size_t{4095};

void* MapReadOnly() {
#ifdef _WIN32
    void* region = VirtualAlloc(nullptr, ReadOnlyBytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    Require(region != nullptr, "VirtualAlloc");
    DWORD previous = 0;
    Require(VirtualProtect(region, ReadOnlyBytes, PAGE_READONLY, &previous) != 0, "VirtualProtect");
#else
    void* region = mmap(nullptr, ReadOnlyBytes, PROT_READ, MAP_ANON | MAP_PRIVATE, -1, 0);
    Require(region != MAP_FAILED, "mmap");
#endif
    return region;
}

void Unmap(void* region) {
#ifdef _WIN32
    Require(VirtualFree(region, 0, MEM_RELEASE) != 0, "VirtualFree");
#else
    Require(munmap(region, ReadOnlyBytes) == 0, "munmap");
#endif
}

void RejectsUnusableGuestPointers() {
    auto* unmappedKey = reinterpret_cast<EcKey*>(std::uintptr_t{0x10});
    Require(Rejected([&] { EC_KEY_set_asn1_flag_nid_postfix(unmappedKey, NamedCurveFlag); }), "unreadable key throws and keeps errno");

    alignas(EcKey) unsigned char storage[sizeof(EcKey)];
    auto* key = new (storage) EcKey;
    key->group = reinterpret_cast<EcGroup*>(std::uintptr_t{0x10});
    Require(Rejected([&] { EC_KEY_set_asn1_flag_nid_postfix(key, NamedCurveFlag); }), "unmapped group throws and keeps errno");

    void* region = MapReadOnly();
    key->group = static_cast<EcGroup*>(region);
    Require(Rejected([&] { EC_KEY_set_asn1_flag_nid_postfix(key, NamedCurveFlag); }), "read-only group throws and keeps errno");
    Require(static_cast<const EcGroup*>(region)->asn1Flag == 0, "read-only group is untouched");
    Unmap(region);
    key->group = nullptr;
}

}

int main() {
    StoresTheFlagOnTheGroup();
    IgnoresKeyWithoutGroup();
    SetGroupCopiesTheFlag();
    RejectsNullKey();
    RejectsUnusableGuestPointers();
    std::puts("libSceLibreSsl3 EC key asn1 flag tests passed");
}
