#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"

#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <source_location>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {

using Bytes = std::vector<unsigned char>;

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "LibreSSL3 AES_unwrap_key check failed at line %u\n", location.line());
    std::abort();
}

template<typename TAction> void RequireInvalidArgument(TAction action, std::source_location location = std::source_location::current()) {
    try {
        action();
    } catch (const std::invalid_argument&) {
        return;
    }
    Require(false, location);
}

Bytes Hex(std::string_view text) {
    Bytes bytes;
    for (std::size_t i = 0; i + 1 < text.size(); i += 2) bytes.push_back(static_cast<unsigned char>(std::stoi(std::string(text.substr(i, 2)), nullptr, 16)));
    return bytes;
}

Bytes Sequence(std::size_t length) {
    Bytes bytes(length);
    for (std::size_t i = 0; i < length; ++i) bytes[i] = static_cast<unsigned char>(i);
    return bytes;
}

AesKey DecryptKey(const Bytes& kek) {
    AesKey key {};
    Require(AES_set_decrypt_key_nid_postfix(kek.data(), static_cast<int>(kek.size() * 8), &key) == 0);
    return key;
}

int Unwrap(AesKey& key, const unsigned char* iv, Bytes& out, const Bytes& in) {
    return AES_unwrap_key_nid_postfix(&key, iv, out.data(), in.data(), static_cast<unsigned int>(in.size()));
}

bool Filled(const unsigned char* data, std::size_t length, unsigned char value) {
    for (std::size_t i = 0; i < length; ++i)
        if (data[i] != value) return false;
    return true;
}

const Bytes Data16 = Hex("00112233445566778899AABBCCDDEEFF");
const Bytes Data24 = Hex("00112233445566778899AABBCCDDEEFF0001020304050607");
const Bytes Data32 = Hex("00112233445566778899AABBCCDDEEFF000102030405060708090A0B0C0D0E0F");
const Bytes Rfc41 = Hex("1FA68B0A8112B447AEF34BD8FB5A7B829D3E862371D2CFE5");
const unsigned char DefaultIv[8] = {0xA6, 0xA6, 0xA6, 0xA6, 0xA6, 0xA6, 0xA6, 0xA6};

void CheckUnwrap(const Bytes& kek, const Bytes& wrapped, const Bytes& expected, const unsigned char* iv = nullptr) {
    AesKey key = DecryptKey(kek);
    Bytes out(wrapped.size(), 0xCC);
    Require(Unwrap(key, iv, out, wrapped) == static_cast<int>(expected.size()));
    Require(std::memcmp(out.data(), expected.data(), expected.size()) == 0);
    Require(Filled(out.data() + expected.size(), 8, 0xCC));
}

void TestRfc3394Vectors() {
    CheckUnwrap(Sequence(16), Rfc41, Data16);
    CheckUnwrap(Sequence(24), Hex("96778B25AE6CA435F92B5B97C050AED2468AB8A17AD84E5D"), Data16);
    CheckUnwrap(Sequence(32), Hex("64E8C3F9CE0F5BA263E9777905818A2A93C8191E7D6E8AE7"), Data16);
    CheckUnwrap(Sequence(24), Hex("031D33264E15D33268F24EC260743EDCE1C6C7DDEE725A936BA814915C6762D2"), Data24);
    CheckUnwrap(Sequence(32), Hex("A8F9BC1612C68B3FF6E6F4FBE30E71E4769C8B80A32CB8958CD5D17D6B254DA1"), Data24);
    CheckUnwrap(Sequence(32), Hex("28C9F404C4B810F4CBCCB35CFB87F8263F5786E2D80ED326CBC7F0E71A99F43BFB988B9B7A02DD21"), Data32);
    CheckUnwrap(Sequence(16), Rfc41, Data16, DefaultIv);
}

