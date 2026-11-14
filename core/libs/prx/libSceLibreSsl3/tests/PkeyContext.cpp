#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include "prx/libSceLibreSsl3/include/PkeyMethod.hpp"
#include "prx/libSceLibreSsl3/include/PkeyState.hpp"
#include "prx/libSceLibreSsl3/include/RsaCrypto.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <initializer_list>
#include <source_location>
#include <stdexcept>
#include <unordered_map>
#include <vector>

namespace {

std::unordered_map<void*, std::size_t> allocations;

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "LibreSSL3 EVP_PKEY_CTX check failed at line %u\n", location.line());
    std::abort();
}

void* APS5_VABI Allocate_nid_no_patch(std::size_t size) {
    void* result = std::malloc(size ? size : 1);
    Require(result != nullptr);
    Require(allocations.emplace(result, size).second);
    std::memset(result, 0xa5, size ? size : 1);
    return result;
}

void APS5_VABI Free_nid_no_patch(void* pointer) {
    if (!pointer) return;
    Require(allocations.erase(pointer) == 1);
    std::free(pointer);
}

void* APS5_VABI Calloc_nid_no_patch(std::size_t count, std::size_t size) {
    auto* result = static_cast<unsigned char*>(Allocate_nid_no_patch(count * size));
    std::memset(result, 0, count * size);
    return result;
}

void* APS5_VABI Reallocate_nid_no_patch(void*, std::size_t) { throw std::runtime_error("unexpected reallocation"); }
void* APS5_VABI Align_nid_no_patch(std::size_t, std::size_t) { throw std::runtime_error("unexpected aligned allocation"); }
void* APS5_VABI Realign_nid_no_patch(void*, std::size_t, std::size_t) { throw std::runtime_error("unexpected realigned allocation"); }
int APS5_VABI PosixAlign_nid_no_patch(void**, std::size_t, std::size_t) { throw std::runtime_error("unexpected aligned allocation"); }

constexpr std::uint64_t EvpCode(std::uint64_t reason) { return 0x06fff000ULL | reason; }
constexpr std::uint64_t RsaCode(std::uint64_t reason) { return 0x04fff000ULL | reason; }

void ExpectErrors(std::initializer_list<std::uint64_t> codes) {
    for (const auto code : codes) Require(ERR_get_error_nid_postfix() == code);
    Require(ERR_get_error_nid_postfix() == 0);
}

void ExpectRsaQueue() {
    std::uint64_t code = ERR_get_error_nid_postfix();
    int queued = 0;
    while (code != 0) {
        Require(static_cast<int>((code >> 24) & 0xff) == 4);
        ++queued;
        code = ERR_get_error_nid_postfix();
    }
    Require(queued > 0);
}

void* Pointer(const void* value) { return const_cast<void*>(value); }

template<class TAction>
bool Throws(TAction action) {
    try {
        action();
    } catch (const std::exception&) {
        return true;
    }
    return false;
}

constexpr char PlainMessage[] {"AnyPS5 EVP vector!"};
constexpr std::size_t MessageLength = sizeof(PlainMessage) - 1;

constexpr unsigned char KeyModulus[] {
    190, 151, 144, 147, 130, 214, 20, 24, 80, 240, 239, 214, 182, 18, 112, 85,
    63, 39, 33, 58, 54, 38, 0, 33, 157, 225, 239, 240, 245, 124, 69, 106,
    168, 68, 173, 131, 103, 123, 105, 245, 60, 9, 239, 222, 78, 28, 163, 228,
    160, 224, 12, 49, 196, 3, 97, 17, 166, 26, 118, 241, 53, 74, 100, 28,
    44, 230, 215, 92, 213, 59, 101, 7, 120, 157, 217, 36, 203, 52, 24, 250,
    96, 104, 212, 85, 44, 138, 57, 154, 65, 144, 168, 205, 180, 8, 245, 240,
    234, 80, 55, 179, 161, 49, 101, 65, 30, 39, 92, 149, 6, 200, 70, 200,
    226, 171, 41, 172, 205, 143, 207, 5, 220, 129, 137, 107, 52, 252, 113, 138,
    40, 165, 56, 82, 149, 2, 91, 235, 17, 255, 10, 230, 98, 214, 46, 65,
    99, 189, 45, 78, 115, 170, 130, 93, 221, 93, 76, 151, 145, 121, 65, 65,
    236, 200, 166, 149, 152, 195, 209, 48, 89, 149, 106, 112, 117, 26, 129, 231,
    125, 188, 192, 248, 137, 116, 152, 62, 21, 41, 148, 125, 19, 203, 47, 141,
    33, 235, 20, 71, 180, 54, 104, 216, 242, 247, 160, 192, 90, 242, 195, 170,
    149, 191, 185, 241, 33, 239, 56, 149, 196, 86, 58, 39, 86, 181, 73, 155,
    33, 35, 7, 196, 169, 49, 216, 203, 219, 223, 220, 88, 30, 78, 2, 130,
    189, 148, 164, 98, 155, 179, 142, 93, 69, 0, 31, 8, 56, 199, 241, 241,
};

constexpr unsigned char KeyPrivateExponent[] {
    17, 121, 130, 99, 38, 169, 1, 48, 255, 142, 218, 97, 79, 39, 243, 249,
    15, 154, 195, 89, 166, 125, 167, 202, 109, 66, 228, 197, 215, 132, 49, 139,
    39, 106, 197, 235, 238, 144, 213, 95, 96, 240, 175, 162, 86, 208, 26, 110,
    33, 26, 153, 190, 75, 6, 7, 160, 52, 203, 104, 12, 189, 65, 153, 4,
    132, 251, 105, 253, 233, 123, 188, 203, 116, 17, 126, 71, 120, 216, 31, 40,
    41, 175, 24, 52, 199, 170, 41, 107, 141, 114, 100, 42, 110, 196, 217, 142,
    140, 10, 151, 40, 214, 241, 46, 162, 245, 249, 164, 65, 71, 178, 20, 220,
    181, 252, 253, 54, 95, 167, 61, 83, 43, 107, 97, 188, 71, 117, 233, 58,
    68, 119, 250, 137, 243, 99, 234, 163, 197, 65, 135, 160, 170, 88, 230, 161,
    87, 91, 19, 102, 170, 138, 142, 42, 198, 139, 249, 106, 204, 97, 99, 52,
    126, 151, 54, 185, 97, 227, 155, 4, 164, 26, 240, 54, 178, 110, 137, 179,
    208, 127, 176, 127, 134, 138, 240, 156, 96, 128, 89, 211, 61, 24, 187, 194,
    144, 223, 24, 231, 228, 242, 86, 150, 202, 84, 73, 62, 114, 6, 6, 233,
    67, 107, 160, 184, 189, 1, 237, 116, 193, 31, 106, 110, 121, 149, 95, 76,
    224, 99, 32, 209, 100, 210, 159, 246, 38, 184, 34, 162, 177, 155, 161, 173,
    82, 242, 237, 234, 96, 155, 62, 42, 100, 132, 179, 248, 15, 144, 93, 215,
};

constexpr unsigned char Pkcs1Cipher[] {
    30, 84, 141, 188, 191, 214, 191, 155, 176, 123, 106, 186, 79, 84, 252, 44,
    51, 95, 234, 39, 218, 40, 60, 125, 106, 219, 209, 99, 72, 23, 181, 148,
    224, 238, 164, 207, 116, 121, 187, 79, 228, 119, 165, 35, 117, 10, 68, 198,
    174, 83, 128, 99, 32, 224, 135, 240, 135, 187, 208, 85, 210, 31, 168, 102,
    48, 17, 85, 235, 215, 137, 195, 7, 205, 171, 44, 190, 109, 163, 192, 220,
    154, 74, 51, 221, 96, 67, 161, 160, 70, 146, 113, 146, 188, 151, 200, 145,
    251, 36, 10, 28, 123, 61, 43, 63, 233, 104, 190, 47, 16, 9, 131, 217,
    10, 32, 197, 126, 221, 129, 206, 83, 113, 126, 96, 233, 234, 174, 112, 195,
    24, 21, 118, 7, 76, 78, 24, 137, 225, 65, 101, 174, 186, 71, 233, 167,
    252, 249, 249, 181, 225, 84, 200, 183, 153, 164, 32, 156, 50, 225, 33, 226,
    116, 43, 245, 37, 3, 197, 9, 38, 36, 111, 88, 88, 61, 247, 250, 204,
    54, 145, 251, 16, 197, 211, 211, 67, 239, 164, 241, 82, 248, 138, 236, 166,
    62, 224, 221, 228, 90, 86, 34, 142, 12, 162, 122, 68, 169, 194, 193, 210,
    150, 135, 56, 76, 254, 178, 206, 62, 7, 71, 52, 147, 98, 159, 159, 159,
    188, 23, 133, 100, 85, 252, 48, 69, 249, 137, 245, 182, 52, 71, 179, 13,
    118, 173, 187, 126, 205, 16, 176, 7, 116, 14, 157, 59, 80, 146, 241, 0,
};

constexpr unsigned char OaepSha1Cipher[] {
    133, 2, 237, 97, 87, 185, 187, 22, 101, 55, 189, 38, 34, 28, 116, 83,
    229, 131, 51, 65, 99, 211, 105, 171, 77, 189, 165, 62, 55, 29, 145, 69,
    104, 225, 218, 33, 80, 118, 216, 166, 60, 62, 5, 226, 221, 236, 246, 106,
    233, 12, 29, 202, 234, 95, 129, 95, 249, 128, 115, 197, 207, 195, 39, 6,
    87, 248, 95, 27, 255, 4, 218, 134, 245, 69, 41, 74, 224, 128, 186, 250,
    27, 75, 92, 76, 236, 118, 29, 140, 120, 13, 13, 111, 54, 32, 163, 212,
    50, 6, 251, 9, 54, 255, 3, 92, 230, 237, 29, 126, 239, 137, 20, 179,
    30, 78, 141, 100, 28, 181, 215, 154, 105, 79, 206, 247, 202, 190, 42, 50,
    74, 149, 243, 169, 208, 99, 110, 95, 109, 105, 118, 102, 33, 84, 109, 128,
    33, 11, 247, 227, 157, 60, 130, 121, 41, 158, 106, 43, 100, 231, 229, 103,
    214, 230, 136, 72, 103, 226, 129, 100, 146, 200, 94, 209, 200, 32, 105, 140,
    167, 151, 105, 174, 220, 57, 16, 165, 73, 13, 140, 194, 195, 172, 210, 206,
    193, 31, 149, 174, 157, 158, 41, 118, 167, 215, 194, 173, 46, 153, 254, 147,
    173, 207, 116, 210, 101, 172, 131, 96, 203, 241, 221, 179, 215, 189, 70, 112,
    130, 218, 22, 87, 236, 78, 96, 5, 210, 115, 205, 195, 33, 12, 188, 222,
    79, 208, 162, 217, 6, 249, 187, 43, 28, 254, 164, 0, 101, 82, 185, 217,
};

