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

extern "C" {
int APS5_VABI sceSslInit_nid_postfix(std::uint64_t);
int APS5_VABI sceSslTerm_nid_postfix(int);
int APS5_VABI sceSslGetMemoryPoolStats(int, SceSsl::MemoryPoolStats*);
int APS5_VABI sceSslLoadCert(int, int, SceSsl::Data**, SceSsl::Data*, SceSsl::Data*);
int APS5_VABI sceSslUnloadCert(int);
int APS5_VABI sceSslGetCaList(int, SceSsl::CaList*);
int APS5_VABI sceSslFreeCaList(int, SceSsl::CaList*);
int APS5_VABI sceSslGetCaCerts(int, SceSsl::CaCerts*);
int APS5_VABI sceSslFreeCaCerts(int, SceSsl::CaCerts*);
int APS5_VABI sceSslCreateConnection(int, int, const char*);
int APS5_VABI sceSslDeleteConnection(int);
int APS5_VABI sceSslSetVerifyCallback(int, int (APS5_VABI *)(int, unsigned int, void**, int, void*), void*);
int APS5_VABI sceSslGetSerialNumber(int, void*, unsigned char*, std::size_t*);
void* APS5_VABI sceSslGetSubjectName(int, void*);
void* APS5_VABI sceSslGetIssuerName(int, void*);
int APS5_VABI sceSslGetNameEntryCount(int, void*);
int APS5_VABI sceSslGetNameEntryInfo(int, void*, int, char*, std::size_t, char*, std::size_t, std::size_t*);
int APS5_VABI sceSslFreeSslCertName(int, void*);
int APS5_VABI sceSslClose(int);
int APS5_VABI sceSslGetNotBefore(int, void*, SceSsl::RtcTick*);
int APS5_VABI sceSslGetNotAfter(int, void*, SceSsl::RtcTick*);
int APS5_VABI sceSslGetFingerprint(int, void*, int, unsigned char*);
int APS5_VABI sceSslGetPem(int, void*, char*, std::size_t*);
}

namespace {

constexpr int NotFound = static_cast<int>(0x8095F004);

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
    Throws([&] { sceSslSetVerifyCallback(retained, nullptr, nullptr); });
    Check(sceSslDeleteConnection(retained) == 0, "terminated context's connection cannot be released");
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
    Throws([&] { sceSslLoadCert(id, -1, nullptr, nullptr, nullptr); });
    Throws([&] { sceSslLoadCert(id, 1, nullptr, nullptr, nullptr); });
    SceSsl::Data* absent = nullptr;
    Throws([&] { sceSslLoadCert(id, 1, &absent, nullptr, nullptr); });
    Throws([&] { sceSslLoadCert(id, 0, nullptr, &der, nullptr); });
    Check(Stats(id).currentInuseSize == before, "argument failure changed retained CA");
    Check(sceSslTerm_nid_postfix(id) == 0, "term failed");
    Throws([&] { sceSslLoadCert(id, 1, &pointer, nullptr, nullptr); });
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
    Throws([&] { sceSslLoadCert(id, 0, nullptr, &client, &wrong); });
    Check(Stats(id).currentInuseSize == before, "mismatched key changed identity");
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
    Check(sceSslGetSerialNumber(id, cert, shortBuf, &querySize) == 0 && querySize == 3, "zero-len serial query failed");
    querySize = sizeof(shortBuf);
    Check(sceSslGetSerialNumber(id, cert, shortBuf, &querySize) == static_cast<int>(0x8095177a) && querySize == 3,
        "short serial buffer accepted");
    unsigned char serialData[8]{};
    std::size_t serialLen = sizeof(serialData);
    Check(sceSslGetSerialNumber(id, cert, serialData, &serialLen) == 0, "get serial failed");
    Check(serialLen == 3, "serial length mismatch");
    Check(serialData[0] == 0x00 && serialData[1] == 0x80 && serialData[2] == 0xa1, "serial bytes mismatch");
    Check(sceSslGetSerialNumber(id, cert, serialData, nullptr) == static_cast<int>(0x8095177a), "null serial length accepted");
    Throws([&] { sceSslGetSerialNumber(id, nullptr, serialData, &serialLen); });
    void* fake = &serialLen;
    Throws([&] { sceSslGetSerialNumber(id, fake, serialData, &serialLen); });
    Throws([&] { sceSslGetSerialNumber(9999, cert, serialData, &serialLen); });

