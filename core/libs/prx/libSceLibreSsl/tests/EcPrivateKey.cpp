#include "prx/libSceLibreSsl/libSceLibreSsl.hpp"
#include "prx/libSceLibreSsl/include/Ec.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"

#include <array>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <vector>

namespace {

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "failed: %s\n", message);
    std::abort();
}

long allocations = 0;

void* APS5_VABI Allocate(std::size_t size) { void* p = std::malloc(size ? size : 1); if (p) ++allocations; return p; }
void APS5_VABI Release(void* pointer) { if (pointer) --allocations; std::free(pointer); }
void* APS5_VABI Callocate(std::size_t count, std::size_t size) { void* p = std::calloc(count ? count : 1, size ? size : 1); if (p) ++allocations; return p; }
void* APS5_VABI Reallocate(void* pointer, std::size_t size) { if (!pointer) return Allocate(size); void* p = std::realloc(pointer, size); return p; }
void* APS5_VABI Align(std::size_t, std::size_t) { throw std::runtime_error("unexpected aligned allocation"); }
void* APS5_VABI Realign(void*, std::size_t) { throw std::runtime_error("unexpected aligned reallocation"); }
int APS5_VABI PosixAlign(void**, std::size_t, std::size_t) { throw std::runtime_error("unexpected posix_memalign"); }

void RegisterHeap() {
    std::array<void*, 7> api{reinterpret_cast<void*>(Allocate), reinterpret_cast<void*>(Release),
        reinterpret_cast<void*>(Callocate), reinterpret_cast<void*>(Reallocate), reinterpret_cast<void*>(Align),
        reinterpret_cast<void*>(Realign), reinterpret_cast<void*>(PosixAlign)};
    ApplicationHeapRegister_nid_no_patch(api.data());
}

std::vector<unsigned char> Magnitude(const Bignum* number) {
    const int bits = BN_num_bits_nid_postfix(number);
    Require(bits >= 0, "bit length");
    const int length = (bits + 7) / 8;
    std::vector<unsigned char> bytes(static_cast<std::size_t>(length));
    if (length != 0) Require(BN_bn2bin_nid_postfix(number, bytes.data()) == length, "big-endian conversion");
    return bytes;
}

void Run() {
    const long baseline = allocations;
    EcKey* key = EC_KEY_new_by_curve_name_nid_postfix(415);
    Require(key != nullptr, "key");
    Require(EC_KEY_generate_key_nid_postfix(key) == 1, "key generation");
    const Bignum* number = EC_KEY_get0_private_key_nid_postfix(key);
    Require(number != nullptr, "the private key is exposed");
    Require(EC_KEY_get0_private_key_nid_postfix(key) == number, "the private key pointer is stable");
    const std::vector<unsigned char> exposed = Magnitude(number);
    std::vector<unsigned char> expected(mbedtls_mpi_size(&key->privateKey->value));
    Require(mbedtls_mpi_write_binary(&key->privateKey->value, expected.data(), expected.size()) == 0, "mpi conversion");
    Require(exposed == expected, "the exposed value matches the key");

    const unsigned char one = 1;
    Bignum* scalar = BN_bin2bn_nid_postfix(&one, 1, nullptr);
    Require(scalar != nullptr, "scalar");
    Require(EC_KEY_set_private_key_nid_postfix(key, scalar) == 1, "the private key is set");
    const Bignum* replaced = EC_KEY_get0_private_key_nid_postfix(key);
    Require(replaced != nullptr && replaced != number, "a new value replaces the old one");
    Require(Magnitude(replaced) == std::vector<unsigned char>{1}, "the replaced value is 1");
    Require(EC_KEY_set_private_key_nid_postfix(key, nullptr) == 1, "the private key is cleared");
    Require(EC_KEY_get0_private_key_nid_postfix(key) == nullptr, "a cleared key has no private key");
    BN_free_nid_postfix(scalar);
    EC_KEY_free_nid_postfix(key);

    EcKey* groupLess = EC_KEY_new_nid_postfix();
    Require(groupLess != nullptr, "group-less key");
    Bignum* other = BN_bin2bn_nid_postfix(&one, 1, nullptr);
    Require(other != nullptr, "second scalar");
    bool threw = false;
    try {
        EC_KEY_set_private_key_nid_postfix(groupLess, other);
    } catch (const std::exception&) {
        threw = true;
    }
    Require(threw, "setting a private key before its group throws");
    BN_free_nid_postfix(other);
    EC_KEY_free_nid_postfix(groupLess);
    Require(allocations == baseline, "the private-key number is released with the key");
}

}

int main() {
    RegisterHeap();
    try {
        Run();
    } catch (const std::exception& error) {
        std::fprintf(stderr, "EC private key threw: %s\n", error.what());
        return 1;
    }
    std::puts("libSceLibreSsl EC private key tests passed");
    return 0;
}
