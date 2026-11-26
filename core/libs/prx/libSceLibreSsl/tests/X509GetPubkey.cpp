#include "prx/libSceLibreSsl/libSceLibreSsl.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"

#include <array>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>

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
void* APS5_VABI Reallocate(void* pointer, std::size_t size) { if (!pointer) return Allocate(size); return std::realloc(pointer, size); }
void* APS5_VABI Align(std::size_t, std::size_t) { throw std::runtime_error("unexpected aligned allocation"); }
void* APS5_VABI Realign(void*, std::size_t) { throw std::runtime_error("unexpected aligned reallocation"); }
int APS5_VABI PosixAlign(void**, std::size_t, std::size_t) { throw std::runtime_error("unexpected posix_memalign"); }

void RegisterHeap() {
    std::array<void*, 7> api{reinterpret_cast<void*>(Allocate), reinterpret_cast<void*>(Release),
        reinterpret_cast<void*>(Callocate), reinterpret_cast<void*>(Reallocate), reinterpret_cast<void*>(Align),
        reinterpret_cast<void*>(Realign), reinterpret_cast<void*>(PosixAlign)};
    ApplicationHeapRegister_nid_no_patch(api.data());
}

struct PubkeyLayout {
    void* algor;
    Asn1String* publicKey;
    void* pkey;
};

struct FakePkey {
    int type;
    int saveType;
    int references;
};

void Run() {
    Require(X509_get_pubkey_nid_postfix(nullptr) == nullptr, "a null certificate has no key");

    auto* pkey = static_cast<FakePkey*>(calloc_nid_postfix(1, sizeof(FakePkey)));
    Require(pkey != nullptr, "key");
    pkey->references = 1;
    PubkeyLayout publicKey{};
    publicKey.pkey = pkey;
    LibreSslX509CertInfo info{};
    info.key = &publicKey;
    LibreSslX509 certificate{};
    certificate.certificateInfo = &info;
    Require(X509_get_pubkey_nid_postfix(&certificate) == pkey, "the cached key is returned");
    Require(pkey->references == 2, "the returned key is referenced");
    Require(X509_get_pubkey_nid_postfix(&certificate) == pkey, "the key is returned again");
    Require(pkey->references == 3, "each call adds a reference");
    pkey->references = 1;
    free_nid_postfix(pkey);
    Require(allocations == 0, "the key is released");

    PubkeyLayout empty{};
    info.key = &empty;
    Require(X509_get_pubkey_nid_postfix(&certificate) == nullptr, "an empty public key has no key");
    unsigned char bit = 0;
    Asn1String encoded{1, 3, &bit, 0};
    empty.publicKey = &encoded;
    bool threw = false;
    try {
        X509_get_pubkey_nid_postfix(&certificate);
    } catch (const std::exception&) {
        threw = true;
    }
    Require(threw, "an undecoded public key throws");
}

}

int main() {
    RegisterHeap();
    try {
        Run();
    } catch (const std::exception& error) {
        std::fprintf(stderr, "X509_get_pubkey threw: %s\n", error.what());
        return 1;
    }
    std::puts("libSceLibreSsl X509_get_pubkey tests passed");
    return 0;
}
