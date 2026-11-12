#include "prx/libSceLibreSsl3/include/OcspFindStatus.hpp"

#include <cstdio>
#include <cstdlib>
#include <source_location>
#include <stdexcept>

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "OCSP find status check failed at line %u\n", location.line());
    std::abort();
}

struct Fixture {
    unsigned char oidBytes[2] {0x2b, 0x0e};
    unsigned char differentBytes[2] {0x2b, 0x0f};
    unsigned char nameBytes[2] {0x20, 0x21};
    unsigned char keyBytes[2] {0x30, 0x31};
    unsigned char serialBytes[2] {0x01, 0x80};
    LibreSslAsn1Object oid {nullptr, nullptr, 0, 2, oidBytes, 0};
    LibreSslAsn1Object otherOid {nullptr, nullptr, 0, 2, differentBytes, 0};
    LibreSsl3X509Algor algorithm {&oid, nullptr};
    LibreSsl3Asn1Type parameter {5, nullptr};
    LibreSsl3X509Algor queryAlgorithm {&oid, &parameter};
    Asn1String name {2, 4, nameBytes, 0};
    Asn1String key {2, 4, keyBytes, 0};
    Asn1String serial {2, 2, serialBytes, 0};
    Asn1String querySerial = serial;
    LibreSsl3OcspFindCertId stored {&algorithm, &name, &key, &serial};
    LibreSsl3OcspFindCertId query {&queryAlgorithm, &name, &key, &querySerial};
    Asn1String revocation {}, current {}, next {}, sentinel {};
    unsigned char reasonByte = 6;
    Asn1String reasonValue {1, 10, &reasonByte, 0};
    LibreSslOcspRevokedInfo revoked {&revocation, &reasonValue};
    LibreSslOcspCertStatus certStatus {0, nullptr};
    LibreSslOcspCertStatus secondStatus {2, nullptr};
    LibreSslOcspSingleResponse single {&stored, &certStatus, &current, &next, nullptr};
    LibreSslOcspSingleResponse duplicate {&stored, &secondStatus, nullptr, nullptr, nullptr};
    char* entries[2] {reinterpret_cast<char*>(&single), reinterpret_cast<char*>(&duplicate)};
    LibreSslStack stack {1, entries, 0, 2, nullptr};
    LibreSslOcspResponseData data {nullptr, nullptr, nullptr, &stack, nullptr};
    LibreSslOcspBasicResponse response {&data, nullptr, nullptr, nullptr};

    int Find(int* status, int* reason, Asn1String** rev, Asn1String** now, Asn1String** after) {
        return OCSP_resp_find_status_nid_postfix(&response, reinterpret_cast<OCSP_CERTID*>(&query), status, reason, rev, now, after);
    }

    void Miss(LibreSslOcspBasicResponse* bs) {
        int status = 88, reason = 99;
        Asn1String* rev = &sentinel;
        Asn1String* now = &sentinel;
        Asn1String* after = &sentinel;
        Require(OCSP_resp_find_status_nid_postfix(bs, reinterpret_cast<OCSP_CERTID*>(&query), &status, &reason, &rev, &now, &after) == 0);
        Require(status == 88 && reason == 99);
        Require(rev == &sentinel && now == &sentinel && after == &sentinel);
    }

    void Match(int expected, int expectedReason) {
        for (unsigned mask = 0; mask < 32; ++mask) {
            int status = 88, reason = 99;
            Asn1String* rev = &sentinel;
            Asn1String* now = &sentinel;
            Asn1String* after = &sentinel;
            Require(Find(mask & 1 ? &status : nullptr, mask & 2 ? &reason : nullptr,
                mask & 4 ? &rev : nullptr, mask & 8 ? &now : nullptr, mask & 16 ? &after : nullptr) == 1);
            Require(status == (mask & 1 ? expected : 88));
            Require(reason == ((mask & 2) && expected == 1 ? expectedReason : 99));
            Require(rev == ((mask & 4) && expected == 1 ? &revocation : &sentinel));
            Require(now == (mask & 8 ? &current : &sentinel));
            Require(after == (mask & 16 ? single.nextUpdate : &sentinel));
        }
    }
};

void TestFind() {
    Fixture f;
    f.Miss(nullptr);
    f.data.responses = nullptr;
    f.Miss(&f.response);
    f.data.responses = &f.stack;
    f.stack.num = 0;
    f.Miss(&f.response);
    f.stack.num = 1;
    f.Match(0, 99);
    f.queryAlgorithm.algorithm = &f.otherOid;
    f.Miss(&f.response);
    f.queryAlgorithm.algorithm = &f.oid;
    Asn1String different {2, 4, f.differentBytes, 0};
    f.query.issuerNameHash = &different;
    f.Miss(&f.response);
    f.query.issuerNameHash = &f.name;
    f.query.issuerKeyHash = &different;
    f.Miss(&f.response);
    f.query.issuerKeyHash = &f.key;
    f.querySerial.data = f.differentBytes;
    f.Miss(&f.response);
    f.querySerial.data = f.serialBytes;
    f.querySerial.length = 1;
    f.Miss(&f.response);
    f.querySerial.length = 2;
    f.querySerial.type = 0x102;
    f.Miss(&f.response);
    f.serial.type = 0x102;
    f.Match(0, 99);
    f.querySerial.data = f.differentBytes;
    f.Miss(&f.response);
    f.querySerial.data = f.serialBytes;
    f.querySerial.type = 2;
    f.Miss(&f.response);
    f.serial.type = 2;
    f.stack.num = 2;
    f.Match(0, 99);
    f.certStatus.type = 1;
    f.certStatus.value = &f.revoked;
    f.Match(1, 6);
    f.revoked.revocationReason = nullptr;
    f.Match(1, -1);
    f.certStatus.type = 2;
    f.Match(2, 99);
    f.single.nextUpdate = nullptr;
    f.Match(2, 99);
    f.response.tbsResponseData = nullptr;
    bool threw = false;
    try { f.Find(nullptr, nullptr, nullptr, nullptr, nullptr); }
    catch (const std::invalid_argument&) { threw = true; }
    Require(threw);
}

}

int main() {
    TestFind();
}