void TestCounterAboveOneByte() {
    Bytes expected(48 * 8);
    for (std::size_t i = 0; i < expected.size(); ++i) expected[i] = static_cast<unsigned char>(i * 7 + 3);
    CheckUnwrap(Sequence(16), Hex("2BB9AAF56EFAC1A2BB4F72257806BA025E0863ABCCADE9EBC33FE4C3726A8447D16CC8E87533790ECD7F446274F98FAF59F5BEA317681CC6C510159B19942C9CE35F83305711A45989CE169251182E878D5852D9CAD57744726449AF9D020EB70B1BED7DEB3BD731403CA12DA74D1B184F7F43BFCD7D111F8A1C4A4236B3927BDCA09A72FF73B036E3503C84BF07BF1B4125A19AE1BC88C55DAC1E3546E5B31D643C7854EB4A5A325C837DE74041102704DE6123A037E5DB9B3DBB8FD1936F4727AD51110989AD7C05EFB42B8BBAB7D953E866AD5FEA5C319FE1CB3BE8A847106C6B37E4ECF892E8D721203989F068187D29D19D756102CEEAB2AE3BBB01CDF958DBC220E915589F69F8CF1078A3829AEE9EFD7A13F526B709D1C822D134DA08D87956F98EA3B8E525855C417E778A23362CE3EA52B0F6C6AE8B375F8925FDF86C4DBCEFB62F475BF32B4BBD39B8AE644E48775F1E583A1E23709B17CC235E9D1BF95863818DF2F7C159A67DEE0A4591974C4B2DB3025981C7612DEE2589E8CD0E913278FC64E9F9"), expected);
}

void RequireRejected(AesKey& key, const unsigned char* iv, const Bytes& wrapped) {
    Bytes out(wrapped.size(), 0xCC);
    Require(Unwrap(key, iv, out, wrapped) == 0);
    Require(Filled(out.data(), wrapped.size() - 8, 0));
    Require(Filled(out.data() + wrapped.size() - 8, 8, 0xCC));
}

void TestExplicitIv() {
    const unsigned char iv[8] = {0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF};
    const Bytes wrapped = Hex("A0F76F4B09E1F2191B8D94DA2CA57ADFD45EE9732992A98F");
    CheckUnwrap(Sequence(16), wrapped, Data16, iv);
    AesKey key = DecryptKey(Sequence(16));
    RequireRejected(key, nullptr, wrapped);
    RequireRejected(key, iv, Rfc41);
}

void TestIntegrityFailure() {
    AesKey key = DecryptKey(Sequence(16));
    for (std::size_t index : {std::size_t {0}, std::size_t {7}, std::size_t {8}, std::size_t {23}}) {
        Bytes corrupted = Rfc41;
        corrupted[index] ^= 0x01;
        RequireRejected(key, nullptr, corrupted);
    }
    AesKey wrongKey = DecryptKey(Sequence(24));
    RequireRejected(wrongKey, nullptr, Rfc41);
}

void TestInvalidLength() {
    AesKey key = DecryptKey(Sequence(16));
    for (unsigned int length : {0u, 8u, 16u, 17u, 20u, 23u, 25u, 28u, 31u}) {
        Bytes in(length > 24 ? length : 24, 0x5A);
        Bytes out(in.size(), 0xCC);
        Require(AES_unwrap_key_nid_postfix(&key, nullptr, out.data(), in.data(), length) == -1);
        Require(Filled(out.data(), out.size(), 0xCC));
        Require(AES_unwrap_key_nid_postfix(nullptr, nullptr, nullptr, nullptr, length) == -1);
    }
}

void TestInPlace() {
    AesKey key = DecryptKey(Sequence(16));
    Bytes buffer = Rfc41;
    Require(AES_unwrap_key_nid_postfix(&key, nullptr, buffer.data(), buffer.data(), static_cast<unsigned int>(buffer.size())) == 16);
    Require(std::memcmp(buffer.data(), Data16.data(), Data16.size()) == 0);
}

void TestNullArguments() {
    AesKey key = DecryptKey(Sequence(16));
    Bytes out(Rfc41.size(), 0xCC);
    RequireInvalidArgument([&] { AES_unwrap_key_nid_postfix(nullptr, nullptr, out.data(), Rfc41.data(), 24); });
    RequireInvalidArgument([&] { AES_unwrap_key_nid_postfix(&key, nullptr, nullptr, Rfc41.data(), 24); });
    RequireInvalidArgument([&] { AES_unwrap_key_nid_postfix(&key, nullptr, out.data(), nullptr, 24); });
    Require(Filled(out.data(), out.size(), 0xCC));
}

}

int main() {
    TestRfc3394Vectors();
    TestCounterAboveOneByte();
    TestExplicitIv();
    TestIntegrityFailure();
    TestInvalidLength();
    TestInPlace();
    TestNullArguments();
    std::puts("LibreSSL3 AES_unwrap_key tests passed");
}
