#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include "prx/libSceLibreSsl/include/Ec.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <source_location>
#include <string>
#include <vector>

namespace {

constexpr int Secp256r1 = 415;
constexpr int Secp384r1 = 715;
constexpr std::uint64_t PassedNullParameter = 0x10fff043;
constexpr std::uint64_t EcLibraryFailure = 0x10fff010;
constexpr std::uint64_t D2iFailure = 0x10fff075;
constexpr std::uint64_t GroupByNameFailure = 0x10fff077;
constexpr std::uint64_t ToGroupFailure = 0x10fff07f;
constexpr std::uint64_t UnknownGroup = 0x10fff081;
constexpr std::uint64_t Nested = 0x0dfff03a;
constexpr std::uint64_t BadHeader = 0x0dfff066;
constexpr std::uint64_t NoMatchingChoice = 0x0dfff08f;
constexpr std::uint64_t NullWrongLength = 0x0dfff090;
constexpr std::uint64_t SequenceNotConstructed = 0x0dfff095;
constexpr std::uint64_t TooLong = 0x0dfff09b;
constexpr std::uint64_t InvalidObject = 0x0dfff0d8;
constexpr std::uint64_t NotPrimitive = 0x0dfff0e7;

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "requirement failed at line %u\n", static_cast<unsigned>(location.line()));
    std::abort();
}

std::vector<unsigned char> Hex(const char* text) {
    std::vector<unsigned char> bytes;
    for (std::size_t index = 0; text[index] != '\0'; index += 2) {
        const char pair[3] = {text[index], text[index + 1], '\0'};
        bytes.push_back(static_cast<unsigned char>(std::strtoul(pair, nullptr, 16)));
    }
    return bytes;
}

void RequireErrors(std::initializer_list<std::uint64_t> expected, std::source_location location = std::source_location::current()) {
    for (const std::uint64_t error : expected) Require(ERR_get_error_nid_postfix() == error, location);
    Require(ERR_get_error_nid_postfix() == 0, location);
}

struct Curve {
    int nid;
    int degree;
    const char* der;
};

constexpr Curve Curves[] = {
    {409, 192, "06082a8648ce3d030101"},
    {415, 256, "06082a8648ce3d030107"},
    {711, 192, "06052b8104001f"},
    {712, 224, "06052b81040020"},
    {713, 224, "06052b81040021"},
    {714, 256, "06052b8104000a"},
    {715, 384, "06052b81040022"},
    {716, 521, "06052b81040023"},
    {928, 256, "06092b2403030208010107"},
    {932, 384, "06092b240303020801010b"},
    {934, 512, "06092b240303020801010d"},
};

const std::vector<unsigned char> P256 = Hex("06082a8648ce3d030107");
const std::vector<unsigned char> P384 = Hex("06052b81040022");
const std::vector<unsigned char> P256Point = Hex(
    "0460fed4ba255a9d31c961eb74c6356d68c049b8923b61fa6ce669622e60f29fb6"
    "7903fe1008b8bc99a41ae9e95628bc64f2f1b20c2d7e9f5177a3c294d4462299");

void RequireGroup(const EcKey* key, int nid) {
    Require(key != nullptr && key->group != nullptr);
    Require(key->group->curve.nid == nid);
    Require(key->group->asn1Flag == 1);
}

void SignsAndVerifies(EcKey* key) {
    Require(EC_KEY_generate_key_nid_postfix(key) == 1);
    Require(EC_KEY_check_key_nid_postfix(key) == 1);
    unsigned char digest[32];
    for (std::size_t index = 0; index < sizeof(digest); ++index) digest[index] = static_cast<unsigned char>(index * 7 + 1);
    std::vector<unsigned char> signature(static_cast<std::size_t>(ECDSA_size_nid_postfix(key)));
    unsigned int length = 0;
    Require(ECDSA_sign_nid_postfix(0, digest, static_cast<int>(sizeof(digest)), signature.data(), &length, key) == 1);
    Require(ECDSA_verify_nid_postfix(0, digest, static_cast<int>(sizeof(digest)), signature.data(), static_cast<int>(length), key) == 1);
    digest[0] ^= 1;
    Require(ECDSA_verify_nid_postfix(0, digest, static_cast<int>(sizeof(digest)), signature.data(), static_cast<int>(length), key) == 0);
}