constexpr unsigned char OaepSha256Cipher[] {
    100, 203, 145, 192, 155, 244, 108, 227, 110, 164, 24, 46, 202, 16, 162, 251,
    156, 112, 1, 156, 51, 33, 64, 87, 96, 62, 250, 92, 59, 118, 136, 128,
    216, 30, 183, 148, 170, 28, 55, 28, 12, 223, 57, 93, 87, 182, 90, 158,
    199, 81, 124, 36, 132, 1, 44, 100, 1, 130, 127, 42, 14, 10, 29, 42,
    103, 227, 88, 95, 44, 46, 166, 141, 129, 13, 174, 120, 148, 111, 76, 6,
    10, 71, 10, 255, 174, 123, 65, 183, 207, 179, 235, 168, 120, 37, 107, 93,
    157, 163, 111, 106, 195, 241, 166, 85, 102, 66, 214, 232, 145, 18, 188, 111,
    223, 16, 208, 168, 88, 175, 40, 71, 87, 17, 163, 31, 129, 165, 175, 36,
    108, 38, 197, 65, 14, 174, 204, 178, 163, 133, 102, 202, 16, 44, 43, 157,
    231, 2, 197, 143, 206, 201, 171, 239, 90, 38, 41, 161, 235, 147, 187, 45,
    150, 56, 188, 28, 148, 65, 196, 34, 130, 29, 175, 198, 71, 21, 145, 101,
    158, 202, 206, 221, 222, 194, 91, 149, 117, 125, 108, 17, 236, 174, 146, 122,
    78, 249, 250, 80, 100, 2, 114, 83, 116, 192, 37, 25, 115, 250, 71, 127,
    25, 143, 171, 172, 231, 239, 162, 124, 201, 35, 151, 239, 235, 215, 242, 127,
    103, 72, 113, 36, 220, 29, 204, 10, 240, 99, 201, 218, 117, 110, 124, 158,
    72, 172, 43, 42, 180, 137, 143, 161, 79, 160, 141, 150, 213, 201, 230, 71,
};

constexpr unsigned char OaepLabelCipher[] {
    11, 48, 8, 127, 218, 117, 176, 36, 199, 55, 0, 155, 162, 170, 137, 204,
    55, 186, 19, 42, 169, 210, 61, 89, 242, 240, 151, 253, 47, 157, 243, 85,
    97, 162, 227, 137, 79, 120, 144, 132, 152, 114, 203, 91, 114, 86, 141, 238,
    254, 50, 77, 188, 208, 159, 123, 0, 251, 105, 196, 2, 206, 228, 34, 209,
    0, 237, 48, 48, 183, 9, 69, 227, 228, 31, 112, 206, 167, 205, 104, 2,
    178, 95, 131, 0, 11, 180, 228, 9, 157, 147, 252, 195, 208, 209, 4, 230,
    183, 32, 178, 5, 197, 124, 76, 37, 168, 158, 183, 140, 41, 67, 70, 18,
    220, 22, 19, 183, 234, 111, 159, 244, 7, 79, 168, 97, 114, 8, 117, 36,
    38, 239, 126, 245, 223, 196, 103, 66, 155, 234, 118, 222, 236, 207, 79, 52,
    235, 30, 26, 230, 102, 17, 125, 116, 241, 223, 159, 253, 104, 107, 196, 131,
    218, 1, 87, 20, 95, 217, 47, 208, 87, 116, 5, 254, 215, 29, 64, 218,
    27, 32, 36, 40, 149, 43, 189, 230, 215, 64, 251, 70, 145, 144, 139, 234,
    124, 62, 40, 209, 145, 243, 201, 111, 115, 173, 228, 181, 253, 172, 25, 61,
    194, 91, 40, 96, 248, 186, 190, 99, 156, 208, 77, 72, 97, 43, 167, 232,
    66, 83, 255, 105, 42, 133, 167, 187, 214, 150, 34, 233, 4, 96, 158, 188,
    66, 82, 126, 237, 153, 185, 213, 180, 239, 211, 11, 224, 74, 246, 145, 50,
};

constexpr unsigned char KeyPrimeP[] {
    241, 135, 187, 253, 160, 63, 32, 230, 45, 94, 54, 249, 128, 137, 209, 214,
    98, 8, 74, 106, 186, 196, 34, 212, 122, 1, 147, 28, 223, 155, 175, 234,
    81, 95, 247, 48, 113, 74, 68, 119, 12, 255, 67, 84, 97, 155, 209, 183,
    44, 29, 12, 34, 38, 127, 168, 146, 110, 184, 54, 139, 205, 116, 23, 30,
    57, 109, 6, 223, 161, 245, 253, 27, 225, 25, 124, 90, 178, 50, 61, 254,
    172, 125, 29, 188, 134, 196, 56, 56, 212, 43, 116, 70, 124, 52, 68, 48,
    228, 184, 208, 250, 179, 214, 86, 235, 21, 90, 249, 213, 205, 100, 202, 121,
    71, 92, 113, 129, 41, 15, 159, 53, 240, 112, 253, 11, 15, 158, 74, 163,
};

constexpr unsigned char KeyPrimeQ[] {
    202, 2, 155, 255, 39, 159, 136, 197, 59, 167, 193, 132, 234, 207, 36, 81,
    208, 106, 216, 9, 89, 226, 55, 176, 25, 80, 242, 128, 9, 55, 139, 43,
    111, 123, 132, 81, 133, 237, 162, 156, 155, 117, 14, 166, 52, 197, 210, 242,
    22, 83, 123, 32, 219, 67, 194, 219, 228, 244, 84, 239, 94, 54, 188, 155,
    230, 65, 126, 69, 150, 245, 202, 154, 222, 145, 192, 12, 64, 33, 211, 67,
    34, 74, 75, 76, 152, 185, 178, 161, 169, 124, 171, 109, 10, 138, 18, 132,
    240, 208, 98, 60, 164, 106, 144, 20, 239, 176, 208, 206, 170, 137, 45, 110,
    51, 87, 130, 230, 43, 55, 130, 128, 46, 26, 223, 206, 128, 63, 142, 91,
};

constexpr unsigned char KeyExponentP[] {
    107, 217, 128, 72, 48, 86, 0, 10, 200, 23, 89, 212, 239, 96, 155, 34,
    111, 131, 228, 54, 228, 78, 208, 126, 176, 60, 35, 196, 176, 212, 132, 40,
    205, 77, 92, 195, 116, 211, 4, 223, 100, 30, 87, 162, 70, 51, 157, 225,
    125, 1, 59, 121, 196, 87, 28, 158, 198, 113, 219, 105, 113, 196, 116, 206,
    58, 54, 68, 235, 119, 22, 176, 126, 210, 116, 222, 226, 96, 207, 174, 127,
    25, 230, 132, 112, 186, 124, 195, 222, 28, 95, 138, 252, 46, 89, 232, 89,
    44, 200, 154, 8, 88, 101, 77, 74, 139, 174, 164, 0, 164, 70, 109, 98,
    218, 74, 157, 36, 95, 129, 106, 70, 91, 167, 24, 86, 232, 18, 118, 161,
};

constexpr unsigned char KeyExponentQ[] {
    141, 233, 183, 93, 80, 160, 170, 156, 164, 131, 253, 87, 58, 253, 186, 46,
    31, 148, 111, 160, 98, 28, 161, 178, 124, 150, 121, 69, 169, 51, 243, 248,
    215, 160, 150, 28, 15, 82, 25, 226, 154, 243, 105, 15, 42, 19, 175, 85,
    146, 137, 162, 254, 58, 11, 40, 97, 2, 63, 3, 76, 5, 139, 190, 132,
    103, 5, 171, 135, 135, 189, 180, 79, 135, 252, 44, 70, 110, 231, 84, 90,
    221, 68, 4, 18, 19, 88, 161, 61, 32, 172, 12, 97, 213, 253, 153, 116,
    251, 23, 233, 107, 96, 181, 219, 242, 89, 80, 133, 224, 41, 189, 164, 196,
    190, 28, 196, 64, 158, 43, 168, 234, 19, 121, 157, 176, 156, 56, 9, 241,
};

constexpr unsigned char KeyCoefficient[] {
    114, 10, 141, 5, 172, 81, 202, 45, 176, 79, 122, 252, 92, 74, 44, 173,
    21, 25, 18, 55, 134, 8, 129, 252, 2, 74, 103, 202, 8, 187, 192, 255,
    47, 107, 163, 106, 16, 131, 49, 146, 163, 111, 9, 19, 107, 59, 95, 249,
    213, 67, 93, 67, 246, 241, 167, 146, 113, 51, 88, 113, 127, 243, 146, 113,
    188, 104, 184, 246, 224, 72, 10, 119, 13, 36, 7, 219, 76, 165, 57, 197,
    157, 191, 219, 18, 39, 145, 201, 94, 65, 198, 131, 203, 181, 13, 6, 143,
    171, 141, 173, 138, 39, 103, 56, 243, 127, 133, 208, 50, 18, 179, 30, 16,
    45, 114, 5, 227, 86, 171, 70, 156, 138, 152, 125, 110, 59, 26, 236, 58,
};

const unsigned char* MessageBytes() { return reinterpret_cast<const unsigned char*>(PlainMessage); }

constexpr unsigned char PublicExponent[] {1, 0, 1};
constexpr unsigned char EvenExponent[] {2};
constexpr unsigned char OddExponent[] {3};
constexpr unsigned char UnitExponent[] {1};

