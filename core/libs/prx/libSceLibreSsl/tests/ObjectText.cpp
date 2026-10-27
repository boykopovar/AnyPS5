#include "prx/libSceLibreSsl/libSceLibreSsl.hpp"
#include "prx/libSceLibreSsl/include/ObjectTable.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <initializer_list>
#include <string>
#include <vector>

namespace {

constexpr std::uint64_t UnknownNidError = (8ull << 24) | (0xfffull << 12) | 101;

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "failed: %s\n", message);
    std::abort();
}

template<typename TAction>
void RequireThrows(TAction action, const char* message) {
    try {
        action();
    } catch (const std::exception&) {
        return;
    }
    Require(false, message);
}

struct Encoded {
    std::vector<unsigned char> bytes;
    LibreSslAsn1Object object;

    Encoded(std::initializer_list<unsigned char> data, int nid = 0) : bytes(data), object{nullptr, nullptr, nid, static_cast<int>(bytes.size()), bytes.data(), 0} {}
};

void RequireText(const LibreSslAsn1Object& object, int space, int expected, const char* text, const char* message) {
    char buffer[128];
    std::memset(buffer, 0x5a, sizeof(buffer));
    Require(i2t_ASN1_OBJECT_nid_postfix(buffer, space, &object) == expected, message);
    Require(std::strcmp(buffer, text) == 0, message);
    Require(static_cast<unsigned char>(buffer[std::strlen(text) + 1]) == 0x5a || std::strlen(text) + 1 >= static_cast<std::size_t>(space),
        "nothing is written after the terminator");
}

void Table() {
    const auto& undefined = LibreSslObjects_nid_no_patch[0];
    Require(std::strcmp(undefined.shortName, "UNDEF") == 0 && std::strcmp(undefined.longName, "undefined") == 0 && !undefined.data, "NID 0");
    const auto& commonName = LibreSslObjects_nid_no_patch[13];
    Require(commonName.nid == 13 && std::strcmp(commonName.shortName, "CN") == 0 && std::strcmp(commonName.longName, "commonName") == 0
        && commonName.length == 3 && std::memcmp(commonName.data, "\x55\x04\x03", 3) == 0 && commonName.flags == 0, "commonName");
    const auto& last = LibreSslObjects_nid_no_patch[LibreSslObjectCount - 1];
    Require(last.nid == 955 && std::strcmp(last.shortName, "Ed448ph") == 0, "the last 2.5.4 NID");
    for (int nid : {102, 118, 350, 511, 772, 774, 775}) {
        const auto& gap = LibreSslObjects_nid_no_patch[nid];
        Require(!gap.shortName && !gap.longName && gap.nid == 0 && gap.length == 0 && !gap.data, "unassigned NIDs are empty");
    }
    for (int index = 0; index < LibreSslObjectOrderCount; ++index)
        Require(LibreSslObjectOrder_nid_no_patch[index] < LibreSslObjectCount, "the search order indexes the table");
}

void Lookup() {
    Require(LibreSslObjectNid_nid_no_patch(nullptr) == 0, "null object");
    Encoded commonName{0x55, 0x04, 0x03};
    Require(LibreSslObjectNid_nid_no_patch(&commonName.object) == 13, "an encoding found by search");
    Encoded preset{0x55, 0x04, 0x03, 0x01}, unknown{0x2a, 0x03, 0x04};
    preset.object.nid = 6;
    Require(LibreSslObjectNid_nid_no_patch(&preset.object) == 6, "a set NID wins without a search");
    Require(LibreSslObjectNid_nid_no_patch(&unknown.object) == 0, "an unknown encoding");
    Encoded organization{0x2b};
    Require(LibreSslObjectNid_nid_no_patch(&organization.object) == 379, "1.3 is listed twice; the search lands on ORG as in LibreSSL");
    unsigned char none = 0;
    LibreSslAsn1Object empty{nullptr, nullptr, 0, 0, &none, 0};
    Require(LibreSslObjectNid_nid_no_patch(&empty) == 404, "an empty encoding lands on NID_ccitt");
}

void Names() {
    Encoded commonName{0x55, 0x04, 0x03};
    RequireText(commonName.object, 64, 10, "commonName", "the long name");
    RequireText(commonName.object, 5, 10, "comm", "a short buffer truncates and returns the full length");
    RequireText(commonName.object, 1, 10, "", "a one-byte buffer holds the terminator");
    Require(i2t_ASN1_OBJECT_nid_postfix(nullptr, 0, &commonName.object) == 10, "a zero-length buffer is not written");
    Encoded named{0x2a, 0x03, 0x04}, sha256{0x60, 0x86, 0x48, 0x01, 0x65, 0x03, 0x04, 0x02, 0x01};
    named.object.nid = 13;
    RequireText(named.object, 64, 10, "commonName", "a set NID names the object whatever its encoding");
    RequireText(sha256.object, 64, 6, "sha256", "a long name equal to the short name");
}

void Numbers() {
    Encoded unknown{0x2a, 0x03, 0x04};
    RequireText(unknown.object, 64, 7, "1.2.3.4", "an unknown OID is printed in dotted form");
    RequireText(unknown.object, 4, 7, "1.2", "a dotted OID is truncated part by part");
    RequireText(unknown.object, 2, 7, "1", "a two-byte buffer keeps the first arc");
    RequireText(unknown.object, 1, 7, "", "a one-byte buffer keeps nothing");
    Encoded second{0x88, 0x37, 0x05};
    RequireText(second.object, 64, 7, "2.999.5", "a first subidentifier of 80 or more starts with 2");
    Encoded large{0x2a, 0x82, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x00};
    RequireText(large.object, 64, 24, "1.2.18446744073709551616", "an arc past 64 bits is printed exactly");
    Encoded largeFirst{0x82, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x00};
    RequireText(largeFirst.object, 64, 22, "2.18446744073709551536", "a first subidentifier past 64 bits loses 80");
    Encoded truncated{0x2a, 0x03, 0x83};
    RequireText(truncated.object, 64, 0, "1.2.3", "a truncated subidentifier returns 0 and keeps what was written");
    Encoded high{0x2a, 0x05};
    high.object.nid = 5000;
    RequireText(high.object, 64, 5, "1.2.5", "a NID past the table prints the encoding");
    Require(ERR_get_error_nid_postfix() == 0, "a NID past the table reports no error");
}

void Errors() {
    ERR_clear_error_nid_postfix();
    Encoded gap{0x2a, 0x07};
    gap.object.nid = 102;
    RequireText(gap.object, 64, 5, "1.2.7", "an unassigned NID prints the encoding");
    Require(ERR_get_error_nid_postfix() == UnknownNidError && ERR_get_error_nid_postfix() == UnknownNidError
        && ERR_get_error_nid_postfix() == 0, "the long and short name lookups each report OBJ_R_UNKNOWN_NID");
    LibreSslAsn1Object none{nullptr, nullptr, 13, 3, nullptr, 0};
    RequireText(none, 64, 0, "", "an object without data");
    Require(i2t_ASN1_OBJECT_nid_postfix(nullptr, 0, nullptr) == 0, "a null object");
    char buffer[8];
    RequireThrows([&] { i2t_ASN1_OBJECT_nid_postfix(buffer, -1, &gap.object); }, "a negative length");
    RequireThrows([&] { i2t_ASN1_OBJECT_nid_postfix(nullptr, 8, &gap.object); }, "a null buffer with a length");
}

} // namespace

int main() {
    std::array<void*, 10> api{};
    ApplicationHeapRegister_nid_no_patch(api.data());
    Table();
    Lookup();
    Names();
    Numbers();
    Errors();
}
