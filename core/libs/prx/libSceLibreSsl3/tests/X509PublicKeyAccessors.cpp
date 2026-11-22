#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include <array>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <source_location>
#include <stdexcept>
#include <utility>
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
    unsigned char ecOid[] = {0x2a, 0x86, 0x48, 0xce, 0x3d, 0x02, 0x01};
    unsigned char p256Oid[] = {0x2a, 0x86, 0x48, 0xce, 0x3d, 0x03, 0x01, 0x07};
    unsigned char rsaOid[] = {0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01, 0x01};
    unsigned char ed25519Oid[] = {0x2b, 0x65, 0x70};
    LibreSslAsn1Object ecObject{nullptr, nullptr, 0, sizeof(ecOid), ecOid, 0};
    LibreSslAsn1Object curveObject{nullptr, nullptr, 0, sizeof(p256Oid), p256Oid, 0};
    LibreSslAsn1Object rsaObject{nullptr, nullptr, 0, sizeof(rsaOid), rsaOid, 0};
    LibreSslAsn1Object ed25519Object{nullptr, nullptr, 0, sizeof(ed25519Oid), ed25519Oid, 0};
    LibreSsl3Asn1Type curve{6, &curveObject};
    LibreSsl3Asn1Type nullParameter{5, nullptr};
    LibreSsl3X509Algor ecAlgor{&ecObject, &curve};
    LibreSsl3X509Algor rsaAlgor{&rsaObject, &nullParameter};
    LibreSsl3X509Algor ed25519Algor{&ed25519Object, nullptr};
    LibreSsl3X509Algor nullObjectAlgor{nullptr, nullptr};

    auto point = Hex(
        "042e5af3d5f0aa64e34ea5379e970d618dbaf910fb101fa1b087340df7c9b046a4649333ae32cba4b12aedaecef6a450ec6f2c6807b7c2"
        "0a497bec0d55a6075410");
    LibreSsl3Asn1String encoded{static_cast<int>(point.size()), 3, point.data(), 0};
    publicKey.algor = &ecAlgor;
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

    auto rsaBits = Hex(
        "3046024100a83d5c5ff034052a0ff029c00ff99ee36055c3e2d9d466b55c31cb09872ca4b32a60a9e5b5b50fefca76519f54c24545ce"
        "b9a051baf49f7a4f7f665dc363d6ab020103");
    LibreSsl3Asn1String rsaKey{static_cast<int>(rsaBits.size()), 3, rsaBits.data(), 0x08 | 0x03};
    publicKey.algor = &rsaAlgor;
    publicKey.publicKey = &rsaKey;
    decoded = static_cast<LibreSsl3EvpPkey*>(X509_get0_pubkey_nid_postfix(&certificate));
    Require(decoded != nullptr);
    Require(decoded->type == 6);
    Require(EVP_PKEY_bits_nid_postfix(decoded) == 512);
    Require(publicKey.pkey == decoded);
    EVP_PKEY_free_nid_disambig1_nid_postfix(decoded);
    publicKey.pkey = nullptr;

    const char* rsa1024 =
        "30818702818100b2c65a5b2e19700fa0bbbbb1515e559ee28867c50752fb4f1955b3c2db68a8c81ebd43099b700dae0951d128f35101"
        "c13002ece7998ead9b1fd89d82b533016d526d160385b7edb368e4008ae53ec1fad97a94507cd27b21df775e6c8eba51fadc0ff8c89c"
        "0008cb460376d2b3e69b9d60acf6c046cee9974a01937090e53e39020103";
    const char* rsa2048 =
        "308201080282010100abb2e3a341fc73f4de0d71c4728c09212394d15c61214a64f8def817dc40a3efb9de27fddd19fb37d17f7d125e"
        "4be0dce340540f95a617112ed1e42917056cd2333390244682d2a882626156802141c6f329f90e0193f73376c77cfa5b4e0afb7f482f"
        "ddd938ee90d1b13ae4b471e017026a7dc96adc506c87589989961fb6493961ad1bd9aaaa736c321bc422a37aaec5a755dd01a3fb42d1"
        "871b85db4f58b72e9ef73882e9726665755dcd9e9206065bc706a60fc3177aa8ab5c4a3e22d36b337742af21cfb0f7cbe973330bab15"
        "6698bb5fa5ec33a52dd6061b3dda2eb770bb3159eb109b2c01e7cb9dad962953d7f1f42958b4111612f51933d67a608fb1020103";
    for (const auto& [hex, bitCount] : {std::pair{rsa1024, 1024}, std::pair{rsa2048, 2048}}) {
        auto longKey = Hex(hex);
        LibreSsl3Asn1String longBits{static_cast<int>(longKey.size()), 3, longKey.data(), 0};
        publicKey.publicKey = &longBits;
        decoded = static_cast<LibreSsl3EvpPkey*>(X509_get0_pubkey_nid_postfix(&certificate));
        Require(decoded != nullptr);
        Require(EVP_PKEY_bits_nid_postfix(decoded) == bitCount);
        EVP_PKEY_free_nid_disambig1_nid_postfix(decoded);
        publicKey.pkey = nullptr;
    }

    LibreSsl3Asn1Type falseFlag{1, nullptr};
    LibreSsl3X509Algor booleanAlgor{&ecObject, &falseFlag};
    publicKey.algor = &booleanAlgor;
    publicKey.publicKey = &encoded;
    Require(X509_get0_pubkey_nid_postfix(&certificate) == nullptr);
    Require(publicKey.pkey == nullptr);
    LibreSsl3Asn1Type undefinedFlag{1, reinterpret_cast<void*>(static_cast<std::uintptr_t>(0xffffffffu))};
    LibreSsl3X509Algor undefinedAlgor{&rsaObject, &undefinedFlag};
    publicKey.algor = &undefinedAlgor;
    publicKey.publicKey = &rsaKey;
    decoded = static_cast<LibreSsl3EvpPkey*>(X509_get0_pubkey_nid_postfix(&certificate));
    Require(decoded != nullptr && EVP_PKEY_bits_nid_postfix(decoded) == 512);
    EVP_PKEY_free_nid_disambig1_nid_postfix(decoded);
    publicKey.pkey = nullptr;

    auto spki = Hex(
        "3059301306072a8648ce3d020106082a8648ce3d030107034200042e5af3d5f0aa64e34ea5379e970d618dbaf910fb101fa1b087340d"
        "f7c9b046a4649333ae32cba4b12aedaecef6a450ec6f2c6807b7c20a497bec0d55a6075410");
    LibreSsl3Asn1String wholeSpki{static_cast<int>(spki.size()), 3, spki.data(), 0};
    publicKey.algor = &ecAlgor;
    publicKey.publicKey = &wholeSpki;
    Require(X509_get0_pubkey_nid_postfix(&certificate) == nullptr);
    Require(publicKey.pkey == nullptr);

    publicKey.publicKey = &encoded;
    publicKey.algor = &ed25519Algor;
    Require(X509_get0_pubkey_nid_postfix(&certificate) == nullptr);
    publicKey.algor = &nullObjectAlgor;
    Require(X509_get0_pubkey_nid_postfix(&certificate) == nullptr);
    Require(publicKey.pkey == nullptr);
    publicKey.algor = nullptr;
    Rejects([&] { X509_get0_pubkey_nid_postfix(&certificate); });
    publicKey.algor = &ecAlgor;

    unsigned char garbage[] = {0x30, 0x03, 0x02, 0x01, 0x01};
    LibreSsl3Asn1String invalid{static_cast<int>(sizeof(garbage)), 3, garbage, 0};
    publicKey.publicKey = &invalid;
    errno = EINVAL;
    Require(X509_get0_pubkey_nid_postfix(&certificate) == nullptr);
    Require(publicKey.pkey == nullptr);
    Require(errno == EINVAL);

    LibreSsl3Asn1String negative{-1, 3, point.data(), 0};
    publicKey.publicKey = &negative;
    Require(X509_get0_pubkey_nid_postfix(&certificate) == nullptr);

    LibreSsl3Asn1String nullData{1, 3, nullptr, 0};
    publicKey.publicKey = &nullData;
    Rejects([&] { X509_get0_pubkey_nid_postfix(&certificate); });

    {
        ReadOnlyPage keyPage;
        LibreSsl3X509Pubkey decodedValue{};
        decodedValue.algor = &ecAlgor;
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
