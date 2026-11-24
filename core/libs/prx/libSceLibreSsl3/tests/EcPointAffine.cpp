#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include "prx/libSceLibreSsl/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <source_location>
#include <stdexcept>
#include <vector>

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "LibreSSL3 EC_POINT affine check failed at line %u\n", location.line());
    std::abort();
}

void* APS5_VABI Allocate(std::size_t size) { return std::malloc(size); }
void APS5_VABI Release(void* pointer) { std::free(pointer); }
void* APS5_VABI Callocate(std::size_t count, std::size_t size) { return std::calloc(count, size); }
void* APS5_VABI Reallocate(void* pointer, std::size_t size) { return std::realloc(pointer, size); }
void* APS5_VABI Align(std::size_t, std::size_t) { throw std::runtime_error("unexpected aligned allocation"); }
void* APS5_VABI Realign(void*, std::size_t) { throw std::runtime_error("unexpected aligned reallocation"); }
int APS5_VABI PosixAlign(void**, std::size_t, std::size_t) { throw std::runtime_error("unexpected posix_memalign"); }

void RegisterHeap() {
    std::array<void*, 7> api{reinterpret_cast<void*>(Allocate), reinterpret_cast<void*>(Release),
        reinterpret_cast<void*>(Callocate), reinterpret_cast<void*>(Reallocate), reinterpret_cast<void*>(Align),
        reinterpret_cast<void*>(Realign), reinterpret_cast<void*>(PosixAlign)};
    ApplicationHeapRegister_nid_no_patch(api.data());
}

constexpr int P256 = 415;
constexpr int P384 = 715;
constexpr int EcLibrary = 16;
constexpr int ErrRPassedNullParameter = 67;
constexpr int EcRIncompatibleObjects = 101;
constexpr int EcRPointAtInfinity = 106;

const std::array<unsigned char, 32> GeneratorX{
    0x6b, 0x17, 0xd1, 0xf2, 0xe1, 0x2c, 0x42, 0x47, 0xf8, 0xbc, 0xe6, 0xe5, 0x63, 0xa4, 0x40, 0xf2,
    0x77, 0x03, 0x7d, 0x81, 0x2d, 0xeb, 0x33, 0xa0, 0xf4, 0xa1, 0x39, 0x45, 0xd8, 0x98, 0xc2, 0x96};
const std::array<unsigned char, 32> GeneratorY{
    0x4f, 0xe3, 0x42, 0xe2, 0xfe, 0x1a, 0x7f, 0x9b, 0x8e, 0xe7, 0xeb, 0x4a, 0x7c, 0x0f, 0x9e, 0x16,
    0x2b, 0xce, 0x33, 0x57, 0x6b, 0x31, 0x5e, 0xce, 0xcb, 0xb6, 0x40, 0x68, 0x37, 0xbf, 0x51, 0xf5};

std::uint64_t ErrorCode(int reason) {
    return (static_cast<std::uint64_t>(EcLibrary) << 24) | (0xfffULL << 12) | static_cast<std::uint64_t>(reason);
}

void ClearErrors() {
    while (ERR_get_error_nid_postfix() != 0) {
    }
}

LibreSsl3Bignum* AsNative(Bignum* value) {
    return reinterpret_cast<LibreSsl3Bignum*>(value);
}

const LibreSsl3Bignum* AsNative(const Bignum* value) {
    return reinterpret_cast<const LibreSsl3Bignum*>(value);
}

std::vector<unsigned char> Bytes(const Bignum* value) {
    const int bits = BN_num_bits_nid_postfix(value);
    Require(bits >= 0);
    const int length = (bits + 7) / 8;
    std::vector<unsigned char> bytes(static_cast<std::size_t>(length));
    if (length != 0) Require(BN_bn2bin_nid_postfix(value, bytes.data()) == length);
    return bytes;
}