void DecodesEachNamedCurve() {
    for (const Curve& curve : Curves) {
        ERR_clear_error_nid_postfix();
        const std::vector<unsigned char> der = Hex(curve.der);
        std::vector<unsigned char> buffer = der;
        buffer.push_back(0x05);
        buffer.push_back(0x00);
        const unsigned char* cursor = buffer.data();
        EcKey* key = d2i_ECParameters_nid_postfix(nullptr, &cursor, static_cast<std::int64_t>(buffer.size()));
        RequireGroup(key, curve.nid);
        Require(cursor == buffer.data() + der.size());
        Require(EC_GROUP_get_degree_nid_postfix(key->group) == curve.degree);
        Require(key->publicKey == nullptr && key->privateKey == nullptr);
        RequireErrors({});
        SignsAndVerifies(key);
        EC_KEY_free_nid_postfix(key);
    }
}

void ReadsBerHeaders() {
    for (const char* text : {"1f06082a8648ce3d030107", "0681082a8648ce3d030107", "068200082a8648ce3d030107"}) {
        ERR_clear_error_nid_postfix();
        const std::vector<unsigned char> der = Hex(text);
        const unsigned char* cursor = der.data();
        EcKey* key = d2i_ECParameters_nid_postfix(nullptr, &cursor, static_cast<std::int64_t>(der.size()));
        RequireGroup(key, Secp256r1);
        Require(cursor == der.data() + der.size());
        RequireErrors({});
        EC_KEY_free_nid_postfix(key);
    }
}

void FillsOrReusesTheCallersKey() {
    ERR_clear_error_nid_postfix();
    EcKey* filled = nullptr;
    const unsigned char* cursor = P256.data();
    EcKey* result = d2i_ECParameters_nid_postfix(&filled, &cursor, static_cast<std::int64_t>(P256.size()));
    Require(result != nullptr && filled == result);
    RequireGroup(filled, Secp256r1);
    EC_KEY_free_nid_postfix(filled);

    EcKey* key = EC_KEY_new_by_curve_name_nid_postfix(Secp384r1);
    Require(key != nullptr);
    EcKey* const original = key;
    cursor = P256.data();
    Require(d2i_ECParameters_nid_postfix(&key, &cursor, static_cast<std::int64_t>(P256.size())) == original);
    Require(key == original);
    RequireGroup(key, Secp256r1);
    cursor = P256Point.data();
    Require(o2i_ECPublicKey_nid_postfix(&key, &cursor, static_cast<std::int64_t>(P256Point.size())) == key);
    Require(EC_KEY_check_key_nid_postfix(key) == 1);
    RequireErrors({});
    EC_KEY_free_nid_postfix(key);

    key = EC_KEY_new_nid_postfix();
    Require(key != nullptr && key->group == nullptr);
    cursor = P384.data();
    Require(d2i_ECParameters_nid_postfix(&key, &cursor, static_cast<std::int64_t>(P384.size())) == key);
    RequireGroup(key, Secp384r1);
    SignsAndVerifies(key);
    RequireErrors({});
    EC_KEY_free_nid_postfix(key);
}

void Fails(const std::vector<unsigned char>& der, std::int64_t size, std::size_t consumed, std::initializer_list<std::uint64_t> errors, std::source_location location) {
    const unsigned char* const start = der.empty() ? P256.data() : der.data();
    ERR_clear_error_nid_postfix();
    const unsigned char* cursor = start;
    Require(d2i_ECParameters_nid_postfix(nullptr, &cursor, size) == nullptr, location);
    Require(cursor == start + consumed, location);
    RequireErrors(errors, location);

    EcKey* none = nullptr;
    cursor = start;
    Require(d2i_ECParameters_nid_postfix(&none, &cursor, size) == nullptr, location);
    Require(none == nullptr && cursor == start + consumed, location);
    RequireErrors(errors, location);

    EcKey* key = EC_KEY_new_by_curve_name_nid_postfix(Secp384r1);
    Require(key != nullptr, location);
    EcKey* const original = key;
    const EcGroup* const group = key->group;
    cursor = start;
    Require(d2i_ECParameters_nid_postfix(&key, &cursor, size) == nullptr, location);
    Require(key == original && key->group == group && key->group->curve.nid == Secp384r1, location);
    Require(cursor == start + consumed, location);
    RequireErrors(errors, location);
    SignsAndVerifies(key);
    EC_KEY_free_nid_postfix(key);
}

