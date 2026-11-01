#include "prx/libSceSsl/include/Ssl.hpp"
#include "prx/libSceSsl/SslTypes.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "SslCertificateFixture.hpp"

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstring>
#include <exception>
#include <stdexcept>
#include <vector>
#include <string>

namespace {

void Check(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}

template <class Function>
void Throws(Function function) {
    try { function(); } catch (const std::exception&) { return; }
    throw std::runtime_error("unsupported reader arguments were accepted");
}

void Readers() {
    const int context = sceSslInit_nid_postfix(1024 * 1024);
    const int other = sceSslInit_nid_postfix(1024 * 1024);
    SceSsl::Data data{reinterpret_cast<std::uint8_t*>(const_cast<char*>(RootPem)), std::strlen(RootPem)};
    auto* pointer = &data;
    Check(sceSslLoadCert(context, 1, &pointer, nullptr, nullptr) == 0, "certificate load failed");
    SceSsl::CaList list{};
    Check(sceSslGetCaList(context, &list) == 0 && list.num == 1, "certificate handle absent");
    void* certificate = list.certs[0];
    std::size_t length = SIZE_MAX;
    Check(sceSslGetSerialNumber(context, certificate, nullptr, &length) == 0 && length == 3, "PS5 serial size query failed");
    std::array<unsigned char, 5> serial;
    serial.fill(0xa5);
    Check(sceSslGetSerialNumber(context, certificate, serial.data(), &length) == 0 && length == 3, "PS5 serial copy failed");
    Check(serial == std::array<unsigned char, 5>{0x00, 0x80, 0xa1, 0xa5, 0xa5}, "serial copy changed its bytes or guards");
    serial.fill(0xa5);
    length = 2;
    Check(sceSslGetSerialNumber(context, certificate, serial.data(), &length) == 0 && length == 2, "short serial copy changed length");
    Check(serial == std::array<unsigned char, 5>{0x00, 0x80, 0xa5, 0xa5, 0xa5}, "short serial copy changed guard bytes");
    Throws([&] { sceSslGetSerialNumber(other, certificate, nullptr, &length); });

    auto* subject = sceSslGetSubjectName(context, certificate);
    auto* issuer = sceSslGetIssuerName(context, certificate);
    Check(subject && issuer && subject != issuer, "certificate name absent");
    Check(sceSslGetNameEntryCount(context, subject) == 3 && sceSslGetNameEntryCount(context, issuer) == 3, "PS5 name entry count failed");
    Throws([&] { sceSslGetSubjectName(other, certificate); });
    Throws([&] { sceSslGetNameEntryCount(other, subject); });
    Check(sceSslFreeSslCertName(other, subject) == 0, "non-null name release checked ownership");
    std::array<char, 32> oid{}, value{};
    length = SIZE_MAX;
    Check(sceSslGetNameEntryInfo(context, subject, 1, oid.data(), oid.size(), value.data(), value.size(), &length) == 0, "PS5 eight-argument name query failed");
    Check(std::strcmp(oid.data(), "organizationName") == 0 && std::strcmp(value.data(), "AnyPS5 Tests") == 0 && length == 12, "name output or full-width length changed");

    std::string expectedPem;
    for (const char byte : std::string(RootPem)) {
        if (byte == '\n') expectedPem += '\r';
        expectedPem += byte;
    }
    length = SIZE_MAX;
    Check(sceSslGetPem(context, certificate, nullptr, &length) == 0 && length == expectedPem.size() + 1, "PS5 PEM size query failed");
    std::vector<char> pem(length + 2, static_cast<char>(0x5a));
    Check(sceSslGetPem(context, certificate, pem.data(), &length) == 0 && length == expectedPem.size() + 1, "PS5 PEM copy failed");
    Check(std::strcmp(pem.data(), expectedPem.c_str()) == 0 && pem[length] == 0x5a && pem[length + 1] == 0x5a, "PEM copy changed bytes or guards");
    std::array<char, 4> small{'x', 'x', 'x', 'x'};
    length = 2;
    Check(sceSslGetPem(context, certificate, small.data(), &length) == static_cast<int>(0x8095f008) && length == expectedPem.size() + 1, "short PEM error or size mismatch");
    Check(small == std::array<char, 4>{0, 0, 'x', 'x'}, "short PEM clearing or guards mismatch");
    Throws([&] { sceSslGetPem(other, certificate, nullptr, &length); });
    Check(sceSslUnloadCert(context) == 0, "unload failed");
    Check(sceSslGetSerialNumber(context, certificate, nullptr, &length) == 0 && length == 3, "CA snapshot lost its certificate on unload");
    Check(sceSslFreeCaList(context, &list) == 0, "CA list free failed");
    Throws([&] { sceSslGetSerialNumber(context, certificate, nullptr, &length); });
    Throws([&] { sceSslGetNameEntryCount(context, subject); });
    Check(sceSslFreeSslCertName(context, subject) == 0, "name free failed");
    Throws([&] { sceSslGetNameEntryCount(context, subject); });
    Check(sceSslTerm_nid_postfix(context) == 0, "context term failed");
    Check(sceSslGetNameEntryCount(context, issuer) == static_cast<int>(0x8095f006), "terminated context count error mismatch");
    Check(sceSslTerm_nid_postfix(other) == 0, "other context term failed");
}

}

int main() {
    try { Readers(); } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