LibreSsl3Rsa* NewRsaKey() {
    auto* rsa = RSA_new_nid_disambig1_nid_postfix();
    Require(rsa != nullptr);
    auto* modulus = LibreSsl3::NewRsaBignum_nid_no_patch(KeyModulus, sizeof(KeyModulus));
    auto* exponent = LibreSsl3::NewRsaBignum_nid_no_patch(PublicExponent, sizeof(PublicExponent));
    auto* privateExponent = LibreSsl3::NewRsaBignum_nid_no_patch(KeyPrivateExponent, sizeof(KeyPrivateExponent));
    Require(modulus != nullptr && exponent != nullptr && privateExponent != nullptr);
    Require(RSA_set0_key_nid_postfix(rsa, modulus, exponent, privateExponent) == 1);
    return rsa;
}

LibreSsl3EvpPkey* NewNativeKey(std::int32_t keyType) {
    auto* key = EVP_PKEY_new_nid_disambig1_nid_postfix();
    Require(key != nullptr);
    key->type = keyType;
    key->method = FindNativePkeyAsn1Method_nid_no_patch(keyType);
    Require(key->method != nullptr);
    key->key = NewRsaKey();
    Require(key->key != nullptr);
    return key;
}

void ReleaseKey(LibreSsl3EvpPkey* key) {
    EVP_PKEY_free_nid_disambig1_nid_postfix(key);
}

void AlgorithmMethodSelection() {
    Require(EVP_PKEY_CTX_new_nid_postfix(nullptr, nullptr) == nullptr);
    Require(ERR_get_error_nid_postfix() == 0);
    auto* withoutMethod = EVP_PKEY_new_nid_disambig1_nid_postfix();
    Require(withoutMethod != nullptr);
    Require(EVP_PKEY_CTX_new_nid_postfix(withoutMethod, nullptr) == nullptr);
    Require(ERR_get_error_nid_postfix() == 0);
    LibreSsl3PkeyAsn1Method foreign {};
    foreign.keyType = 116;
    auto* foreignMethod = EVP_PKEY_new_nid_disambig1_nid_postfix();
    Require(foreignMethod != nullptr);
    foreignMethod->type = 116;
    foreignMethod->method = &foreign;
    Require(EVP_PKEY_CTX_new_nid_postfix(foreignMethod, nullptr) == nullptr);
    ExpectErrors({EvpCode(156)});
    foreignMethod->method = nullptr;
    ReleaseKey(withoutMethod);
    ReleaseKey(foreignMethod);
    EVP_PKEY_CTX_free_nid_postfix(nullptr);
}

void OperationGuards() {
    auto* key = NewNativeKey(LibreSsl3PkeyRsa);
    Require(EVP_PKEY_size_nid_postfix(key) == 256);
    Require(EVP_PKEY_bits_nid_postfix(key) == 2048);
    auto* context = EVP_PKEY_CTX_new_nid_postfix(key, nullptr);
    Require(context != nullptr);
    Require(context->method == FindNativePkeyMethod_nid_no_patch(LibreSsl3PkeyRsa));
    Require(context->key == key);
    Require(context->peerKey == nullptr);
    Require(context->operation == LibreSsl3PkeyOperationUndefined);
    Require(context->data != nullptr);
    Require(context->generationInformation ==
        static_cast<void*>(static_cast<LibreSsl3RsaKeyContext*>(context->data)->generationTemporary));
    Require(context->generationInformationCount == 2);
    Require(key->references == 2);
    std::array<unsigned char, 256> buffer {};
    std::size_t length = 0;
    Require(EVP_PKEY_encrypt_nid_postfix(context, buffer.data(), &length, MessageBytes(), MessageLength) == -1);
    ExpectErrors({EvpCode(151)});
    Require(EVP_PKEY_decrypt_nid_postfix(context, buffer.data(), &length, Pkcs1Cipher, sizeof(Pkcs1Cipher)) == -1);
    ExpectErrors({EvpCode(151)});
    int padding = 0;
    Require(EVP_PKEY_CTX_ctrl_nid_postfix(context, LibreSsl3PkeyRsa, -1, LibreSsl3PkeyControlGetRsaPadding, 0,
        &padding) == -1);
    ExpectErrors({EvpCode(149)});
    Require(EVP_PKEY_encrypt_init_nid_postfix(context) == 1);
    Require(context->operation == LibreSsl3PkeyOperationEncrypt);
    Require(EVP_PKEY_CTX_ctrl_nid_postfix(context, 116, -1, LibreSsl3PkeyControlGetRsaPadding, 0, &padding) == -1);
    Require(ERR_get_error_nid_postfix() == 0);
    Require(EVP_PKEY_CTX_ctrl_nid_postfix(context, -1, LibreSsl3PkeyOperationTypeSignature,
        LibreSsl3PkeyControlMessageDigest, 0, Pointer(EVP_sha1_nid_postfix())) == -1);
    ExpectErrors({EvpCode(148)});
    Require(Throws([&] { EVP_PKEY_encrypt_nid_postfix(context, buffer.data(), nullptr, MessageBytes(), MessageLength); }));
    length = 0;
    Require(EVP_PKEY_encrypt_nid_postfix(context, nullptr, &length, MessageBytes(), MessageLength) == 1);
    Require(length == 256);
    Require(ERR_get_error_nid_postfix() == 0);
    length = 255;
    Require(EVP_PKEY_encrypt_nid_postfix(context, buffer.data(), &length, MessageBytes(), MessageLength) == 0);
    ExpectErrors({EvpCode(155)});
    Require(EVP_PKEY_CTX_ctrl_nid_postfix(context, LibreSsl3PkeyRsa, LibreSsl3PkeyOperationTypeCrypt,
        LibreSsl3PkeyControlGetRsaPadding, 0, &padding) == 1);
    Require(padding == LibreSsl3RsaPaddingPkcs1);
    EVP_PKEY_CTX_free_nid_postfix(context);
    Require(key->references == 1);
    ReleaseKey(key);
}

void Pkcs1Vector() {
    auto* key = NewNativeKey(LibreSsl3PkeyRsa);
    auto* context = EVP_PKEY_CTX_new_nid_postfix(key, nullptr);
    Require(context != nullptr);
    Require(EVP_PKEY_encrypt_init_nid_postfix(context) == 1);
    std::array<unsigned char, 256> cipher {};
    std::size_t length = cipher.size();
    Require(EVP_PKEY_encrypt_nid_postfix(context, cipher.data(), &length, MessageBytes(), MessageLength) == 1);
    Require(length == 256);
    Require(EVP_PKEY_decrypt_init_nid_postfix(context) == 1);
    Require(context->operation == LibreSsl3PkeyOperationDecrypt);
    std::array<unsigned char, 256> plain {};
    length = plain.size();
    Require(EVP_PKEY_decrypt_nid_postfix(context, plain.data(), &length, cipher.data(), cipher.size()) == 1);
    Require(length == MessageLength);
    Require(std::memcmp(plain.data(), PlainMessage, MessageLength) == 0);
    length = plain.size();
    Require(EVP_PKEY_decrypt_nid_postfix(context, plain.data(), &length, Pkcs1Cipher, sizeof(Pkcs1Cipher)) == 1);
    Require(length == MessageLength);
    Require(std::memcmp(plain.data(), PlainMessage, MessageLength) == 0);
    std::array<unsigned char, 256> zeros {};
    length = plain.size();
    Require(EVP_PKEY_decrypt_nid_postfix(context, plain.data(), &length, zeros.data(), zeros.size()) == -1);
    Require(ERR_peek_last_error_nid_postfix() == RsaCode(114));
    ExpectRsaQueue();
    std::array<unsigned char, 300> oversized {};
    length = plain.size();
    Require(EVP_PKEY_decrypt_nid_postfix(context, plain.data(), &length, oversized.data(), oversized.size()) == -1);
    ExpectErrors({RsaCode(108)});
    EVP_PKEY_CTX_free_nid_postfix(context);
    ReleaseKey(key);
}

void OaepDigestVectors() {
    auto* key = NewNativeKey(LibreSsl3PkeyRsa);
    auto* context = EVP_PKEY_CTX_new_nid_postfix(key, nullptr);
    Require(context != nullptr);
    Require(EVP_PKEY_encrypt_init_nid_postfix(context) == 1);
    const LibreSsl3::DigestMethod* digest = nullptr;
    Require(EVP_PKEY_CTX_ctrl_nid_postfix(context, LibreSsl3PkeyRsa, -1, LibreSsl3PkeyControlRsaOaepDigest, 0,
        Pointer(EVP_sha256_nid_postfix())) == -2);
    ExpectErrors({RsaCode(141), EvpCode(147)});
    Require(EVP_PKEY_CTX_ctrl_nid_postfix(context, LibreSsl3PkeyRsa, -1, LibreSsl3PkeyControlRsaPadding,
        LibreSsl3RsaPaddingOaep, nullptr) == 1);
    Require(EVP_PKEY_CTX_ctrl_nid_postfix(context, LibreSsl3PkeyRsa, -1, LibreSsl3PkeyControlGetMessageDigest, 0,
        &digest) == 1);
    Require(Pointer(digest) == Pointer(EVP_sha1_nid_postfix()));
    std::array<unsigned char, 256> cipher {};
    std::size_t length = cipher.size();
    Require(EVP_PKEY_decrypt_init_nid_postfix(context) == 1);
    Require(EVP_PKEY_decrypt_nid_postfix(context, nullptr, &length, OaepSha1Cipher, sizeof(OaepSha1Cipher)) == 1);
    Require(length == 256);
    length = cipher.size();
    std::array<unsigned char, 256> plain {};
    Require(EVP_PKEY_decrypt_nid_postfix(context, plain.data(), &length, OaepSha1Cipher, sizeof(OaepSha1Cipher)) == 1);
    Require(length == MessageLength);
    Require(std::memcmp(plain.data(), PlainMessage, MessageLength) == 0);
    Require(EVP_PKEY_CTX_ctrl_nid_postfix(context, LibreSsl3PkeyRsa, -1, LibreSsl3PkeyControlRsaOaepDigest, 0,
        Pointer(EVP_sha256_nid_postfix())) == 1);
    Require(EVP_PKEY_CTX_ctrl_nid_postfix(context, LibreSsl3PkeyRsa, -1, LibreSsl3PkeyControlGetRsaOaepDigest, 0,
        &digest) == 1);
    Require(Pointer(digest) == Pointer(EVP_sha256_nid_postfix()));
    Require(EVP_PKEY_CTX_ctrl_nid_postfix(context, LibreSsl3PkeyRsa, -1, LibreSsl3PkeyControlRsaMaskDigest, 0,
        Pointer(EVP_sha256_nid_postfix())) == 1);
    Require(EVP_PKEY_CTX_ctrl_nid_postfix(context, LibreSsl3PkeyRsa, -1, LibreSsl3PkeyControlGetRsaMaskDigest, 0,
        &digest) == 1);
    Require(Pointer(digest) == Pointer(EVP_sha256_nid_postfix()));
    Require(EVP_PKEY_decrypt_init_nid_postfix(context) == 1);
    length = plain.size();
    Require(EVP_PKEY_decrypt_nid_postfix(context, plain.data(), &length, OaepSha256Cipher,
        sizeof(OaepSha256Cipher)) == 1);
    Require(length == MessageLength);
    Require(std::memcmp(plain.data(), PlainMessage, MessageLength) == 0);
    Require(EVP_PKEY_CTX_ctrl_nid_postfix(context, LibreSsl3PkeyRsa, -1, LibreSsl3PkeyControlRsaMaskDigest, 0,
        Pointer(EVP_sha224_nid_postfix())) == 1);
    Require(EVP_PKEY_CTX_ctrl_nid_postfix(context, LibreSsl3PkeyRsa, -1, LibreSsl3PkeyControlGetRsaMaskDigest, 0,
        &digest) == 1);
    Require(Pointer(digest) == Pointer(EVP_sha224_nid_postfix()));
    Require(EVP_PKEY_encrypt_init_nid_postfix(context) == 1);
    length = cipher.size();
    Require(EVP_PKEY_encrypt_nid_postfix(context, cipher.data(), &length, MessageBytes(), MessageLength) == 1);
    Require(length == 256);
    Require(EVP_PKEY_decrypt_init_nid_postfix(context) == 1);
    length = plain.size();
    Require(EVP_PKEY_decrypt_nid_postfix(context, plain.data(), &length, cipher.data(), cipher.size()) == 1);
    Require(length == MessageLength);
    Require(std::memcmp(plain.data(), PlainMessage, MessageLength) == 0);
    EVP_PKEY_CTX_free_nid_postfix(context);
    ReleaseKey(key);
}

