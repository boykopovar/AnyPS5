#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
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
    std::fprintf(stderr, "LibreSSL3 ECDSA_do_sign check failed at line %u\n", location.line());
    std::abort();
}

template<typename TCall> void ThrowsInvalidArgument(TCall call) {
    bool threw = false;
    try { call(); } catch (const std::invalid_argument&) { threw = true; }
    Require(threw);
}

void* APS5_VABI Allocate(std::size_t size) { return std::malloc(size); }
void APS5_VABI Release(void* pointer) { std::free(pointer); }
void* APS5_VABI Callocate(std::size_t count, std::size_t size) { return std::calloc(count, size); }
void* APS5_VABI Reallocate(void* pointer, std::size_t size) { return std::realloc(pointer, size); }
void* APS5_VABI Align(std::size_t, std::size_t) { throw std::runtime_error("unexpected aligned allocation"); }
void* APS5_VABI Realign(void*, std::size_t, std::size_t) { throw std::runtime_error("unexpected aligned reallocation"); }
int APS5_VABI PosixAlign(void**, std::size_t, std::size_t) { throw std::runtime_error("unexpected posix_memalign"); }

void RegisterHeap() {
    std::array<void*, 7> api{reinterpret_cast<void*>(Allocate), reinterpret_cast<void*>(Release),
        reinterpret_cast<void*>(Callocate), reinterpret_cast<void*>(Reallocate), reinterpret_cast<void*>(Align),
        reinterpret_cast<void*>(Realign), reinterpret_cast<void*>(PosixAlign)};
    ApplicationHeapRegister_nid_no_patch(api.data());
}

constexpr std::array<unsigned char, 32> P256Order{
    0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xbc, 0xe6, 0xfa, 0xad, 0xa7, 0x17, 0x9e, 0x84, 0xf3, 0xb9, 0xca, 0xc2, 0xfc, 0x63, 0x25, 0x51};

void RequireBelowOrder(const Bignum* value) {
    Require(value != nullptr);
    Require(value->neg == 0);
    std::array<unsigned char, 33> encoded{};
    const int length = BN_bn2bin_nid_postfix(value, encoded.data());
    Require(length > 0 && length <= 32);
    std::array<unsigned char, 32> padded{};
    for (int index = 0; index < length; ++index) padded[static_cast<std::size_t>(32 - length + index)] = encoded[static_cast<std::size_t>(index)];
    Require(padded < P256Order);
}

void AppendInteger(std::vector<unsigned char>& body, const Bignum* value) {
    std::array<unsigned char, 33> encoded{};
    const int length = BN_bn2bin_nid_postfix(value, encoded.data());
    Require(length > 0 && length <= 32);
    const bool pad = (encoded[0] & 0x80) != 0;
    body.push_back(0x02);
    body.push_back(static_cast<unsigned char>(length + (pad ? 1 : 0)));
    if (pad) body.push_back(0);
    body.insert(body.end(), encoded.begin(), encoded.begin() + length);
}

std::vector<unsigned char> SignatureDer(const LibreSsl3EcdsaSig* signature) {
    const void* r = nullptr;
    const void* s = nullptr;
    ECDSA_SIG_get0_nid_postfix(signature, &r, &s);
    Require(r != nullptr && s != nullptr);
    std::vector<unsigned char> body;
    AppendInteger(body, static_cast<const Bignum*>(r));
    AppendInteger(body, static_cast<const Bignum*>(s));
    Require(body.size() < 128);
    std::vector<unsigned char> der{0x30, static_cast<unsigned char>(body.size())};
    der.insert(der.end(), body.begin(), body.end());
    return der;
}

void RequireInRange(const LibreSsl3EcdsaSig* signature) {
    const void* r = nullptr;
    const void* s = nullptr;
    ECDSA_SIG_get0_nid_postfix(signature, &r, &s);
    RequireBelowOrder(static_cast<const Bignum*>(r));
    RequireBelowOrder(static_cast<const Bignum*>(s));
}

