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

namespace {

struct Span {
    const unsigned char* data;
    std::size_t size;
};

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "LibreSSL3 ECDSA_do_verify check failed at line %u\n", location.line());
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

bool ReadElement(const unsigned char*& cursor, const unsigned char* end, unsigned char tag, Span& element) {
    if (end - cursor < 2 || cursor[0] != tag) return false;
    std::size_t length = cursor[1];
    cursor += 2;
    if (length & 0x80) {
        const std::size_t count = length & 0x7f;
        if (count == 0 || count > sizeof(std::size_t) || static_cast<std::size_t>(end - cursor) < count) return false;
        length = 0;
        for (std::size_t index = 0; index < count; ++index) length = (length << 8) | *cursor++;
    }
    if (static_cast<std::size_t>(end - cursor) < length) return false;
    element = {cursor, length};
    cursor += length;
    return true;
}

Span SignatureSequence(const unsigned char* signature, unsigned int length) {
    const unsigned char* cursor = signature;
    Span sequence {};
    Require(ReadElement(cursor, signature + length, 0x30, sequence) && cursor == signature + length);
    return sequence;
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

    std::array<unsigned char, 256> der {};
    unsigned int derLength = 0;
    Require(ECDSA_sign_nid_postfix(0, digest.data(), static_cast<int>(digest.size()), der.data(), &derLength, key) == 1);
    const Span sequence = SignatureSequence(der.data(), derLength);
    const unsigned char* cursor = sequence.data;
    const unsigned char* const end = sequence.data + sequence.size;
    Span rBytes {};
    Span sBytes {};
    Require(ReadElement(cursor, end, 0x02, rBytes) && ReadElement(cursor, end, 0x02, sBytes) && cursor == end);

    Bignum* r = BN_bin2bn_nid_postfix(rBytes.data, static_cast<int>(rBytes.size), nullptr);
    Bignum* s = BN_bin2bn_nid_postfix(sBytes.data, static_cast<int>(sBytes.size), nullptr);
    Require(r != nullptr && s != nullptr);
    LibreSsl3EcdsaSig* signature = ECDSA_SIG_new_nid_postfix();
    Require(signature != nullptr);
    Require(ECDSA_SIG_set0_nid_postfix(signature, r, s) == 1);

    Require(ECDSA_do_verify_nid_postfix(digest.data(), static_cast<int>(digest.size()), signature, key) == 1);
    Require(ECDSA_do_verify_nid_postfix(otherDigest.data(), static_cast<int>(otherDigest.size()), signature, key) == 0);

    const unsigned char byte = reinterpret_cast<unsigned char*>(r->d)[0];
    reinterpret_cast<unsigned char*>(r->d)[0] ^= 0xff;
    Require(ECDSA_do_verify_nid_postfix(digest.data(), static_cast<int>(digest.size()), signature, key) == 0);
    reinterpret_cast<unsigned char*>(r->d)[0] = byte;
    Require(ECDSA_do_verify_nid_postfix(digest.data(), static_cast<int>(digest.size()), signature, key) == 1);

    const int top = r->top;
    r->top = 0;
    Require(ECDSA_do_verify_nid_postfix(digest.data(), static_cast<int>(digest.size()), signature, key) == 0);
    r->top = top;

    r->neg = 1;
    Require(ECDSA_do_verify_nid_postfix(digest.data(), static_cast<int>(digest.size()), signature, key) == 0);
    r->neg = 0;
    Require(ECDSA_do_verify_nid_postfix(digest.data(), static_cast<int>(digest.size()), signature, key) == 1);

    const LibreSsl3EcdsaSig noR{nullptr, signature->s};
    ThrowsInvalidArgument([&] { ECDSA_do_verify_nid_postfix(digest.data(), static_cast<int>(digest.size()), &noR, key); });
    Require(ECDSA_do_verify_nid_postfix(digest.data(), static_cast<int>(digest.size()), nullptr, key) == -1);
    Require(ECDSA_do_verify_nid_postfix(digest.data(), -1, signature, key) == -1);
    ThrowsInvalidArgument([&] { ECDSA_do_verify_nid_postfix(nullptr, 1, signature, key); });
    ThrowsInvalidArgument([&] { ECDSA_do_verify_nid_postfix(digest.data(), static_cast<int>(digest.size()), signature, nullptr); });

    EcKey* noGroup = EC_KEY_new_nid_postfix();
    Require(noGroup != nullptr);
    Require(ECDSA_do_verify_nid_postfix(digest.data(), static_cast<int>(digest.size()), signature, noGroup) == -1);
    EC_KEY_free_nid_postfix(noGroup);

    EcKey* noPublicKey = EC_KEY_new_by_curve_name_nid_postfix(P256);
    Require(noPublicKey != nullptr);
    Require(ECDSA_do_verify_nid_postfix(digest.data(), static_cast<int>(digest.size()), signature, noPublicKey) == -1);
    EC_KEY_free_nid_postfix(noPublicKey);

    ECDSA_SIG_free_nid_postfix(signature);
    EC_KEY_free_nid_postfix(key);
}

}

int main() {
    RegisterHeap();
    try {
        Run();
    } catch (const std::exception& error) {
        std::fprintf(stderr, "ECDSA_do_verify threw: %s\n", error.what());
        return 1;
    }
    std::puts("libSceLibreSsl3 ECDSA_do_verify tests passed");
}
