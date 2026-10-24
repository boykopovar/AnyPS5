#include "prx/libc/include/general/VabiMacros.hpp"
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

struct TestOcspResponse {
    TestAsn1String* responseStatus;
    void* responseBytes;
};

extern "C" {
int APS5_VABI OCSP_response_status_nid_postfix(const TestOcspResponse*);
}

static void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "LibreSSL OCSP response check failed at line %u\n", location.line());
    std::abort();
}

template<typename TAction> static void RequireThrows(TAction action, std::source_location location = std::source_location::current()) {
    try { action(); } catch (const std::exception&) { return; }
    Require(false, location);
}

constexpr int Enumerated = 10;
constexpr int NegativeEnumerated = 0x10A;
constexpr int Integer = 2;

static int Status(TestAsn1String* value) {
    TestOcspResponse response {value, nullptr};
    return OCSP_response_status_nid_postfix(&response);
}

static void TestResponseStatus() {
    unsigned char zero[] {0};
    unsigned char three[] {3};
    unsigned char six[] {0, 6};
    unsigned char wide[] {0x01, 0x02, 0x03, 0x04, 0x05};
    unsigned char full[] {0x80, 0, 0, 0, 0, 0, 0, 0};
    unsigned char nine[] {0, 0, 0, 0, 0, 0, 0, 0, 1};
    TestAsn1String value {1, Enumerated, zero, 0};
    Require(Status(&value) == 0);
    value = {1, Enumerated, three, 0};
    Require(Status(&value) == 3);
    value = {2, Enumerated, six, 0};
    Require(Status(&value) == 6);
    value = {1, NegativeEnumerated, three, 0};
    Require(Status(&value) == -3);
    value = {5, Enumerated, wide, 0};
    Require(Status(&value) == 0x02030405);
    value = {8, Enumerated, full, 0};
    Require(Status(&value) == 0);
    value = {9, Enumerated, nine, 0};
    Require(Status(&value) == -1);
    value = {0, Enumerated, three, 0};
    Require(Status(&value) == 0);
    value = {1, Enumerated, nullptr, 0};
    Require(Status(&value) == 0);
    value = {1, Integer, three, 0};
    Require(Status(&value) == -1);
    value = {9, Integer, nullptr, 0};
    Require(Status(&value) == -1);
    Require(Status(nullptr) == 0);
    RequireThrows([] { OCSP_response_status_nid_postfix(nullptr); });
}

int main() {
    TestResponseStatus();
    std::puts("libSceLibreSsl OCSP response tests passed");
}
