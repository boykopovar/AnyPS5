#include "prx/libSceSsl/include/Ssl.hpp"
#include "prx/libSceSsl/SslTypes.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "SslCertificateFixture.hpp"
#include "Testing.hpp"

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <exception>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace {

constexpr int NotFound = static_cast<int>(0x8095F004);
constexpr int InvalidId = static_cast<int>(0x8095F006);

SceSsl::Data Data(const char* text) {
    return {reinterpret_cast<std::uint8_t*>(const_cast<char*>(text)), std::strlen(text)};
}

SceSsl::MemoryPoolStats Stats(int id) {
    SceSsl::MemoryPoolStats stats{};
    Require(sceSslGetMemoryPoolStats(id, &stats) == 0, "stats failed");
    Require(stats.currentInuseSize <= stats.maxInuseSize && stats.maxInuseSize <= stats.poolSize && !stats.reserved,
        "pool accounting bounds failed");
    return stats;
}

void CheckRoot(const SceSsl::CaCerts& snapshot) {
    Require(snapshot.num == 1 && snapshot.certs[0].size == RootDer.size(), "CA count or DER size differs");
    Require(std::memcmp(snapshot.certs[0].ptr, RootDer.data(), RootDer.size()) == 0, "CA bytes changed");
}

void Lifecycle() {
    const int id = sceSslInit_nid_postfix(1024 * 1024);
    std::string original = RootPem;
    auto data = Data(original.c_str());
    auto* pointer = &data;
    Require(sceSslLoadCert(id, 1, &pointer, nullptr, nullptr) == 0, "initial CA load failed");
    original.assign(original.size(), '?');
    Require(Stats(id).currentInuseSize > RootDer.size(), "loaded certificate is not retained/accounted");
    SceSsl::CaCerts bytes{};
    Require(sceSslGetCaCerts(id, &bytes) == 0, "encoded CA query failed");
    CheckRoot(bytes);
    SceSsl::CaList handles{};
    Require(sceSslGetCaList(id, &handles) == 0 && handles.num == 1 && handles.certs[0], "CA handle absent");
    const int other = sceSslInit_nid_postfix(1024 * 1024);
    SceSsl::CaList empty{};
    Require(sceSslGetCaList(other, &empty) == NotFound, "CA escaped its context");
    auto foreign = handles;
    ExpectErr([&] { sceSslFreeCaList(other, &foreign); });
    Require(sceSslUnloadCert(id) == 0, "unload failed");
    Require(sceSslGetCaList(id, &empty) == NotFound && !empty.certs && !empty.num, "unload retained active CA");
    CheckRoot(bytes);
    Require(sceSslFreeCaList(id, &handles) == 0 && !handles.certs && !handles.num, "handle snapshot release failed");
    Require(sceSslFreeCaCerts(id, &bytes) == 0 && !bytes.certs && !bytes.pool && !bytes.num, "encoded snapshot release failed");
    Require(Stats(id).currentInuseSize == 0, "certificate allocation leaked after release");
    ExpectErr([&] { sceSslFreeCaList(id, &foreign); });
    Require(sceSslTerm_nid_postfix(id) == 0 && sceSslTerm_nid_postfix(other) == 0, "term failed");
}

void ConnectionLifetime() {
    const int id = sceSslInit_nid_postfix(1024 * 1024);
    const int connection = sceSslCreateConnection(id, -1, "example.invalid");
    const int other = sceSslInit_nid_postfix(1024 * 1024);
    Require(connection > 0 && connection != id && connection != other, "SSL handles overlap");
    auto data = Data(RootPem);
    auto* pointer = &data;
    Require(sceSslLoadCert(id, 1, &pointer, nullptr, nullptr) == 0, "CA load with a connection failed");
    Require(sceSslSetVerifyCallback(connection, nullptr, nullptr) == 0, "certificate load invalidated the connection");
    SceSsl::CaCerts bytes{};
    Require(sceSslGetCaCerts(id, &bytes) == 0, "CA query with a connection failed");
    CheckRoot(bytes);
    SceSsl::CaCerts empty{};
    Require(sceSslGetCaCerts(other, &empty) == NotFound, "CA escaped into another context");
    SceSsl::MemoryPoolStats stats{};
    Require(sceSslGetMemoryPoolStats(connection, &stats) == static_cast<int>(0x8095F006), "connection accepted as context");
    const int retained = sceSslCreateConnection(id, -1, "example.invalid");
    Require(retained > 0 && retained != connection && retained != other, "SSL connection handle was reused");
    Require(sceSslDeleteConnection(connection) == 0, "connection deletion failed");
    Require(sceSslUnloadCert(id) == 0, "credential unload failed with a retained connection");
    CheckRoot(bytes);
    Require(sceSslSetVerifyCallback(retained, nullptr, nullptr) == 0, "credential unload invalidated the connection");
    Require(sceSslFreeCaCerts(id, &bytes) == 0 && Stats(id).currentInuseSize == 0, "CA snapshot leaked with a retained connection");
    Require(sceSslTerm_nid_postfix(id) == 0, "context termination failed with a retained connection");
    Require(sceSslSetVerifyCallback(retained, nullptr, nullptr) == InvalidId, "terminated context's connection did not report an invalid id");
    Require(sceSslDeleteConnection(retained) == InvalidId, "terminated context's connection was still deletable");
    Require(sceSslGetCaCerts(other, &empty) == NotFound, "unrelated context was invalidated");
    Require(sceSslTerm_nid_postfix(other) == 0, "unrelated context termination failed");
}

void AtomicFailures() {
    const int id = sceSslInit_nid_postfix(1024 * 1024);
    SceSsl::Data der{const_cast<std::uint8_t*>(RootDer.data()), RootDer.size()};
    auto* pointer = &der;
    Require(sceSslLoadCert(id, 1, &pointer, nullptr, nullptr) == 0, "DER CA load failed");
    const auto before = Stats(id).currentInuseSize;
    auto bad = Data("not a certificate");
    SceSsl::Data* mixed[]{&der, &bad};
    ExpectErr([&] { sceSslLoadCert(id, 2, mixed, nullptr, nullptr); });
    Require(Stats(id).currentInuseSize == before, "failed chain changed live allocation");
    SceSsl::CaCerts bytes{};
    Require(sceSslGetCaCerts(id, &bytes) == 0, "CA vanished after failure");
    CheckRoot(bytes);
    Require(sceSslFreeCaCerts(id, &bytes) == 0, "snapshot free failed");
    Require(sceSslLoadCert(id, -1, nullptr, nullptr, nullptr) == 0, "negative CA count was not ignored");
    Require(sceSslLoadCert(id, 1, nullptr, nullptr, nullptr) == 0, "null CA list was not ignored");
    SceSsl::Data* absent = nullptr;
    ExpectErr([&] { sceSslLoadCert(id, 1, &absent, nullptr, nullptr); });

    Require(Stats(id).currentInuseSize == before, "argument failure changed retained CA");
    Require(sceSslTerm_nid_postfix(id) == 0, "term failed");
    Require(sceSslLoadCert(id, 1, &pointer, nullptr, nullptr) == InvalidId, "invalid load context error mismatch");
    const int tiny = sceSslInit_nid_postfix(32);
    ExpectErr([&] { sceSslLoadCert(tiny, 1, &pointer, nullptr, nullptr); });
    Require(Stats(tiny).currentInuseSize == 0, "failed small-pool load leaked");
    Require(sceSslTerm_nid_postfix(tiny) == 0, "small-pool term failed");
}

void ClientIdentity() {
    const int id = sceSslInit_nid_postfix(1024 * 1024);
    auto client = Data(ClientPem);
    auto key = Data(ClientKey);
    auto wrong = Data(RootKey);
    Require(sceSslLoadCert(id, 0, nullptr, &client, &key) == 0, "client identity load failed");
    const auto before = Stats(id).currentInuseSize;
    Require(before > 0, "identity not retained");
    Require(sceSslLoadCert(id, 0, nullptr, &client, &wrong) == 0, "key pair was checked during loading");
    Require(sceSslLoadCert(id, 0, nullptr, &client, nullptr) == 0, "certificate-only load failed");
    const auto certificateBytes = Stats(id).currentInuseSize;
    Require(certificateBytes > 0 && certificateBytes < before, "certificate replacement retained the old key");
    Require(sceSslLoadCert(id, 0, nullptr, nullptr, &key) == 0, "independent key load failed");
    Require(Stats(id).currentInuseSize == before, "separate identity load retained different data");
    auto arbitraryKey = Data("invalid key encoding");
    Require(sceSslLoadCert(id, 0, nullptr, nullptr, &arbitraryKey) == 0, "key was parsed during loading");
    Require(Stats(id).currentInuseSize == certificateBytes + arbitraryKey.size, "raw key bytes were not retained");
    SceSsl::Data missing{};
    Require(sceSslLoadCert(id, 0, nullptr, &missing, nullptr) == static_cast<int>(0x80951771), "null certificate payload error mismatch");
    Require(sceSslLoadCert(id, 0, nullptr, nullptr, &missing) == static_cast<int>(0x80951771), "null key payload error mismatch");
    Require(Stats(id).currentInuseSize == certificateBytes + arbitraryKey.size, "null payload cleared the identity");
    Require(sceSslLoadCert(-1, -1, nullptr, &missing, &missing) == InvalidId, "load context validation precedence mismatch");
    Require(sceSslUnloadCert(id) == 0 && Stats(id).currentInuseSize == 0, "identity release failed");
    Require(sceSslTerm_nid_postfix(id) == 0, "identity term failed");
}

void CertificateReaders() {
    const int id = sceSslInit_nid_postfix(1024 * 1024);
    auto data = Data(RootPem);
    auto* pointer = &data;
    Require(sceSslLoadCert(id, 1, &pointer, nullptr, nullptr) == 0, "CA load failed");
    SceSsl::CaList handles{};
    Require(sceSslGetCaList(id, &handles) == 0 && handles.num == 1 && handles.certs[0], "CA handle absent");
    void* cert = handles.certs[0];

    std::size_t querySize = 0;
    Require(sceSslGetSerialNumber(id, cert, nullptr, &querySize) == 0 && querySize == 3, "query serial size failed");
    querySize = 0;
    unsigned char shortBuf[2]{};
    Require(sceSslGetSerialNumber(id, cert, shortBuf, &querySize) == 0 && querySize == 0, "zero-len serial copy changed length");
    querySize = sizeof(shortBuf);
    Require(sceSslGetSerialNumber(id, cert, shortBuf, &querySize) == 0 && querySize == 2 && shortBuf[0] == 0 && shortBuf[1] == 0x80,
        "short serial copy mismatch");
    unsigned char serialData[8]{};
    std::size_t serialLen = sizeof(serialData);
    Require(sceSslGetSerialNumber(id, cert, serialData, &serialLen) == 0, "get serial failed");
    Require(serialLen == sizeof(serialData) && serialData[3] == 0, "a large enough serial buffer changed the caller's length");
    Require(serialData[0] == 0x00 && serialData[1] == 0x80 && serialData[2] == 0xa1, "serial bytes mismatch");
    ExpectErr([&] { sceSslGetSerialNumber(id, cert, serialData, nullptr); });
    Require(sceSslGetSerialNumber(id, cert, nullptr, nullptr) == 0, "empty serial query failed");
    Require(sceSslGetSerialNumber(id, nullptr, serialData, &serialLen) == static_cast<int>(0x8095f007), "null certificate error mismatch");
    void* fake = &serialLen;
    ExpectErr([&] { sceSslGetSerialNumber(id, fake, serialData, &serialLen); });
    Require(sceSslGetSerialNumber(9999, cert, serialData, &serialLen) == InvalidId, "unknown context error mismatch");

    void* subject = sceSslGetSubjectName(id, cert);
    Require(subject != nullptr, "get subject failed");
    Require(sceSslGetSubjectName(id, cert) == subject, "subject handle identity changed");
    void* issuer = sceSslGetIssuerName(id, cert);
    Require(issuer != nullptr, "get issuer failed");
    Require(sceSslGetSubjectName(id, nullptr) == nullptr, "invalid name query returned a handle");
    Require(sceSslGetIssuerName(id, nullptr) == nullptr, "invalid name query returned a handle");
    ExpectErr([&] { sceSslGetSubjectName(id, fake); });
    ExpectErr([&] { sceSslGetIssuerName(id, fake); });
    Require(sceSslGetSubjectName(9999, cert) == nullptr, "invalid name query returned a handle");
    Require(sceSslGetIssuerName(9999, cert) == nullptr, "invalid name query returned a handle");

    Require(sceSslGetNameEntryCount(id, subject) == 3, "subject entry count mismatch");
    Require(sceSslGetNameEntryCount(id, issuer) == 3, "issuer entry count mismatch");
    Require(sceSslGetNameEntryCount(id, nullptr) == static_cast<int>(0x8095f007), "null cert name count accepted");
    ExpectErr([&] { sceSslGetNameEntryCount(id, fake); });
    Require(sceSslGetNameEntryCount(9999, subject) == InvalidId, "unknown name context error mismatch");

    char oid[32]{};
    char val[64]{};
    std::size_t vlen = 0;
    Require(sceSslGetNameEntryInfo(id, subject, 0, oid, sizeof(oid), val, sizeof(val), &vlen) == 0, "entry 0 info failed");
    Require(std::strcmp(oid, "countryName") == 0 && std::strcmp(val, "GB") == 0 && vlen == 2, "entry 0 mismatch");
    Require(sceSslGetNameEntryInfo(id, subject, 1, oid, sizeof(oid), val, sizeof(val), &vlen) == 0, "entry 1 info failed");
    Require(std::strcmp(oid, "organizationName") == 0 && std::strcmp(val, "AnyPS5 Tests") == 0 && vlen == 12, "entry 1 mismatch");
    Require(sceSslGetNameEntryInfo(id, subject, 2, oid, sizeof(oid), val, sizeof(val), &vlen) == 0, "entry 2 info failed");
    Require(std::strcmp(oid, "commonName") == 0 && std::strcmp(val, "AnyPS5 fixture root") == 0 && vlen == 19, "entry 2 mismatch");

    char smallVal[4]{};
    std::size_t fullLen = 0;
    Require(sceSslGetNameEntryInfo(id, subject, 1, oid, 0, smallVal, sizeof(smallVal), &fullLen) == 0, "truncated query failed");
    Require(std::memcmp(smallVal, "AnyP", 4) == 0 && fullLen == 4, "buffer truncation mismatch");
    char shortOid[5] = {'x', 'x', 'x', 'x', 'x'};
    char shortValue[5] = {'y', 'y', 'y', 'y', 'y'};
    Require(sceSslGetNameEntryInfo(id, subject, 1, shortOid, 4, shortValue, 4, &fullLen) == 0, "bounded entry query failed");
    Require(std::memcmp(shortOid, "orga", 4) == 0 && shortOid[4] == 'x', "oid copy or guard mismatch");
    Require(std::memcmp(shortValue, "AnyP", 4) == 0 && shortValue[4] == 'y' && fullLen == 4, "value copy or guard mismatch");
    Require(sceSslGetNameEntryInfo(id, subject, 1, shortOid, 0, shortValue, 0, &fullLen) == 0 && fullLen == 0, "zero capacity query failed");
    Require(sceSslGetNameEntryInfo(id, subject, 1, nullptr, 0, shortValue, 0, &fullLen) == static_cast<int>(0x8095f007), "null oid accepted");
    Require(sceSslGetNameEntryInfo(id, subject, 1, shortOid, 0, nullptr, 0, &fullLen) == static_cast<int>(0x8095f007), "null value accepted");
    Require(sceSslGetNameEntryInfo(id, subject, 1, shortOid, 0, shortValue, 0, nullptr) == static_cast<int>(0x8095f007), "null value length accepted");

    Require(sceSslGetNameEntryInfo(id, subject, -1, oid, sizeof(oid), val, sizeof(val), &vlen) == static_cast<int>(0x8095f007), "negative index accepted");
    Require(sceSslGetNameEntryInfo(id, subject, 3, oid, sizeof(oid), val, sizeof(val), &vlen) == static_cast<int>(0x8095f007), "out of bounds index accepted");
    Require(sceSslGetNameEntryInfo(id, nullptr, 0, oid, sizeof(oid), val, sizeof(val), &vlen) == static_cast<int>(0x8095f007), "null certName accepted");
    ExpectErr([&] { sceSslGetNameEntryInfo(id, fake, 0, oid, sizeof(oid), val, sizeof(val), &vlen); });
    Require(sceSslGetNameEntryInfo(9999, subject, 0, oid, sizeof(oid), val, sizeof(val), &vlen) == InvalidId, "unknown entry context error mismatch");

    Require(sceSslFreeSslCertName(id, subject) == 0, "free subject name failed");
    Require(sceSslFreeSslCertName(id, issuer) == 0, "free issuer name failed");
    Require(sceSslGetNameEntryCount(id, subject) == 3, "name release invalidated a live certificate name");
    Require(sceSslFreeSslCertName(id, nullptr) == static_cast<int>(0x8095f007), "null cert name free accepted");
    Require(sceSslFreeSslCertName(id, subject) == 0, "non-null name release failed");
    Require(sceSslFreeSslCertName(id, fake) == 0, "non-null name release failed");
    Require(sceSslFreeSslCertName(9999, subject) == InvalidId, "invalid name release context error mismatch");

    SceSsl::RtcTick begin{};
    Require(sceSslGetNotBefore(0, nullptr, nullptr) == InvalidId, "date validation precedence mismatch");
    Require(sceSslGetNotAfter(0, nullptr, nullptr) == InvalidId, "date validation precedence mismatch");
    Require(sceSslGetFingerprint(0, nullptr, -1, nullptr) == InvalidId, "fingerprint validation precedence mismatch");
    Require(sceSslGetNotBefore(id, cert, &begin) == 0, "get not before failed");
    Require(begin.tick == 63927094442000000ULL, "not before tick mismatch");
    Require(sceSslGetNotBefore(id, cert, nullptr) == static_cast<int>(0x8095f007), "null begin accepted");
    Require(sceSslGetNotBefore(id, nullptr, &begin) == static_cast<int>(0x8095f007), "null certificate date error mismatch");
    ExpectErr([&] { sceSslGetNotBefore(id, fake, &begin); });
    Require(sceSslGetNotBefore(9999, cert, &begin) == InvalidId, "invalid date context error mismatch");
    errno = 71;
    Require(sceSslGetNotBefore(id, cert, reinterpret_cast<SceSsl::RtcTick*>(0x10)) == static_cast<int>(0x8095f007), "unreadable begin accepted");
    Require(errno == 71, "errno modified by unreadable begin check");

    SceSsl::RtcTick limit{};
    Require(sceSslGetNotAfter(id, cert, &limit) == 0, "get not after failed");
    Require(limit.tick == 64242454442000000ULL, "not after tick mismatch");
    Require(sceSslGetNotAfter(id, cert, nullptr) == static_cast<int>(0x8095f007), "null limit accepted");
    Require(sceSslGetNotAfter(id, nullptr, &limit) == static_cast<int>(0x8095f007), "null certificate date error mismatch");
    ExpectErr([&] { sceSslGetNotAfter(id, fake, &limit); });
    Require(sceSslGetNotAfter(9999, cert, &limit) == InvalidId, "invalid date context error mismatch");
    errno = 71;
    Require(sceSslGetNotAfter(id, cert, reinterpret_cast<SceSsl::RtcTick*>(0x10)) == static_cast<int>(0x8095f007), "unreadable limit accepted");
    Require(errno == 71, "errno modified by unreadable limit check");

    unsigned char md5[16]{};
    Require(sceSslGetFingerprint(id, cert, 0, md5) == 0, "get md5 fingerprint failed");
    constexpr unsigned char expectedMd5[16] = {0x71, 0x3c, 0xe5, 0x20, 0x88, 0x2b, 0x6b, 0xd0, 0x17, 0x85, 0x07, 0xde, 0xbd, 0xdc, 0xa9, 0x4f};
    Require(std::memcmp(md5, expectedMd5, sizeof(md5)) == 0, "md5 digest mismatch");

    unsigned char sha1[20]{};
    Require(sceSslGetFingerprint(id, cert, 1, sha1) == 0, "get sha1 fingerprint failed");
    constexpr unsigned char expectedSha1[20] = {0x47, 0xe0, 0x89, 0xab, 0xb7, 0x02, 0xee, 0xb6, 0x2f, 0x8b, 0x5b, 0x7a, 0x13, 0xfe, 0x82, 0x6d, 0x2b, 0xe8, 0xee, 0xa7};
    Require(std::memcmp(sha1, expectedSha1, sizeof(sha1)) == 0, "sha1 digest mismatch");

    unsigned char sha256[32]{};
    Require(sceSslGetFingerprint(id, cert, 3, sha256) == 0, "get sha256 fingerprint failed");
    constexpr unsigned char expectedSha256[32] = {0x3d, 0x7d, 0xac, 0xd3, 0x04, 0x90, 0x42, 0xe1, 0x57, 0x19, 0x87, 0x27, 0x54, 0x45, 0xae, 0xed, 0xc4, 0xea, 0xd6, 0xd4, 0x20, 0xa0, 0x7f, 0xc4, 0xf5, 0x26, 0xc1, 0xa0, 0x85, 0x87, 0x5a, 0x4c};
    Require(std::memcmp(sha256, expectedSha256, sizeof(sha256)) == 0, "sha256 digest mismatch");

    Require(sceSslGetFingerprint(id, cert, 3, nullptr) == static_cast<int>(0x8095f007), "null fingerprint buffer accepted");
    Require(sceSslGetFingerprint(id, cert, -1, sha256) == static_cast<int>(0x8095177a), "negative hash type accepted");
    Require(sceSslGetFingerprint(id, cert, 99, sha256) == static_cast<int>(0x8095177a), "invalid hash type accepted");
    Require(sceSslGetFingerprint(id, nullptr, 3, sha256) == static_cast<int>(0x8095f007), "null fingerprint certificate error mismatch");
    ExpectErr([&] { sceSslGetFingerprint(id, fake, 3, sha256); });
    Require(sceSslGetFingerprint(9999, cert, 3, sha256) == InvalidId, "invalid fingerprint context error mismatch");

    std::size_t pemLen = 0;
    Require(sceSslGetPem(id, cert, nullptr, &pemLen) == 0, "query pem size failed");
    Require(pemLen > 0, "pem length is 0");
    std::vector<char> pemBuf(pemLen);
    std::size_t writtenLen = pemBuf.size();
    Require(sceSslGetPem(id, cert, pemBuf.data(), &writtenLen) == 0, "write pem failed");
    Require(writtenLen == pemLen, "written pem length mismatch");
    Require(std::strstr(pemBuf.data(), "-----BEGIN CERTIFICATE-----\r\n") != nullptr, "pem header missing");
    Require(std::strstr(pemBuf.data(), "-----END CERTIFICATE-----\r\n") != nullptr, "pem footer missing");

    char smallPem[8]{};
    std::size_t smallPemLen = sizeof(smallPem);
    Require(sceSslGetPem(id, cert, smallPem, &smallPemLen) == static_cast<int>(0x8095f008) && smallPemLen == pemLen, "short PEM size error mismatch");
    Require(sceSslGetPem(id, cert, pemBuf.data(), nullptr) == static_cast<int>(0x8095f007), "null pem len accepted");
    Require(sceSslGetPem(id, nullptr, pemBuf.data(), &writtenLen) == static_cast<int>(0x8095f007), "null PEM certificate error mismatch");
    ExpectErr([&] { sceSslGetPem(id, fake, pemBuf.data(), &writtenLen); });

    Require(sceSslClose(1) == InvalidId, "closing an unknown connection did not report an invalid id");

    Require(sceSslFreeCaList(id, &handles) == 0, "free ca list failed");
    Require(sceSslTerm_nid_postfix(id) == 0, "term failed");
    Require(sceSslGetSerialNumber(id, cert, serialData, &serialLen) == InvalidId, "terminated context error mismatch");
    Require(sceSslGetSubjectName(id, cert) == nullptr, "invalid name query returned a handle");
    Require(sceSslGetIssuerName(id, cert) == nullptr, "invalid name query returned a handle");
    Require(sceSslGetNotBefore(id, cert, &begin) == InvalidId, "invalid date context error mismatch");
    Require(sceSslGetNotAfter(id, cert, &limit) == InvalidId, "invalid date context error mismatch");
    Require(sceSslGetFingerprint(id, cert, 3, sha256) == InvalidId, "invalid fingerprint context error mismatch");
    Require(sceSslGetPem(id, cert, pemBuf.data(), &writtenLen) == InvalidId, "terminated PEM context error mismatch");

    const int id2 = sceSslInit_nid_postfix(1024 * 1024);
    Require(sceSslLoadCert(id2, 1, &pointer, nullptr, nullptr) == 0, "CA load failed");
    SceSsl::CaList handles2{};
    Require(sceSslGetCaList(id2, &handles2) == 0, "get ca list failed");
    void* cert2 = handles2.certs[0];
    void* subject2 = sceSslGetSubjectName(id2, cert2);
    Require(subject2 != nullptr, "get subject 2 failed");
    Require(sceSslFreeCaList(id2, &handles2) == 0, "free ca list failed");
    Require(sceSslUnloadCert(id2) == 0, "unload failed");
    ExpectErr([&] { sceSslGetSerialNumber(id2, cert2, serialData, &serialLen); });
    ExpectErr([&] { sceSslGetSubjectName(id2, cert2); });
    ExpectErr([&] { sceSslGetIssuerName(id2, cert2); });
    ExpectErr([&] { sceSslGetNotBefore(id2, cert2, &begin); });
    ExpectErr([&] { sceSslGetNotAfter(id2, cert2, &limit); });
    ExpectErr([&] { sceSslGetFingerprint(id2, cert2, 3, sha256); });
    ExpectErr([&] { sceSslGetPem(id2, cert2, pemBuf.data(), &writtenLen); });
    Require(sceSslTerm_nid_postfix(id2) == 0, "term failed");
    Require(sceSslGetNameEntryCount(id2, subject2) == InvalidId, "terminated name context error mismatch");
    Require(sceSslFreeSslCertName(id2, subject2) == InvalidId, "invalid name release context error mismatch");
}

}

int main() {
    try {
        Lifecycle();
        AtomicFailures();
        ClientIdentity();
        ConnectionLifetime();
        CertificateReaders();
        std::vector<std::thread> workers;
        for (int i = 0; i < 3; ++i) workers.emplace_back([] { for (int n = 0; n < 4; ++n) Lifecycle(); });
        for (auto& worker : workers) worker.join();
        std::puts("SSL load lifecycle passed");
    } catch (const std::exception& error) {
        std::fprintf(stderr, "SSL load lifecycle failed: %s\n", error.what());
        return 1;
    }
}
