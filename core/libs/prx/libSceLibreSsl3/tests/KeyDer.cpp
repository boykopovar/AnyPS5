#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include "prx/libSceLibreSsl3/include/PkeyState.hpp"
#include "prx/libSceLibreSsl/include/Api.hpp"
#include "prx/libSceLibreSsl/include/Ec.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <source_location>
#include <vector>

namespace {

constexpr int RsaType = 6;
constexpr int EcType = 408;
constexpr int Secp256r1 = 415;
constexpr int Secp384r1 = 715;
constexpr int PointConversionUncompressed = 4;

constexpr int ErrLibEvp = 6;
constexpr int ErrLibX509 = 11;
constexpr int ErrLibAsn1 = 13;

constexpr int ErrRNestedAsn1Error = 58;
constexpr int Asn1RBadObjectHeader = 102;
constexpr int Asn1RNotEnoughData = 142;
constexpr int Asn1RSequenceLengthMismatch = 148;
constexpr int Asn1RTooLong = 155;
constexpr int Asn1RWrongTag = 168;
constexpr int EvpRUnsupportedPrivateKeyAlgorithm = 118;
constexpr int EvpRUnsupportedAlgorithm = 156;
constexpr int X509RUnsupportedAlgorithm = 111;

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

const std::vector<unsigned char> RsaSpkiDer = Hex(
    "30820122300d06092a864886f70d01010105000382010f003082010a0282010100b059e3d97fed73cbe97f78b51b0601b6c30fb63fe9"
    "d9c20853bd9668b9e5d0dd697ed72631eb0e13c8eb6de9881efed8286c2d312aeb28f1a1b10616828185bd352dd78da0173adfb85477"
    "df5b3f3529c56c19fd66f99231b5f019dc9bc5311dc0ed2ff3d51d2bdc3c0e3518bf60b38903d0ecd148332d81dc5e6a88823dab5b0c"
    "882ba20cb6b4e71eb66651ae759f757c31069cfdb8481680bc20d2dc5c9d2c01a6c41c01a8d4d236479cfbe19f83c17009bd9542c031"
    "ae066c6529ca7d71ca31f873701f43725b8d5bd074f436cfcf34e290e8bd4f63f25116360a5dd70a72edf7e0dcfdcf511eccfd8aac46"
    "7e32f3713ce96546857841c2652850d94962650203010001");

const std::vector<unsigned char> RsaPkcs1PubDer = Hex(
    "3082010a0282010100b059e3d97fed73cbe97f78b51b0601b6c30fb63fe9d9c20853bd9668b9e5d0dd697ed72631eb0e13c8eb6de988"
    "1efed8286c2d312aeb28f1a1b10616828185bd352dd78da0173adfb85477df5b3f3529c56c19fd66f99231b5f019dc9bc5311dc0ed2ff"
    "3d51d2bdc3c0e3518bf60b38903d0ecd148332d81dc5e6a88823dab5b0c882ba20cb6b4e71eb66651ae759f757c31069cfdb8481680b"
    "c20d2dc5c9d2c01a6c41c01a8d4d236479cfbe19f83c17009bd9542c031ae066c6529ca7d71ca31f873701f43725b8d5bd074f436cfcf"
    "34e290e8bd4f63f25116360a5dd70a72edf7e0dcfdcf511eccfd8aac467e32f3713ce96546857841c2652850d94962650203010001");

const std::vector<unsigned char> RsaPkcs8Der = Hex(
    "308204bd020100300d06092a864886f70d0101010500048204a7308204a30201000282010100b059e3d97fed73cbe97f78b51b0601b6"
    "c30fb63fe9d9c20853bd9668b9e5d0dd697ed72631eb0e13c8eb6de9881efed8286c2d312aeb28f1a1b10616828185bd352dd78da0173"
    "adfb85477df5b3f3529c56c19fd66f99231b5f019dc9bc5311dc0ed2ff3d51d2bdc3c0e3518bf60b38903d0ecd148332d81dc5e6a888"
    "23dab5b0c882ba20cb6b4e71eb66651ae759f757c31069cfdb8481680bc20d2dc5c9d2c01a6c41c01a8d4d236479cfbe19f83c17009bd"
    "9542c031ae066c6529ca7d71ca31f873701f43725b8d5bd074f436cfcf34e290e8bd4f63f25116360a5dd70a72edf7e0dcfdcf511eccf"
    "d8aac467e32f3713ce96546857841c2652850d9496265020301000102820100172abfe9874a48ef8d4daf2c4f9002f2a0fe73e9428dd6"
    "5524b71fc6c3412326ab20bc1feae7dfb20eaf8b01665df809aa89c7881184d21ca82aaacb933a73b8ceff26359745e5f894ed17d0525"
    "ae3df886d729e9d0405461d8a1cc3a3fd0c3e45e582eaabe108ad8680314e7e79b42c38419e92c1c2e3640f70a1db2c0cf0f12d9c8590"
    "71905d37602f1a2913685c7033593a1e421db62009c59be7bb999c16a79224a9966151c9e3e800b8c67b7c10dc93050a305b2dc3838c41"
    "afa0ec018840ea33d8dbe18dbbbbc7d94fb0fc05f878dce95584f10925c31a641a41abfc6f7cbe9794b9e4a664e2c80646765121afd86"
    "a0786f6641e5dc746837b6478b2d102818100e5f9b0d7770c58633082e55b1e47b62d09a98b6d32d31c6c1498df050bebd61404795c86e"
    "e653a7366a5b0f67278b09327964aa3aa29ce788397a97b7053572278681c7e58bf43594bfc5819c04971427e682197a6e127f6da4591"
    "02621cb639ef183be4036a7e655ce073c442a110c0ffae986e4d4a4bd6836ebe4140d264fd02818100c44eb9348e0f23729184e45881c"
    "659a9beccf5027ff33686baa5c593d8b2526fe133ffaecb5e5befb3b3357518acfc6991d31a0f92550e9bbe83e0b80adf87f779807f87"
    "c3cc589956200ef22816230569152358cf77a4852ff3f2c86341a9ae3fa5e7b0029ab195873856ecdfbbc570cf132fe29f6688a4d2a17"
    "8431149e3890281804b04721d36e63e06cd1245c2bd53e32739d9a6738e7ab2f0c69f3f50a41ce00e538f4b1170be709ca845303e9404a"
    "e84b20b2279d4ce225dc97f3577e0a22ef03d9aa880c66e4d87d9738d418929ebda26e5ccdf6d3e510d0312624506c01ae937070eb248"
    "32fadc11b9ddc98d7a80be9428ddec4d26092b06609f8b259bfbf9028181009b042f6c38e1eb74054593abf08ed92f19e46f75d1e4152"
    "2bf31389c4f7a416ee7bf50208fa7aba1d59da691193340ff7c8fdefc7ce97de7aec2252a302be40aa07fc0354d8088564d947338f63ec"
    "4d8f8ea2047794d59357370883f8561ccc434aea60e6f8008cbb343e2209b6c83b55950257827b7a81fbfd4924b23d3dd410281807bbd"
    "8ddcc1a90f2bf7c9e95abeade78684cc76a29d75bbd83d43b4f1f519c0e0c3a380678a2e8b7bd0abd107f9e0d1812d0ed3c8fc457c684"
    "c6ba98043638e941e8e76bd52f85292902cc2291c289de9c8c1bc8b50c3a2812c85acbb0a360cef9719aad5a0f76926d60e98fb05d26c"
    "ee7cf610fc6398d3a2be2b864427791599");

const std::vector<unsigned char> P256SpkiDer = Hex(
    "3059301306072a8648ce3d020106082a8648ce3d030107034200042e5af3d5f0aa64e34ea5379e970d618dbaf910fb101fa1b087340d"
    "f7c9b046a4649333ae32cba4b12aedaecef6a450ec6f2c6807b7c20a497bec0d55a6075410");

const std::vector<unsigned char> P256Sec1Der = Hex(
    "30770201010420ce96c1c5d6e395b707a11c8da8536c8003ca8079691f08c9d690deeb67f54867a00a06082a8648ce3d030107a1440342"
    "00042e5af3d5f0aa64e34ea5379e970d618dbaf910fb101fa1b087340df7c9b046a4649333ae32cba4b12aedaecef6a450ec6f2c6807"
    "b7c20a497bec0d55a6075410");

const std::vector<unsigned char> P256Pkcs8Der = Hex(
    "308187020100301306072a8648ce3d020106082a8648ce3d030107046d306b0201010420ce96c1c5d6e395b707a11c8da8536c8003ca80"
    "79691f08c9d690deeb67f54867a144034200042e5af3d5f0aa64e34ea5379e970d618dbaf910fb101fa1b087340df7c9b046a4649333ae"
    "32cba4b12aedaecef6a450ec6f2c6807b7c20a497bec0d55a6075410");

const std::vector<unsigned char> P384Pkcs8Der = Hex(
    "3081b6020100301006072a8648ce3d020106052b8104002204819e30819b02010104306e518f934d0aff63bd695e6dd06c73bc5a8f78"
    "0f8903c22ed479b916944e9add1b9d91d5317e077e38a82cca7d26e775a16403620004fea682de112d46d5b0fb886f995b2d2595d623"
    "2f1a6a9291585bf9feacc96dd831b6879dae519df72c30c9a24b63c4b0dc00672e322b837ee6d40268bca6c8e91a10b79cce0d02429d"
    "36e885907807c1cc9384517b5caed5c9a08f54232823d1");

void CompareBignums(const void* lhs, const void* rhs) {
    Require(lhs != nullptr && rhs != nullptr);
    const auto* left = static_cast<const Bignum*>(lhs);
    const auto* right = static_cast<const Bignum*>(rhs);
    Require(BN_is_negative_nid_postfix(left) == BN_is_negative_nid_postfix(right));
    const int leftBits = BN_num_bits_nid_postfix(left);
    const int rightBits = BN_num_bits_nid_postfix(right);
    Require(leftBits >= 0 && leftBits == rightBits);
    const std::size_t byteCount = static_cast<std::size_t>((leftBits + 7) / 8);
    std::vector<unsigned char> leftBytes(byteCount);
    std::vector<unsigned char> rightBytes(byteCount);
    if (byteCount > 0) {
        Require(BN_bn2bin_nid_postfix(left, leftBytes.data()) == static_cast<int>(byteCount));
        Require(BN_bn2bin_nid_postfix(right, rightBytes.data()) == static_cast<int>(byteCount));
    }
    Require(leftBytes == rightBytes);
}

void CompareRsaKeys(const LibreSsl3Rsa* lhs, const LibreSsl3Rsa* rhs, bool checkPrivate) {
    const void* ln = nullptr;
    const void* le = nullptr;
    const void* ld = nullptr;
    const void* rn = nullptr;
    const void* re = nullptr;
    const void* rd = nullptr;
    RSA_get0_key_nid_postfix(lhs, &ln, &le, &ld);
    RSA_get0_key_nid_postfix(rhs, &rn, &re, &rd);
    CompareBignums(ln, rn);
    CompareBignums(le, re);
    if (!checkPrivate) return;
    CompareBignums(ld, rd);
    const void* lp = nullptr;
    const void* lq = nullptr;
    const void* rp = nullptr;
    const void* rq = nullptr;
    RSA_get0_factors_nid_postfix(lhs, &lp, &lq);
    RSA_get0_factors_nid_postfix(rhs, &rp, &rq);
    CompareBignums(lp, rp);
    CompareBignums(lq, rq);
    const void* ldmp1 = nullptr;
    const void* ldmq1 = nullptr;
    const void* liqmp = nullptr;
    const void* rdmp1 = nullptr;
    const void* rdmq1 = nullptr;
    const void* riqmp = nullptr;
    RSA_get0_crt_params_nid_postfix(lhs, &ldmp1, &ldmq1, &liqmp);
    RSA_get0_crt_params_nid_postfix(rhs, &rdmp1, &rdmq1, &riqmp);
    CompareBignums(ldmp1, rdmp1);
    CompareBignums(ldmq1, rdmq1);
    CompareBignums(liqmp, riqmp);
}

std::vector<unsigned char> EcPublicBytes(const EcKey* ec) {
    const EcGroup* group = EC_KEY_get0_group_nid_postfix(ec);
    const EcPoint* point = EC_KEY_get0_public_key_nid_postfix(ec);
    Require(group != nullptr && point != nullptr);
    std::vector<unsigned char> out(1 + 2 * 66);
    const std::size_t size = EC_POINT_point2oct_nid_postfix(
        group, point, PointConversionUncompressed, out.data(), out.size(), nullptr);
    Require(size != 0);
    out.resize(size);
    return out;
}

void CompareEcKeys(const EcKey* lhs, const EcKey* rhs, bool checkPrivate) {
    Require(lhs != nullptr && rhs != nullptr);
    Require(lhs->group != nullptr && rhs->group != nullptr);
    Require(lhs->group->curve.nid == rhs->group->curve.nid);
    Require(EcPublicBytes(lhs) == EcPublicBytes(rhs));
    if (!checkPrivate) return;
    Require(lhs->privateKey != nullptr && rhs->privateKey != nullptr);
    Require(mbedtls_mpi_cmp_mpi(&lhs->privateKey->value, &rhs->privateKey->value) == 0);
}

void ExpectError(int expectedLib, int expectedReason,
    std::source_location location = std::source_location::current()) {
    const std::uint64_t code = ERR_get_error_nid_postfix();
    const int lib = static_cast<int>((code >> 24) & 0xff);
    const int reason = static_cast<int>(code & 0xfff);
    Require(lib == expectedLib && reason == expectedReason, location);
}

void TestRsaPublicKeys() {
    std::vector<unsigned char> spkiWithTrailing = RsaSpkiDer;
    spkiWithTrailing.push_back(0xaa);
    spkiWithTrailing.push_back(0xbb);
    const unsigned char* cursor = spkiWithTrailing.data();
    LibreSsl3EvpPkey* evpPub = d2i_PUBKEY_nid_postfix(
        nullptr, &cursor, static_cast<std::int64_t>(spkiWithTrailing.size()));
    Require(evpPub != nullptr);
    Require(evpPub->type == RsaType);
    Require(evpPub->method != nullptr);
    Require(cursor == spkiWithTrailing.data() + RsaSpkiDer.size());

    const unsigned char* privCursor = RsaPkcs8Der.data();
    LibreSsl3Pkcs8PrivKeyInfo* p8 = d2i_PKCS8_PRIV_KEY_INFO_nid_postfix(
        nullptr, &privCursor, static_cast<std::int64_t>(RsaPkcs8Der.size()));
    Require(p8 != nullptr);
    LibreSsl3EvpPkey* evpPriv = EVP_PKCS82PKEY_nid_postfix(p8);
    Require(evpPriv != nullptr && evpPriv->type == RsaType);
    CompareRsaKeys(
        static_cast<const LibreSsl3Rsa*>(evpPub->key),
        static_cast<const LibreSsl3Rsa*>(evpPriv->key),
        false);

    PKCS8_PRIV_KEY_INFO_free_nid_postfix(p8);
    EVP_PKEY_free_nid_disambig1_nid_postfix(evpPriv);
    EVP_PKEY_free_nid_disambig1_nid_postfix(evpPub);
}

void TestRsaPrivateRoundTrip() {
    std::vector<unsigned char> pkcs8WithTrailing = RsaPkcs8Der;
    pkcs8WithTrailing.push_back(0x11);
    const unsigned char* cursor = pkcs8WithTrailing.data();
    LibreSsl3Pkcs8PrivKeyInfo* p8 = d2i_PKCS8_PRIV_KEY_INFO_nid_postfix(
        nullptr, &cursor, static_cast<std::int64_t>(pkcs8WithTrailing.size()));
    Require(p8 != nullptr);
    Require(cursor == pkcs8WithTrailing.data() + RsaPkcs8Der.size());

    LibreSsl3EvpPkey* pkey = EVP_PKCS82PKEY_nid_postfix(p8);
    Require(pkey != nullptr && pkey->type == RsaType);

    LibreSsl::Bio* pkcs8Bio = BIO_new_mem_buf_nid_postfix(
        RsaPkcs8Der.data(), static_cast<int>(RsaPkcs8Der.size()));
    Require(pkcs8Bio != nullptr);
    LibreSsl3Pkcs8PrivKeyInfo* p8Bio = d2i_PKCS8_PRIV_KEY_INFO_bio_nid_postfix(pkcs8Bio, nullptr);
    Require(p8Bio != nullptr);
    BIO_free_nid_postfix(pkcs8Bio);

    LibreSsl::Bio* privBio = BIO_new_mem_buf_nid_postfix(
        RsaPkcs8Der.data(), static_cast<int>(RsaPkcs8Der.size()));
    Require(privBio != nullptr);
    LibreSsl3EvpPkey* pkeyBio = d2i_PrivateKey_bio_nid_postfix(privBio, nullptr);
    Require(pkeyBio != nullptr && pkeyBio->type == RsaType);
    BIO_free_nid_postfix(privBio);
    CompareRsaKeys(
        static_cast<const LibreSsl3Rsa*>(pkey->key), static_cast<const LibreSsl3Rsa*>(pkeyBio->key), true);

    LibreSsl3Pkcs8PrivKeyInfo* roundP8 = EVP_PKEY2PKCS8_nid_postfix(pkey);
    Require(roundP8 != nullptr);
    const int sizeOnly = i2d_PKCS8_PRIV_KEY_INFO_nid_postfix(roundP8, nullptr);
    Require(sizeOnly > 0);

    unsigned char* allocated = nullptr;
    const int allocLen = i2d_PKCS8_PRIV_KEY_INFO_nid_postfix(roundP8, &allocated);
    Require(allocLen == sizeOnly && allocated != nullptr);

    std::vector<unsigned char> manual(static_cast<std::size_t>(sizeOnly));
    unsigned char* writePtr = manual.data();
    const int writeLen = i2d_PKCS8_PRIV_KEY_INFO_nid_postfix(roundP8, &writePtr);
    Require(writeLen == sizeOnly && writePtr == manual.data() + sizeOnly);
    Require(std::memcmp(allocated, manual.data(), static_cast<std::size_t>(sizeOnly)) == 0);

    const unsigned char* roundCursor = allocated;
    LibreSsl3Pkcs8PrivKeyInfo* decodedRound = d2i_PKCS8_PRIV_KEY_INFO_nid_postfix(
        nullptr, &roundCursor, allocLen);
    Require(decodedRound != nullptr && roundCursor == allocated + allocLen);
    LibreSsl3EvpPkey* roundPkey = EVP_PKCS82PKEY_nid_postfix(decodedRound);
    Require(roundPkey != nullptr && roundPkey->type == RsaType);
    CompareRsaKeys(
        static_cast<const LibreSsl3Rsa*>(pkey->key), static_cast<const LibreSsl3Rsa*>(roundPkey->key), true);

    free_nid_postfix(allocated);
    PKCS8_PRIV_KEY_INFO_free_nid_postfix(decodedRound);
    PKCS8_PRIV_KEY_INFO_free_nid_postfix(roundP8);
    PKCS8_PRIV_KEY_INFO_free_nid_postfix(p8Bio);
    PKCS8_PRIV_KEY_INFO_free_nid_postfix(p8);
    EVP_PKEY_free_nid_disambig1_nid_postfix(roundPkey);
    EVP_PKEY_free_nid_disambig1_nid_postfix(pkeyBio);
    EVP_PKEY_free_nid_disambig1_nid_postfix(pkey);
}

void TestEcPublicAndPrivate(int expectedCurve, const std::vector<unsigned char>& pkcs8Der,
    const std::vector<unsigned char>* spkiDer, const std::vector<unsigned char>* sec1Der) {
    if (spkiDer) {
        std::vector<unsigned char> spkiTrailing = *spkiDer;
        spkiTrailing.push_back(0x5a);
        const unsigned char* cursor = spkiTrailing.data();
        LibreSsl3EvpPkey* pub = d2i_PUBKEY_nid_postfix(
            nullptr, &cursor, static_cast<std::int64_t>(spkiTrailing.size()));
        Require(pub != nullptr && pub->type == EcType);
        Require(cursor == spkiTrailing.data() + spkiDer->size());
        const auto* ecPub = static_cast<const EcKey*>(pub->key);
        Require(ecPub != nullptr && ecPub->group != nullptr && ecPub->group->curve.nid == expectedCurve);
        EVP_PKEY_free_nid_disambig1_nid_postfix(pub);
    }

    const unsigned char* cursor = pkcs8Der.data();
    LibreSsl3Pkcs8PrivKeyInfo* p8 = d2i_PKCS8_PRIV_KEY_INFO_nid_postfix(
        nullptr, &cursor, static_cast<std::int64_t>(pkcs8Der.size()));
    Require(p8 != nullptr && cursor == pkcs8Der.data() + pkcs8Der.size());
    LibreSsl3EvpPkey* pkey = EVP_PKCS82PKEY_nid_postfix(p8);
    Require(pkey != nullptr && pkey->type == EcType);
    const auto* ec = static_cast<const EcKey*>(pkey->key);
    Require(ec != nullptr && ec->group != nullptr && ec->group->curve.nid == expectedCurve);
    Require(EC_KEY_check_key_nid_postfix(ec) == 1);

    LibreSsl::Bio* bio = BIO_new_mem_buf_nid_postfix(pkcs8Der.data(), static_cast<int>(pkcs8Der.size()));
    Require(bio != nullptr);
    LibreSsl3EvpPkey* pkeyBio = d2i_PrivateKey_bio_nid_postfix(bio, nullptr);
    Require(pkeyBio != nullptr && pkeyBio->type == EcType);
    BIO_free_nid_postfix(bio);
    CompareEcKeys(ec, static_cast<const EcKey*>(pkeyBio->key), true);

    if (sec1Der) {
        LibreSsl::Bio* sec1Bio = BIO_new_mem_buf_nid_postfix(sec1Der->data(), static_cast<int>(sec1Der->size()));
        Require(sec1Bio != nullptr);
        LibreSsl3EvpPkey* pkeySec1 = d2i_PrivateKey_bio_nid_postfix(sec1Bio, nullptr);
        Require(pkeySec1 != nullptr && pkeySec1->type == EcType);
        BIO_free_nid_postfix(sec1Bio);
        CompareEcKeys(ec, static_cast<const EcKey*>(pkeySec1->key), true);
        EVP_PKEY_free_nid_disambig1_nid_postfix(pkeySec1);
    }

    LibreSsl3Pkcs8PrivKeyInfo* roundP8 = EVP_PKEY2PKCS8_nid_postfix(pkey);
    Require(roundP8 != nullptr);
    unsigned char* encoded = nullptr;
    const int encodedLen = i2d_PKCS8_PRIV_KEY_INFO_nid_postfix(roundP8, &encoded);
    Require(encodedLen > 0 && encoded != nullptr);

    const unsigned char* roundCursor = encoded;
    LibreSsl3Pkcs8PrivKeyInfo* decodedRound = d2i_PKCS8_PRIV_KEY_INFO_nid_postfix(
        nullptr, &roundCursor, encodedLen);
    Require(decodedRound != nullptr && roundCursor == encoded + encodedLen);
    LibreSsl3EvpPkey* roundPkey = EVP_PKCS82PKEY_nid_postfix(decodedRound);
    Require(roundPkey != nullptr && roundPkey->type == EcType);
    CompareEcKeys(ec, static_cast<const EcKey*>(roundPkey->key), true);

    free_nid_postfix(encoded);
    PKCS8_PRIV_KEY_INFO_free_nid_postfix(decodedRound);
    PKCS8_PRIV_KEY_INFO_free_nid_postfix(roundP8);
    PKCS8_PRIV_KEY_INFO_free_nid_postfix(p8);
    EVP_PKEY_free_nid_disambig1_nid_postfix(roundPkey);
    EVP_PKEY_free_nid_disambig1_nid_postfix(pkeyBio);
    EVP_PKEY_free_nid_disambig1_nid_postfix(pkey);
}

void TestErrorsAndNulls() {
    PKCS8_PRIV_KEY_INFO_free_nid_postfix(nullptr);

    const std::array<unsigned char, 4> malformed{0x30, 0x05, 0x02, 0x01};
    const unsigned char* cursor = malformed.data();

    ERR_clear_error_nid_postfix();
    Require(d2i_PUBKEY_nid_postfix(nullptr, &cursor, static_cast<std::int64_t>(malformed.size())) == nullptr);
    Require(cursor == malformed.data());
    ExpectError(ErrLibAsn1, Asn1RTooLong);

    ERR_clear_error_nid_postfix();
    Require(d2i_PKCS8_PRIV_KEY_INFO_nid_postfix(
        nullptr, &cursor, static_cast<std::int64_t>(malformed.size())) == nullptr);
    Require(cursor == malformed.data());
    ExpectError(ErrLibAsn1, Asn1RTooLong);

    const std::array<unsigned char, 6> seqMismatch{0x30, 0x04, 0x02, 0x01, 0x00, 0x00};
    cursor = seqMismatch.data();
    ERR_clear_error_nid_postfix();
    Require(d2i_PUBKEY_nid_postfix(nullptr, &cursor, static_cast<std::int64_t>(seqMismatch.size())) == nullptr);
    ExpectError(ErrLibAsn1, Asn1RWrongTag);
    ExpectError(ErrLibAsn1, ErrRNestedAsn1Error);

    const std::vector<unsigned char> extraPkcs8Field = Hex(
        "3016020100300706032a0304050004050102030405020100");
    cursor = extraPkcs8Field.data();
    ERR_clear_error_nid_postfix();
    Require(d2i_PKCS8_PRIV_KEY_INFO_nid_postfix(
        nullptr, &cursor, static_cast<std::int64_t>(extraPkcs8Field.size())) == nullptr);
    Require(cursor == extraPkcs8Field.data());
    ExpectError(ErrLibAsn1, Asn1RSequenceLengthMismatch);
    ExpectError(ErrLibAsn1, ErrRNestedAsn1Error);

    const std::vector<unsigned char> unknownSpki = Hex(
        "3012300706032a03040500030700010203040506");
    cursor = unknownSpki.data();
    ERR_clear_error_nid_postfix();
    Require(d2i_PUBKEY_nid_postfix(nullptr, &cursor, static_cast<std::int64_t>(unknownSpki.size())) == nullptr);
    ExpectError(ErrLibEvp, EvpRUnsupportedAlgorithm);
    ExpectError(ErrLibX509, X509RUnsupportedAlgorithm);

    const std::vector<unsigned char> unknownPkcs8 = Hex(
        "3013020100300706032a0304050004050102030405");
    cursor = unknownPkcs8.data();
    ERR_clear_error_nid_postfix();
    LibreSsl3Pkcs8PrivKeyInfo* unknownP8 = d2i_PKCS8_PRIV_KEY_INFO_nid_postfix(
        nullptr, &cursor, static_cast<std::int64_t>(unknownPkcs8.size()));
    Require(unknownP8 != nullptr);
    Require(EVP_PKCS82PKEY_nid_postfix(unknownP8) == nullptr);
    ExpectError(ErrLibEvp, EvpRUnsupportedAlgorithm);
    ExpectError(ErrLibEvp, EvpRUnsupportedPrivateKeyAlgorithm);
    PKCS8_PRIV_KEY_INFO_free_nid_postfix(unknownP8);

    LibreSsl::Bio* badPrivBio = BIO_new_mem_buf_nid_postfix(malformed.data(), static_cast<int>(malformed.size()));
    Require(badPrivBio != nullptr);
    ERR_clear_error_nid_postfix();
    Require(d2i_PrivateKey_bio_nid_postfix(badPrivBio, nullptr) == nullptr);
    Require(ERR_get_error_nid_postfix() != 0);
    BIO_free_nid_postfix(badPrivBio);

    ERR_clear_error_nid_postfix();
    Require(EVP_PKCS82PKEY_nid_postfix(nullptr) == nullptr);
    ExpectError(ErrLibEvp, EvpRUnsupportedPrivateKeyAlgorithm);

    ERR_clear_error_nid_postfix();
    Require(EVP_PKEY2PKCS8_nid_postfix(nullptr) == nullptr);
    ExpectError(ErrLibEvp, EvpRUnsupportedPrivateKeyAlgorithm);
}

void TestOutputReplacementAndTruncation() {
    const unsigned char* pubCursor = RsaSpkiDer.data();
    LibreSsl3EvpPkey* existingPub = d2i_PUBKEY_nid_postfix(
        nullptr, &pubCursor, static_cast<std::int64_t>(RsaSpkiDer.size()));
    Require(existingPub != nullptr);
    const unsigned char* ecPubCursor = P256SpkiDer.data();
    LibreSsl3EvpPkey* replacedPub = d2i_PUBKEY_nid_postfix(
        &existingPub, &ecPubCursor, static_cast<std::int64_t>(P256SpkiDer.size()));
    Require(replacedPub != nullptr && replacedPub == existingPub && existingPub->type == EcType);
    EVP_PKEY_free_nid_disambig1_nid_postfix(existingPub);

    LibreSsl::Bio* rsaBio = BIO_new_mem_buf_nid_postfix(
        RsaPkcs8Der.data(), static_cast<int>(RsaPkcs8Der.size()));
    Require(rsaBio != nullptr);
    LibreSsl3EvpPkey* existingPriv = d2i_PrivateKey_bio_nid_postfix(rsaBio, nullptr);
    Require(existingPriv != nullptr);
    BIO_free_nid_postfix(rsaBio);
    LibreSsl::Bio* ecBio = BIO_new_mem_buf_nid_postfix(
        P256Pkcs8Der.data(), static_cast<int>(P256Pkcs8Der.size()));
    Require(ecBio != nullptr);
    LibreSsl3EvpPkey* replacedPriv = d2i_PrivateKey_bio_nid_postfix(ecBio, &existingPriv);
    Require(replacedPriv != nullptr && replacedPriv == existingPriv && existingPriv->type == EcType);
    BIO_free_nid_postfix(ecBio);
    EVP_PKEY_free_nid_disambig1_nid_postfix(existingPriv);

    const unsigned char* p8Cursor1 = RsaPkcs8Der.data();
    LibreSsl3Pkcs8PrivKeyInfo* existingP8 = d2i_PKCS8_PRIV_KEY_INFO_nid_postfix(
        nullptr, &p8Cursor1, static_cast<std::int64_t>(RsaPkcs8Der.size()));
    Require(existingP8 != nullptr);
    const unsigned char* p8Cursor2 = P256Pkcs8Der.data();
    LibreSsl3Pkcs8PrivKeyInfo* replacedP8 = d2i_PKCS8_PRIV_KEY_INFO_nid_postfix(
        &existingP8, &p8Cursor2, static_cast<std::int64_t>(P256Pkcs8Der.size()));
    Require(replacedP8 != nullptr && replacedP8 == existingP8);
    PKCS8_PRIV_KEY_INFO_free_nid_postfix(existingP8);

    std::vector<unsigned char> truncRsa = RsaSpkiDer;
    truncRsa.pop_back();
    const unsigned char* truncRsaCursor = truncRsa.data();
    ERR_clear_error_nid_postfix();
    Require(d2i_PUBKEY_nid_postfix(
        nullptr, &truncRsaCursor, static_cast<std::int64_t>(truncRsa.size())) == nullptr);
    Require(truncRsaCursor == truncRsa.data());
    ExpectError(ErrLibAsn1, Asn1RTooLong);

    std::vector<unsigned char> truncP256 = P256Pkcs8Der;
    truncP256.pop_back();
    const unsigned char* truncP256Cursor = truncP256.data();
    ERR_clear_error_nid_postfix();
    Require(d2i_PKCS8_PRIV_KEY_INFO_nid_postfix(
        nullptr, &truncP256Cursor, static_cast<std::int64_t>(truncP256.size())) == nullptr);
    Require(truncP256Cursor == truncP256.data());
    ExpectError(ErrLibAsn1, Asn1RTooLong);
}

void TestConsecutiveBioObjects() {
    std::vector<unsigned char> twoPkcs8 = RsaPkcs8Der;
    twoPkcs8.insert(twoPkcs8.end(), P256Pkcs8Der.begin(), P256Pkcs8Der.end());
    LibreSsl::Bio* bio = BIO_new_mem_buf_nid_postfix(twoPkcs8.data(), static_cast<int>(twoPkcs8.size()));
    Require(bio != nullptr);
    LibreSsl3Pkcs8PrivKeyInfo* firstP8 = d2i_PKCS8_PRIV_KEY_INFO_bio_nid_postfix(bio, nullptr);
    LibreSsl3Pkcs8PrivKeyInfo* secondP8 = d2i_PKCS8_PRIV_KEY_INFO_bio_nid_postfix(bio, nullptr);
    Require(firstP8 != nullptr && secondP8 != nullptr);
    LibreSsl3EvpPkey* firstKey = EVP_PKCS82PKEY_nid_postfix(firstP8);
    LibreSsl3EvpPkey* secondKey = EVP_PKCS82PKEY_nid_postfix(secondP8);
    Require(firstKey != nullptr && firstKey->type == RsaType);
    Require(secondKey != nullptr && secondKey->type == EcType);
    EVP_PKEY_free_nid_disambig1_nid_postfix(secondKey);
    EVP_PKEY_free_nid_disambig1_nid_postfix(firstKey);
    PKCS8_PRIV_KEY_INFO_free_nid_postfix(secondP8);
    PKCS8_PRIV_KEY_INFO_free_nid_postfix(firstP8);
    BIO_free_nid_postfix(bio);

    std::vector<unsigned char> twoPriv = RsaPkcs8Der;
    twoPriv.insert(twoPriv.end(), P256Sec1Der.begin(), P256Sec1Der.end());
    LibreSsl::Bio* privBio = BIO_new_mem_buf_nid_postfix(twoPriv.data(), static_cast<int>(twoPriv.size()));
    Require(privBio != nullptr);
    LibreSsl3EvpPkey* p1 = d2i_PrivateKey_bio_nid_postfix(privBio, nullptr);
    LibreSsl3EvpPkey* p2 = d2i_PrivateKey_bio_nid_postfix(privBio, nullptr);
    Require(p1 != nullptr && p1->type == RsaType);
    Require(p2 != nullptr && p2->type == EcType);
    EVP_PKEY_free_nid_disambig1_nid_postfix(p2);
    EVP_PKEY_free_nid_disambig1_nid_postfix(p1);
    BIO_free_nid_postfix(privBio);
}

void TestPkcs8AttributesRoundTrip() {
    std::vector<unsigned char> p8WithAttr = P256Pkcs8Der;
    const std::array<unsigned char, 12> attrTlv{
        0xa0, 0x0a, 0x30, 0x08, 0x06, 0x03, 0x55, 0x04, 0x03, 0x31, 0x01, 0x05};
    p8WithAttr.insert(p8WithAttr.end(), attrTlv.begin(), attrTlv.end());
    const std::size_t newLen = p8WithAttr.size() - 3;
    Require(newLen >= 128 && newLen <= 255);
    p8WithAttr[2] = static_cast<unsigned char>(newLen);

    const unsigned char* cursor = p8WithAttr.data();
    LibreSsl3Pkcs8PrivKeyInfo* info = d2i_PKCS8_PRIV_KEY_INFO_nid_postfix(
        nullptr, &cursor, static_cast<std::int64_t>(p8WithAttr.size()));
    Require(info != nullptr && cursor == p8WithAttr.data() + p8WithAttr.size());
    Require(info->attributes != nullptr);

    unsigned char* reencoded = nullptr;
    const int reencodedLen = i2d_PKCS8_PRIV_KEY_INFO_nid_postfix(info, &reencoded);
    Require(reencodedLen == static_cast<int>(p8WithAttr.size()) && reencoded != nullptr);
    Require(std::memcmp(reencoded, p8WithAttr.data(), p8WithAttr.size()) == 0);

    LibreSsl3EvpPkey* pkey = EVP_PKCS82PKEY_nid_postfix(info);
    Require(pkey != nullptr && pkey->type == EcType);
    EVP_PKEY_free_nid_disambig1_nid_postfix(pkey);
    free_nid_postfix(reencoded);
    PKCS8_PRIV_KEY_INFO_free_nid_postfix(info);
}

void TestBioMalformedAndMultiChunkObjects() {
    const std::array<unsigned char, 9> hugeLengthHeader{
        0x30, 0x84, 0x7f, 0xff, 0xff, 0x00, 0x02, 0x01, 0x00};
    LibreSsl::Bio* hugeP8Bio = BIO_new_mem_buf_nid_postfix(
        hugeLengthHeader.data(), static_cast<int>(hugeLengthHeader.size()));
    Require(hugeP8Bio != nullptr);
    ERR_clear_error_nid_postfix();
    Require(d2i_PKCS8_PRIV_KEY_INFO_bio_nid_postfix(hugeP8Bio, nullptr) == nullptr);
    ExpectError(ErrLibAsn1, Asn1RNotEnoughData);
    BIO_free_nid_postfix(hugeP8Bio);

    LibreSsl::Bio* hugePrivBio = BIO_new_mem_buf_nid_postfix(
        hugeLengthHeader.data(), static_cast<int>(hugeLengthHeader.size()));
    Require(hugePrivBio != nullptr);
    ERR_clear_error_nid_postfix();
    Require(d2i_PrivateKey_bio_nid_postfix(hugePrivBio, nullptr) == nullptr);
    ExpectError(ErrLibAsn1, Asn1RNotEnoughData);
    BIO_free_nid_postfix(hugePrivBio);

    const std::array<unsigned char, 7> indefiniteHeader{
        0x30, 0x80, 0x02, 0x01, 0x00, 0x00, 0x00};
    LibreSsl::Bio* indefP8Bio = BIO_new_mem_buf_nid_postfix(
        indefiniteHeader.data(), static_cast<int>(indefiniteHeader.size()));
    Require(indefP8Bio != nullptr);
    ERR_clear_error_nid_postfix();
    Require(d2i_PKCS8_PRIV_KEY_INFO_bio_nid_postfix(indefP8Bio, nullptr) == nullptr);
    ExpectError(ErrLibAsn1, Asn1RBadObjectHeader);
    BIO_free_nid_postfix(indefP8Bio);

    LibreSsl::Bio* indefPrivBio = BIO_new_mem_buf_nid_postfix(
        indefiniteHeader.data(), static_cast<int>(indefiniteHeader.size()));
    Require(indefPrivBio != nullptr);
    ERR_clear_error_nid_postfix();
    Require(d2i_PrivateKey_bio_nid_postfix(indefPrivBio, nullptr) == nullptr);
    ExpectError(ErrLibAsn1, Asn1RBadObjectHeader);
    BIO_free_nid_postfix(indefPrivBio);

    constexpr std::size_t payloadLen = 17000;
    auto pushTagLen16 = [](std::vector<unsigned char>& dst, unsigned char tag, std::size_t len) {
        dst.push_back(tag);
        dst.push_back(0x82);
        dst.push_back(static_cast<unsigned char>((len >> 8) & 0xff));
        dst.push_back(static_cast<unsigned char>(len & 0xff));
    };
    std::vector<unsigned char> attrBody;
    attrBody.insert(attrBody.end(), {0x06, 0x03, 0x55, 0x04, 0x03});
    pushTagLen16(attrBody, 0x31, 4 + payloadLen);
    pushTagLen16(attrBody, 0x04, payloadLen);
    attrBody.insert(attrBody.end(), payloadLen, 0x55);

    const std::size_t attrSeqLen = 4 + attrBody.size();
    const std::size_t outerBodyLen = (P256Pkcs8Der.size() - 3) + 4 + attrSeqLen;
    std::vector<unsigned char> largePkcs8;
    largePkcs8.reserve(4 + outerBodyLen);
    pushTagLen16(largePkcs8, 0x30, outerBodyLen);
    largePkcs8.insert(largePkcs8.end(), P256Pkcs8Der.begin() + 3, P256Pkcs8Der.end());
    pushTagLen16(largePkcs8, 0xa0, attrSeqLen);
    pushTagLen16(largePkcs8, 0x30, attrBody.size());
    largePkcs8.insert(largePkcs8.end(), attrBody.begin(), attrBody.end());
    Require(largePkcs8.size() > 16384);

    const unsigned char* refCursor = P256Pkcs8Der.data();
    LibreSsl3Pkcs8PrivKeyInfo* refP8 = d2i_PKCS8_PRIV_KEY_INFO_nid_postfix(
        nullptr, &refCursor, static_cast<std::int64_t>(P256Pkcs8Der.size()));
    Require(refP8 != nullptr);
    LibreSsl3EvpPkey* refKey = EVP_PKCS82PKEY_nid_postfix(refP8);
    Require(refKey != nullptr && refKey->type == EcType);

    LibreSsl::Bio* largeP8Bio = BIO_new_mem_buf_nid_postfix(
        largePkcs8.data(), static_cast<int>(largePkcs8.size()));
    Require(largeP8Bio != nullptr);
    LibreSsl3Pkcs8PrivKeyInfo* largeP8 = d2i_PKCS8_PRIV_KEY_INFO_bio_nid_postfix(largeP8Bio, nullptr);
    Require(largeP8 != nullptr);
    BIO_free_nid_postfix(largeP8Bio);
    LibreSsl3EvpPkey* largeP8Key = EVP_PKCS82PKEY_nid_postfix(largeP8);
    Require(largeP8Key != nullptr && largeP8Key->type == EcType);
    CompareEcKeys(
        static_cast<const EcKey*>(refKey->key), static_cast<const EcKey*>(largeP8Key->key), true);

    LibreSsl::Bio* largePrivBio = BIO_new_mem_buf_nid_postfix(
        largePkcs8.data(), static_cast<int>(largePkcs8.size()));
    Require(largePrivBio != nullptr);
    LibreSsl3EvpPkey* largePrivKey = d2i_PrivateKey_bio_nid_postfix(largePrivBio, nullptr);
    Require(largePrivKey != nullptr && largePrivKey->type == EcType);
    BIO_free_nid_postfix(largePrivBio);
    CompareEcKeys(
        static_cast<const EcKey*>(refKey->key), static_cast<const EcKey*>(largePrivKey->key), true);

    EVP_PKEY_free_nid_disambig1_nid_postfix(largePrivKey);
    EVP_PKEY_free_nid_disambig1_nid_postfix(largeP8Key);
    PKCS8_PRIV_KEY_INFO_free_nid_postfix(largeP8);
    EVP_PKEY_free_nid_disambig1_nid_postfix(refKey);
    PKCS8_PRIV_KEY_INFO_free_nid_postfix(refP8);
}

void* APS5_VABI HeapAllocate(std::size_t size) { return std::malloc(size == 0 ? 1 : size); }
void APS5_VABI HeapFree(void* pointer) { std::free(pointer); }
void* APS5_VABI HeapCallocate(std::size_t count, std::size_t size) {
    return std::calloc(count == 0 ? 1 : count, size == 0 ? 1 : size);
}
void* APS5_VABI HeapReallocate(void* pointer, std::size_t size) {
    return std::realloc(pointer, size == 0 ? 1 : size);
}
void* APS5_VABI UnexpectedAlign(std::size_t, std::size_t) { std::abort(); }
void* APS5_VABI UnexpectedRealign(void*, std::size_t, std::size_t) { std::abort(); }
int APS5_VABI UnexpectedPosixAlign(void**, std::size_t, std::size_t) { std::abort(); }

}

int main() {
    std::array<void*, 10> heapApi{
        reinterpret_cast<void*>(HeapAllocate),
        reinterpret_cast<void*>(HeapFree),
        reinterpret_cast<void*>(HeapCallocate),
        reinterpret_cast<void*>(HeapReallocate),
        reinterpret_cast<void*>(UnexpectedAlign),
        reinterpret_cast<void*>(UnexpectedRealign),
        reinterpret_cast<void*>(UnexpectedPosixAlign),
    };
    ApplicationHeapRegister_nid_no_patch(heapApi.data());
    TestRsaPublicKeys();
    TestRsaPrivateRoundTrip();
    TestEcPublicAndPrivate(Secp256r1, P256Pkcs8Der, &P256SpkiDer, &P256Sec1Der);
    TestEcPublicAndPrivate(Secp384r1, P384Pkcs8Der, nullptr, nullptr);
    TestErrorsAndNulls();
    TestOutputReplacementAndTruncation();
    TestConsecutiveBioObjects();
    TestPkcs8AttributesRoundTrip();
    TestBioMalformedAndMultiChunkObjects();
    return 0;
}
