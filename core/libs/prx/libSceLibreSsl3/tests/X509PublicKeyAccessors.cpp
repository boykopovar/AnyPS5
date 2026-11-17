#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include "Testing.hpp"
#include <cerrno>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <source_location>
#include <stdexcept>
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace {

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
}

int main() {
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
    LibreSsl3Asn1String encoded{};
    publicKey.publicKey = &encoded;
    bool threw = false;
    try { X509_get0_pubkey_nid_postfix(&certificate); } catch (const std::runtime_error&) { threw = true; }
    Require(threw);
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
