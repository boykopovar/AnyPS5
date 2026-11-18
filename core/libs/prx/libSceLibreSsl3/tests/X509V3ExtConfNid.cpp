#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include "prx/libSceLibreSsl/libSceLibreSsl.hpp"
#include "prx/libc/libc.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <source_location>
#include <stdexcept>
#include <unordered_map>
#include <vector>

namespace {

std::unordered_map<void*, std::size_t> allocations;
int allocationsUntilFailure = -1;

void Require(bool condition, std::source_location location = std::source_location::current());

void* APS5_VABI Allocate_nid_no_patch(std::size_t size) {
    if (allocationsUntilFailure == 0) {
        allocationsUntilFailure = -1;
        return nullptr;
    }
    if (allocationsUntilFailure > 0) --allocationsUntilFailure;
    void* result = std::malloc(size == 0 ? 1 : size);
    Require(result != nullptr);
    allocations.emplace(result, size);
    return result;
}

void APS5_VABI Free_nid_no_patch(void* pointer) {
    if (!pointer) return;
    Require(allocations.erase(pointer) == 1);
    std::free(pointer);
}

void* APS5_VABI Callocate_nid_no_patch(std::size_t count, std::size_t size) {
    if (allocationsUntilFailure == 0) {
        allocationsUntilFailure = -1;
        return nullptr;
    }
    if (allocationsUntilFailure > 0) --allocationsUntilFailure;
    void* result = std::calloc(count == 0 ? 1 : count, size == 0 ? 1 : size);
    Require(result != nullptr);
    allocations.emplace(result, count * size);
    return result;
}

void* APS5_VABI Reallocate_nid_no_patch(void* pointer, std::size_t size) {
    if (allocationsUntilFailure == 0) {
        allocationsUntilFailure = -1;
        return nullptr;
    }
    if (allocationsUntilFailure > 0) --allocationsUntilFailure;
    if (!pointer) return Allocate_nid_no_patch(size);
    Require(allocations.erase(pointer) == 1);
    void* result = std::realloc(pointer, size == 0 ? 1 : size);
    Require(result != nullptr);
    allocations.emplace(result, size);
    return result;
}

void* APS5_VABI UnexpectedAlign_nid_no_patch(std::size_t, std::size_t) { throw std::runtime_error("unexpected aligned allocation"); }
void* APS5_VABI UnexpectedRealign_nid_no_patch(void*, std::size_t, std::size_t) { throw std::runtime_error("unexpected aligned reallocation"); }
int APS5_VABI UnexpectedPosixAlign_nid_no_patch(void**, std::size_t, std::size_t) { throw std::runtime_error("unexpected posix_memalign"); }

void Require(bool condition, std::source_location location) {
    if (condition) return;
    std::fprintf(stderr, "LibreSSL3 X509V3_EXT_conf_nid check failed at line %u\n", location.line());
    std::abort();
}

template <typename Callable>
void RequireNotImplemented(Callable call, std::source_location location = std::source_location::current()) {
    try {
        call();
    } catch (const std::runtime_error&) {
        return;
    }
    Require(false, location);
}

LibreSsl3X509Extension* Conf(int nid, const char* value, void* conf = nullptr) {
    return X509V3_EXT_conf_nid_nid_postfix(conf, nullptr, nid, value);
}

void CheckValue(LibreSsl3X509Extension* extension, int nid, int critical,
    std::initializer_list<unsigned char> expected) {
    Require(extension != nullptr);
    Require(extension->object != nullptr);
    Require(OBJ_obj2nid_nid_postfix(extension->object) == nid);
    Require(extension->critical == critical);
    auto* value = static_cast<Asn1String*>(extension->value);
    Require(value != nullptr);
    Require(value->type == 4);
    Require(value->length == static_cast<std::int32_t>(expected.size()));
    const std::vector<unsigned char> bytes(expected);
    if (!bytes.empty()) Require(std::memcmp(value->data, bytes.data(), bytes.size()) == 0);
    X509_EXTENSION_free_nid_postfix(extension);
    Require(allocations.empty());
}

void TestDer() {
    CheckValue(Conf(83, "DER:03:02:05:A0"), 83, 0, {0x03, 0x02, 0x05, 0xa0});
    CheckValue(Conf(83, "DER:030205a0"), 83, 0, {0x03, 0x02, 0x05, 0xa0});
    CheckValue(Conf(83, "DER:"), 83, 0, {});
    CheckValue(Conf(103, "DER:3000"), 103, 0, {0x30, 0x00});
    CheckValue(Conf(83, "critical,DER:0500"), 83, 1, {0x05, 0x00});
    CheckValue(Conf(83, "critical,  DER:0A0B"), 83, 1, {0x0a, 0x0b});
}

void TestAsn1() {
    CheckValue(Conf(83, "ASN1:INTEGER:5"), 83, 0, {0x02, 0x01, 0x05});
    CheckValue(Conf(83, "ASN1:INT:0x10"), 83, 0, {0x02, 0x01, 0x10});
    CheckValue(Conf(83, "ASN1:INTEGER:-5"), 83, 0, {0x02, 0x01, 0xfb});
    CheckValue(Conf(83, "ASN1:ENUMERATED:5"), 83, 0, {0x0a, 0x01, 0x05});
    CheckValue(Conf(83, "ASN1:BOOLEAN:true"), 83, 0, {0x01, 0x01, 0xff});
    CheckValue(Conf(83, "ASN1:BOOL:FALSE"), 83, 0, {0x01, 0x01, 0x00});
    CheckValue(Conf(83, "ASN1:NULL"), 83, 0, {0x05, 0x00});
    CheckValue(Conf(83, "ASN1:OBJECT:1.2.3"), 83, 0, {0x06, 0x02, 0x2a, 0x03});
    CheckValue(Conf(83, "ASN1:OBJECT:serverAuth"), 83, 0,
        {0x06, 0x08, 0x2b, 0x06, 0x01, 0x05, 0x05, 0x07, 0x03, 0x01});
    CheckValue(Conf(83, "ASN1:IA5STRING:abc"), 83, 0, {0x16, 0x03, 'a', 'b', 'c'});
    CheckValue(Conf(83, "ASN1:UTF8String:abc"), 83, 0, {0x0c, 0x03, 'a', 'b', 'c'});
    CheckValue(Conf(83, "ASN1:OCTETSTRING:abc"), 83, 0, {0x04, 0x03, 'a', 'b', 'c'});
    CheckValue(Conf(83, "ASN1:FORMAT:HEX,OCTETSTRING:0102"), 83, 0, {0x04, 0x02, 0x01, 0x02});
    CheckValue(Conf(83, "ASN1:FORMAT:BITLIST,BITSTRING:0"), 83, 0, {0x03, 0x02, 0x07, 0x80});
    CheckValue(Conf(83, "critical,ASN1:INTEGER:5"), 83, 1, {0x02, 0x01, 0x05});
}

void TestMalformed() {
    Require(Conf(83, nullptr) == nullptr);
    Require(Conf(83, "DER:0") == nullptr);
    Require(Conf(83, "DER:0G") == nullptr);
    Require(Conf(83, "DER:0:0") == nullptr);
    Require(Conf(83, "ASN1:") == nullptr);
    Require(Conf(83, "ASN1:INTEGER:xx") == nullptr);
    Require(Conf(83, "ASN1:INTEGER:") == nullptr);
    Require(Conf(83, "ASN1:NOPE:1") == nullptr);
    Require(Conf(83, "ASN1:NULL:x") == nullptr);
    Require(Conf(83, "ASN1:BOOLEAN:maybe") == nullptr);
    Require(Conf(83, "ASN1:FORMAT:HEX,INTEGER:5") == nullptr);
    Require(Conf(83, "ASN1:NULL,INTEGER:5") == nullptr);
    Require(Conf(83, "ASN1:NULL ") == nullptr);
    Require(allocations.empty());
}

void TestUnknownNid() {
    Require(Conf(0, "DER:0102") == nullptr);
    Require(Conf(0, "ASN1:INTEGER:5") == nullptr);
    Require(Conf(123456, "DER:0102") == nullptr);
    Require(Conf(123456, "ASN1:INTEGER:5") == nullptr);
    Require(Conf(-1, "DER:0102") == nullptr);
    Require(allocations.empty());
}

void TestPlain() {
    CheckValue(Conf(83, "digitalSignature"), 83, 0, {0x03, 0x02, 0x07, 0x80});
    CheckValue(Conf(83, "Digital Signature"), 83, 0, {0x03, 0x02, 0x07, 0x80});
    CheckValue(Conf(83, "keyCertSign,cRLSign"), 83, 0, {0x03, 0x02, 0x01, 0x06});
    CheckValue(Conf(83, "decipherOnly"), 83, 0, {0x03, 0x03, 0x07, 0x00, 0x80});
    CheckValue(Conf(71, "client,server"), 71, 0, {0x03, 0x02, 0x06, 0xc0});
    CheckValue(Conf(71, "SSL Client"), 71, 0, {0x03, 0x02, 0x07, 0x80});
    CheckValue(Conf(126, "serverAuth"), 126, 0,
        {0x30, 0x0a, 0x06, 0x08, 0x2b, 0x06, 0x01, 0x05, 0x05, 0x07, 0x03, 0x01});
    CheckValue(Conf(126, "serverAuth,clientAuth"), 126, 0,
        {0x30, 0x14, 0x06, 0x08, 0x2b, 0x06, 0x01, 0x05, 0x05, 0x07, 0x03, 0x01,
            0x06, 0x08, 0x2b, 0x06, 0x01, 0x05, 0x05, 0x07, 0x03, 0x02});
    CheckValue(Conf(83, "critical,digitalSignature"), 83, 1, {0x03, 0x02, 0x07, 0x80});
}

void TestPlainFailures() {
    Require(Conf(83, "") == nullptr);
    Require(Conf(83, "bogus") == nullptr);
    Require(Conf(83, "digitalSignature,") == nullptr);
    Require(Conf(126, "serverAuth,,clientAuth") == nullptr);
    Require(Conf(13, "commonName") == nullptr);
    Require(Conf(83, "@section") == nullptr);
    RequireNotImplemented([] { Conf(83, "@section", reinterpret_cast<void*>(1)); });
    RequireNotImplemented([] { Conf(87, "CA:TRUE"); });
    RequireNotImplemented([] { Conf(82, "0102"); });
    Require(allocations.empty());
}

void TestAllocationFailures() {
    allocationsUntilFailure = 0;
    Require(Conf(83, "DER:0102") == nullptr);
    Require(allocations.empty());
    allocationsUntilFailure = 1;
    Require(Conf(83, "DER:0102") == nullptr);
    Require(allocations.empty());
    allocationsUntilFailure = 2;
    Require(Conf(83, "DER:0102") == nullptr);
    Require(allocations.empty());
    allocationsUntilFailure = -1;
    CheckValue(Conf(83, "DER:0102"), 83, 0, {0x01, 0x02});
}

}

int main() {
    std::array<void*, 7> api{
        reinterpret_cast<void*>(Allocate_nid_no_patch), reinterpret_cast<void*>(Free_nid_no_patch),
        reinterpret_cast<void*>(Callocate_nid_no_patch), reinterpret_cast<void*>(Reallocate_nid_no_patch),
        reinterpret_cast<void*>(UnexpectedAlign_nid_no_patch), reinterpret_cast<void*>(UnexpectedRealign_nid_no_patch),
        reinterpret_cast<void*>(UnexpectedPosixAlign_nid_no_patch)};
    ApplicationHeapRegister_nid_no_patch(api.data());
    TestDer();
    TestAsn1();
    TestMalformed();
    TestUnknownNid();
    TestPlain();
    TestPlainFailures();
    TestAllocationFailures();
    Require(allocations.empty());
    std::puts("libSceLibreSsl3 X509V3_EXT_conf_nid tests passed");
}
