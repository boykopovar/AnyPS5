#include "prx/libSceLibreSsl/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <source_location>
#include <stdexcept>

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

static int Status(Asn1String* value) {
    LibreSslOcspResponse response {value, nullptr};
    return OCSP_response_status_nid_postfix(&response);
}

static void TestResponseStatus() {
    unsigned char zero[] {0};
    unsigned char three[] {3};
    unsigned char six[] {0, 6};
    unsigned char wide[] {0x01, 0x02, 0x03, 0x04, 0x05};
    unsigned char full[] {0x80, 0, 0, 0, 0, 0, 0, 0};
    unsigned char nine[] {0, 0, 0, 0, 0, 0, 0, 0, 1};
    Asn1String value {1, Enumerated, zero, 0};
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

static void TestSingleStatus() {
    Asn1String revoked {}, thisUpdate {}, nextUpdate {}, other {};
    unsigned char keyCompromise[] {1};
    Asn1String reasonValue {1, Enumerated, keyCompromise, 0};
    LibreSslOcspRevokedInfo revokedInfo {&revoked, &reasonValue};
    LibreSslOcspCertStatus status {0, nullptr};
    LibreSslOcspSingleResponse single {nullptr, &status, &thisUpdate, &nextUpdate, nullptr};
    int reason = 77;
    Asn1String* revocationTime = &other;
    Asn1String* thisResult = nullptr;
    Asn1String* nextResult = nullptr;

    Require(OCSP_single_get0_status_nid_postfix(nullptr, &reason, &revocationTime, &thisResult, &nextResult) == -1);
    Require(reason == 77 && revocationTime == &other && thisResult == nullptr && nextResult == nullptr);

    Require(OCSP_single_get0_status_nid_postfix(&single, &reason, &revocationTime, &thisResult, &nextResult) == 0);
    Require(reason == 77 && revocationTime == &other && thisResult == &thisUpdate && nextResult == &nextUpdate);

    status = {2, nullptr};
    single.nextUpdate = nullptr;
    nextResult = &other;
    Require(OCSP_single_get0_status_nid_postfix(&single, &reason, &revocationTime, &thisResult, &nextResult) == 2);
    Require(reason == 77 && revocationTime == &other && nextResult == nullptr);

    status = {1, &revokedInfo};
    single.nextUpdate = &nextUpdate;
    Require(OCSP_single_get0_status_nid_postfix(&single, &reason, &revocationTime, &thisResult, &nextResult) == 1);
    Require(reason == 1 && revocationTime == &revoked && thisResult == &thisUpdate && nextResult == &nextUpdate);

    revokedInfo.revocationReason = nullptr;
    reason = 77;
    Require(OCSP_single_get0_status_nid_postfix(&single, &reason, nullptr, nullptr, nullptr) == 1);
    Require(reason == -1);

    Asn1String wrongType {1, Integer, keyCompromise, 0};
    revokedInfo.revocationReason = &wrongType;
    Require(OCSP_single_get0_status_nid_postfix(&single, &reason, nullptr, nullptr, nullptr) == 1);
    Require(reason == -1);

    Require(OCSP_single_get0_status_nid_postfix(&single, nullptr, nullptr, nullptr, nullptr) == 1);

    status = {1, nullptr};
    Require(OCSP_single_get0_status_nid_postfix(&single, nullptr, nullptr, &thisResult, nullptr) == 1);
    RequireThrows([&] { OCSP_single_get0_status_nid_postfix(&single, &reason, nullptr, nullptr, nullptr); });
    RequireThrows([&] { OCSP_single_get0_status_nid_postfix(&single, nullptr, &revocationTime, nullptr, nullptr); });

    status = {5, nullptr};
    Require(OCSP_single_get0_status_nid_postfix(&single, &reason, &revocationTime, nullptr, nullptr) == 5);

    single.certStatus = nullptr;
    RequireThrows([&] { OCSP_single_get0_status_nid_postfix(&single, nullptr, nullptr, nullptr, nullptr); });
}

int main() {
    TestResponseStatus();
    TestSingleStatus();
    std::puts("libSceLibreSsl OCSP response tests passed");
}
