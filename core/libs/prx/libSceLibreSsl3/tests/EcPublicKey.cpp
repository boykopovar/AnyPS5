#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include "prx/libSceLibreSsl/include/Ec.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <source_location>
#include <stdexcept>
#include <vector>

namespace {

constexpr int Secp256r1 = 415;
constexpr int Secp384r1 = 715;
constexpr std::uint64_t PassedNullParameter = 0x10fff043;
constexpr std::uint64_t EcLibraryFailure = 0x10fff010;

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

const std::vector<unsigned char> P256Uncompressed = Hex(
    "0460fed4ba255a9d31c961eb74c6356d68c049b8923b61fa6ce669622e60f29fb6"
    "7903fe1008b8bc99a41ae9e95628bc64f2f1b20c2d7e9f5177a3c294d4462299");
const std::vector<unsigned char> P256Compressed = Hex("0360fed4ba255a9d31c961eb74c6356d68c049b8923b61fa6ce669622e60f29fb6");
const std::vector<unsigned char> P384Uncompressed = Hex(
    "04ec3a4e415b4e19a4568618029f427fa5da9a8bc4ae92e02e06aae5286b300c64def8f0ea9055866064a254515480bc13"
    "8015d9b72d7d57244ea8ef9ac0c621896708a59367f9dfb9f54ca84b3f1c9db1288b231c3ae0d4fe7344fd2533264720");
const std::vector<unsigned char> P384Compressed = Hex("02ec3a4e415b4e19a4568618029f427fa5da9a8bc4ae92e02e06aae5286b300c64def8f0ea9055866064a254515480bc13");

std::vector<unsigned char> Encoded(const EcKey* key) {
    const EcGroup* group = EC_KEY_get0_group_nid_postfix(key);
    const EcPoint* point = EC_KEY_get0_public_key_nid_postfix(key);
    Require(group != nullptr && point != nullptr);
    std::vector<unsigned char> out(1 + 2 * 66);
    const std::size_t size = EC_POINT_point2oct_nid_postfix(group, point, 4, out.data(), out.size(), nullptr);
    Require(size != 0);
    out.resize(size);
    return out;
}

void Decode(int nid, const std::vector<unsigned char>& input, int form, const std::vector<unsigned char>& expected) {
    ERR_clear_error_nid_postfix();
    EcKey* key = EC_KEY_new_by_curve_name_nid_postfix(nid);
    Require(key != nullptr && key->publicKey == nullptr);
    std::vector<unsigned char> buffer = input;
    buffer.push_back(0xa5);
    const unsigned char* cursor = buffer.data();
    Require(o2i_ECPublicKey_nid_postfix(&key, &cursor, static_cast<std::int64_t>(input.size())) == key);
    Require(cursor == buffer.data() + input.size());
    Require(key->conversionForm == form);
    Require(Encoded(key) == expected);
    Require(EC_KEY_check_key_nid_postfix(key) == 1);
    Require(ERR_get_error_nid_postfix() == 0);
    EC_KEY_free_nid_postfix(key);
}

void DecodesEachForm() {
    std::vector<unsigned char> hybrid = P256Uncompressed;
    hybrid[0] = 0x07;
    Decode(Secp256r1, P256Uncompressed, 4, P256Uncompressed);
    Decode(Secp256r1, P256Compressed, 2, P256Uncompressed);
    Decode(Secp256r1, hybrid, 6, P256Uncompressed);
    Decode(Secp384r1, P384Uncompressed, 4, P384Uncompressed);
    Decode(Secp384r1, P384Compressed, 2, P384Uncompressed);
}

void ReusesTheExistingPoint() {
    ERR_clear_error_nid_postfix();
    EcKey* key = EC_KEY_new_by_curve_name_nid_postfix(Secp256r1);
    Require(key != nullptr);
    const unsigned char* cursor = P256Compressed.data();
    Require(o2i_ECPublicKey_nid_postfix(&key, &cursor, static_cast<std::int64_t>(P256Compressed.size())) == key);
    const EcPoint* point = key->publicKey;
    const unsigned char infinity = 0;
    cursor = &infinity;
    Require(o2i_ECPublicKey_nid_postfix(&key, &cursor, 1) == key);
    Require(cursor == &infinity + 1);
    Require(key->publicKey == point);
    Require(key->conversionForm == 0);
    unsigned char octet = 0xff;
    Require(EC_POINT_point2oct_nid_postfix(key->group, point, 4, &octet, 1, nullptr) == 1 && octet == 0);
    Require(ERR_get_error_nid_postfix() == 0);
    EC_KEY_free_nid_postfix(key);
}

void RejectsAMissingKeyOrGroup() {
    ERR_clear_error_nid_postfix();
    const unsigned char* cursor = P256Uncompressed.data();
    Require(o2i_ECPublicKey_nid_postfix(nullptr, &cursor, 65) == nullptr);
    Require(ERR_get_error_nid_postfix() == PassedNullParameter);
    EcKey* none = nullptr;
    Require(o2i_ECPublicKey_nid_postfix(&none, &cursor, 65) == nullptr);
    Require(none == nullptr);
    Require(ERR_get_error_nid_postfix() == PassedNullParameter);
    EcKey* key = EC_KEY_new_nid_postfix();
    Require(key != nullptr);
    Require(o2i_ECPublicKey_nid_postfix(&key, &cursor, 65) == nullptr);
    Require(key->publicKey == nullptr && key->conversionForm == 4);
    Require(ERR_get_error_nid_postfix() == PassedNullParameter);
    Require(ERR_get_error_nid_postfix() == 0);
    Require(cursor == P256Uncompressed.data());
    EC_KEY_free_nid_postfix(key);
}

void Rejects(EcKey* key, const std::vector<unsigned char>& input, std::int64_t length, const std::vector<unsigned char>& point) {
    ERR_clear_error_nid_postfix();
    const unsigned char* cursor = input.data();
    Require(o2i_ECPublicKey_nid_postfix(&key, &cursor, length) == nullptr);
    Require(cursor == input.data());
    Require(key->conversionForm == 2);
    Require(ERR_get_error_nid_postfix() == EcLibraryFailure);
    Require(ERR_get_error_nid_postfix() == 0);
    Require(Encoded(key) == point);
}

void RejectsBadEncodings() {
    EcKey* key = EC_KEY_new_by_curve_name_nid_postfix(Secp256r1);
    Require(key != nullptr);
    const unsigned char* cursor = P256Compressed.data();
    Require(o2i_ECPublicKey_nid_postfix(&key, &cursor, static_cast<std::int64_t>(P256Compressed.size())) == key);
    const EcPoint* point = key->publicKey;
    std::vector<unsigned char> badForm = P256Uncompressed;
    badForm[0] = 0x05;
    std::vector<unsigned char> hybridParity = P256Uncompressed;
    hybridParity[0] = 0x06;
    std::vector<unsigned char> xAboveField = P256Uncompressed;
    std::memset(xAboveField.data() + 1, 0xff, 32);
    const std::vector<unsigned char> compressedOffCurve = Hex("0260fed4ba255a9d31c961eb74c6356d68c049b8923b61fa6ce669622e60f29fb7");
    Rejects(key, badForm, 65, P256Uncompressed);
    Rejects(key, hybridParity, 65, P256Uncompressed);
    Rejects(key, xAboveField, 65, P256Uncompressed);
    Rejects(key, compressedOffCurve, 33, P256Uncompressed);
    Rejects(key, P256Uncompressed, 64, P256Uncompressed);
    Rejects(key, P256Uncompressed, 0, P256Uncompressed);
    Rejects(key, P384Uncompressed, static_cast<std::int64_t>(P384Uncompressed.size()), P256Uncompressed);
    std::vector<unsigned char> offCurve = P256Uncompressed;
    offCurve.back() ^= 1;
    Rejects(key, offCurve, 65, offCurve);
    std::vector<unsigned char> hybridOffCurve = P256Uncompressed;
    hybridOffCurve[0] = 0x07;
    hybridOffCurve.back() ^= 2;
    std::vector<unsigned char> storedHybrid = hybridOffCurve;
    storedHybrid[0] = 0x04;
    Rejects(key, hybridOffCurve, 65, storedHybrid);
    Require(key->publicKey == point);
    EC_KEY_free_nid_postfix(key);
    key = EC_KEY_new_by_curve_name_nid_postfix(Secp256r1);
    Require(key != nullptr);
    key->conversionForm = 2;
    Rejects(key, offCurve, 65, offCurve);
    EC_KEY_free_nid_postfix(key);
}

void RejectsANullInput() {
    EcKey* key = EC_KEY_new_by_curve_name_nid_postfix(Secp256r1);
    Require(key != nullptr);
    bool threw = false;
    try {
        o2i_ECPublicKey_nid_postfix(&key, nullptr, 65);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);
    Require(key->publicKey == nullptr);
    EC_KEY_free_nid_postfix(key);
}

}

int main() {
    DecodesEachForm();
    ReusesTheExistingPoint();
    RejectsAMissingKeyOrGroup();
    RejectsBadEncodings();
    RejectsANullInput();
    std::puts("libSceLibreSsl3 o2i_ECPublicKey tests passed");
}
