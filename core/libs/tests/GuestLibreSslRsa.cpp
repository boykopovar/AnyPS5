#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <source_location>
#include <stdexcept>

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

extern "C" {
int APS5_VABI RSA_flags_nid_postfix(const TestRsa*);
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
    Require(RSA_flags_nid_postfix(&rsa) == 0);
    method.flags = 0x0400;
    Require(RSA_flags_nid_postfix(&rsa) == 0x0400);
    method.flags = -1;
    Require(RSA_flags_nid_postfix(&rsa) == -1);
    rsa.meth = nullptr;
    RequireThrows([&] { RSA_flags_nid_postfix(&rsa); });
}

int main() {
    TestFlags();
    std::puts("libSceLibreSsl RSA tests passed");
}
