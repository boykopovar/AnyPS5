#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include <array>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <source_location>
namespace {
void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "LibreSSL3 OBJ_obj2txt check failed at line %u\n", location.line());
    std::abort();
}
}

int main() {
    const std::array<unsigned char, 3> keyUsage{0x55, 0x1d, 0x0f};
    Asn1Object_nid_no_patch object{};
    object.longName = "X509v3 Key Usage";
    object.length = 3;
    object.data = keyUsage.data();
    std::array<char, 32> buffer{};
    Require(OBJ_obj2txt_nid_postfix(buffer.data(), buffer.size(), &object, 1) == 9);
    Require(std::strcmp(buffer.data(), "2.5.29.15") == 0);
    Require(OBJ_obj2txt_nid_postfix(buffer.data(), buffer.size(), &object, 0) == 16);
    Require(std::strcmp(buffer.data(), "X509v3 Key Usage") == 0);
    std::memset(buffer.data(), 'x', buffer.size());
    Require(OBJ_obj2txt_nid_postfix(buffer.data(), 5, &object, 1) == 9);
    Require(std::strcmp(buffer.data(), "2.5.") == 0);
    Require(OBJ_obj2txt_nid_postfix(nullptr, 0, &object, 1) == 9);

    const std::array<unsigned char, 1> zero{0x00};
    object.longName = nullptr;
    object.length = 1;
    object.data = zero.data();
    Require(OBJ_obj2txt_nid_postfix(buffer.data(), buffer.size(), &object, 1) == 3);
    Require(std::strcmp(buffer.data(), "0.0") == 0);
    object.length = 0;
    object.data = nullptr;
    Require(OBJ_obj2txt_nid_postfix(buffer.data(), buffer.size(), &object, 1) == 0);

    const std::array<unsigned char, 2> truncatedArc{0x55, 0x81};
    object.length = 2;
    object.data = truncatedArc.data();
    Require(OBJ_obj2txt_nid_postfix(buffer.data(), buffer.size(), &object, 1) == 0);

    object.shortName = "rsaEncryption";
    object.longName = nullptr;
    object.length = 9;
    const std::array<unsigned char, 9> rsaOid{0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01, 0x01};
    object.data = rsaOid.data();
    Require(OBJ_obj2txt_nid_postfix(buffer.data(), buffer.size(), &object, 0) == 13);
    Require(std::strcmp(buffer.data(), "rsaEncryption") == 0);

    std::memset(buffer.data(), 'x', buffer.size());
    Require(OBJ_obj2txt_nid_postfix(buffer.data(), buffer.size(), nullptr, 1) == 0);
    Require(buffer[0] == '\0');
    object.longName = "rsaEncryption";
    object.length = 9;
    object.data = nullptr;
    std::memset(buffer.data(), 'x', buffer.size());
    Require(OBJ_obj2txt_nid_postfix(buffer.data(), buffer.size(), &object, 0) == 0);
    Require(buffer[0] == '\0');
    object.length = -1;
    object.data = zero.data();
    Require(OBJ_obj2txt_nid_postfix(buffer.data(), buffer.size(), &object, 1) == 0);
    std::puts("libSceLibreSsl3 OBJ_obj2txt tests passed");
}