void OaepLabelVector() {
    auto* key = NewNativeKey(LibreSsl3PkeyRsa);
    auto* context = EVP_PKEY_CTX_new_nid_postfix(key, nullptr);
    Require(context != nullptr);
    Require(EVP_PKEY_encrypt_init_nid_postfix(context) == 1);
    auto* rejected = static_cast<unsigned char*>(malloc_nid_postfix(5));
    Require(rejected != nullptr);
    Require(EVP_PKEY_CTX_ctrl_nid_postfix(context, LibreSsl3PkeyRsa, -1, LibreSsl3PkeyControlRsaOaepLabel, 5,
        rejected) == -2);
    ExpectErrors({RsaCode(141), EvpCode(147)});
    free_nid_postfix(rejected);
    Require(EVP_PKEY_CTX_ctrl_nid_postfix(context, LibreSsl3PkeyRsa, -1, LibreSsl3PkeyControlRsaPadding,
        LibreSsl3RsaPaddingOaep, nullptr) == 1);
    auto* label = static_cast<unsigned char*>(malloc_nid_postfix(5));
    Require(label != nullptr);
    std::memcpy(label, "label", 5);
    Require(EVP_PKEY_CTX_ctrl_nid_postfix(context, LibreSsl3PkeyRsa, -1, LibreSsl3PkeyControlRsaOaepLabel, 5,
        label) == 1);
    unsigned char* stored = nullptr;
    Require(EVP_PKEY_CTX_ctrl_nid_postfix(context, LibreSsl3PkeyRsa, -1, LibreSsl3PkeyControlGetRsaOaepLabel, 0,
        &stored) == 5);
    Require(stored == label);
    Require(EVP_PKEY_decrypt_init_nid_postfix(context) == 1);
    std::array<unsigned char, 256> plain {};
    std::size_t length = plain.size();
    Require(EVP_PKEY_decrypt_nid_postfix(context, plain.data(), &length, OaepLabelCipher, sizeof(OaepLabelCipher)) == 1);
    Require(length == MessageLength);
    Require(std::memcmp(plain.data(), PlainMessage, MessageLength) == 0);
    Require(EVP_PKEY_CTX_ctrl_nid_postfix(context, LibreSsl3PkeyRsa, -1, LibreSsl3PkeyControlRsaOaepLabel, 0,
        nullptr) == 1);
    Require(EVP_PKEY_CTX_ctrl_nid_postfix(context, LibreSsl3PkeyRsa, -1, LibreSsl3PkeyControlGetRsaOaepLabel, 0,
        &stored) == 0);
    Require(stored == nullptr);
    EVP_PKEY_CTX_free_nid_postfix(context);
    ReleaseKey(key);
}

void WithoutPadding() {
    auto* key = NewNativeKey(LibreSsl3PkeyRsa);
    auto* context = EVP_PKEY_CTX_new_nid_postfix(key, nullptr);
    Require(context != nullptr);
    Require(EVP_PKEY_encrypt_init_nid_postfix(context) == 1);
    Require(EVP_PKEY_CTX_ctrl_nid_postfix(context, LibreSsl3PkeyRsa, -1, LibreSsl3PkeyControlRsaPadding,
        LibreSsl3RsaPaddingNone, nullptr) == 1);
    Require(EVP_PKEY_CTX_ctrl_nid_postfix(context, LibreSsl3PkeyRsa, -1, LibreSsl3PkeyControlMessageDigest, 0,
        Pointer(EVP_sha1_nid_postfix())) == 0);
    ExpectErrors({RsaCode(141)});
    std::array<unsigned char, 256> block {};
    std::memcpy(block.data(), KeyModulus, sizeof(KeyModulus));
    block[255] = static_cast<unsigned char>(block[255] - 1);
    std::array<unsigned char, 256> cipher {};
    std::size_t length = cipher.size();
    Require(EVP_PKEY_encrypt_nid_postfix(context, cipher.data(), &length, block.data(), block.size()) == 1);
    Require(length == 256);
    Require(EVP_PKEY_decrypt_init_nid_postfix(context) == 1);
    std::array<unsigned char, 256> plain {};
    length = plain.size();
    Require(EVP_PKEY_decrypt_nid_postfix(context, plain.data(), &length, cipher.data(), cipher.size()) == 1);
    Require(length == 256);
    Require(std::memcmp(plain.data(), block.data(), block.size()) == 0);
    EVP_PKEY_CTX_free_nid_postfix(context);
    ReleaseKey(key);
}

void RsaPaddingControls() {
    auto* key = NewNativeKey(LibreSsl3PkeyRsa);
    auto* context = EVP_PKEY_CTX_new_nid_postfix(key, nullptr);
    Require(context != nullptr);
    Require(EVP_PKEY_encrypt_init_nid_postfix(context) == 1);
    int padding = 0;
    const LibreSsl3::DigestMethod* digest = nullptr;
    Require(EVP_PKEY_CTX_ctrl_nid_postfix(context, LibreSsl3PkeyRsa, -1, LibreSsl3PkeyControlRsaPadding,
        LibreSsl3RsaPaddingPss, nullptr) == -2);
    ExpectErrors({RsaCode(144), EvpCode(147)});
    Require(EVP_PKEY_CTX_ctrl_nid_postfix(context, LibreSsl3PkeyRsa, -1, LibreSsl3PkeyControlRsaPadding,
        LibreSsl3RsaPaddingX931, nullptr) == 1);
    Require(EVP_PKEY_CTX_ctrl_nid_postfix(context, LibreSsl3PkeyRsa, -1, LibreSsl3PkeyControlGetRsaPadding, 0,
        &padding) == 1);
    Require(padding == LibreSsl3RsaPaddingX931);
    Require(EVP_PKEY_CTX_ctrl_nid_postfix(context, LibreSsl3PkeyRsa, -1, LibreSsl3PkeyControlRsaPadding,
        LibreSsl3RsaPaddingPkcs1, nullptr) == 1);
    int saltLength = 0;
    Require(EVP_PKEY_CTX_ctrl_nid_postfix(context, LibreSsl3PkeyRsa, -1, LibreSsl3PkeyControlRsaPssSaltLength, 20,
        nullptr) == -2);
    ExpectErrors({RsaCode(146), EvpCode(147)});
    Require(EVP_PKEY_CTX_ctrl_nid_postfix(context, LibreSsl3PkeyRsa, -1, LibreSsl3PkeyControlGetRsaPssSaltLength, 0,
        &saltLength) == -2);
    ExpectErrors({RsaCode(146), EvpCode(147)});
    Require(EVP_PKEY_CTX_ctrl_nid_postfix(context, LibreSsl3PkeyRsa, -1, LibreSsl3PkeyControlRsaKeyGenerationBits,
        256, nullptr) == -2);
    ExpectErrors({RsaCode(120), EvpCode(147)});
    Require(EVP_PKEY_CTX_ctrl_nid_postfix(context, LibreSsl3PkeyRsa, -1, LibreSsl3PkeyControlRsaKeyGenerationBits,
        1024, nullptr) == 1);
    Require(EVP_PKEY_CTX_ctrl_nid_postfix(context, LibreSsl3PkeyRsa, -1, LibreSsl3PkeyControlRsaMaskDigest, 0,
        Pointer(EVP_sha224_nid_postfix())) == -2);
    ExpectErrors({RsaCode(156), EvpCode(147)});
    LibreSsl3::DigestMethod unsupported {};
    unsupported.type = 999;
    Require(EVP_PKEY_CTX_ctrl_nid_postfix(context, LibreSsl3PkeyRsa, -1, LibreSsl3PkeyControlMessageDigest, 0,
        &unsupported) == 0);
    ExpectErrors({RsaCode(157)});
    Require(EVP_PKEY_CTX_ctrl_nid_postfix(context, LibreSsl3PkeyRsa, -1, LibreSsl3PkeyControlMessageDigest, 0,
        Pointer(EVP_sha256_nid_postfix())) == 1);
    Require(EVP_PKEY_CTX_ctrl_nid_postfix(context, LibreSsl3PkeyRsa, -1, LibreSsl3PkeyControlGetMessageDigest, 0,
        &digest) == 1);
    Require(Pointer(digest) == Pointer(EVP_sha256_nid_postfix()));
    Require(EVP_PKEY_CTX_ctrl_nid_postfix(context, LibreSsl3PkeyRsa, -1, LibreSsl3PkeyControlPeerKey, 0,
        Pointer(key)) == -2);
    ExpectErrors({RsaCode(148), EvpCode(147)});
    Require(EVP_PKEY_CTX_ctrl_nid_postfix(context, LibreSsl3PkeyRsa, -1, LibreSsl3PkeyControlPkcs7Encrypt, 0,
        nullptr) == 1);
    Require(EVP_PKEY_CTX_ctrl_nid_postfix(context, LibreSsl3PkeyRsa, -1, LibreSsl3PkeyControlDigestInit, 0,
        nullptr) == 1);
    Require(EVP_PKEY_CTX_ctrl_nid_postfix(context, LibreSsl3PkeyRsa, -1, LibreSsl3PkeyControlCipher, 0,
        nullptr) == -2);
    ExpectErrors({EvpCode(147)});
    auto* even = LibreSsl3::NewRsaBignum_nid_no_patch(EvenExponent, sizeof(EvenExponent));
    Require(even != nullptr);
    Require(EVP_PKEY_CTX_ctrl_nid_postfix(context, LibreSsl3PkeyRsa, -1,
        LibreSsl3PkeyControlRsaKeyGenerationPublicExponent, 0, even) == -2);
    ExpectErrors({RsaCode(101), EvpCode(147)});
    BN_clear_free_nid_postfix(even);
    auto* unit = LibreSsl3::NewRsaBignum_nid_no_patch(UnitExponent, sizeof(UnitExponent));
    Require(unit != nullptr);
    Require(EVP_PKEY_CTX_ctrl_nid_postfix(context, LibreSsl3PkeyRsa, -1,
        LibreSsl3PkeyControlRsaKeyGenerationPublicExponent, 0, unit) == -2);
    ExpectErrors({RsaCode(101), EvpCode(147)});
    BN_clear_free_nid_postfix(unit);
    auto* odd = LibreSsl3::NewRsaBignum_nid_no_patch(OddExponent, sizeof(OddExponent));
    Require(odd != nullptr);
    Require(EVP_PKEY_CTX_ctrl_nid_postfix(context, LibreSsl3PkeyRsa, -1,
        LibreSsl3PkeyControlRsaKeyGenerationPublicExponent, 0, odd) == 1);
    auto* replacement = LibreSsl3::NewRsaBignum_nid_no_patch(OddExponent, sizeof(OddExponent));
    Require(replacement != nullptr);
    Require(EVP_PKEY_CTX_ctrl_nid_postfix(context, LibreSsl3PkeyRsa, -1,
        LibreSsl3PkeyControlRsaKeyGenerationPublicExponent, 0, replacement) == 1);
    EVP_PKEY_CTX_free_nid_postfix(context);
    ReleaseKey(key);
}

