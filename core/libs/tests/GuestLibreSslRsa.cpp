#include "prx/libSceLibreSsl/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <source_location>
#include <stdexcept>

struct TestBignum {
    std::uint64_t* d;
    int top;
    int dmax;
    int neg;
    int flags;
};

struct TestRsaMethod {
    const char* name;
    void* operations[8];
    int flags;
    char* appData;
    void* sign;
    void* verify;
    void* keygen;
};

struct TestRsa {
    int pad;
    std::int64_t version;
    const TestRsaMethod* meth;
    void* engine;
    void* n;
    void* rest[15];
};

static_assert(offsetof(TestRsaMethod, flags) == 72);
static_assert(offsetof(TestRsa, meth) == 16 && offsetof(TestRsa, n) == 32 && sizeof(TestRsa) == 160);

static const LibreSslRsa* Rsa(const TestRsa* rsa) {
    return reinterpret_cast<const LibreSslRsa*>(rsa);
}

static void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "LibreSSL RSA check failed at line %u\n", location.line());
    std::abort();
}

template<typename TAction> static void RequireThrows(TAction action, std::source_location location = std::source_location::current()) {
    try { action(); } catch (const std::exception&) { return; }
    Require(false, location);
}

static void TestFlags() {
    TestRsaMethod method {};
    TestRsa rsa {};
    rsa.meth = &method;
    Require(RSA_flags_nid_postfix(nullptr) == 0);
    Require(RSA_flags_nid_postfix(Rsa(&rsa)) == 0);
    method.flags = 0x0400;
    Require(RSA_flags_nid_postfix(Rsa(&rsa)) == 0x0400);
    method.flags = -1;
    Require(RSA_flags_nid_postfix(Rsa(&rsa)) == -1);
    rsa.meth = nullptr;
    RequireThrows([&] { RSA_flags_nid_postfix(Rsa(&rsa)); });
}

static int Size(TestBignum* modulus) {
    TestRsa rsa {};
    rsa.n = modulus;
    return RSA_size_nid_postfix(Rsa(&rsa));
}

static void TestSize() {
    std::uint64_t words[32] {};
    TestBignum modulus {words, 0, 32, 0, 0};
    Require(Size(&modulus) == 0);
    modulus.d = nullptr;
    Require(Size(&modulus) == 0);
    modulus.d = words;
    modulus.top = 1;
    words[0] = 1;
    Require(Size(&modulus) == 1);
    words[0] = 0xFF;
    Require(Size(&modulus) == 1);
    words[0] = 0x100;
    Require(Size(&modulus) == 2);
    words[0] = 0x8000000000000000ull;
    Require(Size(&modulus) == 8);
    modulus.top = 2;
    words[1] = 1;
    Require(Size(&modulus) == 9);
    modulus.top = 32;
    words[31] = 0xC000000000000000ull;
    Require(Size(&modulus) == 256);
    modulus.neg = 1;
    Require(Size(&modulus) == 256);
    modulus.neg = 0;
    modulus.top = 16;
    words[15] = 0x00FFFFFFFFFFFFFFull;
    Require(Size(&modulus) == 127);
    words[15] = 0;
    Require(Size(&modulus) == 120);
    modulus.top = -1;
    RequireThrows([&] { Size(&modulus); });
    modulus.top = 1;
    modulus.d = nullptr;
    RequireThrows([&] { Size(&modulus); });
    RequireThrows([] { Size(nullptr); });
    RequireThrows([] { RSA_size_nid_postfix(nullptr); });
}

int main() {
    TestFlags();
    TestSize();
    std::puts("libSceLibreSsl RSA tests passed");
}
