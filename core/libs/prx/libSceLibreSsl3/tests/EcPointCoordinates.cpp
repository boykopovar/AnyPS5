#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

#include <array>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <source_location>
#include <vector>

namespace {

constexpr int Uncompressed = 4;

using Bytes = std::vector<unsigned char>;

const Bytes GeneratorX{
    0x6b, 0x17, 0xd1, 0xf2, 0xe1, 0x2c, 0x42, 0x47, 0xf8, 0xbc, 0xe6, 0xe5, 0x63, 0xa4, 0x40, 0xf2,
    0x77, 0x03, 0x7d, 0x81, 0x2d, 0xeb, 0x33, 0xa0, 0xf4, 0xa1, 0x39, 0x45, 0xd8, 0x98, 0xc2, 0x96,
};
const Bytes GeneratorY{
    0x4f, 0xe3, 0x42, 0xe2, 0xfe, 0x1a, 0x7f, 0x9b, 0x8e, 0xe7, 0xeb, 0x4a, 0x7c, 0x0f, 0x9e, 0x16,
    0x2b, 0xce, 0x33, 0x57, 0x6b, 0x31, 0x5e, 0xce, 0xcb, 0xb6, 0x40, 0x68, 0x37, 0xbf, 0x51, 0xf5,
};
const Bytes Prime{
    0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
};

int liveBlocks = 0;

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "LibreSSL3 EC_POINT affine coordinates check failed at line %u\n", location.line());
    std::abort();
}

void* APS5_VABI Allocate(std::size_t bytes) {
    ++liveBlocks;
    return std::malloc(bytes ? bytes : 1);
}

void APS5_VABI Release(void* block) {
    if (block) --liveBlocks;
    std::free(block);
}

void* APS5_VABI AllocateZeroed(std::size_t count, std::size_t bytes) {
    ++liveBlocks;
    return std::calloc(count ? count : 1, bytes ? bytes : 1);
}

void* APS5_VABI Reallocate(void*, std::size_t) { return nullptr; }
void* APS5_VABI Align(std::size_t, std::size_t) { return nullptr; }
void* APS5_VABI Realign(void*, std::size_t, std::size_t) { return nullptr; }
int APS5_VABI PosixAlign(void**, std::size_t, std::size_t) { return 12; }

void InitializeHeap() {
    void* const api[10] {
        reinterpret_cast<void*>(&Allocate), reinterpret_cast<void*>(&Release), reinterpret_cast<void*>(&AllocateZeroed),
        reinterpret_cast<void*>(&Reallocate), reinterpret_cast<void*>(&Align), reinterpret_cast<void*>(&Realign),
        reinterpret_cast<void*>(&PosixAlign),
    };
    ApplicationHeapRegister_nid_no_patch(api);
}

Bignum* FromBytes(const Bytes& bytes) {
    Bignum* value = BN_bin2bn_nid_postfix(bytes.data(), static_cast<int>(bytes.size()), nullptr);
    Require(value != nullptr);
    return value;
}

Bytes ToBytes(const Bignum* value) {
    Bytes bytes(static_cast<std::size_t>(BN_num_bits_nid_postfix(value) + 7) / 8);
    Require(BN_bn2bin_nid_postfix(value, bytes.data()) == static_cast<int>(bytes.size()));
    return bytes;
}

Bytes Padded(const Bignum* value, std::size_t size) {
    Bytes bytes = ToBytes(value);
    Require(bytes.size() <= size);
    bytes.insert(bytes.begin(), size - bytes.size(), 0);
    return bytes;
}

Bytes Encode(const EcGroup* group, const EcPoint* point) {
    const std::size_t size = EC_POINT_point2oct_nid_postfix(group, point, Uncompressed, nullptr, 0, nullptr);
    Require(size != 0);
    Bytes encoded(size);
    Require(EC_POINT_point2oct_nid_postfix(group, point, Uncompressed, encoded.data(), encoded.size(), nullptr) == size);
    return encoded;
}

Bytes Concatenate(const Bytes& x, const Bytes& y) {
    Bytes encoded{static_cast<unsigned char>(Uncompressed)};
    encoded.insert(encoded.end(), x.begin(), x.end());
    encoded.insert(encoded.end(), y.begin(), y.end());
    return encoded;
}

void RequireRejected(const EcGroup* group, const Bytes& x, const Bytes& y) {
    EcPoint* point = EC_POINT_new_nid_postfix(group);
    Require(point != nullptr);
    Bignum* bx = FromBytes(x);
    Bignum* by = FromBytes(y);
    Require(EC_POINT_set_affine_coordinates_GFp_nid_postfix(group, point, bx, by, nullptr) == 0);
    BN_free_nid_postfix(bx);
    BN_free_nid_postfix(by);
    EC_POINT_free_nid_postfix(point);
}

