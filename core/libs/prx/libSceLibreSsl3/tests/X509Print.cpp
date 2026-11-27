#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include "prx/libSceLibreSsl/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <source_location>
#include <string>

namespace {

void Require(bool condition, const char* what = nullptr, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "LibreSSL3 X509_print check failed at line %u: %s\n", location.line(), what ? what : "");
    std::abort();
}

int liveBlocks = 0;

void* APS5_VABI Allocate(std::size_t bytes) {
    ++liveBlocks;
    return std::malloc(bytes ? bytes : 1);
}

void APS5_VABI Release(void* block) {
    if (block) --liveBlocks;
    std::free(block);
}

void* APS5_VABI AllocateZeroed(std::size_t count, std::size_t bytes) {
    ++liveBlocks;
    return std::calloc(count ? count : 1, bytes ? bytes : 1);
}

void* APS5_VABI Reallocate(void*, std::size_t) { return nullptr; }
void* APS5_VABI Align(std::size_t, std::size_t) { return nullptr; }
void* APS5_VABI Realign(void*, std::size_t, std::size_t) { return nullptr; }
int APS5_VABI PosixAlign(void**, std::size_t, std::size_t) { return 12; }

void InitializeHeap() {
    void* const api[10] {
        reinterpret_cast<void*>(&Allocate), reinterpret_cast<void*>(&Release), reinterpret_cast<void*>(&AllocateZeroed),
        reinterpret_cast<void*>(&Reallocate), reinterpret_cast<void*>(&Align), reinterpret_cast<void*>(&Realign),
        reinterpret_cast<void*>(&PosixAlign),
    };
    ApplicationHeapRegister_nid_no_patch(api);
}

std::string printed;
int writesBeforeFailure = -1;

int APS5_VABI Write(LibreSsl::Bio*, const char* data, int size) {
    if (writesBeforeFailure == 0) return -1;
    if (writesBeforeFailure > 0) --writesBeforeFailure;
    printed.append(data, static_cast<std::size_t>(size));
    return size;
}

struct Output {
    LibreSsl::BioMethod method {};
    LibreSsl::Bio bio {};

    Output() {
        method.write = &Write;
        bio.method = &method;
        bio.initialized = 1;
    }
};

const unsigned char OidCommonName[] = {0x55, 0x04, 0x03};
const unsigned char OidCountry[] = {0x55, 0x04, 0x06};
const unsigned char OidSha256WithRsa[] = {0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01, 0x0b};
const unsigned char CommonNameText[] = "AnyPS5 test";
const unsigned char CountryText[] = "JP";
const unsigned char NotBeforeText[] = "261117200104Z";
const unsigned char NotAfterText[] = "361114200104Z";
const unsigned char SignatureText[] = {
    0x30, 0x45, 0x02, 0x21, 0x00, 0x83, 0x08, 0xa4, 0xa1, 0x7a, 0x2e, 0x64, 0xc3, 0x69, 0x09, 0x5e, 0xfa, 0xfd, 0x74, 0x6f,
};

struct Certificate {
    unsigned char versionBytes[16] {};
    unsigned char serialBytes[16] {};
    unsigned char signatureBytes[24] {};
    unsigned char notBeforeBytes[16] {};
    unsigned char notAfterBytes[16] {};
    unsigned char algorithmBytes[16] {};
    LibreSsl3Asn1String version {};
    LibreSsl3Asn1String serial {};
    LibreSsl3Asn1String signature {};
    LibreSsl3Asn1String notBefore {};
    LibreSsl3Asn1String notAfter {};
    LibreSslAsn1Object commonName {};
    LibreSslAsn1Object country {};
    LibreSslAsn1Object algorithmObject {};
    LibreSsl3X509Algor algorithm {};
    LibreSsl3Certificate certificate {};
    LibreSsl3CertificateInfo info {};
    LibreSsl3X509Validity validity {};
    LibreSslX509Name issuer {};
    LibreSslX509Name subject {};
    LibreSslStack issuerStack {};
    LibreSslStack subjectStack {};
    LibreSslX509NameEntry issuerEntries[1] {};
    LibreSslX509NameEntry subjectEntries[2] {};
    void* issuerPointers[1] {};
    void* subjectPointers[2] {};
    LibreSsl3Asn1String issuerCommonName {};
    LibreSsl3Asn1String subjectCommonName {};
    LibreSsl3Asn1String subjectCountry {};

    void Wire();
};