    void* subject = sceSslGetSubjectName(id, cert);
    Check(subject != nullptr, "get subject failed");
    void* issuer = sceSslGetIssuerName(id, cert);
    Check(issuer != nullptr, "get issuer failed");
    Throws([&] { sceSslGetSubjectName(id, nullptr); });
    Throws([&] { sceSslGetIssuerName(id, nullptr); });
    Throws([&] { sceSslGetSubjectName(id, fake); });
    Throws([&] { sceSslGetIssuerName(id, fake); });
    Throws([&] { sceSslGetSubjectName(9999, cert); });
    Throws([&] { sceSslGetIssuerName(9999, cert); });

    Check(sceSslGetNameEntryCount(id, subject) == 3, "subject entry count mismatch");
    Check(sceSslGetNameEntryCount(id, issuer) == 3, "issuer entry count mismatch");
    Check(sceSslGetNameEntryCount(id, nullptr) == static_cast<int>(0x8095177a), "null cert name count accepted");
    Throws([&] { sceSslGetNameEntryCount(id, fake); });
    Throws([&] { sceSslGetNameEntryCount(9999, subject); });

    char oid[32]{};
    char val[64]{};
    std::size_t vlen = 0;
    Check(sceSslGetNameEntryInfo(id, subject, 0, oid, sizeof(oid), val, sizeof(val), &vlen) == 0, "entry 0 info failed");
    Check(std::strcmp(oid, "C") == 0 && std::strcmp(val, "GB") == 0 && vlen == 2, "entry 0 mismatch");
    Check(sceSslGetNameEntryInfo(id, subject, 1, oid, sizeof(oid), val, sizeof(val), &vlen) == 0, "entry 1 info failed");
    Check(std::strcmp(oid, "O") == 0 && std::strcmp(val, "AnyPS5 Tests") == 0 && vlen == 12, "entry 1 mismatch");
    Check(sceSslGetNameEntryInfo(id, subject, 2, oid, sizeof(oid), val, sizeof(val), &vlen) == 0, "entry 2 info failed");
    Check(std::strcmp(oid, "CN") == 0 && std::strcmp(val, "AnyPS5 fixture root") == 0 && vlen == 19, "entry 2 mismatch");

    char smallVal[4]{};
    std::size_t fullLen = 0;
    Check(sceSslGetNameEntryInfo(id, subject, 1, nullptr, 0, smallVal, sizeof(smallVal), &fullLen) == 0, "truncated query failed");
    Check(std::strcmp(smallVal, "Any") == 0 && fullLen == 12, "buffer truncation mismatch");

    Check(sceSslGetNameEntryInfo(id, subject, -1, oid, sizeof(oid), val, sizeof(val), &vlen) == static_cast<int>(0x8095177a), "negative index accepted");
    Check(sceSslGetNameEntryInfo(id, subject, 3, oid, sizeof(oid), val, sizeof(val), &vlen) == static_cast<int>(0x8095177a), "out of bounds index accepted");
    Check(sceSslGetNameEntryInfo(id, nullptr, 0, oid, sizeof(oid), val, sizeof(val), &vlen) == static_cast<int>(0x8095177a), "null certName accepted");
    Throws([&] { sceSslGetNameEntryInfo(id, fake, 0, oid, sizeof(oid), val, sizeof(val), &vlen); });
    Throws([&] { sceSslGetNameEntryInfo(9999, subject, 0, oid, sizeof(oid), val, sizeof(val), &vlen); });

    Check(sceSslFreeSslCertName(id, subject) == 0, "free subject name failed");
    Check(sceSslFreeSslCertName(id, issuer) == 0, "free issuer name failed");
    Check(sceSslFreeSslCertName(id, nullptr) == static_cast<int>(0x8095177a), "null cert name free accepted");
    Throws([&] { sceSslFreeSslCertName(id, subject); });
    Throws([&] { sceSslFreeSslCertName(id, fake); });
    Throws([&] { sceSslFreeSslCertName(9999, subject); });