void Run() {
    constexpr int P256 = 415;
    const std::array<unsigned char, 32> digest{
        0x7f, 0x2c, 0x91, 0x0a, 0xd4, 0x63, 0x38, 0xe5, 0x1b, 0x86, 0x4d, 0xf2, 0x09, 0xa7, 0x5e, 0xc3,
        0x24, 0xbd, 0x70, 0x13, 0x98, 0x46, 0xdf, 0x5a, 0x01, 0xeb, 0x72, 0x2f, 0xb8, 0x4c, 0x65, 0x9d};
    std::array<unsigned char, 32> otherDigest = digest;
    otherDigest[0] ^= 0x80;

    EcKey* key = EC_KEY_new_by_curve_name_nid_postfix(P256);
    Require(key != nullptr);
    Require(EC_KEY_generate_key_nid_postfix(key) == 1);

    LibreSsl3EcdsaSig* signature = ECDSA_do_sign_nid_postfix(digest.data(), static_cast<int>(digest.size()), key);
    Require(signature != nullptr);
    RequireInRange(signature);
    const std::vector<unsigned char> der = SignatureDer(signature);
    Require(ECDSA_verify_nid_postfix(0, digest.data(), static_cast<int>(digest.size()), der.data(),
        static_cast<int>(der.size()), key) == 1);
    Require(ECDSA_verify_nid_postfix(0, otherDigest.data(), static_cast<int>(otherDigest.size()), der.data(),
        static_cast<int>(der.size()), key) == 0);

    LibreSsl3EcdsaSig* second = ECDSA_do_sign_nid_postfix(digest.data(), static_cast<int>(digest.size()), key);
    Require(second != nullptr);
    RequireInRange(second);
    const std::vector<unsigned char> secondDer = SignatureDer(second);
    Require(secondDer != der);
    Require(ECDSA_verify_nid_postfix(0, digest.data(), static_cast<int>(digest.size()), secondDer.data(),
        static_cast<int>(secondDer.size()), key) == 1);
    ECDSA_SIG_free_nid_postfix(second);

    LibreSsl3EcdsaSig* zeroDigest = ECDSA_do_sign_nid_postfix(nullptr, 0, key);
    Require(zeroDigest != nullptr);
    RequireInRange(zeroDigest);
    const std::vector<unsigned char> zeroDer = SignatureDer(zeroDigest);
    Require(ECDSA_verify_nid_postfix(0, nullptr, 0, zeroDer.data(), static_cast<int>(zeroDer.size()), key) == 1);
    ECDSA_SIG_free_nid_postfix(zeroDigest);

    std::array<unsigned char, 64> longDigest{};
    for (std::size_t index = 0; index < otherDigest.size(); ++index) longDigest[index] = otherDigest[index];
    for (std::size_t index = otherDigest.size(); index < longDigest.size(); ++index) longDigest[index] = static_cast<unsigned char>(index);
    LibreSsl3EcdsaSig* longSignature = ECDSA_do_sign_nid_postfix(longDigest.data(), static_cast<int>(longDigest.size()), key);
    Require(longSignature != nullptr);
    RequireInRange(longSignature);
    const std::vector<unsigned char> longDer = SignatureDer(longSignature);
    Require(ECDSA_verify_nid_postfix(0, longDigest.data(), static_cast<int>(longDigest.size()), longDer.data(),
        static_cast<int>(longDer.size()), key) == 1);
    Require(ECDSA_verify_nid_postfix(0, otherDigest.data(), static_cast<int>(otherDigest.size()), longDer.data(),
        static_cast<int>(longDer.size()), key) == 1);
    Require(ECDSA_verify_nid_postfix(0, digest.data(), static_cast<int>(digest.size()), longDer.data(),
        static_cast<int>(longDer.size()), key) == 0);
    ECDSA_SIG_free_nid_postfix(longSignature);

    ECDSA_SIG_free_nid_postfix(signature);

    ThrowsInvalidArgument([&] { ECDSA_do_sign_nid_postfix(digest.data(), static_cast<int>(digest.size()), nullptr); });
    ThrowsInvalidArgument([&] { ECDSA_do_sign_nid_postfix(nullptr, 1, key); });
    Require(ECDSA_do_sign_nid_postfix(digest.data(), -1, key) == nullptr);

    EcKey* noGroup = EC_KEY_new_nid_postfix();
    Require(noGroup != nullptr);
    Require(ECDSA_do_sign_nid_postfix(digest.data(), static_cast<int>(digest.size()), noGroup) == nullptr);
    EC_KEY_free_nid_postfix(noGroup);

    EcKey* noPrivateKey = EC_KEY_new_by_curve_name_nid_postfix(P256);
    Require(noPrivateKey != nullptr);
    Require(ECDSA_do_sign_nid_postfix(digest.data(), static_cast<int>(digest.size()), noPrivateKey) == nullptr);
    EC_KEY_free_nid_postfix(noPrivateKey);

    EC_KEY_set_group_nid_postfix(key, nullptr);
    Require(ECDSA_do_sign_nid_postfix(digest.data(), static_cast<int>(digest.size()), key) == nullptr);
    EC_KEY_free_nid_postfix(key);
}

}

int main() {
    RegisterHeap();
    try {
        Run();
    } catch (const std::exception& error) {
        std::fprintf(stderr, "ECDSA_do_sign threw: %s\n", error.what());
        return 1;
    }
    std::puts("libSceLibreSsl3 ECDSA_do_sign tests passed");
}
