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

extern "C" {
void APS5_VABI RSA_get0_key_nid_postfix(const TestKey*, const void**, const void**, const void**);
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
    RSA_get0_key_nid_postfix(&key, &first, &second, &third);
    Require(first == &Number[3] && second == &Number[4] && third == &Number[5]);
    first = second = third = Unset;
    RSA_get0_key_nid_postfix(&key, nullptr, &second, nullptr);
    Require(first == Unset && second == &Number[4] && third == Unset);
    RSA_get0_key_nid_postfix(&key, nullptr, nullptr, nullptr);
    key.slots[3] = nullptr;
    key.slots[5] = nullptr;
    RSA_get0_key_nid_postfix(&key, &first, nullptr, &third);
    Require(first == nullptr && third == nullptr);
    RequireThrows([&] { RSA_get0_key_nid_postfix(nullptr, &first, &second, &third); });
    RequireThrows([] { RSA_get0_key_nid_postfix(nullptr, nullptr, nullptr, nullptr); });
}

int main() {
    TestRsaKey();
    return 0;
}