void Run() {
    EcGroup* group = EC_GROUP_new_by_curve_name_nid_postfix(P256);
    Require(group != nullptr);
    EcPoint* point = EC_POINT_new_nid_postfix(group);
    Require(point != nullptr);
    Bignum* generatorX = BN_bin2bn_nid_postfix(GeneratorX.data(), static_cast<int>(GeneratorX.size()), nullptr);
    Bignum* generatorY = BN_bin2bn_nid_postfix(GeneratorY.data(), static_cast<int>(GeneratorY.size()), nullptr);
    Require(generatorX != nullptr && generatorY != nullptr);

    ClearErrors();
    Require(EC_POINT_set_affine_coordinates_GFp_nid_postfix(group, point, AsNative(generatorX), AsNative(generatorY), nullptr) == 1);
    Require(ERR_get_error_nid_postfix() == 0);

    Bignum* x = BN_bin2bn_nid_postfix(nullptr, 0, nullptr);
    Bignum* y = BN_bin2bn_nid_postfix(nullptr, 0, nullptr);
    Require(x != nullptr && y != nullptr);
    Require(EC_POINT_get_affine_coordinates_GFp_nid_postfix(group, point, AsNative(x), AsNative(y), nullptr) == 1);
    Require(Bytes(x) == std::vector<unsigned char>(GeneratorX.begin(), GeneratorX.end()));
    Require(Bytes(y) == std::vector<unsigned char>(GeneratorY.begin(), GeneratorY.end()));

    Bignum* onlyX = BN_bin2bn_nid_postfix(nullptr, 0, nullptr);
    Require(onlyX != nullptr);
    Require(EC_POINT_get_affine_coordinates_GFp_nid_postfix(group, point, AsNative(onlyX), nullptr, nullptr) == 1);
    Require(Bytes(onlyX) == std::vector<unsigned char>(GeneratorX.begin(), GeneratorX.end()));

    ClearErrors();
    Require(EC_POINT_set_affine_coordinates_GFp_nid_postfix(group, point, nullptr, AsNative(generatorY), nullptr) == 0);
    Require(ERR_get_error_nid_postfix() == ErrorCode(ErrRPassedNullParameter));
    Require(ERR_get_error_nid_postfix() == 0);

    EcPoint* infinity = EC_POINT_new_nid_postfix(group);
    Require(infinity != nullptr);
    ClearErrors();
    Require(EC_POINT_get_affine_coordinates_GFp_nid_postfix(group, infinity, AsNative(x), AsNative(y), nullptr) == 0);
    Require(ERR_get_error_nid_postfix() == ErrorCode(EcRPointAtInfinity));
    Require(ERR_get_error_nid_postfix() == 0);

    EcGroup* otherCurve = EC_GROUP_new_by_curve_name_nid_postfix(P384);
    Require(otherCurve != nullptr);
    ClearErrors();
    Require(EC_POINT_set_affine_coordinates_GFp_nid_postfix(otherCurve, point, AsNative(generatorX), AsNative(generatorY), nullptr) == 0);
    Require(ERR_get_error_nid_postfix() == ErrorCode(EcRIncompatibleObjects));
    Require(ERR_get_error_nid_postfix() == 0);

    const unsigned char one = 1;
    Bignum* scalar = BN_bin2bn_nid_postfix(&one, 1, nullptr);
    Require(scalar != nullptr);
    EcPoint* product = EC_POINT_new_nid_postfix(group);
    Require(product != nullptr);
    Require(EC_POINT_mul_nid_postfix(group, product, nullptr, point, scalar, nullptr) == 1);
    Require(EC_POINT_get_affine_coordinates_GFp_nid_postfix(group, product, AsNative(x), AsNative(y), nullptr) == 1);
    Require(Bytes(x) == std::vector<unsigned char>(GeneratorX.begin(), GeneratorX.end()));
    Require(Bytes(y) == std::vector<unsigned char>(GeneratorY.begin(), GeneratorY.end()));

    BN_free_nid_postfix(onlyX);
    BN_free_nid_postfix(x);
    BN_free_nid_postfix(y);
    BN_free_nid_postfix(scalar);
    BN_free_nid_postfix(generatorX);
    BN_free_nid_postfix(generatorY);
    EC_POINT_free_nid_postfix(product);
    EC_POINT_free_nid_postfix(infinity);
    EC_POINT_free_nid_postfix(point);
    EC_GROUP_free_nid_postfix(otherCurve);
    EC_GROUP_free_nid_postfix(group);
}

}

int main() {
    RegisterHeap();
    try {
        Run();
    } catch (const std::exception& error) {
        std::fprintf(stderr, "EC_POINT affine threw: %s\n", error.what());
        return 1;
    }
    std::puts("LibreSSL3 EC_POINT affine tests passed");
}
