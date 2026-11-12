#include "prx/libSceLibreSsl/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

using Bytes = std::vector<unsigned char>;

struct Vector {
    int nid;
    const char* order;
    const char* scalar;
    const char* multiple;
};

static constexpr Vector Vectors[] = {
    {409, "ffffffffffffffffffffffff99def836146bc9b1b4d22831", "6a25d16c4fcf77b5ed8dfab882061d8c86db869c0020dc91", "0303d67464bac6bfb449b3369cc2fd6289abeb46ebbb348f80"},
    {415, "ffffffff00000000ffffffffffffffffbce6faada7179e84f3b9cac2fc632551", "1071a8a1bfe051ab9ac01e74e297a42655fbc7c030142af21b18a86b546ee36e", "039ac895cfa55447ce3a455f303486050a2c066d750171cb4c9f13ddf47ec16a66"},
    {711, "fffffffffffffffffffffffe26f2fc170f69466a74defd8d", "4cbb396df1675b15a74f3295b6d79ae3a88fcede6efd1252", "035da3066b4fc3e6716ea4c7d6e5ef9393a9efd8601f48b132"},
    {712, "10000000000000000000000000001dce8d2ec6184caf0a971769fb1f7", "b02068b3b226bfb9efc9024b5dba6e5c062edc4baaa9e19fbda1f844", "036e12815ad4749d77f162b1f063563f5cd4f10528311a5670313b11af"},
    {713, "ffffffffffffffffffffffffffff16a2e0b8f03e13dd29455c5c2a3d", "61d27ed47260a83d907fef219857710de850fac08df421c03512d411", "029c4a25d25d95f94953f3b8f219212e10dc590e08e0398d3ccf2c6682"},
    {714, "fffffffffffffffffffffffffffffffebaaedce6af48a03bbfd25e8cd0364141", "1c1e88789ce7969db43ef652d2f3e9cb0db459f746d30f5a6be9c15e0a11e78f", "023e8cd8ec31c6afa3dff6c748e96fc9f2a85367b3254d2b9b83f9161666647a14"},
    {715, "ffffffffffffffffffffffffffffffffffffffffffffffffc7634d81f4372ddf581a0db248b0a77aecec196accc52973", "40347d65fa6c16e4ab66dd9c76245af39da47a05fc379e3a3ad34a187887f61f9787e7cb69cf8a0b504472f0fd6d28d", "0208ad788dcb59721a1ef7ed6a378bfea22efed26a16f7fa76fcb182d6ac49fa3dfe3434cb798839e7df2466c166879fe3"},
    {716, "1fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffa51868783bf2f966b7fcc0148f709a5d03bb5c9b8899c47aebb6fb71e91386409", "bebf0cd22a70a30d634df1b572dd6b506ea309eec34640c472e5cb3106622ecf5cd1b3386030a83b7270cb3c3657c958aa24b4b40ff008e7d917b39ce0a14b31bf", "03001224efd98ca54918d833f3ccb2e0c907b3969ba8498e6b41807ddf8d954f4a4c24ff3c812b2b979efbd1b21d1fa04f435e5817e832ba11a3fbebf4ca3e393ae4c2"},
    {928, "a9fb57dba1eea9bc3e660a909d838d718c397aa3b561a6f7901e0e82974856a7", "507fed3956f820d51ab166ecf80d55604342476b361219e0fbc4a4df8ef9ebc4", "024b76514134d610e3319bb0c4dfadf6c7c27f593fcf1957299452d34c649ebb66"},
    {932, "8cb91e82a3386d280f5d6f7e50e641df152f7109ed5456b31f166e6cac0425a7cf3ab6af6b7fc3103b883202e9046565", "1c7a536594b11e9c0edd7d95b7e35a8747d0a6cb9909049889d626f3f514bd62dbdf10fb5d81bc88ab73ef1358255fc", "03583dba6b80e7e00d1621b94464ce2d86dbcbe139aeb83f5fedcb68db19999ecfd7a27c1f222db89ffa1c2474e7767411"},
    {934, "aadd9db8dbe9c48b3fd4e6ae33c9fc07cb308db3b3c9d20ed6639cca70330870553e5c414ca92619418661197fac10471db1d381085ddaddb58796829ca90069", "1a15ef6b1655a4db4bc6bc65b932744c244dc6c5e121a974bb569c9d6a1e959b0e560931790c1cbbd15febf962be3bbf3696291cae3e43cfff4ef5b746ebd218", "021b1a0f7e71e9d0fc199f7b90b3429b8cfa1271ac95a911b81368b747574130f527b0fd8141bf98dd3ef8bc5c47bf1b4fbb0cbbc2c32fbd86547ef70365727c11"},
};