void RestrictedPssKey() {
    auto* key = NewNativeKey(LibreSsl3PkeyRsaPss);
    Require(EVP_PKEY_size_nid_postfix(key) == 256);
    Require(EVP_PKEY_bits_nid_postfix(key) == 2048);
    auto* context = EVP_PKEY_CTX_new_nid_postfix(key, nullptr);
    Require(context != nullptr);
    Require(context->method == FindNativePkeyMethod_nid_no_patch(LibreSsl3PkeyRsaPss));
    Require(EVP_PKEY_encrypt_init_nid_postfix(context) == -2);
    ExpectErrors({EvpCode(150)});
    Require(EVP_PKEY_decrypt_init_nid_postfix(context) == -2);
    ExpectErrors({EvpCode(150)});
    Require(context->operation == LibreSsl3PkeyOperationUndefined);
    std::array<unsigned char, 256> plain {};
    std::size_t length = plain.size();
    Require(EVP_PKEY_encrypt_nid_postfix(context, plain.data(), &length, MessageBytes(), MessageLength) == -2);
    ExpectErrors({EvpCode(150)});
    EVP_PKEY_CTX_free_nid_postfix(context);
    ReleaseKey(key);
}

constexpr unsigned char DigestSha1[] {
    2, 143, 154, 225, 251, 141, 54, 37, 60, 176, 242, 166, 156, 38, 75, 29, 11, 9, 101, 3
};

constexpr unsigned char DigestSha224[] {
    141, 232, 75, 114, 125, 142, 232, 40, 218, 153, 99, 248, 121, 68, 42, 102, 70, 207, 163, 152, 32, 3, 255,
    232, 21, 102, 154, 215
};

constexpr unsigned char DigestSha256[] {
    159, 137, 3, 84, 147, 61, 171, 50, 102, 91, 179, 55, 243, 194, 108, 237, 74, 228, 98, 53, 69, 206, 21, 216,
    3, 0, 173, 128, 130, 189, 76, 120
};

constexpr unsigned char Pkcs1Sha1Signature[] {
    93, 96, 74, 143, 12, 220, 18, 48, 178, 233, 56, 115, 7, 175, 68, 84, 251, 4, 114, 147, 116, 93, 19, 250, 193,
    4, 173, 2, 121, 17, 86, 16, 61, 110, 39, 156, 93, 183, 54, 162, 16, 145, 80, 239, 172, 115, 233, 32, 168, 7,
    46, 196, 208, 25, 41, 147, 247, 156, 62, 46, 171, 240, 158, 13, 123, 136, 41, 7, 159, 222, 159, 221, 156, 3,
    131, 74, 242, 144, 133, 215, 36, 217, 42, 23, 69, 56, 197, 194, 35, 205, 202, 21, 139, 248, 5, 242, 240, 179,
    124, 178, 28, 78, 157, 161, 225, 64, 56, 170, 42, 30, 122, 92, 134, 100, 51, 131, 112, 2, 85, 165, 200, 127,
    49, 161, 165, 95, 93, 91, 125, 226, 225, 11, 166, 190, 199, 218, 42, 8, 220, 206, 99, 54, 129, 162, 45, 230,
    40, 44, 201, 70, 120, 116, 226, 64, 142, 65, 87, 213, 122, 40, 233, 147, 106, 16, 28, 102, 223, 28, 220, 217,
    8, 168, 183, 189, 155, 236, 49, 137, 152, 249, 138, 169, 158, 224, 29, 215, 212, 69, 134, 136, 104, 66, 118, 38,
    116, 85, 84, 32, 112, 26, 56, 75, 46, 33, 226, 228, 114, 86, 242, 153, 87, 133, 36, 123, 244, 252, 84, 187,
    77, 106, 102, 112, 190, 74, 78, 49, 239, 141, 43, 173, 244, 228, 102, 2, 91, 240, 232, 26, 151, 164, 34, 247,
    177, 97, 7, 145, 127, 156, 159, 171, 217, 79, 112, 237, 11, 175
};

constexpr unsigned char Pkcs1Sha224Signature[] {
    86, 85, 59, 70, 236, 245, 70, 171, 199, 81, 196, 37, 227, 43, 168, 110, 25, 114, 54, 71, 71, 203, 28, 233,
    249, 181, 186, 201, 132, 124, 244, 229, 27, 136, 37, 183, 110, 239, 91, 144, 212, 133, 42, 97, 191, 145, 123,
    99, 7, 70, 153, 55, 234, 63, 119, 85, 161, 120, 9, 212, 18, 18, 226, 105, 26, 45, 117, 34, 239, 144, 49, 137,
    16, 22, 198, 203, 243, 164, 239, 198, 97, 123, 8, 209, 134, 66, 126, 19, 118, 186, 246, 123, 244, 98, 112, 231,
    114, 203, 215, 111, 161, 194, 230, 128, 249, 194, 61, 101, 2, 49, 12, 137, 89, 140, 183, 201, 148, 91, 57, 40,
    244, 49, 170, 48, 101, 123, 121, 218, 103, 62, 21, 71, 250, 25, 60, 147, 43, 124, 118, 226, 144, 84, 226, 251,
    90, 204, 244, 217, 150, 58, 176, 139, 38, 178, 28, 81, 143, 8, 227, 71, 130, 188, 121, 100, 25, 152, 205, 33,
    102, 123, 125, 43, 19, 186, 139, 1, 219, 228, 56, 1, 114, 225, 144, 116, 207, 193, 168, 120, 127, 157, 153,
    159, 255, 83, 164, 148, 128, 169, 133, 50, 229, 180, 194, 65, 54, 65, 239, 94, 236, 144, 107, 105, 185, 152,
    131, 241, 133, 137, 94, 191, 208, 222, 251, 41, 119, 13, 106, 106, 144, 120, 58, 248, 120, 220, 140, 157, 248,
    210, 110, 72, 129, 216, 146, 210, 217, 98, 97, 168, 99, 20, 203, 197, 27, 78, 5, 1
};

constexpr unsigned char Pkcs1Sha256Signature[] {
    180, 68, 146, 161, 91, 165, 160, 232, 147, 133, 150, 4, 67, 34, 201, 168, 55, 44, 56, 141, 249, 241, 86, 83,
    99, 48, 140, 86, 141, 128, 24, 203, 219, 106, 140, 216, 202, 232, 190, 139, 23, 89, 214, 252, 124, 240, 93,
    103, 85, 119, 177, 92, 44, 196, 198, 9, 97, 226, 42, 3, 122, 103, 104, 117, 109, 88, 218, 230, 116, 93, 25,
    233, 140, 222, 19, 71, 56, 163, 100, 62, 134, 106, 176, 157, 229, 123, 121, 80, 240, 32, 15, 246, 231, 71, 91,
    43, 96, 206, 141, 123, 54, 241, 246, 182, 209, 171, 112, 255, 229, 160, 30, 134, 132, 228, 183, 20, 136, 233,
    81, 53, 244, 145, 47, 114, 209, 85, 255, 111, 94, 253, 155, 79, 86, 207, 86, 171, 152, 111, 147, 29, 12, 192,
    216, 151, 253, 149, 184, 107, 86, 83, 43, 54, 57, 164, 205, 210, 38, 132, 137, 146, 6, 104, 128, 172, 44, 22,
    73, 27, 201, 32, 54, 16, 140, 240, 128, 49, 118, 2, 193, 159, 21, 104, 188, 84, 128, 53, 146, 53, 160, 149,
    12, 90, 211, 196, 195, 143, 204, 65, 254, 115, 21, 240, 45, 19, 142, 216, 146, 130, 107, 154, 61, 102, 241,
    64, 21, 103, 192, 228, 42, 200, 21, 173, 171, 71, 193, 68, 197, 69, 137, 67, 132, 208, 119, 198, 27, 82, 12,
    172, 205, 5, 236, 1, 101, 69, 188, 212, 71, 31, 7, 119, 73, 175, 75, 2, 178, 242
};