void Certificate::Wire() {
    version.data = versionBytes;
    serial.data = serialBytes;
    signature.data = signatureBytes;
    notBefore.data = notBeforeBytes;
    notAfter.data = notAfterBytes;
    algorithmObject.data = algorithmBytes;
    issuerCommonName = LibreSsl3Asn1String {11, 12, const_cast<unsigned char*>(CommonNameText), 0};
    subjectCommonName = LibreSsl3Asn1String {11, 12, const_cast<unsigned char*>(CommonNameText), 0};
    subjectCountry = LibreSsl3Asn1String {2, 12, const_cast<unsigned char*>(CountryText), 0};
    commonName = LibreSslAsn1Object {nullptr, nullptr, 13, 3, OidCommonName, 0};
    country = LibreSslAsn1Object {nullptr, nullptr, 14, 3, OidCountry, 0};
    issuerEntries[0] = LibreSslX509NameEntry {&commonName, &issuerCommonName, 0, 1};
    subjectEntries[0] = LibreSslX509NameEntry {&country, &subjectCountry, 0, 1};
    subjectEntries[1] = LibreSslX509NameEntry {&commonName, &subjectCommonName, 0, 1};
    issuerStack.num = 1;
    issuerStack.data = reinterpret_cast<char**>(issuerPointers);
    issuerPointers[0] = &issuerEntries[0];
    subjectStack.num = 2;
    subjectStack.data = reinterpret_cast<char**>(subjectPointers);
    subjectPointers[0] = &subjectEntries[0];
    subjectPointers[1] = &subjectEntries[1];
    issuer.entries = &issuerStack;
    subject.entries = &subjectStack;
    validity.notBefore = &notBefore;
    validity.notAfter = &notAfter;
    info.version = &version;
    info.serialNumber = &serial;
    info.signature = &algorithm;
    info.issuer = &issuer;
    info.validity = &validity;
    info.subject = &subject;
    info.key = nullptr;
    info.issuerUid = nullptr;
    info.subjectUid = nullptr;
    info.extensions = nullptr;
    algorithm = LibreSsl3X509Algor {&algorithmObject, nullptr};
    certificate.certInfo = &info;
    certificate.sigAlg = &algorithm;
    const LibreSsl3Asn1String* address = &signature;
    std::memcpy(certificate.rest, &address, sizeof(address));
}

const unsigned char VersionTwo[] = {0x02};
const unsigned char SerialValue[] = {0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef};

Certificate MakeCertificate() {
    Certificate value;
    std::memcpy(value.versionBytes, VersionTwo, 1);
    std::memcpy(value.serialBytes, SerialValue, 8);
    std::memcpy(value.signatureBytes, SignatureText, sizeof(SignatureText));
    std::memcpy(value.notBeforeBytes, NotBeforeText, 13);
    std::memcpy(value.notAfterBytes, NotAfterText, 13);
    std::memcpy(value.algorithmBytes, OidSha256WithRsa, 9);
    value.version = LibreSsl3Asn1String {1, 2, value.versionBytes, 0};
    value.serial = LibreSsl3Asn1String {8, 2, value.serialBytes, 0};
    value.signature = LibreSsl3Asn1String {20, 3, value.signatureBytes, 0};
    value.notBefore = LibreSsl3Asn1String {13, 23, value.notBeforeBytes, 0};
    value.notAfter = LibreSsl3Asn1String {13, 23, value.notAfterBytes, 0};
    value.algorithmObject = LibreSslAsn1Object {nullptr, nullptr, 0, 9, value.algorithmBytes, 0};
    value.Wire();
    return value;
}

const char Header[] = "Certificate:\n    Data:\n";
const char VersionLine[] = "        Version: 3 (0x2)\n";
const char SerialLine[] = "        Serial Number: 81985529216486895 (0x123456789abcdef)\n";
const char Body[] =
    "    Signature Algorithm: sha256WithRSAEncryption\n"
    "        Issuer: CN=AnyPS5 test\n"
    "        Validity\n"
    "            Not Before: Nov 17 20:01:04 2026 GMT\n"
    "            Not After : Nov 14 20:01:04 2036 GMT\n"
    "        Subject: C=JP, CN=AnyPS5 test\n";
const char DumpBlock[] =
    "    Signature Algorithm: sha256WithRSAEncryption\n"
    "         30:45:02:21:00:83:08:a4:a1:7a:2e:64:c3:69:09:5e:fa:fd:\n"
    "         74:6f\n";

std::string Expected() {
    return std::string(Header) + VersionLine + SerialLine + Body + DumpBlock;
}

