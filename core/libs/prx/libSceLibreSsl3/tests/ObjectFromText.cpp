#include "prx/libSceLibreSsl3/include/Api.hpp"
#include "prx/libSceLibreSsl/include/Api.hpp"
#include "prx/libSceLibreSsl/include/Error.hpp"

#include <array>
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
    for (const char* text : {"", "1", "1.", ".1.2", "1..2", "3.2", "1.2.x", "1.2.3 "}) {
        ERR_clear_error_nid_postfix();
        Require(OBJ_txt2obj_nid_postfix(text, 1) == nullptr);
        Require(ERR_get_error_nid_postfix() != 0);
    }
    ERR_clear_error_nid_postfix();
    Require(OBJ_txt2obj_nid_postfix(nullptr, 1) == nullptr);
}

} // namespace

int main() {
    CheckNumeric();
    CheckNames();
    CheckInvalid();
    std::puts("libSceLibreSsl3 OBJ_txt2obj tests passed");
}