constexpr unsigned char PssSha256Salt20Signature[] {
    118, 63, 202, 130, 192, 51, 246, 190, 130, 224, 113, 143, 82, 61, 8, 173, 204, 105, 114, 75, 117, 138, 147,
    222, 106, 134, 117, 112, 180, 93, 32, 124, 197, 199, 81, 120, 238, 173, 41, 120, 242, 180, 63, 61, 103, 178,
    204, 183, 124, 69, 145, 135, 160, 111, 48, 47, 17, 37, 185, 208, 188, 145, 68, 58, 41, 54, 140, 234, 16, 122,
    228, 91, 53, 223, 118, 164, 43, 4, 71, 142, 227, 205, 119, 31, 170, 210, 115, 62, 55, 200, 115, 160, 3, 76, 52,
    25, 95, 102, 210, 251, 66, 204, 58, 43, 207, 115, 59, 15, 131, 162, 29, 97, 105, 21, 159, 32, 157, 51, 24, 227,
    175, 187, 89, 91, 84, 37, 105, 246, 179, 110, 86, 0, 45, 97, 226, 152, 69, 54, 129, 105, 10, 212, 252, 64, 196,
    229, 123, 184, 193, 102, 31, 222, 147, 217, 98, 90, 228, 218, 0, 223, 37, 250, 223, 161, 209, 52, 95, 31, 95,
    98, 118, 83, 187, 206, 42, 54, 170, 147, 44, 101, 244, 82, 178, 110, 88, 243, 45, 10, 226, 113, 207, 21, 180,
    35, 80, 204, 215, 82, 19, 101, 60, 227, 102, 240, 233, 196, 23, 154, 168, 207, 102, 126, 215, 78, 50, 214, 162,
    147, 182, 167, 137, 112, 3, 212, 94, 155, 3, 125, 68, 13, 19, 108, 32, 16, 47, 80, 55, 139, 38, 219, 73, 140,
    30, 137, 18, 168, 60, 51, 223, 162, 187, 122, 130, 234, 141, 189
};

LibreSsl3EvpPkey* NewSigningKey(bool factors, bool crt, bool withPrivateExponent) {
    auto* key = EVP_PKEY_new_nid_disambig1_nid_postfix();
    Require(key != nullptr);
    key->type = LibreSsl3PkeyRsa;
    key->method = FindNativePkeyAsn1Method_nid_no_patch(LibreSsl3PkeyRsa);
    Require(key->method != nullptr);
    auto* rsa = RSA_new_nid_disambig1_nid_postfix();
    Require(rsa != nullptr);
    key->key = rsa;
    auto* modulus = LibreSsl3::NewRsaBignum_nid_no_patch(KeyModulus, sizeof(KeyModulus));
    auto* exponent = LibreSsl3::NewRsaBignum_nid_no_patch(PublicExponent, sizeof(PublicExponent));
    Require(modulus != nullptr && exponent != nullptr);
    auto* privateExponent = withPrivateExponent ?
        LibreSsl3::NewRsaBignum_nid_no_patch(KeyPrivateExponent, sizeof(KeyPrivateExponent)) :
        nullptr;
    Require(RSA_set0_key_nid_postfix(rsa, modulus, exponent, privateExponent) == 1);
    if (factors) {
        auto* p = LibreSsl3::NewRsaBignum_nid_no_patch(KeyPrimeP, sizeof(KeyPrimeP));
        auto* q = LibreSsl3::NewRsaBignum_nid_no_patch(KeyPrimeQ, sizeof(KeyPrimeQ));
        Require(p != nullptr && q != nullptr);
        Require(RSA_set0_factors_nid_postfix(rsa, p, q) == 1);
        if (crt) {
            auto* dmp1 = LibreSsl3::NewRsaBignum_nid_no_patch(KeyExponentP, sizeof(KeyExponentP));
            auto* dmq1 = LibreSsl3::NewRsaBignum_nid_no_patch(KeyExponentQ, sizeof(KeyExponentQ));
            auto* iqmp = LibreSsl3::NewRsaBignum_nid_no_patch(KeyCoefficient, sizeof(KeyCoefficient));
            Require(dmp1 != nullptr && dmq1 != nullptr && iqmp != nullptr);
            Require(RSA_set0_crt_params_nid_postfix(rsa, dmp1, dmq1, iqmp) == 1);
        }
    }
    return key;
}

std::array<unsigned char, 256> Flipped(const unsigned char* bytes, std::size_t index) {
    std::array<unsigned char, 256> result {};
    std::memcpy(result.data(), bytes, 256);
    result[index] ^= 0x01;
    return result;
}

LibreSsl3PkeyContext* SignatureContext(LibreSsl3EvpPkey* key, bool sign, const void* digest) {
    auto* context = EVP_PKEY_CTX_new_nid_postfix(key, nullptr);
    Require(context != nullptr);
    if (sign) {
        Require(EVP_PKEY_sign_init_nid_postfix(context) == 1);
    } else {
        Require(EVP_PKEY_verify_init_nid_postfix(context) == 1);
    }
    if (digest != nullptr) {
        Require(EVP_PKEY_CTX_ctrl_nid_postfix(context, LibreSsl3PkeyRsa, -1,
            LibreSsl3PkeyControlMessageDigest, 0, Pointer(digest)) == 1);
    }
    return context;
}

void Pkcs1Signatures() {
    struct {
        const char* name;
        const void* digest;
        const unsigned char* digestBytes;
        std::size_t digestSize;
        const unsigned char* signature;
    } tests[] = {
        {"SHA-1", EVP_sha1_nid_postfix(), DigestSha1, sizeof(DigestSha1), Pkcs1Sha1Signature},
        {"SHA-224", EVP_sha224_nid_postfix(), DigestSha224, sizeof(DigestSha224), Pkcs1Sha224Signature},
        {"SHA-256", EVP_sha256_nid_postfix(), DigestSha256, sizeof(DigestSha256), Pkcs1Sha256Signature},
    };
    for (const auto& test : tests) {
        auto* key = NewNativeKey(LibreSsl3PkeyRsa);
        auto* context = SignatureContext(key, true, test.digest);
        std::array<unsigned char, 256> buffer {};
        std::size_t length = buffer.size();
        Require(EVP_PKEY_sign_nid_postfix(context, nullptr, &length, test.digestBytes, test.digestSize) == 1);
        Require(length == 256);
        length = buffer.size();
        Require(EVP_PKEY_sign_nid_postfix(context, buffer.data(), &length, test.digestBytes, test.digestSize) == 1);
        Require(length == 256);
        Require(std::memcmp(buffer.data(), test.signature, 256) == 0);
        EVP_PKEY_CTX_free_nid_postfix(context);
        auto* verifyContext = SignatureContext(key, false, test.digest);
        Require(EVP_PKEY_verify_nid_postfix(verifyContext, test.signature, 256, test.digestBytes,
            test.digestSize) == 1);
        EVP_PKEY_CTX_free_nid_postfix(verifyContext);
        ReleaseKey(key);
    }
    auto* key = NewNativeKey(LibreSsl3PkeyRsa);
    auto* context = SignatureContext(key, true, EVP_sha1_nid_postfix());
    std::array<unsigned char, 255> shortBuffer {};
    std::size_t length = 255;
    Require(EVP_PKEY_sign_nid_postfix(context, shortBuffer.data(), &length, DigestSha1, sizeof(DigestSha1)) == 0);
    ExpectErrors({EvpCode(155)});
    EVP_PKEY_CTX_free_nid_postfix(context);
    context = SignatureContext(key, true, EVP_sha1_nid_postfix());
    std::array<unsigned char, 256> signature {};
    length = signature.size();
    Require(EVP_PKEY_sign_nid_postfix(context, signature.data(), &length, DigestSha1, sizeof(DigestSha1) - 1) == -1);
    ExpectErrors({RsaCode(143)});
    EVP_PKEY_CTX_free_nid_postfix(context);
    ReleaseKey(key);
}

void Pkcs1ChineseRemainderSignature() {
    auto* key = NewSigningKey(true, true, false);
    auto* context = SignatureContext(key, true, EVP_sha1_nid_postfix());
    std::array<unsigned char, 256> signature {};
    std::size_t length = signature.size();
    Require(EVP_PKEY_sign_nid_postfix(context, signature.data(), &length, DigestSha1, sizeof(DigestSha1)) == 1);
    Require(length == 256);
    Require(std::memcmp(signature.data(), Pkcs1Sha1Signature, 256) == 0);
    EVP_PKEY_CTX_free_nid_postfix(context);
    ReleaseKey(key);
}

void Pkcs1VerificationFailures() {
    auto* key = NewNativeKey(LibreSsl3PkeyRsa);
    auto* context = SignatureContext(key, false, EVP_sha1_nid_postfix());
    auto flipped = Flipped(Pkcs1Sha1Signature, 100);
    Require(EVP_PKEY_verify_nid_postfix(context, flipped.data(), 256, DigestSha1, sizeof(DigestSha1)) == 0);
    ExpectRsaQueue();
    EVP_PKEY_CTX_free_nid_postfix(context);
    context = SignatureContext(key, false, EVP_sha1_nid_postfix());
    std::array<unsigned char, 20> digestCopy {};
    std::memcpy(digestCopy.data(), DigestSha1, sizeof(DigestSha1));
    digestCopy[0] ^= 0x01;
    Require(EVP_PKEY_verify_nid_postfix(context, Pkcs1Sha1Signature, 256, digestCopy.data(),
        digestCopy.size()) == 0);
    ExpectErrors({RsaCode(104)});
    EVP_PKEY_CTX_free_nid_postfix(context);
    context = SignatureContext(key, false, EVP_sha1_nid_postfix());
    Require(EVP_PKEY_verify_nid_postfix(context, Pkcs1Sha1Signature, 256, DigestSha1, 19) == 0);
    ExpectErrors({RsaCode(104)});
    EVP_PKEY_CTX_free_nid_postfix(context);
    context = SignatureContext(key, false, EVP_sha1_nid_postfix());
    Require(EVP_PKEY_verify_nid_postfix(context, Pkcs1Sha1Signature, 255, DigestSha1, sizeof(DigestSha1)) == 0);
    ExpectErrors({RsaCode(119)});
    EVP_PKEY_CTX_free_nid_postfix(context);
    ReleaseKey(key);
}

