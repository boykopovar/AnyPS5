#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include <array>
#include <cerrno>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <source_location>
#include <stdexcept>
#include <vector>
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace {
void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "LibreSSL3 X509_get0_pubkey check failed at line %u\n", location.line());
    std::abort();
}

class ReadOnlyPage {
public:
    ReadOnlyPage() {
#ifdef _WIN32
        SYSTEM_INFO info{};
        GetSystemInfo(&info);
        bytes = info.dwPageSize;
        data = VirtualAlloc(nullptr, bytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
        Require(data != nullptr);
#else
        const long page = sysconf(_SC_PAGESIZE);
        Require(page > 0);
        bytes = static_cast<std::size_t>(page);
        data = mmap(nullptr, bytes, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        Require(data != nullptr && data != MAP_FAILED);
#endif
    }

    ~ReadOnlyPage() {
#ifdef _WIN32
        VirtualFree(data, 0, MEM_RELEASE);
#else
        munmap(data, bytes);
#endif
    }

    template <typename T>
    T* Set(const T& value) {
        Require(sizeof(value) <= bytes);
        std::memcpy(data, &value, sizeof(value));
        return static_cast<T*>(data);
    }

    void Protect() {
#ifdef _WIN32
        DWORD previous{};
        Require(VirtualProtect(data, bytes, PAGE_READONLY, &previous) != 0);
#else
        Require(mprotect(data, bytes, PROT_READ) == 0);
#endif
    }

private:
    void* data = nullptr;
    std::size_t bytes = 0;
};

template <typename TCall>
void Rejects(TCall call) {
    errno = EINVAL;
    bool threw = false;
    try {
        call();
    } catch (const std::runtime_error&) {
        threw = true;
    }
    Require(threw);
    Require(errno == EINVAL);
}

std::vector<unsigned char> Hex(const char* text) {
    std::vector<unsigned char> bytes;
    for (std::size_t index = 0; text[index] != '\0'; index += 2) {
        const char pair[3] = {text[index], text[index + 1], '\0'};
        bytes.push_back(static_cast<unsigned char>(std::strtoul(pair, nullptr, 16)));
    }
    return bytes;
}

void* APS5_VABI HeapAllocate(std::size_t size) { return std::malloc(size == 0 ? 1 : size); }
void APS5_VABI HeapRelease(void* pointer) { std::free(pointer); }
void* APS5_VABI HeapCallocate(std::size_t count, std::size_t size) {
    return std::calloc(count == 0 ? 1 : count, size == 0 ? 1 : size);
}
void* APS5_VABI HeapReallocate(void* pointer, std::size_t size) { return std::realloc(pointer, size == 0 ? 1 : size); }
void* APS5_VABI UnexpectedAlign(std::size_t, std::size_t) { std::abort(); }
void* APS5_VABI UnexpectedRealign(void*, std::size_t, std::size_t) { std::abort(); }
int APS5_VABI UnexpectedPosixAlign(void**, std::size_t, std::size_t) { std::abort(); }
}

int main() {
    std::array<void*, 10> heapApi{
        reinterpret_cast<void*>(HeapAllocate),
        reinterpret_cast<void*>(HeapRelease),
        reinterpret_cast<void*>(HeapCallocate),
        reinterpret_cast<void*>(HeapReallocate),
        reinterpret_cast<void*>(UnexpectedAlign),
        reinterpret_cast<void*>(UnexpectedRealign),
        reinterpret_cast<void*>(UnexpectedPosixAlign),
    };
    ApplicationHeapRegister_nid_no_patch(heapApi.data());

    LibreSsl3EvpPkey pkey{};
    LibreSsl3X509Pubkey publicKey{};
    publicKey.pkey = &pkey;
    LibreSsl3CertificateInfo info{};
    info.key = &publicKey;
    LibreSsl3Certificate certificate{};
    certificate.certInfo = &info;

    Require(X509_get0_pubkey_nid_postfix(&certificate) == &pkey);
    publicKey.pkey = nullptr;
    Require(X509_get0_pubkey_nid_postfix(&certificate) == nullptr);
    auto spki = Hex(
        "3059301306072a8648ce3d020106082a8648ce3d030107034200042e5af3d5f0aa64e34ea5379e970d618dbaf910fb101fa1b087340d"
        "f7c9b046a4649333ae32cba4b12aedaecef6a450ec6f2c6807b7c20a497bec0d55a6075410");
    LibreSsl3Asn1String encoded{static_cast<int>(spki.size()), 3, spki.data(), 0};
    publicKey.publicKey = &encoded;
    errno = EINVAL;
    auto* decoded = static_cast<LibreSsl3EvpPkey*>(X509_get0_pubkey_nid_postfix(&certificate));
    Require(decoded != nullptr);
    Require(decoded->type == 408);
    Require(publicKey.pkey == decoded);
    Require(errno == EINVAL);
    Require(X509_get0_pubkey_nid_postfix(&certificate) == decoded);
    EVP_PKEY_free_nid_disambig1_nid_postfix(decoded);
    publicKey.pkey = nullptr;

    unsigned char garbage[] = {0x30, 0x03, 0x02, 0x01, 0x01};
    LibreSsl3Asn1String invalid{static_cast<int>(sizeof(garbage)), 3, garbage, 0};
    publicKey.publicKey = &invalid;
    errno = EINVAL;
    Require(X509_get0_pubkey_nid_postfix(&certificate) == nullptr);
    Require(publicKey.pkey == nullptr);
    Require(errno == EINVAL);

    LibreSsl3Asn1String negative{-1, 3, spki.data(), 0};
    publicKey.publicKey = &negative;
    Require(X509_get0_pubkey_nid_postfix(&certificate) == nullptr);

    LibreSsl3Asn1String nullData{1, 3, nullptr, 0};
    publicKey.publicKey = &nullData;
    Rejects([&] { X509_get0_pubkey_nid_postfix(&certificate); });

    {
        ReadOnlyPage keyPage;
        LibreSsl3X509Pubkey decodedValue{};
        decodedValue.publicKey = &encoded;
        auto* roKey = keyPage.Set(decodedValue);
        keyPage.Protect();
        LibreSsl3CertificateInfo decodeInfo{};
        decodeInfo.key = roKey;
        LibreSsl3Certificate decodeCert{};
        decodeCert.certInfo = &decodeInfo;
        Rejects([&] { X509_get0_pubkey_nid_postfix(&decodeCert); });
    }
    publicKey.publicKey = nullptr;
    info.key = nullptr;
    Require(X509_get0_pubkey_nid_postfix(&certificate) == nullptr);
    Require(X509_get0_pubkey_nid_postfix(nullptr) == nullptr);

    {
        ReadOnlyPage keyPage;
        publicKey.pkey = &pkey;
        publicKey.publicKey = nullptr;
        auto* roKey = keyPage.Set(publicKey);
        keyPage.Protect();
        ReadOnlyPage infoPage;
        info.key = roKey;
        auto* roInfo = infoPage.Set(info);
        infoPage.Protect();
        ReadOnlyPage certPage;
        certificate.certInfo = roInfo;
        auto* roCert = certPage.Set(certificate);
        certPage.Protect();

        errno = EINVAL;
        Require(X509_get0_pubkey_nid_postfix(roCert) == &pkey);
        Require(errno == EINVAL);
    }

    Rejects([] { X509_get0_pubkey_nid_postfix(reinterpret_cast<const LibreSsl3Certificate*>(0x10)); });

    LibreSsl3Certificate badInfoCert{};
    badInfoCert.certInfo = reinterpret_cast<LibreSsl3CertificateInfo*>(0x10);
    Rejects([&] { X509_get0_pubkey_nid_postfix(&badInfoCert); });

    LibreSsl3CertificateInfo badKeyInfo{};
    badKeyInfo.key = reinterpret_cast<LibreSsl3X509Pubkey*>(0x10);
    LibreSsl3Certificate badKeyCert{};
    badKeyCert.certInfo = &badKeyInfo;
    Rejects([&] { X509_get0_pubkey_nid_postfix(&badKeyCert); });

    std::puts("libSceLibreSsl3 X509_get0_pubkey tests passed");
}