void Fails(const char* text, std::size_t consumed, std::initializer_list<std::uint64_t> errors, std::source_location location = std::source_location::current()) {
    const std::vector<unsigned char> der = Hex(text);
    Fails(der, static_cast<std::int64_t>(der.size()), consumed, errors, location);
}

void RejectsCurvesItCannotName() {
    Fails("06032a0304", 5, {GroupByNameFailure, ToGroupFailure, EcLibraryFailure});
    Fails("0609608648016503040201", 11, {UnknownGroup, GroupByNameFailure, ToGroupFailure, EcLibraryFailure});
    Fails("06052b81040001", 7, {UnknownGroup, GroupByNameFailure, ToGroupFailure, EcLibraryFailure});
    Fails("0500", 2, {ToGroupFailure, EcLibraryFailure});
    Fails("058100", 3, {ToGroupFailure, EcLibraryFailure});
}

void RejectsMalformedDer() {
    const std::initializer_list<std::uint64_t> badHeader = {BadHeader, Nested, Nested, Nested, D2iFailure, EcLibraryFailure};
    const std::initializer_list<std::uint64_t> tooLong = {TooLong, Nested, Nested, Nested, D2iFailure, EcLibraryFailure};
    Fails("", 0, badHeader);
    Fails("1f", 0, badHeader);
    Fails("1f86", 0, badHeader);
    Fails("06", 0, badHeader);
    Fails("068000", 0, badHeader);
    Fails("06ff", 0, badHeader);
    Fails((std::string("06ff") + std::string(254, '0')).c_str(), 0, badHeader);
    Fails("0684000000", 0, badHeader);
    Fails("30", 0, badHeader);
    Fails("06082a8648", 0, tooLong);
    Fails("300500", 0, tooLong);
    Fails("0201000500", 0, {NoMatchingChoice, D2iFailure, EcLibraryFailure});
    Fails("a0030500", 0, {NoMatchingChoice, D2iFailure, EcLibraryFailure});
    Fails("86012a", 0, {NoMatchingChoice, D2iFailure, EcLibraryFailure});
    Fails("2600", 0, {NotPrimitive, Nested, Nested, D2iFailure, EcLibraryFailure});
    Fails("26800000", 0, {NotPrimitive, Nested, Nested, D2iFailure, EcLibraryFailure});
    Fails("2500", 0, {NotPrimitive, Nested, Nested, D2iFailure, EcLibraryFailure});
    Fails("0600", 0, {InvalidObject, Nested, Nested, D2iFailure, EcLibraryFailure});
    Fails("06028001", 0, {InvalidObject, Nested, Nested, D2iFailure, EcLibraryFailure});
    Fails("060181", 0, {InvalidObject, Nested, Nested, D2iFailure, EcLibraryFailure});
    Fails("050100", 0, {NullWrongLength, Nested, Nested, D2iFailure, EcLibraryFailure});
    Fails("1000", 0, {SequenceNotConstructed, Nested, Nested, D2iFailure, EcLibraryFailure});
    Fails(P256, -1, 0, {D2iFailure, EcLibraryFailure}, std::source_location::current());
}

void RejectsANullInput() {
    ERR_clear_error_nid_postfix();
    Require(d2i_ECParameters_nid_postfix(nullptr, nullptr, 10) == nullptr);
    RequireErrors({PassedNullParameter});
    EcKey* key = EC_KEY_new_by_curve_name_nid_postfix(Secp384r1);
    Require(key != nullptr);
    EcKey* const original = key;
    const unsigned char* cursor = nullptr;
    Require(d2i_ECParameters_nid_postfix(&key, &cursor, 10) == nullptr);
    Require(key == original && key->group->curve.nid == Secp384r1 && cursor == nullptr);
    RequireErrors({PassedNullParameter});
    EC_KEY_free_nid_postfix(key);
}

}

int main() {
    DecodesEachNamedCurve();
    ReadsBerHeaders();
    FillsOrReusesTheCallersKey();
    RejectsCurvesItCannotName();
    RejectsMalformedDer();
    RejectsANullInput();
    std::puts("libSceLibreSsl3 d2i_ECParameters tests passed");
}
