#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceLibreSsl3/include/Api.hpp"
#include "prx/libSceLibreSsl/include/Api.hpp"
#include "prx/libSceLibreSsl/include/Error.hpp"

#include <array>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <span>
#include <source_location>

namespace {

void Require(bool value, std::source_location location = std::source_location::current()) {
    if (value) return;
    std::fprintf(stderr, "OBJ_txt2obj check failed at line %u\n", location.line());
    std::abort();
}

bool SameData(const Asn1Object_nid_no_patch* object, std::span<const unsigned char> expected) {
    if (!object || object->length != static_cast<int>(expected.size()) || !object->data) return false;
    return std::memcmp(object->data, expected.data(), expected.size()) == 0;
}

void CheckNumeric() {
    constexpr std::array<unsigned char, 9> sha256WithRsa{
        0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01, 0x0b};
    ERR_clear_error_nid_postfix();
    auto* object = OBJ_txt2obj_nid_postfix("1.2.840.113549.1.1.11", 1);
    Require(object != nullptr);
    Require(SameData(object, sha256WithRsa));
    Require(object->nid == 0);
    Require(object->flags == 0x09);
    Require(OBJ_obj2nid_nid_postfix(reinterpret_cast<const LibreSslAsn1Object*>(object)) == 668);
    Require(ERR_get_error_nid_postfix() == 0);
    ASN1_OBJECT_free_nid_postfix(object);

    constexpr std::array<unsigned char, 2> small{0x2a, 0x03};
    auto* shortObject = OBJ_txt2obj_nid_postfix("1.2.3", 1);
    Require(SameData(shortObject, small));
    ASN1_OBJECT_free_nid_postfix(shortObject);

    constexpr std::array<unsigned char, 2> zeroPrefix{0x00, 0x01};
    auto* zeroPrefixObject = OBJ_txt2obj_nid_postfix("0.0.1", 1);
    Require(SameData(zeroPrefixObject, zeroPrefix));
    ASN1_OBJECT_free_nid_postfix(zeroPrefixObject);

    constexpr std::array<unsigned char, 2> boundarySecond{0x4f, 0x01};
    auto* boundarySecondObject = OBJ_txt2obj_nid_postfix("1.39.1", 1);
    Require(SameData(boundarySecondObject, boundarySecond));
    ASN1_OBJECT_free_nid_postfix(boundarySecondObject);

    constexpr std::array<unsigned char, 3> secondArc{0x88, 0x37, 0x03};
    auto* secondArcObject = OBJ_txt2obj_nid_postfix("2.999.3", 1);
    Require(SameData(secondArcObject, secondArc));
    ASN1_OBJECT_free_nid_postfix(secondArcObject);

    constexpr std::array<unsigned char, 9> rsaEncryption{
        0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01, 0x01};
    auto* rsa = OBJ_txt2obj_nid_postfix("1.2.840.113549.1.1.1", 0);
    Require(SameData(rsa, rsaEncryption));
    Require(OBJ_obj2nid_nid_postfix(reinterpret_cast<const LibreSslAsn1Object*>(rsa)) == 6);
    ASN1_OBJECT_free_nid_postfix(rsa);
}

void CheckNames() {
    ERR_clear_error_nid_postfix();
    auto* shortNamed = OBJ_txt2obj_nid_postfix("sha256WithRSAEncryption", 0);
    Require(shortNamed != nullptr);
    Require(shortNamed->nid == 668);
    Require(SameData(shortNamed, std::array<unsigned char, 9>{
        0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01, 0x0b}));
    Require(ERR_get_error_nid_postfix() == 0);
    ASN1_OBJECT_free_nid_postfix(shortNamed);

    auto* longNamed = OBJ_txt2obj_nid_postfix("ACME Identifier", 0);
    Require(longNamed != nullptr);
    Require(longNamed->nid == 1053);
    ASN1_OBJECT_free_nid_postfix(longNamed);

    auto* exactOnly = OBJ_txt2obj_nid_postfix("ACME Identifier", 1);
    Require(exactOnly == nullptr);
    ERR_clear_error_nid_postfix();
}

void CheckInvalid() {
    for (const char* text : {
             "", "1", "1.", ".1.2", "1..2", "3.2", "1.2.x", "1.2.3 ",
             " 1.2.3", "0.40", "1.40", "0.99.1", "1.999.2", "00.1.2", "1.00.2"}) {
        ERR_clear_error_nid_postfix();
        Require(OBJ_txt2obj_nid_postfix(text, 1) == nullptr);
        Require(ERR_get_error_nid_postfix() != 0);
    }
    ERR_clear_error_nid_postfix();
    Require(OBJ_txt2obj_nid_postfix("0.40", 0) == nullptr);
    Require(ERR_get_error_nid_postfix() != 0);
    ERR_clear_error_nid_postfix();
    Require(OBJ_txt2obj_nid_postfix(nullptr, 1) == nullptr);
}

} // namespace

namespace {

void* APS5_VABI HeapAllocate(std::size_t bytes) { return std::malloc(bytes); }
void APS5_VABI HeapFree(void* pointer) { std::free(pointer); }
void* APS5_VABI HeapCallocate(std::size_t count, std::size_t bytes) { return std::calloc(count, bytes); }
void* APS5_VABI HeapReallocate(void* pointer, std::size_t bytes) { return std::realloc(pointer, bytes); }
void* APS5_VABI UnexpectedAlign(std::size_t, std::size_t) { std::abort(); }
void* APS5_VABI UnexpectedRealign(void*, std::size_t, std::size_t) { std::abort(); }
int APS5_VABI UnexpectedPosixAlign(void**, std::size_t, std::size_t) { std::abort(); }

std::array<void*, 10> HeapApi() {
    return {reinterpret_cast<void*>(HeapAllocate), reinterpret_cast<void*>(HeapFree),
        reinterpret_cast<void*>(HeapCallocate), reinterpret_cast<void*>(HeapReallocate),
        reinterpret_cast<void*>(UnexpectedAlign), reinterpret_cast<void*>(UnexpectedRealign),
        reinterpret_cast<void*>(UnexpectedPosixAlign)};
}

}

int main() {
    std::array<void*, 10> heapApi = HeapApi();
    ApplicationHeapRegister_nid_no_patch(heapApi.data());
    CheckNumeric();
    CheckNames();
    CheckInvalid();
    std::puts("libSceLibreSsl3 OBJ_txt2obj tests passed");
}