void PssSignatures() {
    auto* key = NewNativeKey(LibreSsl3PkeyRsa);
    auto* context = SignatureContext(key, true, EVP_sha256_nid_postfix());
    Require(EVP_PKEY_CTX_ctrl_nid_postfix(context, LibreSsl3PkeyRsa, -1, LibreSsl3PkeyControlRsaPadding,
        LibreSsl3RsaPaddingPss, nullptr) == 1);
    std::array<unsigned char, 256> first {}, second {};
    std::size_t length = first.size();
    Require(EVP_PKEY_sign_nid_postfix(context, first.data(), &length, DigestSha256, sizeof(DigestSha256)) == 1);
    Require(length == 256);
    length = second.size();
    Require(EVP_PKEY_sign_nid_postfix(context, second.data(), &length, DigestSha256, sizeof(DigestSha256)) == 1);
    Require(length == 256);
    Require(std::memcmp(first.data(), second.data(), 256) != 0);
    EVP_PKEY_CTX_free_nid_postfix(context);
    auto* verifyContext = SignatureContext(key, false, EVP_sha256_nid_postfix());
    Require(EVP_PKEY_CTX_ctrl_nid_postfix(verifyContext, LibreSsl3PkeyRsa, -1, LibreSsl3PkeyControlRsaPadding,
        LibreSsl3RsaPaddingPss, nullptr) == 1);
    Require(EVP_PKEY_verify_nid_postfix(verifyContext, first.data(), 256, DigestSha256, sizeof(DigestSha256)) == 1);
    Require(EVP_PKEY_verify_nid_postfix(verifyContext, second.data(), 256, DigestSha256, sizeof(DigestSha256)) == 1);
    auto flipped = Flipped(first.data(), 7);
    Require(EVP_PKEY_verify_nid_postfix(verifyContext, flipped.data(), 256, DigestSha256, sizeof(DigestSha256)) == 0);
    ExpectRsaQueue();
    Require(EVP_PKEY_verify_nid_postfix(verifyContext, PssSha256Salt20Signature, 256, DigestSha256,
        sizeof(DigestSha256)) == 1);
    std::array<unsigned char, sizeof(DigestSha256)> otherDigest {};
    std::memcpy(otherDigest.data(), DigestSha256, otherDigest.size());
    otherDigest[31] ^= 0x01;
    Require(EVP_PKEY_verify_nid_postfix(verifyContext, PssSha256Salt20Signature, 256, otherDigest.data(),
        otherDigest.size()) == 0);
    ExpectErrors({RsaCode(104)});
    Require(EVP_PKEY_CTX_ctrl_nid_postfix(verifyContext, LibreSsl3PkeyRsa, -1, LibreSsl3PkeyControlRsaPssSaltLength,
        LibreSsl3RsaPssSaltLengthDigest, nullptr) == 1);
    Require(EVP_PKEY_verify_nid_postfix(verifyContext, PssSha256Salt20Signature, 256, DigestSha256,
        sizeof(DigestSha256)) == 0);
    ExpectErrors({RsaCode(136)});
    Require(EVP_PKEY_CTX_ctrl_nid_postfix(verifyContext, LibreSsl3PkeyRsa, -1, LibreSsl3PkeyControlRsaPssSaltLength, 20,
        nullptr) == 1);
    Require(EVP_PKEY_verify_nid_postfix(verifyContext, PssSha256Salt20Signature, 256, DigestSha256,
        sizeof(DigestSha256)) == 1);
    Require(EVP_PKEY_CTX_ctrl_nid_postfix(verifyContext, LibreSsl3PkeyRsa, -1, LibreSsl3PkeyControlRsaPssSaltLength,
        LibreSsl3RsaPssSaltLengthAuto, nullptr) == 1);
    Require(EVP_PKEY_verify_nid_postfix(verifyContext, PssSha256Salt20Signature, 256, DigestSha256,
        sizeof(DigestSha256)) == 1);
    EVP_PKEY_CTX_free_nid_postfix(verifyContext);
    ReleaseKey(key);
}

void PssPaddingBlock() {
    auto* key = NewNativeKey(LibreSsl3PkeyRsa);
    auto* rsa = static_cast<LibreSsl3Rsa*>(key->key);
    std::array<unsigned char, 256> block {};
    Require(RSA_padding_add_PKCS1_PSS_mgf1_nid_postfix(rsa, block.data(), DigestSha256,
        reinterpret_cast<const LibreSsl3::DigestMethod*>(EVP_sha256_nid_postfix()),
        reinterpret_cast<const LibreSsl3::DigestMethod*>(EVP_sha256_nid_postfix()), 20) == 1);
    Require(block[255] == 0xbc);
    Require((block[0] & 0x80) == 0);
    auto* context = SignatureContext(key, true, nullptr);
    Require(EVP_PKEY_CTX_ctrl_nid_postfix(context, LibreSsl3PkeyRsa, -1, LibreSsl3PkeyControlRsaPadding,
        LibreSsl3RsaPaddingNone, nullptr) == 1);
    std::array<unsigned char, 256> signature {};
    std::size_t length = signature.size();
    Require(EVP_PKEY_sign_nid_postfix(context, signature.data(), &length, block.data(), 256) == 1);
    Require(length == 256);
    EVP_PKEY_CTX_free_nid_postfix(context);
    auto* verifyContext = SignatureContext(key, false, EVP_sha256_nid_postfix());
    Require(EVP_PKEY_CTX_ctrl_nid_postfix(verifyContext, LibreSsl3PkeyRsa, -1, LibreSsl3PkeyControlRsaPadding,
        LibreSsl3RsaPaddingPss, nullptr) == 1);
    Require(EVP_PKEY_CTX_ctrl_nid_postfix(verifyContext, LibreSsl3PkeyRsa, -1, LibreSsl3PkeyControlRsaPssSaltLength, 20,
        nullptr) == 1);
    Require(EVP_PKEY_verify_nid_postfix(verifyContext, signature.data(), 256, DigestSha256, sizeof(DigestSha256)) == 1);
    EVP_PKEY_CTX_free_nid_postfix(verifyContext);
    Require(RSA_padding_add_PKCS1_PSS_mgf1_nid_postfix(rsa, block.data(), DigestSha256,
        reinterpret_cast<const LibreSsl3::DigestMethod*>(EVP_sha256_nid_postfix()),
        reinterpret_cast<const LibreSsl3::DigestMethod*>(EVP_sha256_nid_postfix()), -4) == 0);
    ExpectErrors({RsaCode(136)});
    ReleaseKey(key);
}

void RawPkcs1Signature() {
    auto* key = NewNativeKey(LibreSsl3PkeyRsa);
    auto* context = SignatureContext(key, true, nullptr);
    std::array<unsigned char, 256> signature {};
    std::size_t length = signature.size();
    Require(EVP_PKEY_sign_nid_postfix(context, signature.data(), &length, DigestSha1, sizeof(DigestSha1)) == 1);
    Require(length == 256);
    EVP_PKEY_CTX_free_nid_postfix(context);
    auto* verifyContext = SignatureContext(key, false, nullptr);
    Require(EVP_PKEY_verify_nid_postfix(verifyContext, signature.data(), 256, DigestSha1, sizeof(DigestSha1)) == 1);
    Require(EVP_PKEY_verify_nid_postfix(verifyContext, signature.data(), 256, DigestSha1, 19) == 0);
    ExpectErrors({});
    EVP_PKEY_CTX_free_nid_postfix(verifyContext);
    ReleaseKey(key);
}

void SignatureControls() {
    auto* key = NewNativeKey(LibreSsl3PkeyRsa);
    auto* context = SignatureContext(key, true, nullptr);
    Require(RSA_pkey_ctx_ctrl_nid_postfix(context, LibreSsl3PkeyOperationTypeSignature,
        LibreSsl3PkeyControlRsaPadding, LibreSsl3RsaPaddingPss, nullptr) == 1);
    int padding = 0;
    Require(RSA_pkey_ctx_ctrl_nid_postfix(context, LibreSsl3PkeyOperationTypeSignature,
        LibreSsl3PkeyControlGetRsaPadding, 0, &padding) == 1);
    Require(padding == LibreSsl3RsaPaddingPss);
    EVP_PKEY_CTX_free_nid_postfix(context);
    ReleaseKey(key);
}

void SignatureOperationGuards() {
    auto* key = NewNativeKey(LibreSsl3PkeyRsa);
    auto* context = EVP_PKEY_CTX_new_nid_postfix(key, nullptr);
    Require(context != nullptr);
    std::array<unsigned char, 256> signature {};
    std::size_t length = signature.size();
    Require(EVP_PKEY_sign_nid_postfix(context, signature.data(), &length, DigestSha1, sizeof(DigestSha1)) == -1);
    ExpectErrors({EvpCode(151)});
    Require(EVP_PKEY_verify_nid_postfix(context, signature.data(), 256, DigestSha1, sizeof(DigestSha1)) == -1);
    ExpectErrors({EvpCode(151)});
    Require(EVP_PKEY_sign_init_nid_postfix(context) == 1);
    Require(EVP_PKEY_verify_nid_postfix(context, signature.data(), 256, DigestSha1, sizeof(DigestSha1)) == -1);
    ExpectErrors({EvpCode(151)});
    EVP_PKEY_CTX_free_nid_postfix(context);
    context = EVP_PKEY_CTX_new_nid_postfix(key, nullptr);
    Require(EVP_PKEY_verify_init_nid_postfix(context) == 1);
    Require(EVP_PKEY_sign_nid_postfix(context, signature.data(), &length, DigestSha1, sizeof(DigestSha1)) == -1);
    ExpectErrors({EvpCode(151)});
    EVP_PKEY_CTX_free_nid_postfix(context);
    ReleaseKey(key);
}

