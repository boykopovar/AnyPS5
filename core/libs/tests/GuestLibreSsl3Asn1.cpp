#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <source_location>
#include <stdexcept>

struct TestAsn1String {
    int length;
    int type;
    unsigned char* data;
    std::int64_t flags;
};

static_assert(sizeof(TestAsn1String) == 24 && offsetof(TestAsn1String, data) == 8 && offsetof(TestAsn1String, flags) == 16);

extern "C" {
const unsigned char* APS5_VABI ASN1_STRING_get0_data_nid_postfix(const TestAsn1String*);
}

static void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "LibreSSL 3 ASN.1 check failed at line %u\n", location.line());
    std::abort();
}

template<typename TAction> static void RequireThrows(TAction action, std::source_location location = std::source_location::current()) {
    try { action(); } catch (const std::exception&) { return; }
    Require(false, location);
}

static void TestGet0Data() {
    unsigned char bytes[] {1, 2, 3};
    TestAsn1String string {3, 2, bytes, 0};
    Require(ASN1_STRING_get0_data_nid_postfix(&string) == bytes);
    string = {0, 4, nullptr, 0};
    Require(ASN1_STRING_get0_data_nid_postfix(&string) == nullptr);
    string = {-1, 0x102, bytes, 0x010};
    Require(ASN1_STRING_get0_data_nid_postfix(&string) == bytes);
    RequireThrows([] { ASN1_STRING_get0_data_nid_postfix(nullptr); });
}

int main() {
    TestGet0Data();
    std::puts("libSceLibreSsl3 ASN.1 tests passed");
}
