#include "prx/libSceLibreSsl3/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdio>
#include <cstdlib>
#include <source_location>
#include <stdexcept>

static int Number[12];

static const void* const Unset = &Number[0];

static LibreSsl3Rsa FilledRsa() {
    LibreSsl3Rsa rsa {};
    rsa.meth = &Number[1];
    rsa.n = &Number[2];
    rsa.e = &Number[3];
    rsa.d = &Number[4];
    rsa.p = &Number[5];
    rsa.q = &Number[6];
    rsa.dmp1 = &Number[7];
    rsa.dmq1 = &Number[8];
    rsa.iqmp = &Number[9];
    return rsa;
}

static LibreSsl3Dh FilledDh() {
    LibreSsl3Dh dh {};
    dh.p = &Number[1];
    dh.g = &Number[2];
    dh.pubKey = &Number[3];
    dh.privKey = &Number[4];
    dh.methodMontP = &Number[5];
    dh.q = &Number[6];
    return dh;
}

static LibreSsl3Dsa FilledDsa() {
    LibreSsl3Dsa dsa {};
    dsa.p = &Number[1];
    dsa.q = &Number[2];
    dsa.g = &Number[3];
    dsa.pubKey = &Number[4];
    dsa.privKey = &Number[5];
    return dsa;
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
    LibreSsl3Rsa key = FilledRsa();
    const void* first = Unset;
    const void* second = Unset;
    const void* third = Unset;
    RSA_get0_key_nid_postfix(&key, &first, &second, &third);
    Require(first == &Number[2] && second == &Number[3] && third == &Number[4]);
    first = second = third = Unset;
    RSA_get0_key_nid_postfix(&key, nullptr, &second, nullptr);
    Require(first == Unset && second == &Number[3] && third == Unset);
    RSA_get0_key_nid_postfix(&key, nullptr, nullptr, nullptr);
    key.n = nullptr;
    key.d = nullptr;
    RSA_get0_key_nid_postfix(&key, &first, nullptr, &third);
    Require(first == nullptr && third == nullptr);
    RSA_get0_key_nid_postfix(nullptr, nullptr, nullptr, nullptr);
    RequireThrows([&] { RSA_get0_key_nid_postfix(nullptr, &first, &second, &third); });
    RequireThrows([&] { RSA_get0_key_nid_postfix(nullptr, nullptr, nullptr, &third); });
}

static void TestDhPqg() {
    LibreSsl3Dh key = FilledDh();
    const void* first = Unset;
    const void* second = Unset;
    const void* third = Unset;
    DH_get0_pqg_nid_postfix(&key, &first, &second, &third);
    Require(first == &Number[1] && second == &Number[6] && third == &Number[2]);
    first = second = third = Unset;
    DH_get0_pqg_nid_postfix(&key, nullptr, &second, nullptr);
    Require(first == Unset && second == &Number[6] && third == Unset);
    DH_get0_pqg_nid_postfix(&key, nullptr, nullptr, nullptr);
    key.p = nullptr;
    key.g = nullptr;
    DH_get0_pqg_nid_postfix(&key, &first, nullptr, &third);
    Require(first == nullptr && third == nullptr);
    DH_get0_pqg_nid_postfix(nullptr, nullptr, nullptr, nullptr);
    RequireThrows([&] { DH_get0_pqg_nid_postfix(nullptr, &first, &second, &third); });
    RequireThrows([&] { DH_get0_pqg_nid_postfix(nullptr, nullptr, &second, nullptr); });
}

static void TestDhKey() {
    LibreSsl3Dh key = FilledDh();
    const void* first = Unset;
    const void* second = Unset;
    DH_get0_key_nid_postfix(&key, &first, &second);
    Require(first == &Number[3] && second == &Number[4]);
    first = second = Unset;
    DH_get0_key_nid_postfix(&key, nullptr, &second);
    Require(first == Unset && second == &Number[4]);
    second = Unset;
    DH_get0_key_nid_postfix(&key, &first, nullptr);
    Require(first == &Number[3] && second == Unset);
    key.privKey = nullptr;
    DH_get0_key_nid_postfix(&key, &first, &second);
    Require(first == &Number[3] && second == nullptr);
    DH_get0_key_nid_postfix(nullptr, nullptr, nullptr);
    RequireThrows([&] { DH_get0_key_nid_postfix(nullptr, &first, &second); });
    RequireThrows([&] { DH_get0_key_nid_postfix(nullptr, &first, nullptr); });
}

static void TestDsaPqg() {
    LibreSsl3Dsa key = FilledDsa();
    const void* first = Unset;
    const void* second = Unset;
    const void* third = Unset;
    DSA_get0_pqg_nid_postfix(&key, &first, &second, &third);
    Require(first == &Number[1] && second == &Number[2] && third == &Number[3]);
    first = second = third = Unset;
    DSA_get0_pqg_nid_postfix(&key, nullptr, &second, nullptr);
    Require(first == Unset && second == &Number[2] && third == Unset);
    DSA_get0_pqg_nid_postfix(&key, nullptr, nullptr, nullptr);
    key.p = nullptr;
    key.g = nullptr;
    DSA_get0_pqg_nid_postfix(&key, &first, nullptr, &third);
    Require(first == nullptr && third == nullptr);
    DSA_get0_pqg_nid_postfix(nullptr, nullptr, nullptr, nullptr);
    RequireThrows([&] { DSA_get0_pqg_nid_postfix(nullptr, &first, &second, &third); });
    RequireThrows([&] { DSA_get0_pqg_nid_postfix(nullptr, &first, nullptr, nullptr); });
}

static void TestDsaKey() {
    LibreSsl3Dsa key = FilledDsa();
    const void* first = Unset;
    const void* second = Unset;
    DSA_get0_key_nid_postfix(&key, &first, &second);
    Require(first == &Number[4] && second == &Number[5]);
    first = second = Unset;
    DSA_get0_key_nid_postfix(&key, nullptr, &second);
    Require(first == Unset && second == &Number[5]);
    second = Unset;
    DSA_get0_key_nid_postfix(&key, &first, nullptr);
    Require(first == &Number[4] && second == Unset);
    key.privKey = nullptr;
    DSA_get0_key_nid_postfix(&key, &first, &second);
    Require(first == &Number[4] && second == nullptr);
    DSA_get0_key_nid_postfix(nullptr, nullptr, nullptr);
    RequireThrows([&] { DSA_get0_key_nid_postfix(nullptr, &first, &second); });
    RequireThrows([&] { DSA_get0_key_nid_postfix(nullptr, nullptr, &second); });
}

int main() {
    TestRsaKey();
    TestDhPqg();
    TestDhKey();
    TestDsaPqg();
    TestDsaKey();
    return 0;
}
