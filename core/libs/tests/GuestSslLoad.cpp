#include "prx/libSceSsl/include/Ssl.hpp"
#include "prx/libSceSsl/SslTypes.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "SslCertificateFixture.hpp"

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

void Check(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

template <typename TAction>
void Throws(TAction action) {
    bool caught = false;
    try { action(); } catch (const std::exception&) { caught = true; }
    Check(caught, "unsupported or malformed load was accepted");
}

SceSsl::Data Data(const char* text) {
    return {reinterpret_cast<std::uint8_t*>(const_cast<char*>(text)), std::strlen(text)};
}

SceSsl::MemoryPoolStats Stats(int id) {
    SceSsl::MemoryPoolStats stats{};
    Check(sceSslGetMemoryPoolStats(id, &stats) == 0, "stats failed");
    Check(stats.currentInuseSize <= stats.maxInuseSize && stats.maxInuseSize <= stats.poolSize && !stats.reserved,
        "pool accounting bounds failed");
    return stats;
}

void CheckRoot(const SceSsl::CaCerts& snapshot) {
    Check(snapshot.num == 1 && snapshot.certs[0].size == RootDer.size(), "CA count or DER size differs");
    Check(std::memcmp(snapshot.certs[0].ptr, RootDer.data(), RootDer.size()) == 0, "CA bytes changed");
}

void Lifecycle() {
    const int id = sceSslInit_nid_postfix(1024 * 1024);
    std::string original = RootPem;
    auto data = Data(original.c_str());
    auto* pointer = &data;
    Check(sceSslLoadCert(id, 1, &pointer, nullptr, nullptr) == 0, "initial CA load failed");
    original.assign(original.size(), '?');
    Check(Stats(id).currentInuseSize > RootDer.size(), "loaded certificate is not retained/accounted");
    SceSsl::CaCerts bytes{};
    Check(sceSslGetCaCerts(id, &bytes) == 0, "encoded CA query failed");
    CheckRoot(bytes);
    SceSsl::CaList handles{};
    Check(sceSslGetCaList(id, &handles) == 0 && handles.num == 1 && handles.certs[0], "CA handle absent");
    const int other = sceSslInit_nid_postfix(1024 * 1024);
    SceSsl::CaList empty{};
    Check(sceSslGetCaList(other, &empty) == NotFound, "CA escaped its context");
    auto foreign = handles;
    Throws([&] { sceSslFreeCaList(other, &foreign); });
    Check(sceSslUnloadCert(id) == 0, "unload failed");
    Check(sceSslGetCaList(id, &empty) == NotFound && !empty.certs && !empty.num, "unload retained active CA");
    CheckRoot(bytes);
    Check(sceSslFreeCaList(id, &handles) == 0 && !handles.certs && !handles.num, "handle snapshot release failed");
    Check(sceSslFreeCaCerts(id, &bytes) == 0 && !bytes.certs && !bytes.pool && !bytes.num, "encoded snapshot release failed");
    Check(Stats(id).currentInuseSize == 0, "certificate allocation leaked after release");
    Throws([&] { sceSslFreeCaList(id, &foreign); });
    Check(sceSslTerm_nid_postfix(id) == 0 && sceSslTerm_nid_postfix(other) == 0, "term failed");
}

void ConnectionLifetime() {
    const int id = sceSslInit_nid_postfix(1024 * 1024);
    const int connection = sceSslCreateConnection(id, -1, "example.invalid");
    const int other = sceSslInit_nid_postfix(1024 * 1024);
    Check(connection > 0 && connection != id && connection != other, "SSL handles overlap");
    auto data = Data(RootPem);
    auto* pointer = &data;
    Check(sceSslLoadCert(id, 1, &pointer, nullptr, nullptr) == 0, "CA load with a connection failed");
    Check(sceSslSetVerifyCallback(connection, nullptr, nullptr) == 0, "certificate load invalidated the connection");
    SceSsl::CaCerts bytes{};
    Check(sceSslGetCaCerts(id, &bytes) == 0, "CA query with a connection failed");
    CheckRoot(bytes);
    SceSsl::CaCerts empty{};
    Check(sceSslGetCaCerts(other, &empty) == NotFound, "CA escaped into another context");
    SceSsl::MemoryPoolStats stats{};
    Check(sceSslGetMemoryPoolStats(connection, &stats) == static_cast<int>(0x8095F006), "connection accepted as context");
    const int retained = sceSslCreateConnection(id, -1, "example.invalid");
    Check(retained > 0 && retained != connection && retained != other, "SSL connection handle was reused");
    Check(sceSslDeleteConnection(connection) == 0, "connection deletion failed");
    Check(sceSslUnloadCert(id) == 0, "credential unload failed with a retained connection");
    CheckRoot(bytes);
    Check(sceSslSetVerifyCallback(retained, nullptr, nullptr) == 0, "credential unload invalidated the connection");
    Check(sceSslFreeCaCerts(id, &bytes) == 0 && Stats(id).currentInuseSize == 0, "CA snapshot leaked with a retained connection");
    Check(sceSslTerm_nid_postfix(id) == 0, "context termination failed with a retained connection");
    Check(sceSslSetVerifyCallback(retained, nullptr, nullptr) == InvalidId, "terminated context's connection did not report an invalid id");
    Check(sceSslDeleteConnection(retained) == InvalidId, "terminated context's connection was still deletable");
    Check(sceSslGetCaCerts(other, &empty) == NotFound, "unrelated context was invalidated");
    Check(sceSslTerm_nid_postfix(other) == 0, "unrelated context termination failed");
}

void AtomicFailures() {
    const int id = sceSslInit_nid_postfix(1024 * 1024);
    SceSsl::Data der{const_cast<std::uint8_t*>(RootDer.data()), RootDer.size()};
    auto* pointer = &der;
    Check(sceSslLoadCert(id, 1, &pointer, nullptr, nullptr) == 0, "DER CA load failed");
    const auto before = Stats(id).currentInuseSize;
    auto bad = Data("not a certificate");
    SceSsl::Data* mixed[]{&der, &bad};
    Throws([&] { sceSslLoadCert(id, 2, mixed, nullptr, nullptr); });
    Check(Stats(id).currentInuseSize == before, "failed chain changed live allocation");
    SceSsl::CaCerts bytes{};
    Check(sceSslGetCaCerts(id, &bytes) == 0, "CA vanished after failure");
    CheckRoot(bytes);
    Check(sceSslFreeCaCerts(id, &bytes) == 0, "snapshot free failed");
    Check(sceSslLoadCert(id, -1, nullptr, nullptr, nullptr) == 0, "negative CA count was not ignored");
    Check(sceSslLoadCert(id, 1, nullptr, nullptr, nullptr) == 0, "null CA list was not ignored");
    SceSsl::Data* absent = nullptr;
    Throws([&] { sceSslLoadCert(id, 1, &absent, nullptr, nullptr); });

    Check(Stats(id).currentInuseSize == before, "argument failure changed retained CA");
    Check(sceSslTerm_nid_postfix(id) == 0, "term failed");
    Check(sceSslLoadCert(id, 1, &pointer, nullptr, nullptr) == InvalidId, "invalid load context error mismatch");
    const int tiny = sceSslInit_nid_postfix(32);
    Throws([&] { sceSslLoadCert(tiny, 1, &pointer, nullptr, nullptr); });
    Check(Stats(tiny).currentInuseSize == 0, "failed small-pool load leaked");
    Check(sceSslTerm_nid_postfix(tiny) == 0, "small-pool term failed");
}

void ClientIdentity() {
    const int id = sceSslInit_nid_postfix(1024 * 1024);
    auto client = Data(ClientPem);
    auto key = Data(ClientKey);
    auto wrong = Data(RootKey);
    Check(sceSslLoadCert(id, 0, nullptr, &client, &key) == 0, "client identity load failed");
    const auto before = Stats(id).currentInuseSize;
    Check(before > 0, "identity not retained");
    Check(sceSslLoadCert(id, 0, nullptr, &client, &wrong) == 0, "key pair was checked during loading");
    Check(sceSslLoadCert(id, 0, nullptr, &client, nullptr) == 0, "certificate-only load failed");
    const auto certificateBytes = Stats(id).currentInuseSize;
    Check(certificateBytes > 0 && certificateBytes < before, "certificate replacement retained the old key");
    Check(sceSslLoadCert(id, 0, nullptr, nullptr, &key) == 0, "independent key load failed");
    Check(Stats(id).currentInuseSize == before, "separate identity load retained different data");
    auto arbitraryKey = Data("invalid key encoding");
    Check(sceSslLoadCert(id, 0, nullptr, nullptr, &arbitraryKey) == 0, "key was parsed during loading");
    Check(Stats(id).currentInuseSize == certificateBytes + arbitraryKey.size, "raw key bytes were not retained");
    SceSsl::Data missing{};
    Check(sceSslLoadCert(id, 0, nullptr, &missing, nullptr) == static_cast<int>(0x80951771), "null certificate payload error mismatch");
    Check(sceSslLoadCert(id, 0, nullptr, nullptr, &missing) == static_cast<int>(0x80951771), "null key payload error mismatch");
    Check(Stats(id).currentInuseSize == certificateBytes + arbitraryKey.size, "null payload cleared the identity");
    Check(sceSslLoadCert(-1, -1, nullptr, &missing, &missing) == InvalidId, "load context validation precedence mismatch");
    Check(sceSslUnloadCert(id) == 0 && Stats(id).currentInuseSize == 0, "identity release failed");
    Check(sceSslTerm_nid_postfix(id) == 0, "identity term failed");
}

void CertificateReaders() {
    const int id = sceSslInit_nid_postfix(1024 * 1024);
    auto data = Data(RootPem);
    auto* pointer = &data;
    Check(sceSslLoadCert(id, 1, &pointer, nullptr, nullptr) == 0, "CA load failed");
    SceSsl::CaList handles{};
    Check(sceSslGetCaList(id, &handles) == 0 && handles.num == 1 && handles.certs[0], "CA handle absent");
    void* cert = handles.certs[0];

    std::size_t querySize = 0;
    Check(sceSslGetSerialNumber(id, cert, nullptr, &querySize) == 0 && querySize == 3, "query serial size failed");
    querySize = 0;
    unsigned char shortBuf[2]{};
    Check(sceSslGetSerialNumber(id, cert, shortBuf, &querySize) == 0 && querySize == 0, "zero-len serial copy changed length");
    querySize = sizeof(shortBuf);
    Check(sceSslGetSerialNumber(id, cert, shortBuf, &querySize) == 0 && querySize == 2 && shortBuf[0] == 0 && shortBuf[1] == 0x80,
        "short serial copy mismatch");
    unsigned char serialData[8]{};
    std::size_t serialLen = sizeof(serialData);
    Check(sceSslGetSerialNumber(id, cert, serialData, &serialLen) == 0, "get serial failed");
    Check(serialLen == sizeof(serialData) && serialData[3] == 0, "a large enough serial buffer changed the caller's length");
    Check(serialData[0] == 0x00 && serialData[1] == 0x80 && serialData[2] == 0xa1, "serial bytes mismatch");
    Throws([&] { sceSslGetSerialNumber(id, cert, serialData, nullptr); });
    Check(sceSslGetSerialNumber(id, cert, nullptr, nullptr) == 0, "empty serial query failed");
    Check(sceSslGetSerialNumber(id, nullptr, serialData, &serialLen) == static_cast<int>(0x8095f007), "null certificate error mismatch");
    void* fake = &serialLen;
    Throws([&] { sceSslGetSerialNumber(id, fake, serialData, &serialLen); });
    Check(sceSslGetSerialNumber(9999, cert, serialData, &serialLen) == InvalidId, "unknown context error mismatch");

    void* subject = sceSslGetSubjectName(id, cert);
    Check(subject != nullptr, "get subject failed");
    Check(sceSslGetSubjectName(id, cert) == subject, "subject handle identity changed");
    void* issuer = sceSslGetIssuerName(id, cert);
    Check(issuer != nullptr, "get issuer failed");
    Check(sceSslGetSubjectName(id, nullptr) == nullptr, "invalid name query returned a handle");
    Check(sceSslGetIssuerName(id, nullptr) == nullptr, "invalid name query returned a handle");
    Throws([&] { sceSslGetSubjectName(id, fake); });
    Throws([&] { sceSslGetIssuerName(id, fake); });
    Check(sceSslGetSubjectName(9999, cert) == nullptr, "invalid name query returned a handle");
    Check(sceSslGetIssuerName(9999, cert) == nullptr, "invalid name query returned a handle");

    Check(sceSslGetNameEntryCount(id, subject) == 3, "subject entry count mismatch");
    Check(sceSslGetNameEntryCount(id, issuer) == 3, "issuer entry count mismatch");
    Check(sceSslGetNameEntryCount(id, nullptr) == static_cast<int>(0x8095f007), "null cert name count accepted");
    Throws([&] { sceSslGetNameEntryCount(id, fake); });
    Check(sceSslGetNameEntryCount(9999, subject) == InvalidId, "unknown name context error mismatch");

    char oid[32]{};
    char val[64]{};
    std::size_t vlen = 0;
    Check(sceSslGetNameEntryInfo(id, subject, 0, oid, sizeof(oid), val, sizeof(val), &vlen) == 0, "entry 0 info failed");
    Check(std::strcmp(oid, "countryName") == 0 && std::strcmp(val, "GB") == 0 && vlen == 2, "entry 0 mismatch");
    Check(sceSslGetNameEntryInfo(id, subject, 1, oid, sizeof(oid), val, sizeof(val), &vlen) == 0, "entry 1 info failed");
    Check(std::strcmp(oid, "organizationName") == 0 && std::strcmp(val, "AnyPS5 Tests") == 0 && vlen == 12, "entry 1 mismatch");
    Check(sceSslGetNameEntryInfo(id, subject, 2, oid, sizeof(oid), val, sizeof(val), &vlen) == 0, "entry 2 info failed");
    Check(std::strcmp(oid, "commonName") == 0 && std::strcmp(val, "AnyPS5 fixture root") == 0 && vlen == 19, "entry 2 mismatch");

    char smallVal[4]{};
    std::size_t fullLen = 0;
    Check(sceSslGetNameEntryInfo(id, subject, 1, oid, 0, smallVal, sizeof(smallVal), &fullLen) == 0, "truncated query failed");
    Check(std::memcmp(smallVal, "AnyP", 4) == 0 && fullLen == 4, "buffer truncation mismatch");
    char shortOid[5] = {'x', 'x', 'x', 'x', 'x'};
    char shortValue[5] = {'y', 'y', 'y', 'y', 'y'};
    Check(sceSslGetNameEntryInfo(id, subject, 1, shortOid, 4, shortValue, 4, &fullLen) == 0, "bounded entry query failed");
    Check(std::memcmp(shortOid, "orga", 4) == 0 && shortOid[4] == 'x', "oid copy or guard mismatch");
    Check(std::memcmp(shortValue, "AnyP", 4) == 0 && shortValue[4] == 'y' && fullLen == 4, "value copy or guard mismatch");
    Check(sceSslGetNameEntryInfo(id, subject, 1, shortOid, 0, shortValue, 0, &fullLen) == 0 && fullLen == 0, "zero capacity query failed");
    Check(sceSslGetNameEntryInfo(id, subject, 1, nullptr, 0, shortValue, 0, &fullLen) == static_cast<int>(0x8095f007), "null oid accepted");
    Check(sceSslGetNameEntryInfo(id, subject, 1, shortOid, 0, nullptr, 0, &fullLen) == static_cast<int>(0x8095f007), "null value accepted");
    Check(sceSslGetNameEntryInfo(id, subject, 1, shortOid, 0, shortValue, 0, nullptr) == static_cast<int>(0x8095f007), "null value length accepted");

    Check(sceSslGetNameEntryInfo(id, subject, -1, oid, sizeof(oid), val, sizeof(val), &vlen) == static_cast<int>(0x8095f007), "negative index accepted");
    Check(sceSslGetNameEntryInfo(id, subject, 3, oid, sizeof(oid), val, sizeof(val), &vlen) == static_cast<int>(0x8095f007), "out of bounds index accepted");
    Check(sceSslGetNameEntryInfo(id, nullptr, 0, oid, sizeof(oid), val, sizeof(val), &vlen) == static_cast<int>(0x8095f007), "null certName accepted");
    Throws([&] { sceSslGetNameEntryInfo(id, fake, 0, oid, sizeof(oid), val, sizeof(val), &vlen); });
    Check(sceSslGetNameEntryInfo(9999, subject, 0, oid, sizeof(oid), val, sizeof(val), &vlen) == InvalidId, "unknown entry context error mismatch");

    Check(sceSslFreeSslCertName(id, subject) == 0, "free subject name failed");
    Check(sceSslFreeSslCertName(id, issuer) == 0, "free issuer name failed");
    Check(sceSslGetNameEntryCount(id, subject) == 3, "name release invalidated a live certificate name");
    Check(sceSslFreeSslCertName(id, nullptr) == static_cast<int>(0x8095f007), "null cert name free accepted");
    Check(sceSslFreeSslCertName(id, subject) == 0, "non-null name release failed");
    Check(sceSslFreeSslCertName(id, fake) == 0, "non-null name release failed");
    Check(sceSslFreeSslCertName(9999, subject) == InvalidId, "invalid name release context error mismatch");

    SceSsl::RtcTick begin{};
    Check(sceSslGetNotBefore(0, nullptr, nullptr) == InvalidId, "date validation precedence mismatch");
    Check(sceSslGetNotAfter(0, nullptr, nullptr) == InvalidId, "date validation precedence mismatch");
    Check(sceSslGetFingerprint(0, nullptr, -1, nullptr) == InvalidId, "fingerprint validation precedence mismatch");
    Check(sceSslGetNotBefore(id, cert, &begin) == 0, "get not before failed");
    Check(begin.tick == 63927094442000000ULL, "not before tick mismatch");
    Check(sceSslGetNotBefore(id, cert, nullptr) == static_cast<int>(0x8095f007), "null begin accepted");
    Check(sceSslGetNotBefore(id, nullptr, &begin) == static_cast<int>(0x8095f007), "null certificate date error mismatch");
    Throws([&] { sceSslGetNotBefore(id, fake, &begin); });
    Check(sceSslGetNotBefore(9999, cert, &begin) == InvalidId, "invalid date context error mismatch");

    SceSsl::RtcTick limit{};
    Check(sceSslGetNotAfter(id, cert, &limit) == 0, "get not after failed");
    Check(limit.tick == 64242454442000000ULL, "not after tick mismatch");
    Check(sceSslGetNotAfter(id, cert, nullptr) == static_cast<int>(0x8095f007), "null limit accepted");
    Check(sceSslGetNotAfter(id, nullptr, &limit) == static_cast<int>(0x8095f007), "null certificate date error mismatch");
    Throws([&] { sceSslGetNotAfter(id, fake, &limit); });
    Check(sceSslGetNotAfter(9999, cert, &limit) == InvalidId, "invalid date context error mismatch");

    unsigned char md5[16]{};
    Check(sceSslGetFingerprint(id, cert, 0, md5) == 0, "get md5 fingerprint failed");
    constexpr unsigned char expectedMd5[16] = {0x71, 0x3c, 0xe5, 0x20, 0x88, 0x2b, 0x6b, 0xd0, 0x17, 0x85, 0x07, 0xde, 0xbd, 0xdc, 0xa9, 0x4f};
    Check(std::memcmp(md5, expectedMd5, sizeof(md5)) == 0, "md5 digest mismatch");

    unsigned char sha1[20]{};
    Check(sceSslGetFingerprint(id, cert, 1, sha1) == 0, "get sha1 fingerprint failed");
    constexpr unsigned char expectedSha1[20] = {0x47, 0xe0, 0x89, 0xab, 0xb7, 0x02, 0xee, 0xb6, 0x2f, 0x8b, 0x5b, 0x7a, 0x13, 0xfe, 0x82, 0x6d, 0x2b, 0xe8, 0xee, 0xa7};
    Check(std::memcmp(sha1, expectedSha1, sizeof(sha1)) == 0, "sha1 digest mismatch");

    unsigned char sha256[32]{};
    Check(sceSslGetFingerprint(id, cert, 3, sha256) == 0, "get sha256 fingerprint failed");
    constexpr unsigned char expectedSha256[32] = {0x3d, 0x7d, 0xac, 0xd3, 0x04, 0x90, 0x42, 0xe1, 0x57, 0x19, 0x87, 0x27, 0x54, 0x45, 0xae, 0xed, 0xc4, 0xea, 0xd6, 0xd4, 0x20, 0xa0, 0x7f, 0xc4, 0xf5, 0x26, 0xc1, 0xa0, 0x85, 0x87, 0x5a, 0x4c};
    Check(std::memcmp(sha256, expectedSha256, sizeof(sha256)) == 0, "sha256 digest mismatch");

    Check(sceSslGetFingerprint(id, cert, 3, nullptr) == static_cast<int>(0x8095f007), "null fingerprint buffer accepted");
    Check(sceSslGetFingerprint(id, cert, -1, sha256) == static_cast<int>(0x8095177a), "negative hash type accepted");
    Check(sceSslGetFingerprint(id, cert, 99, sha256) == static_cast<int>(0x8095177a), "invalid hash type accepted");
    Check(sceSslGetFingerprint(id, nullptr, 3, sha256) == static_cast<int>(0x8095f007), "null fingerprint certificate error mismatch");
    Throws([&] { sceSslGetFingerprint(id, fake, 3, sha256); });
    Check(sceSslGetFingerprint(9999, cert, 3, sha256) == InvalidId, "invalid fingerprint context error mismatch");

    std::size_t pemLen = 0;
    Check(sceSslGetPem(id, cert, nullptr, &pemLen) == 0, "query pem size failed");
    Check(pemLen > 0, "pem length is 0");
    std::vector<char> pemBuf(pemLen);
    std::size_t writtenLen = pemBuf.size();
    Check(sceSslGetPem(id, cert, pemBuf.data(), &writtenLen) == 0, "write pem failed");
    Check(writtenLen == pemLen, "written pem length mismatch");
    Check(std::strstr(pemBuf.data(), "-----BEGIN CERTIFICATE-----\r\n") != nullptr, "pem header missing");
    Check(std::strstr(pemBuf.data(), "-----END CERTIFICATE-----\r\n") != nullptr, "pem footer missing");

    char smallPem[8]{};
    std::size_t smallPemLen = sizeof(smallPem);
    Check(sceSslGetPem(id, cert, smallPem, &smallPemLen) == static_cast<int>(0x8095f008) && smallPemLen == pemLen, "short PEM size error mismatch");
    Check(sceSslGetPem(id, cert, pemBuf.data(), nullptr) == static_cast<int>(0x8095f007), "null pem len accepted");
    Check(sceSslGetPem(id, nullptr, pemBuf.data(), &writtenLen) == static_cast<int>(0x8095f007), "null PEM certificate error mismatch");
    Throws([&] { sceSslGetPem(id, fake, pemBuf.data(), &writtenLen); });

    Check(sceSslClose(1) == InvalidId, "closing an unknown connection did not report an invalid id");

    Check(sceSslFreeCaList(id, &handles) == 0, "free ca list failed");
    Check(sceSslTerm_nid_postfix(id) == 0, "term failed");
    Check(sceSslGetSerialNumber(id, cert, serialData, &serialLen) == InvalidId, "terminated context error mismatch");
    Check(sceSslGetSubjectName(id, cert) == nullptr, "invalid name query returned a handle");
    Check(sceSslGetIssuerName(id, cert) == nullptr, "invalid name query returned a handle");
    Check(sceSslGetNotBefore(id, cert, &begin) == InvalidId, "invalid date context error mismatch");
    Check(sceSslGetNotAfter(id, cert, &limit) == InvalidId, "invalid date context error mismatch");
    Check(sceSslGetFingerprint(id, cert, 3, sha256) == InvalidId, "invalid fingerprint context error mismatch");
    Check(sceSslGetPem(id, cert, pemBuf.data(), &writtenLen) == InvalidId, "terminated PEM context error mismatch");

    const int id2 = sceSslInit_nid_postfix(1024 * 1024);
    Check(sceSslLoadCert(id2, 1, &pointer, nullptr, nullptr) == 0, "CA load failed");
    SceSsl::CaList handles2{};
    Check(sceSslGetCaList(id2, &handles2) == 0, "get ca list failed");
    void* cert2 = handles2.certs[0];
    void* subject2 = sceSslGetSubjectName(id2, cert2);
    Check(subject2 != nullptr, "get subject 2 failed");
    Check(sceSslFreeCaList(id2, &handles2) == 0, "free ca list failed");
    Check(sceSslUnloadCert(id2) == 0, "unload failed");
    Throws([&] { sceSslGetSerialNumber(id2, cert2, serialData, &serialLen); });
    Throws([&] { sceSslGetSubjectName(id2, cert2); });
    Throws([&] { sceSslGetIssuerName(id2, cert2); });
    Throws([&] { sceSslGetNotBefore(id2, cert2, &begin); });
    Throws([&] { sceSslGetNotAfter(id2, cert2, &limit); });
    Throws([&] { sceSslGetFingerprint(id2, cert2, 3, sha256); });
    Throws([&] { sceSslGetPem(id2, cert2, pemBuf.data(), &writtenLen); });
    Check(sceSslTerm_nid_postfix(id2) == 0, "term failed");
    Check(sceSslGetNameEntryCount(id2, subject2) == InvalidId, "terminated name context error mismatch");
    Check(sceSslFreeSslCertName(id2, subject2) == InvalidId, "invalid name release context error mismatch");
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