void KeyConsistency() {
    auto* key = NewSigningKey(true, true, true);
    Require(RSA_check_key_nid_postfix(static_cast<const LibreSsl3Rsa*>(key->key)) == 1);
    ExpectErrors({});
    ReleaseKey(key);
    auto* key2 = NewSigningKey(true, false, true);
    Require(RSA_check_key_nid_postfix(static_cast<const LibreSsl3Rsa*>(key2->key)) == 1);
    ReleaseKey(key2);
    auto* key3 = NewNativeKey(LibreSsl3PkeyRsa);
    Require(RSA_check_key_nid_postfix(static_cast<const LibreSsl3Rsa*>(key3->key)) == 0);
    ExpectErrors({RsaCode(147)});
    ReleaseKey(key3);
    auto* key4 = NewSigningKey(true, false, true);
    auto* rsa4 = static_cast<LibreSsl3Rsa*>(key4->key);
    std::vector<unsigned char> alteredD(KeyPrivateExponent, KeyPrivateExponent + sizeof(KeyPrivateExponent));
    alteredD[sizeof(KeyPrivateExponent) - 1] += 2;
    auto* d = LibreSsl3::NewRsaBignum_nid_no_patch(alteredD.data(), alteredD.size());
    BN_clear_free_nid_postfix(static_cast<LibreSsl3Bignum*>(rsa4->d));
    rsa4->d = d;
    Require(RSA_check_key_nid_postfix(rsa4) == 0);
    ExpectErrors({RsaCode(123)});
    ReleaseKey(key4);
    auto* key5 = NewSigningKey(true, true, true);
    auto* rsa5 = static_cast<LibreSsl3Rsa*>(key5->key);
    std::vector<unsigned char> alteredIqmp(KeyCoefficient, KeyCoefficient + sizeof(KeyCoefficient));
    alteredIqmp[sizeof(KeyCoefficient) - 1] += 1;
    auto* iqmp = LibreSsl3::NewRsaBignum_nid_no_patch(alteredIqmp.data(), alteredIqmp.size());
    BN_clear_free_nid_postfix(static_cast<LibreSsl3Bignum*>(rsa5->iqmp));
    rsa5->iqmp = iqmp;
    Require(RSA_check_key_nid_postfix(rsa5) == 0);
    ExpectErrors({RsaCode(126)});
    ReleaseKey(key5);
    auto* key6 = NewSigningKey(true, false, true);
    auto* rsa6 = static_cast<LibreSsl3Rsa*>(key6->key);
    std::vector<unsigned char> alteredP(KeyPrimeP, KeyPrimeP + sizeof(KeyPrimeP));
    alteredP[sizeof(KeyPrimeP) - 1] += 2;
    auto* p = LibreSsl3::NewRsaBignum_nid_no_patch(alteredP.data(), alteredP.size());
    BN_clear_free_nid_postfix(static_cast<LibreSsl3Bignum*>(rsa6->p));
    rsa6->p = p;
    Require(RSA_check_key_nid_postfix(rsa6) == 0);
    ExpectErrors({RsaCode(128), RsaCode(127), RsaCode(123)});
    ReleaseKey(key6);
}

} // namespace

void ChineseRemainderKey() {
    auto* key = EVP_PKEY_new_nid_disambig1_nid_postfix();
    Require(key != nullptr);
    key->type = LibreSsl3PkeyRsa;
    key->method = FindNativePkeyAsn1Method_nid_no_patch(LibreSsl3PkeyRsa);
    Require(key->method != nullptr);
    auto* rsa = RSA_new_nid_disambig1_nid_postfix();
    Require(rsa != nullptr);
    key->key = rsa;
    auto* modulus = LibreSsl3::NewRsaBignum_nid_no_patch(KeyModulus, sizeof(KeyModulus));
    auto* exponent = LibreSsl3::NewRsaBignum_nid_no_patch(PublicExponent, sizeof(PublicExponent));
    Require(modulus != nullptr && exponent != nullptr);
    Require(RSA_set0_key_nid_postfix(rsa, modulus, exponent, nullptr) == 1);
    auto* p = LibreSsl3::NewRsaBignum_nid_no_patch(KeyPrimeP, sizeof(KeyPrimeP));
    auto* q = LibreSsl3::NewRsaBignum_nid_no_patch(KeyPrimeQ, sizeof(KeyPrimeQ));
    auto* dmp1 = LibreSsl3::NewRsaBignum_nid_no_patch(KeyExponentP, sizeof(KeyExponentP));
    auto* dmq1 = LibreSsl3::NewRsaBignum_nid_no_patch(KeyExponentQ, sizeof(KeyExponentQ));
    auto* iqmp = LibreSsl3::NewRsaBignum_nid_no_patch(KeyCoefficient, sizeof(KeyCoefficient));
    Require(p != nullptr && q != nullptr && dmp1 != nullptr && dmq1 != nullptr && iqmp != nullptr);
    Require(RSA_set0_factors_nid_postfix(rsa, p, q) == 1);
    Require(RSA_set0_crt_params_nid_postfix(rsa, dmp1, dmq1, iqmp) == 1);
    auto* context = EVP_PKEY_CTX_new_nid_postfix(key, nullptr);
    Require(context != nullptr);
    Require(EVP_PKEY_decrypt_init_nid_postfix(context) == 1);
    std::array<unsigned char, 256> plain {};
    std::size_t length = plain.size();
    Require(EVP_PKEY_decrypt_nid_postfix(context, plain.data(), &length, Pkcs1Cipher, sizeof(Pkcs1Cipher)) == 1);
    Require(length == MessageLength);
    Require(std::memcmp(plain.data(), PlainMessage, MessageLength) == 0);
    EVP_PKEY_CTX_free_nid_postfix(context);
    ReleaseKey(key);
}

void DigestSignatures() {
    struct {
        const char* name;
        const void* digest;
        const unsigned char* digestBytes;
        std::size_t digestSize;
        const unsigned char* signature;
    } tests[] = {
        {"SHA-256", EVP_sha256_nid_postfix(), DigestSha256, sizeof(DigestSha256), Pkcs1Sha256Signature},
        {"SHA-1", EVP_sha1_nid_postfix(), DigestSha1, sizeof(DigestSha1), Pkcs1Sha1Signature},
    };
    for (const auto& test : tests) {
        auto* key = NewNativeKey(LibreSsl3PkeyRsa);
        auto* md = EVP_MD_CTX_new_nid_postfix();
        Require(md != nullptr);
        LibreSsl3PkeyContext* pctx = nullptr;
        Require(EVP_DigestSignInit_nid_postfix(md, &pctx, static_cast<const LibreSsl3::DigestMethod*>(test.digest),
            nullptr, key) == 1);
        Require(pctx != nullptr && md->keyContext == pctx && Pointer(md->digest) == Pointer(test.digest));
        Require(md->update(md, MessageBytes(), MessageLength) == 1);
        std::size_t length = 0;
        Require(EVP_DigestSignFinal_nid_postfix(md, nullptr, &length) == 1);
        Require(length == 256);
        std::array<unsigned char, 256> buffer {};
        length = buffer.size();
        Require(EVP_DigestSignFinal_nid_postfix(md, buffer.data(), &length) == 1);
        Require(length == 256);
        Require(std::memcmp(buffer.data(), test.signature, 256) == 0);
        length = buffer.size();
        Require(EVP_DigestSignFinal_nid_postfix(md, buffer.data(), &length) == 1);
        Require(length == 256);
        Require(std::memcmp(buffer.data(), test.signature, 256) == 0);
        EVP_MD_CTX_free_nid_postfix(md);
        ReleaseKey(key);
    }
}

void SignFinalVerifyFinal() {
    auto* key = NewNativeKey(LibreSsl3PkeyRsa);
    auto* md = EVP_MD_CTX_new_nid_postfix();
    Require(md != nullptr);
    Require(EVP_DigestInit_nid_postfix(md, EVP_sha1_nid_postfix()) == 1);
    Require(md->update(md, MessageBytes(), MessageLength) == 1);
    std::array<unsigned char, 256> buffer {};
    unsigned int length = 0;
    Require(EVP_SignFinal_nid_postfix(md, buffer.data(), &length, key) == 1);
    Require(length == 256);
    Require(std::memcmp(buffer.data(), Pkcs1Sha1Signature, 256) == 0);

    Require(EVP_VerifyFinal_nid_postfix(md, Pkcs1Sha1Signature, 256, key) == 1);
    auto flipped = Flipped(Pkcs1Sha1Signature, 100);
    Require(EVP_VerifyFinal_nid_postfix(md, flipped.data(), 256, key) == 0);
    ExpectRsaQueue();
    Require(EVP_VerifyFinal_nid_postfix(md, Pkcs1Sha1Signature, 255, key) == 0);
    ExpectErrors({RsaCode(119)});
    EVP_MD_CTX_free_nid_postfix(md);
    ReleaseKey(key);
}

void DigestSignInitWithoutDigest() {
    auto* key = NewNativeKey(LibreSsl3PkeyRsa);
    auto* md = EVP_MD_CTX_new_nid_postfix();
    Require(md != nullptr);
    LibreSsl3PkeyContext* pctx = nullptr;
    Require(EVP_DigestSignInit_nid_postfix(md, &pctx, nullptr, nullptr, key) == 1);
    Require(pctx != nullptr);
    Require(md->keyContext == pctx);
    Require(Pointer(md->digest) == Pointer(EVP_sha256_nid_postfix()));
    Require(md->update(md, MessageBytes(), MessageLength) == 1);
    std::array<unsigned char, 256> signature {};
    std::size_t length = signature.size();
    Require(EVP_DigestSignFinal_nid_postfix(md, signature.data(), &length) == 1);
    Require(length == 256);
    Require(std::memcmp(signature.data(), Pkcs1Sha256Signature, 256) == 0);
    EVP_MD_CTX_free_nid_postfix(md);
    ReleaseKey(key);
}

int main() {
    std::array<void*, 10> api{reinterpret_cast<void*>(Allocate_nid_no_patch), reinterpret_cast<void*>(Free_nid_no_patch),
        reinterpret_cast<void*>(Calloc_nid_no_patch), reinterpret_cast<void*>(Reallocate_nid_no_patch),
        reinterpret_cast<void*>(Align_nid_no_patch), reinterpret_cast<void*>(Realign_nid_no_patch),
        reinterpret_cast<void*>(PosixAlign_nid_no_patch)};
    ApplicationHeapRegister_nid_no_patch(api.data());
    AlgorithmMethodSelection();
    OperationGuards();
    Pkcs1Vector();
    ChineseRemainderKey();
    OaepDigestVectors();
    OaepLabelVector();
    WithoutPadding();
    RsaPaddingControls();
    RestrictedPssKey();
    Pkcs1Signatures();
    Pkcs1ChineseRemainderSignature();
    Pkcs1VerificationFailures();
    PssSignatures();
    PssPaddingBlock();
    RawPkcs1Signature();
    SignatureControls();
    SignatureOperationGuards();
    KeyConsistency();
    DigestSignatures();
    SignFinalVerifyFinal();
    DigestSignInitWithoutDigest();
    Require(allocations.empty());
    std::puts("LibreSSL3 EVP_PKEY_CTX RSA encryption and signature tests passed");
}