static constexpr const char* RfcPrivateKey = "c9afa9d845ba75166b5c215767b1d6934e50c3db36e89b127b8a622b120f6721";
static constexpr const char* RfcPublicKey =
    "0460fed4ba255a9d31c961eb74c6356d68c049b8923b61fa6ce669622e60f29fb6"
    "7903fe1008b8bc99a41ae9e95628bc64f2f1b20c2d7e9f5177a3c294d4462299";

static constexpr int Compressed = 2;
static constexpr int Uncompressed = 4;
static const Bytes Infinity{0};

static void Require(bool value, const char* what) {
    if (value) return;
    std::fprintf(stderr, "failed: %s\n", what);
    std::abort();
}

static Bytes FromHex(const char* hex) {
    Bytes out;
    for (const char* digit = hex; digit[0] && digit[1]; digit += 2) out.push_back(static_cast<unsigned char>(std::stoi(std::string(digit, 2), nullptr, 16)));
    return out;
}

static Bignum* Number(const std::string& hex) {
    Bignum* number = nullptr;
    Require(BN_hex2bn_nid_postfix(&number, hex.c_str()) == static_cast<int>(hex.size()) && number != nullptr, "BN_hex2bn");
    return number;
}

static Bignum* Word(std::uint64_t value) {
    Bignum* number = BN_new_nid_postfix();
    Require(number != nullptr && BN_set_word_nid_postfix(number, value) == 1, "BN_set_word");
    return number;
}

static Bytes Encode(const EcGroup* group, const EcPoint* point, int form = Compressed) {
    const std::size_t size = EC_POINT_point2oct_nid_postfix(group, point, form, nullptr, 0, nullptr);
    Bytes out(size);
    Require(size != 0 && EC_POINT_point2oct_nid_postfix(group, point, form, out.data(), out.size(), nullptr) == size, "EC_POINT_point2oct");
    return out;
}

static Bytes Negated(Bytes compressed) {
    compressed[0] ^= 1;
    return compressed;
}

static EcPoint* Product(const EcGroup* group, const Bignum* generatorScalar, const EcPoint* point, const Bignum* pointScalar) {
    EcPoint* result = EC_POINT_new_nid_postfix(group);
    Require(result != nullptr, "EC_POINT_new");
    Require(EC_POINT_mul_nid_postfix(group, result, generatorScalar, point, pointScalar, nullptr) == 1, "EC_POINT_mul");
    return result;
}

static Bytes Multiple(const EcGroup* group, const Bignum* generatorScalar, const EcPoint* point, const Bignum* pointScalar) {
    EcPoint* result = Product(group, generatorScalar, point, pointScalar);
    const Bytes encoded = Encode(group, result);
    EC_POINT_free_nid_postfix(result);
    return encoded;
}

