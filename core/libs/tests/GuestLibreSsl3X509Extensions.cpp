#include "prx/libc/include/general/VabiMacros.hpp"
#include <climits>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <source_location>
#include <stdexcept>

struct TestStack {
    int num;
    char** data;
    int sorted;
    int numAlloc;
    void* comp;
};

struct TestCertInfo {
    void* before[9];
    TestStack* extensions;
    void* rest[3];
};

struct TestX509 {
    TestCertInfo* certInfo;
    void* rest[26];
};

static_assert(sizeof(TestStack) == 32 && offsetof(TestStack, data) == 8);
static_assert(sizeof(TestCertInfo) == 104 && offsetof(TestCertInfo, extensions) == 72);
static_assert(sizeof(TestX509) == 216);

extern "C" {
const TestStack* APS5_VABI X509_get0_extensions_nid_postfix(const TestX509*);
}

static void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "LibreSSL X509 extension check failed at line %u\n", location.line());
    std::abort();
}

template<typename TAction> static void RequireThrows(TAction action, std::source_location location = std::source_location::current()) {
    try { action(); } catch (const std::exception&) { return; }
    Require(false, location);
}

static void TestGet0Extensions() {
    TestStack extensions {};
    TestCertInfo info {};
    TestX509 certificate {};
    RequireThrows([] { X509_get0_extensions_nid_postfix(nullptr); });
    RequireThrows([&] { X509_get0_extensions_nid_postfix(&certificate); });
    certificate.certInfo = &info;
    Require(X509_get0_extensions_nid_postfix(&certificate) == nullptr);
    info.before[8] = &info;
    info.extensions = &extensions;
    Require(X509_get0_extensions_nid_postfix(&certificate) == &extensions);
}

int main() {
    TestGet0Extensions();
    return 0;
}
