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

extern "C" {
int APS5_VABI sceSslInit_nid_postfix(std::uint64_t);
int APS5_VABI sceSslTerm_nid_postfix(int);
int APS5_VABI sceSslLoadCert(int, int, SceSsl::Data**, SceSsl::Data*, SceSsl::Data*);
int APS5_VABI sceSslUnloadCert(int);
int APS5_VABI sceSslGetCaList(int, SceSsl::CaList*);
int APS5_VABI sceSslFreeCaList(int, SceSsl::CaList*);
int APS5_VABI sceSslGetSerialNumber(int, void*, unsigned char*, std::size_t*);
void* APS5_VABI sceSslGetSubjectName(int, void*);
void* APS5_VABI sceSslGetIssuerName(int, void*);
int APS5_VABI sceSslGetNameEntryCount(int, void*);
int APS5_VABI sceSslGetNameEntryInfo(int, void*, int, char*, std::size_t, char*, std::size_t, std::size_t*);
int APS5_VABI sceSslFreeSslCertName(int, void*);
int APS5_VABI sceSslGetPem(int, void*, char*, std::size_t*);
}

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
    Check(sceSslGetSerialNumber(context, certificate, serial.data(), &length) == static_cast<int>(0x8095177a) && length == 3, "short serial buffer did not report its required size");
    Check(std::all_of(serial.begin(), serial.end(), [](auto byte) { return byte == 0xa5; }), "short serial buffer was modified");
    Throws([&] { sceSslGetSerialNumber(other, certificate, nullptr, &length); });

    auto* subject = sceSslGetSubjectName(context, certificate);
    auto* issuer = sceSslGetIssuerName(context, certificate);
    Check(subject && issuer && subject != issuer, "certificate name absent");
    Check(sceSslGetNameEntryCount(context, subject) == 3 && sceSslGetNameEntryCount(context, issuer) == 3, "PS5 name entry count failed");
    Throws([&] { sceSslGetSubjectName(other, certificate); });
    Throws([&] { sceSslGetNameEntryCount(other, subject); });
    Throws([&] { sceSslFreeSslCertName(other, subject); });
    std::array<char, 16> oid{}, value{};
    length = SIZE_MAX;
    Check(sceSslGetNameEntryInfo(context, subject, 1, oid.data(), oid.size(), value.data(), value.size(), &length) == 0, "PS5 eight-argument name query failed");
    Check(std::strcmp(oid.data(), "O") == 0 && std::strcmp(value.data(), "AnyPS5 Tests") == 0 && length == 12, "name output or full-width length changed");

    length = SIZE_MAX;
    Check(sceSslGetPem(context, certificate, nullptr, &length) == 0 && length == std::strlen(RootPem) + 1, "PS5 PEM size query failed");
    std::vector<char> pem(length + 2, static_cast<char>(0x5a));
    Check(sceSslGetPem(context, certificate, pem.data(), &length) == 0 && length == std::strlen(RootPem) + 1, "PS5 PEM copy failed");
    Check(std::strcmp(pem.data(), RootPem) == 0 && pem[length] == 0x5a && pem[length + 1] == 0x5a, "PEM copy changed bytes or guards");
    std::array<char, 4> small{'x', 'x', 'x', 'x'};
    length = 2;
    Throws([&] { sceSslGetPem(context, certificate, small.data(), &length); });
    Check(std::all_of(small.begin(), small.end(), [](char byte) { return byte == 'x'; }), "short PEM buffer was modified");
    Throws([&] { sceSslGetPem(other, certificate, nullptr, &length); });
    Check(sceSslUnloadCert(context) == 0, "unload failed");
    Check(sceSslGetSerialNumber(context, certificate, nullptr, &length) == 0 && length == 3, "CA snapshot lost its certificate on unload");
    Check(sceSslFreeCaList(context, &list) == 0, "CA list free failed");
    Throws([&] { sceSslGetSerialNumber(context, certificate, nullptr, &length); });
    Check(sceSslGetNameEntryCount(context, subject) == 3, "owned name depended on freed certificate storage");
    Check(sceSslFreeSslCertName(context, subject) == 0, "name free failed");
    Throws([&] { sceSslGetNameEntryCount(context, subject); });
    Check(sceSslTerm_nid_postfix(context) == 0, "context term failed");
    Throws([&] { sceSslGetNameEntryCount(context, issuer); });
    Check(sceSslTerm_nid_postfix(other) == 0, "other context term failed");
}

}

int main() {
    try { Readers(); } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
