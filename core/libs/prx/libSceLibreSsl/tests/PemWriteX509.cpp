#include "prx/libSceLibreSsl/libSceLibreSsl.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"

#include <array>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>

namespace {

using LibreSsl::Bio;
using LibreSsl::BioMethod;

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

std::string captured;

int APS5_VABI CaptureWrite(Bio*, const char* source, int size) {
    captured.append(source, static_cast<std::size_t>(size));
    return size;
}

int APS5_VABI InitializeCapture(Bio* bio) {
    bio->initialized = 1;
    return 1;
}

const BioMethod CaptureMethod{0, "capture", CaptureWrite, nullptr, nullptr, nullptr, nullptr, InitializeCapture, nullptr, nullptr};

template<typename TAction>
void RequireThrows(TAction action, const char* message) {
    try {
        action();
    } catch (const std::exception&) {
        return;
    }
    Require(false, message);
}

void Run() {
    const long baseline = allocations;
    auto* certificate = static_cast<LibreSslX509*>(calloc_nid_postfix(1, sizeof(LibreSslX509)));
    Require(certificate != nullptr, "certificate");
    auto* info = static_cast<LibreSslX509CertInfo*>(calloc_nid_postfix(1, sizeof(LibreSslX509CertInfo)));
    Require(info != nullptr, "certificate info");
    certificate->certificateInfo = info;
    info->enc.enc = static_cast<unsigned char*>(malloc_nid_postfix(48));
    Require(info->enc.enc != nullptr, "encoding");
    for (int index = 0; index < 48; ++index) info->enc.enc[index] = static_cast<unsigned char>(index);
    info->enc.len = 48;
    Bio* bio = BIO_new_nid_postfix(&CaptureMethod);
    Require(bio != nullptr, "bio");
    captured.clear();
    Require(PEM_write_bio_X509_nid_postfix(bio, certificate) == 1, "the certificate is written");
    const std::string expected =
        "-----BEGIN CERTIFICATE-----\n"
        "AAECAwQFBgcICQoLDA0ODxAREhMUFRYXGBkaGxwdHh8gISIjJCUmJygpKissLS4v\n"
        "-----END CERTIFICATE-----\n";
    Require(captured == expected, "the PEM text matches");
    captured.clear();
    info->enc.len = 49;
    Require(PEM_write_bio_X509_nid_postfix(bio, certificate) == 1, "a longer encoding is written");
    Require(captured.find("-----BEGIN CERTIFICATE-----\n") == 0, "the header is written first");
    Require(captured.find("-----END CERTIFICATE-----\n") != std::string::npos, "the footer is written");
    RequireThrows([&] { PEM_write_bio_X509_nid_postfix(nullptr, certificate); }, "a null BIO throws");
    RequireThrows([&] { PEM_write_bio_X509_nid_postfix(bio, nullptr); }, "a null certificate throws");
    info->enc.modified = 1;
    RequireThrows([&] { PEM_write_bio_X509_nid_postfix(bio, certificate); }, "an unencoded certificate throws");
    info->enc.modified = 0;
    BIO_free_nid_postfix(bio);
    free_nid_postfix(info->enc.enc);
    free_nid_postfix(info);
    free_nid_postfix(certificate);
    Require(allocations == baseline, "the PEM text and certificate storage are released");
}

}

int main() {
    RegisterHeap();
    try {
        Run();
    } catch (const std::exception& error) {
        std::fprintf(stderr, "PEM_write_bio_X509 threw: %s\n", error.what());
        return 1;
    }
    std::puts("libSceLibreSsl PEM_write_bio_X509 tests passed");
    return 0;
}