static void CheckKnownMultiples() {
    for (const Vector& vector : Vectors) {
        EcGroup* group = EC_GROUP_new_by_curve_name_nid_postfix(vector.nid);
        Require(group != nullptr, "EC_GROUP_new_by_curve_name");
        Bignum* one = Word(1);
        Bignum* scalar = Number(vector.scalar);
        Bignum* negative = Number(std::string("-") + vector.scalar);
        const Bytes expected = FromHex(vector.multiple);
        EcPoint* generator = Product(group, one, nullptr, nullptr);
        const Bytes encodedGenerator = Encode(group, generator);

        Require(Multiple(group, scalar, nullptr, nullptr) == expected, "scalar * generator");
        Require(Multiple(group, nullptr, generator, scalar) == expected, "scalar * point");
        Require(Multiple(group, scalar, generator, nullptr) == expected, "a point without a scalar is left out");
        Require(Multiple(group, scalar, nullptr, scalar) == expected, "a scalar without a point is left out");
        Require(Multiple(group, negative, nullptr, nullptr) == Negated(expected), "a negative scalar");
        Require(Multiple(group, nullptr, generator, negative) == Negated(expected), "a negative scalar * point");

        Bignum* order = Number(vector.order);
        Bignum* three = Word(3);
        Bignum* five = Word(5);
        Bignum* large = BN_new_nid_postfix();
        Require(large != nullptr && BN_mul_nid_postfix(large, order, three, nullptr) == 1 && BN_add_word_nid_postfix(large, 5) == 1, "3 * order + 5");
        Require(Multiple(group, order, nullptr, nullptr) == Infinity, "the order gives infinity");
        Require(Multiple(group, nullptr, generator, order) == Infinity, "the order * point gives infinity");
        Require(BN_sub_word_nid_postfix(order, 1) == 1, "BN_sub_word");
        Require(Multiple(group, order, nullptr, nullptr) == Negated(encodedGenerator), "order - 1 gives the negated generator");
        const Bytes fiveTimes = Multiple(group, five, nullptr, nullptr);
        Require(Multiple(group, large, nullptr, nullptr) == fiveTimes, "a scalar above the order");
        Require(Multiple(group, nullptr, generator, large) == fiveTimes, "a scalar above the order * point");

        Bignum* zero = Word(0);
        Require(Multiple(group, zero, nullptr, nullptr) == Infinity, "0 * generator");
        Require(Multiple(group, zero, generator, zero) == Infinity, "0 * generator + 0 * point");

        Bignum* seven = Word(7);
        Bignum* fifteen = Word(15);
        Bignum* twentyTwo = Word(22);
        EcPoint* fiveG = Product(group, five, nullptr, nullptr);
        Require(Multiple(group, seven, fiveG, three) == Multiple(group, twentyTwo, nullptr, nullptr), "7 * generator + 3 * (5 * generator)");
        Require(Multiple(group, scalar, generator, negative) == Infinity, "terms that cancel");
        Require(Multiple(group, scalar, fiveG, zero) == expected, "a zero point scalar");
        Require(Multiple(group, zero, generator, scalar) == expected, "a zero generator scalar");

        EcPoint* infinity = EC_POINT_new_nid_postfix(group);
        Require(Multiple(group, nullptr, infinity, scalar) == Infinity, "scalar * infinity");
        Require(Multiple(group, scalar, infinity, scalar) == expected, "infinity adds nothing");

        EcPoint* result = Product(group, scalar, nullptr, nullptr);
        Require(EC_POINT_mul_nid_postfix(group, result, nullptr, nullptr, nullptr, nullptr) == 1, "no scalars");
        Require(Encode(group, result) == Infinity, "no scalars give infinity");
        Require(EC_POINT_mul_nid_postfix(group, fiveG, nullptr, fiveG, three, nullptr) == 1, "the result is the point");
        Require(Encode(group, fiveG) == Multiple(group, fifteen, nullptr, nullptr), "3 * (5 * generator) into its own point");
        Require(EC_POINT_mul_nid_postfix(group, fiveG, seven, fiveG, one, nullptr) == 1, "the result is the point, with the generator");
        Require(Encode(group, fiveG) == Multiple(group, twentyTwo, nullptr, nullptr), "7 * generator + (15 * generator) into its own point");

        for (EcPoint* point : {generator, fiveG, infinity, result}) EC_POINT_free_nid_postfix(point);
        for (Bignum* number : {one, scalar, negative, order, large, three, five, zero, seven, fifteen, twentyTwo}) BN_free_nid_postfix(number);
        EC_GROUP_free_nid_postfix(group);
    }

    EcGroup* group = EC_GROUP_new_by_curve_name_nid_postfix(415);
    Bignum* privateKey = Number(RfcPrivateKey);
    EcPoint* publicKey = Product(group, privateKey, nullptr, nullptr);
    Require(Encode(group, publicKey, Uncompressed) == FromHex(RfcPublicKey), "the RFC 6979 A.2.5 public key");
    EC_POINT_free_nid_postfix(publicKey);
    BN_free_nid_postfix(privateKey);
    EC_GROUP_free_nid_postfix(group);
}

static void CheckSharedSecrets() {
    for (const Vector& vector : Vectors) {
        EcKey* key = EC_KEY_new_by_curve_name_nid_postfix(vector.nid);
        Require(key != nullptr && EC_KEY_generate_key_nid_postfix(key) == 1, "EC_KEY_generate_key");
        const EcGroup* group = EC_KEY_get0_group_nid_postfix(key);
        Bignum* scalar = Number(vector.scalar);
        EcPoint* peer = Product(group, scalar, nullptr, nullptr);
        Bytes secret(80);
        const int size = ECDH_compute_key_nid_postfix(secret.data(), secret.size(), peer, key, nullptr);
        Require(size > 0, "ECDH_compute_key");
        secret.resize(static_cast<std::size_t>(size));
        const Bytes product = Multiple(group, nullptr, EC_KEY_get0_public_key_nid_postfix(key), scalar);
        Require(Bytes(product.begin() + 1, product.end()) == secret, "scalar * public key matches ECDH with scalar * generator");
        EC_POINT_free_nid_postfix(peer);
        BN_free_nid_postfix(scalar);
        EC_KEY_free_nid_postfix(key);
    }
}

int main() {
    void* const heapApi[10]{};
    ApplicationHeapRegister_nid_no_patch(heapApi);
    CheckKnownMultiples();
    CheckSharedSecrets();
    Require(ERR_peek_error_nid_postfix() == 0, "no error is queued");
    std::puts("guest libressl EC_POINT_mul tests passed");
    return 0;
}
