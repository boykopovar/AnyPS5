#include "prx/libSceLibreSsl3/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <source_location>
#include <stdexcept>

struct TestKey {
    const void* slots[19];
};

static_assert(sizeof(TestKey) == 152);

static int Number[19];

static TestKey Filled() {
    TestKey key {};
    for (int slot = 1; slot < 19; ++slot) key.slots[slot] = &Number[slot];
    return key;
}

static const void* const Unset = &Number[0];

static const LibreSsl3Rsa* Rsa(const TestKey* key) {
    return reinterpret_cast<const LibreSsl3Rsa*>(key);
}

static const LibreSsl3Dh* Dh(const TestKey* key) {
    return reinterpret_cast<const LibreSsl3Dh*>(key);
}

static const LibreSsl3Dsa* Dsa(const TestKey* key) {
    return reinterpret_cast<const LibreSsl3Dsa*>(key);
}

static void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "LibreSSL key component check failed at line %u\n", location.line());
    std::abort();
}

template<typename TAction> static void RequireThrows(TAction action, std::source_location location = std::source_location::current()) {
    try { action(); } catch (const std::exception&) { return; }
    Require(false, location);
}

static void TestRsaKey() {
    TestKey key = Filled();
    const void* first = Unset;
    const void* second = Unset;
    const void* third = Unset;
    RSA_get0_key_nid_postfix(Rsa(&key), &first, &second, &third);
    Require(first == &Number[3] && second == &Number[4] && third == &Number[5]);
    first = second = third = Unset;
    RSA_get0_key_nid_postfix(Rsa(&key), nullptr, &second, nullptr);
    Require(first == Unset && second == &Number[4] && third == Unset);
    RSA_get0_key_nid_postfix(Rsa(&key), nullptr, nullptr, nullptr);
    key.slots[3] = nullptr;
    key.slots[5] = nullptr;
    RSA_get0_key_nid_postfix(Rsa(&key), &first, nullptr, &third);
    Require(first == nullptr && third == nullptr);
    RequireThrows([&] { RSA_get0_key_nid_postfix(nullptr, &first, &second, &third); });
    RequireThrows([] { RSA_get0_key_nid_postfix(nullptr, nullptr, nullptr, nullptr); });
}

static void TestDhPqg() {
    TestKey key = Filled();
    const void* first = Unset;
    const void* second = Unset;
    const void* third = Unset;
    DH_get0_pqg_nid_postfix(Dh(&key), &first, &second, &third);
    Require(first == &Number[1] && second == &Number[8] && third == &Number[2]);
    first = second = third = Unset;
    DH_get0_pqg_nid_postfix(Dh(&key), nullptr, &second, nullptr);
    Require(first == Unset && second == &Number[8] && third == Unset);
    DH_get0_pqg_nid_postfix(Dh(&key), nullptr, nullptr, nullptr);
    key.slots[1] = nullptr;
    key.slots[2] = nullptr;
    DH_get0_pqg_nid_postfix(Dh(&key), &first, nullptr, &third);
    Require(first == nullptr && third == nullptr);
    RequireThrows([&] { DH_get0_pqg_nid_postfix(nullptr, &first, &second, &third); });
    RequireThrows([] { DH_get0_pqg_nid_postfix(nullptr, nullptr, nullptr, nullptr); });
}

static void TestDhKey() {
    TestKey key = Filled();
    const void* first = Unset;
    const void* second = Unset;
    DH_get0_key_nid_postfix(Dh(&key), &first, &second);
    Require(first == &Number[4] && second == &Number[5]);
    first = second = Unset;
    DH_get0_key_nid_postfix(Dh(&key), nullptr, &second);
    Require(first == Unset && second == &Number[5]);
    second = Unset;
    DH_get0_key_nid_postfix(Dh(&key), &first, nullptr);
    Require(first == &Number[4] && second == Unset);
    key.slots[5] = nullptr;
    DH_get0_key_nid_postfix(Dh(&key), &first, &second);
    Require(first == &Number[4] && second == nullptr);
    RequireThrows([&] { DH_get0_key_nid_postfix(nullptr, &first, &second); });
    RequireThrows([] { DH_get0_key_nid_postfix(nullptr, nullptr, nullptr); });
}

static void TestDsaPqg() {
    TestKey key = Filled();
    const void* first = Unset;
    const void* second = Unset;
    const void* third = Unset;
    DSA_get0_pqg_nid_postfix(Dsa(&key), &first, &second, &third);
    Require(first == &Number[2] && second == &Number[3] && third == &Number[4]);
    first = second = third = Unset;
    DSA_get0_pqg_nid_postfix(Dsa(&key), nullptr, &second, nullptr);
    Require(first == Unset && second == &Number[3] && third == Unset);
    DSA_get0_pqg_nid_postfix(Dsa(&key), nullptr, nullptr, nullptr);
    key.slots[2] = nullptr;
    key.slots[4] = nullptr;
    DSA_get0_pqg_nid_postfix(Dsa(&key), &first, nullptr, &third);
    Require(first == nullptr && third == nullptr);
    RequireThrows([&] { DSA_get0_pqg_nid_postfix(nullptr, &first, &second, &third); });
    RequireThrows([] { DSA_get0_pqg_nid_postfix(nullptr, nullptr, nullptr, nullptr); });
}

static void TestDsaKey() {
    TestKey key = Filled();
    const void* first = Unset;
    const void* second = Unset;
    DSA_get0_key_nid_postfix(Dsa(&key), &first, &second);
    Require(first == &Number[5] && second == &Number[6]);
    first = second = Unset;
    DSA_get0_key_nid_postfix(Dsa(&key), nullptr, &second);
    Require(first == Unset && second == &Number[6]);
    second = Unset;
    DSA_get0_key_nid_postfix(Dsa(&key), &first, nullptr);
    Require(first == &Number[5] && second == Unset);
    key.slots[6] = nullptr;
    DSA_get0_key_nid_postfix(Dsa(&key), &first, &second);
    Require(first == &Number[5] && second == nullptr);
    RequireThrows([&] { DSA_get0_key_nid_postfix(nullptr, &first, &second); });
    RequireThrows([] { DSA_get0_key_nid_postfix(nullptr, nullptr, nullptr); });
}

int main() {
    TestRsaKey();
    TestDhPqg();
    TestDhKey();
    TestDsaPqg();
    TestDsaKey();
    return 0;
}
