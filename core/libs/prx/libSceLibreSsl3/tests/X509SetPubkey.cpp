#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include "prx/libc/libc.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <source_location>
#include <stdexcept>

namespace {
void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "LibreSSL3 X509_set_pubkey check failed at line %u\n", location.line());
    std::abort();
}
struct CertInfo { void* version; void* serial; void* signature; void* issuer; LibreSsl3X509Validity* validity; void* subject; LibreSsl3X509Pubkey* key; };
static_assert(offsetof(CertInfo, key) == 48);
void* APS5_VABI Allocate_nid_no_patch(std::size_t size) { return std::malloc(size); }
void APS5_VABI Free_nid_no_patch(void* pointer) { std::free(pointer); }
void* APS5_VABI Callocate_nid_no_patch(std::size_t count, std::size_t size) { return std::calloc(count, size); }
void* APS5_VABI Reallocate_nid_no_patch(void* pointer, std::size_t size) { return std::realloc(pointer, size); }
void* APS5_VABI UnexpectedAlign_nid_no_patch(std::size_t, std::size_t) { throw std::runtime_error("unexpected aligned allocation"); }
void* APS5_VABI UnexpectedRealign_nid_no_patch(void*, std::size_t, std::size_t) { throw std::runtime_error("unexpected aligned reallocation"); }
int APS5_VABI UnexpectedPosixAlign_nid_no_patch(void**, std::size_t, std::size_t) { throw std::runtime_error("unexpected posix_memalign"); }
}

int main() {
    std::array<void*, 10> api{reinterpret_cast<void*>(Allocate_nid_no_patch), reinterpret_cast<void*>(Free_nid_no_patch),
        reinterpret_cast<void*>(Callocate_nid_no_patch), reinterpret_cast<void*>(Reallocate_nid_no_patch),
        reinterpret_cast<void*>(UnexpectedAlign_nid_no_patch), reinterpret_cast<void*>(UnexpectedRealign_nid_no_patch),
        reinterpret_cast<void*>(UnexpectedPosixAlign_nid_no_patch)};
    ApplicationHeapRegister_nid_no_patch(api.data());
    LibreSsl3EvpPkey pkey{};
    pkey.references = 1;
    CertInfo info{};
    LibreSsl3X509 certificate{};
    certificate.certInfo = reinterpret_cast<LibreSsl3X509CertInfo*>(&info);
    Require(X509_set_pubkey_nid_postfix(&certificate, &pkey) == 1);
    Require(info.key != nullptr && info.key->pkey == &pkey);
    Require(pkey.references == 2);
    LibreSsl3X509Pubkey* first = info.key;
    LibreSsl3EvpPkey replacement{};
    replacement.references = 1;
    Require(X509_set_pubkey_nid_postfix(&certificate, &replacement) == 1);
    Require(info.key != nullptr && info.key->pkey == &replacement);
    Require(replacement.references == 2);
    if (info.key != first) free_nid_postfix(first);
    Require(X509_set_pubkey_nid_postfix(&certificate, nullptr) == 0);
    Require(X509_set_pubkey_nid_postfix(nullptr, &pkey) == 0);
    free_nid_postfix(info.key);
    certificate.certInfo = nullptr;
    Require(X509_set_pubkey_nid_postfix(&certificate, &pkey) == 0);
    std::puts("libSceLibreSsl3 X509_set_pubkey tests passed");
}
