#include "prx/libSceSsl/include/Ssl.hpp"
#include "prx/libSceSsl/SslTypes.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "SslCertificateFixture.hpp"
#include "Testing.hpp"

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstring>
#include <exception>
#include <limits>
#include <stdexcept>
#include <vector>
#include <string>

namespace {

void Readers() {
    const int context = sceSslInit_nid_postfix(1024 * 1024);
    const int other = sceSslInit_nid_postfix(1024 * 1024);
    SceSsl::Data data{reinterpret_cast<std::uint8_t*>(const_cast<char*>(RootPem)), std::strlen(RootPem)};
    auto* pointer = &data;
    Require(sceSslLoadCert(context, 1, &pointer, nullptr, nullptr) == 0, "certificate load failed");
    SceSsl::CaList list{};
    Require(sceSslGetCaList(context, &list) == 0 && list.num == 1, "certificate handle absent");
    void* certificate = list.certs[0];
    std::size_t length = SIZE_MAX;
    Require(sceSslGetSerialNumber(context, certificate, nullptr, &length) == 0 && length == 3, "PS5 serial size query failed");
    std::array<unsigned char, 5> serial;
    serial.fill(0xa5);
    Require(sceSslGetSerialNumber(context, certificate, serial.data(), &length) == 0 && length == 3, "PS5 serial copy failed");
    Require(serial == std::array<unsigned char, 5>{0x00, 0x80, 0xa1, 0xa5, 0xa5}, "serial copy changed its bytes or guards");
    serial.fill(0xa5);
    length = 2;
    Require(sceSslGetSerialNumber(context, certificate, serial.data(), &length) == 0 && length == 2, "short serial copy changed length");
    Require(serial == std::array<unsigned char, 5>{0x00, 0x80, 0xa5, 0xa5, 0xa5}, "short serial copy changed guard bytes");
    ExpectErr([&] { sceSslGetSerialNumber(other, certificate, nullptr, &length); });

    auto* subject = sceSslGetSubjectName(context, certificate);
    auto* issuer = sceSslGetIssuerName(context, certificate);
    Require(subject && issuer && subject != issuer, "certificate name absent");
    Require(sceSslGetNameEntryCount(context, subject) == 3 && sceSslGetNameEntryCount(context, issuer) == 3, "PS5 name entry count failed");
    ExpectErr([&] { sceSslGetSubjectName(other, certificate); });
    ExpectErr([&] { sceSslGetNameEntryCount(other, subject); });
    Require(sceSslFreeSslCertName(other, subject) == 0, "non-null name release checked ownership");
    std::array<char, 32> oid{}, value{};
    length = SIZE_MAX;
    Require(sceSslGetNameEntryInfo(context, subject, 1, oid.data(), oid.size(), value.data(), value.size(), &length) == 0, "PS5 eight-argument name query failed");
    Require(std::strcmp(oid.data(), "organizationName") == 0 && std::strcmp(value.data(), "AnyPS5 Tests") == 0 && length == 12, "name output or full-width length changed");

    std::string expectedPem;
    for (const char byte : std::string(RootPem)) {
        if (byte == '\n') expectedPem += '\r';
        expectedPem += byte;
    }
    length = SIZE_MAX;
    Require(sceSslGetPem(context, certificate, nullptr, &length) == 0 && length == expectedPem.size() + 1, "PS5 PEM size query failed");
    std::vector<char> pem(length + 2, static_cast<char>(0x5a));
    Require(sceSslGetPem(context, certificate, pem.data(), &length) == 0 && length == expectedPem.size() + 1, "PS5 PEM copy failed");
    Require(std::strcmp(pem.data(), expectedPem.c_str()) == 0 && pem[length] == 0x5a && pem[length + 1] == 0x5a, "PEM copy changed bytes or guards");
    std::array<char, 4> small{'x', 'x', 'x', 'x'};
    length = 2;
    Require(sceSslGetPem(context, certificate, small.data(), &length) == static_cast<int>(0x8095f008) && length == expectedPem.size() + 1, "short PEM error or size mismatch");
    Require(small == std::array<char, 4>{0, 0, 'x', 'x'}, "short PEM clearing or guards mismatch");
    ExpectErr([&] { sceSslGetPem(other, certificate, nullptr, &length); });
    std::array<char, 8> oidGuard{'o', 'o', 'o', 'o', 'o', 'o', 'o', 'o'};
    std::array<char, 8> valGuard{'v', 'v', 'v', 'v', 'v', 'v', 'v', 'v'};
    std::size_t outLen = 777;
    Require(sceSslGetNameEntryInfo(context, subject, 0, oidGuard.data(), std::numeric_limits<std::size_t>::max(), valGuard.data(), valGuard.size(), &outLen) == static_cast<int>(0x8095f007), "oversized maxOidnameLen should fail");
    Require(outLen == 777 && valGuard[0] == 'v', "oversized maxOidnameLen should not write value outputs");
    Require(sceSslUnloadCert(context) == 0, "unload failed");
    Require(sceSslGetSerialNumber(context, certificate, nullptr, &length) == 0 && length == 3, "CA snapshot lost its certificate on unload");
    Require(sceSslFreeCaList(context, &list) == 0, "CA list free failed");
    ExpectErr([&] { sceSslGetSerialNumber(context, certificate, nullptr, &length); });
    ExpectErr([&] { sceSslGetNameEntryCount(context, subject); });
    Require(sceSslFreeSslCertName(context, subject) == 0, "name free failed");
    ExpectErr([&] { sceSslGetNameEntryCount(context, subject); });
    Require(sceSslTerm_nid_postfix(context) == 0, "context term failed");
    Require(sceSslGetNameEntryCount(context, issuer) == static_cast<int>(0x8095f006), "terminated context count error mismatch");
    Require(sceSslTerm_nid_postfix(other) == 0, "other context term failed");
}

}

int main() {
    try { Readers(); } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
