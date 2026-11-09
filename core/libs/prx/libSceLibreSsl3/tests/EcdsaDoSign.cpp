#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

#include <array>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <source_location>
#include <stdexcept>
#include <vector>

namespace {

using Bytes = std::vector<unsigned char>;

int liveBlocks = 0;

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "LibreSSL3 ECDSA_do_sign check failed at line %u\n", location.line());
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

Bytes Magnitude(const void* value) {
    const auto* number = static_cast<const Bignum*>(value);
    Require(number != nullptr);
    Require(number->neg == 0);
    Bytes bytes(static_cast<std::size_t>(BN_num_bits_nid_postfix(number) + 7) / 8);
    Require(!bytes.empty());
    Require(BN_bn2bin_nid_postfix(number, bytes.data()) == static_cast<int>(bytes.size()));
    return bytes;
}

void AppendLength(Bytes& out, std::size_t length) {
    if (length >= 0x80) out.push_back(0x81);
    out.push_back(static_cast<unsigned char>(length));
}

void AppendInteger(Bytes& out, const Bytes& magnitude) {
    const bool pad = magnitude.front() & 0x80;
    out.push_back(0x02);
    AppendLength(out, magnitude.size() + (pad ? 1 : 0));
    if (pad) out.push_back(0);
    out.insert(out.end(), magnitude.begin(), magnitude.end());
}

Bytes Encode(const LibreSsl3EcdsaSig* signature) {
    const void* r = nullptr;
    const void* s = nullptr;
    ECDSA_SIG_get0_nid_postfix(signature, &r, &s);
    Bytes integers;
    AppendInteger(integers, Magnitude(r));
    AppendInteger(integers, Magnitude(s));
    Bytes encoded{0x30};
    AppendLength(encoded, integers.size());
    encoded.insert(encoded.end(), integers.begin(), integers.end());
    return encoded;
}

int Verify(const Bytes& digest, const Bytes& encoded, EcKey* key) {
    return ECDSA_verify_nid_postfix(0, digest.data(), static_cast<int>(digest.size()), encoded.data(), static_cast<int>(encoded.size()), key);
}

Bytes Digest(std::size_t size, unsigned char seed) {
    Bytes digest(size);
    for (std::size_t index = 0; index < size; ++index) digest[index] = static_cast<unsigned char>(seed + index * 37);
    return digest;
}

void SignAndVerify(int nid) {
    EcKey* key = EC_KEY_new_by_curve_name_nid_postfix(nid);
    Require(key != nullptr);
    Require(EC_KEY_generate_key_nid_postfix(key) == 1);
    const int degree = EC_GROUP_get_degree_nid_postfix(EC_KEY_get0_group_nid_postfix(key));
    for (const std::size_t size : {std::size_t{20}, std::size_t{32}, std::size_t{64}, std::size_t{0}}) {
        const Bytes digest = Digest(size, static_cast<unsigned char>(nid + size));
        LibreSsl3EcdsaSig* signature = ECDSA_do_sign_nid_postfix(size ? digest.data() : nullptr, static_cast<int>(size), key);
        Require(signature != nullptr);
        const Bytes encoded = Encode(signature);
        Require(static_cast<int>(encoded.size()) <= ECDSA_size_nid_postfix(key));
        const void* r = nullptr;
        const void* s = nullptr;
        ECDSA_SIG_get0_nid_postfix(signature, &r, &s);
        Require(BN_num_bits_nid_postfix(static_cast<const Bignum*>(r)) <= degree);
        Require(BN_num_bits_nid_postfix(static_cast<const Bignum*>(s)) <= degree);
        Require(Verify(digest, encoded, key) == 1);
        Bytes other = Digest(size ? size : 1, static_cast<unsigned char>(nid + size + 1));
        Require(Verify(other, encoded, key) == 0);
        LibreSsl3EcdsaSig* second = ECDSA_do_sign_nid_postfix(size ? digest.data() : nullptr, static_cast<int>(size), key);
        Require(second != nullptr);
        Require(Encode(second) != encoded);
        Require(Verify(digest, Encode(second), key) == 1);
        ECDSA_SIG_free_nid_postfix(second);
        ECDSA_SIG_free_nid_postfix(signature);
    }
    EC_KEY_free_nid_postfix(key);
}

void MissingParameters() {
    const Bytes digest = Digest(32, 3);
    bool threw = false;
    try {
        ECDSA_do_sign_nid_postfix(digest.data(), 32, nullptr);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);
    EcKey* empty = EC_KEY_new_nid_postfix();
    Require(empty != nullptr);
    Require(ECDSA_do_sign_nid_postfix(digest.data(), 32, empty) == nullptr);
    EC_KEY_free_nid_postfix(empty);
    EcKey* noPrivate = EC_KEY_new_by_curve_name_nid_postfix(415);
    Require(noPrivate != nullptr);
    Require(ECDSA_do_sign_nid_postfix(digest.data(), 32, noPrivate) == nullptr);
    EcKey* source = EC_KEY_new_by_curve_name_nid_postfix(415);
    Require(source != nullptr);
    Require(EC_KEY_generate_key_nid_postfix(source) == 1);
    Require(EC_KEY_set_public_key_nid_postfix(noPrivate, EC_KEY_get0_public_key_nid_postfix(source)) == 1);
    Require(ECDSA_do_sign_nid_postfix(digest.data(), 32, noPrivate) == nullptr);
    LibreSsl3EcdsaSig* signature = ECDSA_do_sign_nid_postfix(digest.data(), 32, source);
    Require(signature != nullptr);
    Require(Verify(digest, Encode(signature), noPrivate) == 1);
    ECDSA_SIG_free_nid_postfix(signature);
    EC_KEY_free_nid_postfix(source);
    EC_KEY_free_nid_postfix(noPrivate);
}

} // namespace

int main() {
    InitializeHeap();
    MissingParameters();
    constexpr std::array nids{409, 415, 715, 716};
    for (const int nid : nids) {
        for (int round = 0; round < 4; ++round) SignAndVerify(nid);
    }
    Require(liveBlocks == 0);
    std::puts("LibreSSL3 ECDSA_do_sign tests passed");
    return 0;
}