    SceSsl::RtcTick begin{};
    Check(sceSslGetNotBefore(id, cert, &begin) == 0, "get not before failed");
    Check(begin.tick == 63927094442000000ULL, "not before tick mismatch");
    Check(sceSslGetNotBefore(id, cert, nullptr) == static_cast<int>(0x8095177a), "null begin accepted");
    Throws([&] { sceSslGetNotBefore(id, nullptr, &begin); });
    Throws([&] { sceSslGetNotBefore(id, fake, &begin); });

    SceSsl::RtcTick limit{};
    Check(sceSslGetNotAfter(id, cert, &limit) == 0, "get not after failed");
    Check(limit.tick == 64242454442000000ULL, "not after tick mismatch");
    Check(sceSslGetNotAfter(id, cert, nullptr) == static_cast<int>(0x8095177a), "null limit accepted");
    Throws([&] { sceSslGetNotAfter(id, nullptr, &limit); });
    Throws([&] { sceSslGetNotAfter(id, fake, &limit); });

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

    Check(sceSslGetFingerprint(id, cert, 3, nullptr) == static_cast<int>(0x8095177a), "null fingerprint buffer accepted");
    Check(sceSslGetFingerprint(id, cert, -1, sha256) == static_cast<int>(0x8095177a), "negative hash type accepted");
    Check(sceSslGetFingerprint(id, cert, 99, sha256) == static_cast<int>(0x8095177a), "invalid hash type accepted");
    Throws([&] { sceSslGetFingerprint(id, nullptr, 3, sha256); });
    Throws([&] { sceSslGetFingerprint(id, fake, 3, sha256); });

    std::size_t pemLen = 0;
    Check(sceSslGetPem(id, cert, nullptr, &pemLen) == 0, "query pem size failed");
    Check(pemLen > 0, "pem length is 0");
    std::vector<char> pemBuf(pemLen);
    std::size_t writtenLen = pemBuf.size();
    Check(sceSslGetPem(id, cert, pemBuf.data(), &writtenLen) == 0, "write pem failed");
    Check(writtenLen == pemLen, "written pem length mismatch");
    Check(std::strstr(pemBuf.data(), "-----BEGIN CERTIFICATE-----\n") != nullptr, "pem header missing");
    Check(std::strstr(pemBuf.data(), "-----END CERTIFICATE-----\n") != nullptr, "pem footer missing");

    char smallPem[8]{};
    std::size_t smallPemLen = sizeof(smallPem);
    Throws([&] { sceSslGetPem(id, cert, smallPem, &smallPemLen); });
    Check(sceSslGetPem(id, cert, pemBuf.data(), nullptr) == static_cast<int>(0x8095177a), "null pem len accepted");
    Throws([&] { sceSslGetPem(id, nullptr, pemBuf.data(), &writtenLen); });
    Throws([&] { sceSslGetPem(id, fake, pemBuf.data(), &writtenLen); });

    Throws([&] { sceSslClose(1); });

    Check(sceSslFreeCaList(id, &handles) == 0, "free ca list failed");
    Check(sceSslTerm_nid_postfix(id) == 0, "term failed");
    Throws([&] { sceSslGetSerialNumber(id, cert, serialData, &serialLen); });
    Throws([&] { sceSslGetSubjectName(id, cert); });
    Throws([&] { sceSslGetIssuerName(id, cert); });
    Throws([&] { sceSslGetNotBefore(id, cert, &begin); });
    Throws([&] { sceSslGetNotAfter(id, cert, &limit); });
    Throws([&] { sceSslGetFingerprint(id, cert, 3, sha256); });
    Throws([&] { sceSslGetPem(id, cert, pemBuf.data(), &writtenLen); });

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
    Throws([&] { sceSslGetNameEntryCount(id2, subject2); });
    Throws([&] { sceSslFreeSslCertName(id2, subject2); });
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
