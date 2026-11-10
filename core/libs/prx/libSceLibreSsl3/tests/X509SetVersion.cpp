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
    std::fprintf(stderr, "LibreSSL3 X509_set_version check failed at line %u\n", location.line());
    std::abort();
}
void* APS5_VABI Allocate_nid_no_patch(std::size_t size) { return std::malloc(size); }
void APS5_VABI Free_nid_no_patch(void* pointer) { std::free(pointer); }
void* APS5_VABI Callocate_nid_no_patch(std::size_t count, std::size_t size) { return std::calloc(count, size); }
void* APS5_VABI Reallocate_nid_no_patch(void* pointer, std::size_t size) { return std::realloc(pointer, size); }
void* APS5_VABI UnexpectedAlign_nid_no_patch(std::size_t, std::size_t) { throw std::runtime_error("unexpected aligned allocation"); }
void* APS5_VABI UnexpectedRealign_nid_no_patch(void*, std::size_t, std::size_t) { throw std::runtime_error("unexpected aligned reallocation"); }
int APS5_VABI UnexpectedPosixAlign_nid_no_patch(void**, std::size_t, std::size_t) { throw std::runtime_error("unexpected posix_memalign"); }
}

int main() {
    std::array<void*, 7> api{reinterpret_cast<void*>(Allocate_nid_no_patch), reinterpret_cast<void*>(Free_nid_no_patch),
        reinterpret_cast<void*>(Callocate_nid_no_patch), reinterpret_cast<void*>(Reallocate_nid_no_patch),
        reinterpret_cast<void*>(UnexpectedAlign_nid_no_patch), reinterpret_cast<void*>(UnexpectedRealign_nid_no_patch),
        reinterpret_cast<void*>(UnexpectedPosixAlign_nid_no_patch)};
    ApplicationHeapRegister_nid_no_patch(api.data());
    LibreSsl3CertificateInfo info{};
    LibreSsl3Certificate certificate{};
    certificate.certInfo = &info;
    Require(X509_set_version_nid_postfix(&certificate, 2) == 1);
    auto* version = static_cast<Asn1String*>(info.version);
    Require(version != nullptr && version->type == 2 && version->length == 1 && version->data[0] == 2);
    Require(X509_set_version_nid_postfix(&certificate, 0) == 1);
    Require(version->length == 1 && version->data[0] == 0);
    Require(X509_set_version_nid_postfix(nullptr, 0) == 0);
    LibreSsl3Certificate empty{};
    Require(X509_set_version_nid_postfix(&empty, 0) == 0);
    ASN1_OCTET_STRING_free_nid_postfix(version);
    std::puts("libSceLibreSsl3 X509_set_version tests passed");
}