void CheckPrintedCertificate() {
    Certificate fixture = MakeCertificate();
    fixture.Wire();
    Output output;
    printed.clear();
    Require(X509_print_nid_postfix(&output.bio, &fixture.certificate) == 1, "a certificate prints");
    Require(printed == Expected(), "the printed text matches the literal");
    Require(liveBlocks == 0, "the flat name buffer is released");
}

void CheckNegativeControl() {
    Certificate fixture = MakeCertificate();
    fixture.signatureBytes[0] ^= 0x01;
    fixture.Wire();
    Output output;
    printed.clear();
    Require(X509_print_nid_postfix(&output.bio, &fixture.certificate) == 1, "a perturbed certificate prints");
    std::string perturbed = Expected();
    const std::size_t position = perturbed.find("30:45:02:21");
    Require(position != std::string::npos, "the dump starts with the first signature byte");
    perturbed[position + 1] = '1';
    Require(printed == perturbed, "one printed byte follows one perturbed signature byte");
    Require(printed != Expected(), "a single perturbed byte is detected by the literal comparison");
}

void CheckVersionZeroAndNegativeSerial() {
    Certificate fixture = MakeCertificate();
    fixture.serial.type = 0x102;
    fixture.serial.length = 1;
    fixture.serialBytes[0] = 0x01;
    fixture.Wire();
    fixture.info.version = nullptr;
    Output output;
    printed.clear();
    Require(X509_print_nid_postfix(&output.bio, &fixture.certificate) == 1, "a missing version prints");
    const std::string expected = std::string(Header) + "        Version: 1 (0x0)\n" +
        "        Serial Number: -1 (-0x1)\n" + Body + DumpBlock;
    Require(printed == expected, "a missing version and a negative serial match the literal");
}

void CheckLongSerial() {
    Certificate fixture = MakeCertificate();
    fixture.serial.length = 12;
    for (int index = 0; index < 12; ++index) fixture.serialBytes[index] = static_cast<unsigned char>(0xf0 + index);
    fixture.Wire();
    Output output;
    printed.clear();
    Require(X509_print_nid_postfix(&output.bio, &fixture.certificate) == 1, "a long serial prints");
    const std::string expected = std::string(Header) + VersionLine +
        "        Serial Number:\n            f0:f1:f2:f3:f4:f5:f6:f7:f8:f9:fa:fb\n" + Body + DumpBlock;
    Require(printed == expected, "a serial number above eight bytes prints as a hexadecimal dump");
}

void CheckEmptySignature() {
    Certificate fixture = MakeCertificate();
    fixture.signature.length = 0;
    fixture.Wire();
    Output output;
    printed.clear();
    Require(X509_print_nid_postfix(&output.bio, &fixture.certificate) == 1, "an empty signature prints");
    const std::string expected = std::string(Header) + VersionLine + SerialLine + Body +
        "    Signature Algorithm: sha256WithRSAEncryption\n";
    Require(printed == expected, "an empty signature prints only the algorithm line");
}

void CheckNullArguments() {
    Certificate fixture = MakeCertificate();
    fixture.Wire();
    Output output;
    printed.clear();
    Require(X509_print_nid_postfix(nullptr, &fixture.certificate) == 0, "a null BIO returns 0");
    Require(printed.empty(), "a null BIO prints nothing");
    Require(X509_print_nid_postfix(&output.bio, nullptr) == 0, "a null certificate returns 0");
    Require(printed.empty(), "a null certificate prints nothing");
    Require(X509_print_nid_postfix(nullptr, nullptr) == 0, "two null arguments return 0");
}

void CheckFailedWrites() {
    Certificate fixture = MakeCertificate();
    fixture.Wire();
    Output output;
    printed.clear();
    writesBeforeFailure = 0;
    Require(X509_print_nid_postfix(&output.bio, &fixture.certificate) == 0, "a failed first write returns 0");
    Require(printed.empty(), "a failed first write prints nothing");
    writesBeforeFailure = 4;
    Require(X509_print_nid_postfix(&output.bio, &fixture.certificate) == 0, "a failed later write returns 0");
    Require(!printed.empty() && printed != Expected(), "a failed later write stops before the literal");
    writesBeforeFailure = -1;
}

}

int main() {
    InitializeHeap();
    CheckPrintedCertificate();
    CheckNegativeControl();
    CheckVersionZeroAndNegativeSerial();
    CheckLongSerial();
    CheckEmptySignature();
    CheckNullArguments();
    CheckFailedWrites();
    Require(liveBlocks == 0, "every guest allocation is released");
    std::printf("X509_print checks passed\n");
    return 0;
}
