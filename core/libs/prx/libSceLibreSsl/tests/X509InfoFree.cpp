#include "prx/libSceLibreSsl/libSceLibreSsl.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"

#include <array>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
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

LibreSslX509* NewCertificate() {
    auto* certificate = static_cast<LibreSslX509*>(calloc_nid_postfix(1, sizeof(LibreSslX509)));
    Require(certificate != nullptr, "certificate");
    auto* info = static_cast<LibreSslX509CertInfo*>(calloc_nid_postfix(1, sizeof(LibreSslX509CertInfo)));
    Require(info != nullptr, "certificate info");
    info->enc.enc = static_cast<unsigned char*>(malloc_nid_postfix(4));
    Require(info->enc.enc != nullptr, "encoding");
    info->enc.len = 4;
    certificate->certificateInfo = info;
    certificate->references = 1;
    return certificate;
}

void Run() {
    const long baseline = allocations;
    auto* info = static_cast<LibreSslX509Info*>(calloc_nid_postfix(1, sizeof(LibreSslX509Info)));
    Require(info != nullptr, "info");
    info->certificate = NewCertificate();
    info->encData = static_cast<char*>(malloc_nid_postfix(3));
    Require(info->encData != nullptr, "encrypted data");
    info->references = 2;
    X509_INFO_free_nid_postfix(info);
    Require(info->references == 1, "the first free keeps the info");
    X509_INFO_free_nid_postfix(info);
    Require(allocations == baseline, "the last free releases the info");
    X509_INFO_free_nid_postfix(nullptr);

    auto* unsupported = static_cast<LibreSslX509Info*>(calloc_nid_postfix(1, sizeof(LibreSslX509Info)));
    Require(unsupported != nullptr, "unsupported info");
    static int placeholder = 0;
    unsupported->crl = &placeholder;
    unsupported->references = 1;
    bool threw = false;
    try {
        X509_INFO_free_nid_postfix(unsupported);
    } catch (const std::exception&) {
        threw = true;
    }
    Require(threw, "a CRL throws");
    unsupported->crl = nullptr;
    X509_INFO_free_nid_postfix(unsupported);
    Require(allocations == baseline, "the unsupported info is released");
}

}

int main() {
    RegisterHeap();
    try {
        Run();
    } catch (const std::exception& error) {
        std::fprintf(stderr, "X509_INFO_free threw: %s\n", error.what());
        return 1;
    }
    std::puts("libSceLibreSsl X509_INFO_free tests passed");
    return 0;
}