void Generator() {
    EcGroup* group = EC_GROUP_new_by_curve_name_nid_postfix(415);
    Require(group != nullptr);
    EcPoint* point = EC_POINT_new_nid_postfix(group);
    Require(point != nullptr);
    Bignum* x = FromBytes(GeneratorX);
    Bignum* y = FromBytes(GeneratorY);
    Require(EC_POINT_set_affine_coordinates_GFp_nid_postfix(group, point, x, y, nullptr) == 1);
    Require(Encode(group, point) == Concatenate(GeneratorX, GeneratorY));
    BignumContext* context = BN_CTX_new_nid_postfix();
    Require(context != nullptr);
    Bignum* outX = BN_new_nid_postfix();
    Bignum* outY = BN_new_nid_postfix();
    Require(outX != nullptr && outY != nullptr);
    Require(BN_set_word_nid_postfix(outX, 7) == 1);
    Require(EC_POINT_get_affine_coordinates_GFp_nid_postfix(group, point, outX, outY, context) == 1);
    Require(ToBytes(outX) == GeneratorX);
    Require(ToBytes(outY) == GeneratorY);
    BN_CTX_free_nid_postfix(context);
    Require(BN_set_word_nid_postfix(outX, 0) == 1);
    Require(BN_set_word_nid_postfix(outY, 0) == 1);
    Require(EC_POINT_get_affine_coordinates_GFp_nid_postfix(group, point, nullptr, outY, nullptr) == 1);
    Require(ToBytes(outY) == GeneratorY);
    Require(EC_POINT_get_affine_coordinates_GFp_nid_postfix(group, point, outX, nullptr, nullptr) == 1);
    Require(ToBytes(outX) == GeneratorX);
    Require(EC_POINT_get_affine_coordinates_GFp_nid_postfix(group, point, nullptr, nullptr, nullptr) == 1);
    Require(EC_POINT_set_affine_coordinates_GFp_nid_postfix(group, point, nullptr, y, nullptr) == 0);
    Require(EC_POINT_set_affine_coordinates_GFp_nid_postfix(group, point, x, nullptr, nullptr) == 0);
    Require(EC_POINT_set_affine_coordinates_GFp_nid_postfix(group, point, nullptr, nullptr, nullptr) == 0);
    Require(Encode(group, point) == Concatenate(GeneratorX, GeneratorY));
    Bytes offCurve = GeneratorY;
    ++offCurve.back();
    RequireRejected(group, GeneratorX, offCurve);
    RequireRejected(group, Prime, GeneratorY);
    RequireRejected(group, GeneratorX, Prime);
    Bytes wide = GeneratorX;
    wide.insert(wide.begin(), 1);
    RequireRejected(group, wide, GeneratorY);
    Bignum* negative = FromBytes(GeneratorY);
    negative->neg = 1;
    Require(EC_POINT_set_affine_coordinates_GFp_nid_postfix(group, point, x, negative, nullptr) == 0);
    BN_free_nid_postfix(negative);
    Require(Encode(group, point) == Concatenate(GeneratorX, GeneratorY));
    constexpr unsigned char infinity = 0;
    Require(EC_POINT_oct2point_nid_postfix(group, point, &infinity, 1, nullptr) == 1);
    Require(BN_set_word_nid_postfix(outX, 5) == 1);
    Require(BN_set_word_nid_postfix(outY, 6) == 1);
    Require(EC_POINT_get_affine_coordinates_GFp_nid_postfix(group, point, outX, outY, nullptr) == 0);
    Require(ToBytes(outX) == Bytes{5});
    Require(ToBytes(outY) == Bytes{6});
    Require(EC_POINT_get_affine_coordinates_GFp_nid_postfix(group, point, nullptr, nullptr, nullptr) == 0);
    BN_free_nid_postfix(x);
    BN_free_nid_postfix(y);
    BN_free_nid_postfix(outX);
    BN_free_nid_postfix(outY);
    EC_POINT_free_nid_postfix(point);
    EC_GROUP_free_nid_postfix(group);
}

void RoundTrip(int nid) {
    EcKey* key = EC_KEY_new_by_curve_name_nid_postfix(nid);
    Require(key != nullptr);
    Require(EC_KEY_generate_key_nid_postfix(key) == 1);
    const EcGroup* group = EC_KEY_get0_group_nid_postfix(key);
    const EcPoint* publicKey = EC_KEY_get0_public_key_nid_postfix(key);
    Require(group != nullptr && publicKey != nullptr);
    const Bytes encoded = Encode(group, publicKey);
    const std::size_t fieldSize = (encoded.size() - 1) / 2;
    Require(fieldSize == (static_cast<std::size_t>(EC_GROUP_get_degree_nid_postfix(group)) + 7) / 8);
    Bignum* x = BN_new_nid_postfix();
    Bignum* y = BN_new_nid_postfix();
    Require(x != nullptr && y != nullptr);
    Require(EC_POINT_get_affine_coordinates_GFp_nid_postfix(group, publicKey, x, y, nullptr) == 1);
    Require(Concatenate(Padded(x, fieldSize), Padded(y, fieldSize)) == encoded);
    EcGroup* separate = EC_GROUP_new_by_curve_name_nid_postfix(nid);
    Require(separate != nullptr);
    EC_KEY_free_nid_postfix(key);
    EcPoint* point = EC_POINT_new_nid_postfix(separate);
    Require(point != nullptr);
    Require(EC_POINT_set_affine_coordinates_GFp_nid_postfix(separate, point, x, y, nullptr) == 1);
    Require(Encode(separate, point) == encoded);
    Bignum* again = BN_new_nid_postfix();
    Require(again != nullptr);
    Require(EC_POINT_get_affine_coordinates_GFp_nid_postfix(separate, point, again, nullptr, nullptr) == 1);
    Require(ToBytes(again) == ToBytes(x));
    BN_free_nid_postfix(again);
    BN_free_nid_postfix(x);
    BN_free_nid_postfix(y);
    EC_POINT_free_nid_postfix(point);
    EC_GROUP_free_nid_postfix(separate);
}

} // namespace

int main() {
    InitializeHeap();
    Generator();
    constexpr std::array nids{409, 415, 715, 716};
    for (const int nid : nids) {
        for (int round = 0; round < 8; ++round) RoundTrip(nid);
    }
    Require(liveBlocks == 0);
    std::puts("LibreSSL3 EC_POINT affine coordinates tests passed");
    return 0;
}
