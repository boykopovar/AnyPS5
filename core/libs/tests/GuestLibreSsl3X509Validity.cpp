#include "prx/libSceLibreSsl3/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cerrno>
#include <cstdint>
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

static int Before;
static int After;

static void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "LibreSSL X509 validity check failed at line %u\n", location.line());
    std::abort();
}

template<typename TAction> static void RequireThrows(TAction action, std::source_location location = std::source_location::current()) {
    try { action(); } catch (const std::exception&) { return; }
    Require(false, location);
}

static void TestGetmNotBefore() {
    LibreSsl3X509Validity validity {};
    LibreSsl3ValidityCertInfo info {};
    LibreSsl3ValidityX509 certificate {};
    Require(X509_getm_notBefore_nid_postfix(nullptr) == nullptr);
    Require(X509_getm_notBefore_nid_postfix(&certificate) == nullptr);
    certificate.certInfo = &info;
    Require(X509_getm_notBefore_nid_postfix(&certificate) == nullptr);
    info.validity = &validity;
    Require(X509_getm_notBefore_nid_postfix(&certificate) == nullptr);
    validity.notBefore = &Before;
    validity.notAfter = &After;
    Require(X509_getm_notBefore_nid_postfix(&certificate) == &Before);
    validity.notBefore = nullptr;
    Require(X509_getm_notBefore_nid_postfix(&certificate) == nullptr);
}

static void TestGetmNotAfter() {
    LibreSsl3X509Validity validity {};
    LibreSsl3ValidityCertInfo info {};
    LibreSsl3ValidityX509 certificate {};
    Require(X509_getm_notAfter_nid_postfix(nullptr) == nullptr);
    Require(X509_getm_notAfter_nid_postfix(&certificate) == nullptr);
    certificate.certInfo = &info;
    Require(X509_getm_notAfter_nid_postfix(&certificate) == nullptr);
    info.validity = &validity;
    Require(X509_getm_notAfter_nid_postfix(&certificate) == nullptr);
    validity.notBefore = &Before;
    validity.notAfter = &After;
    Require(X509_getm_notAfter_nid_postfix(&certificate) == &After);
    validity.notAfter = nullptr;
    Require(X509_getm_notAfter_nid_postfix(&certificate) == nullptr);
}

static void TestGet0NotBefore() {
    LibreSsl3X509Validity validity {};
    LibreSsl3ValidityCertInfo info {};
    LibreSsl3ValidityX509 certificate {};
    Require(X509_get0_notBefore_nid_postfix(nullptr) == nullptr);
    Require(X509_get0_notBefore_nid_postfix(&certificate) == nullptr);
    certificate.certInfo = &info;
    Require(X509_get0_notBefore_nid_postfix(&certificate) == nullptr);
    info.validity = &validity;
    Require(X509_get0_notBefore_nid_postfix(&certificate) == nullptr);
    validity.notBefore = &Before;
    validity.notAfter = &After;
    Require(X509_get0_notBefore_nid_postfix(&certificate) == &Before);
    validity.notBefore = nullptr;
    Require(X509_get0_notBefore_nid_postfix(&certificate) == nullptr);
}

static void TestGet0NotAfter() {
    LibreSsl3X509Validity validity {};
    LibreSsl3ValidityCertInfo info {};
    LibreSsl3ValidityX509 certificate {};
    Require(X509_get0_notAfter_nid_postfix(nullptr) == nullptr);
    Require(X509_get0_notAfter_nid_postfix(&certificate) == nullptr);
    certificate.certInfo = &info;
    Require(X509_get0_notAfter_nid_postfix(&certificate) == nullptr);
    info.validity = &validity;
    Require(X509_get0_notAfter_nid_postfix(&certificate) == nullptr);
    validity.notBefore = &Before;
    validity.notAfter = &After;
    Require(X509_get0_notAfter_nid_postfix(&certificate) == &After);
    validity.notAfter = nullptr;
    Require(X509_get0_notAfter_nid_postfix(&certificate) == nullptr);
}

static void TestGet0Signature() {
    int signatureBits = 0;
    int algorithm = 0;
    LibreSsl3ValidityX509 certificate {};
    const void* signature = &Before;
    const void* signatureAlgorithm = &Before;
    X509_get0_signature_nid_postfix(&signature, &signatureAlgorithm, &certificate);
    Require(signature == nullptr && signatureAlgorithm == nullptr);
    certificate.signature = &signatureBits;
    certificate.sigAlg = &algorithm;
    X509_get0_signature_nid_postfix(&signature, &signatureAlgorithm, &certificate);
    Require(signature == &signatureBits && signatureAlgorithm == &algorithm);
    signature = signatureAlgorithm = &After;
    X509_get0_signature_nid_postfix(nullptr, &signatureAlgorithm, &certificate);
    Require(signature == &After && signatureAlgorithm == &algorithm);
    X509_get0_signature_nid_postfix(&signature, nullptr, &certificate);
    Require(signature == &signatureBits);
    X509_get0_signature_nid_postfix(nullptr, nullptr, &certificate);
    X509_get0_signature_nid_postfix(nullptr, nullptr, nullptr);
    RequireThrows([&] { X509_get0_signature_nid_postfix(&signature, &signatureAlgorithm, nullptr); });
    RequireThrows([&] { X509_get0_signature_nid_postfix(&signature, nullptr, nullptr); });
    RequireThrows([&] { X509_get0_signature_nid_postfix(nullptr, &signatureAlgorithm, nullptr); });
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
    T* As() {
        Require(sizeof(T) <= bytes);
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
static void RejectsRuntime(TCall call, std::source_location location = std::source_location::current()) {
    errno = 71;
    bool rejected = false;
    try {
        call();
    } catch (const std::runtime_error&) {
        rejected = errno == 71;
    }
    Require(rejected, location);
}

static void TestGuestPointers() {
    auto* badCertificate = reinterpret_cast<const LibreSsl3ValidityX509*>(std::uintptr_t{0x10});
    auto* badCertInfo = reinterpret_cast<LibreSsl3ValidityCertInfo*>(std::uintptr_t{0x10});
    auto* badValidity = reinterpret_cast<LibreSsl3X509Validity*>(std::uintptr_t{0x10});
    auto* badOut = reinterpret_cast<const void**>(std::uintptr_t{0x10});

    RejectsRuntime([&] { X509_getm_notBefore_nid_postfix(badCertificate); });
    RejectsRuntime([&] { X509_getm_notAfter_nid_postfix(badCertificate); });
    RejectsRuntime([&] { X509_get0_notBefore_nid_postfix(badCertificate); });
    RejectsRuntime([&] { X509_get0_notAfter_nid_postfix(badCertificate); });

    LibreSsl3ValidityX509 certificate{};
    certificate.certInfo = badCertInfo;
    RejectsRuntime([&] { X509_getm_notBefore_nid_postfix(&certificate); });
    RejectsRuntime([&] { X509_getm_notAfter_nid_postfix(&certificate); });
    RejectsRuntime([&] { X509_get0_notBefore_nid_postfix(&certificate); });
    RejectsRuntime([&] { X509_get0_notAfter_nid_postfix(&certificate); });

    LibreSsl3ValidityCertInfo info{};
    info.validity = badValidity;
    certificate.certInfo = &info;
    RejectsRuntime([&] { X509_getm_notBefore_nid_postfix(&certificate); });
    RejectsRuntime([&] { X509_getm_notAfter_nid_postfix(&certificate); });
    RejectsRuntime([&] { X509_get0_notBefore_nid_postfix(&certificate); });
    RejectsRuntime([&] { X509_get0_notAfter_nid_postfix(&certificate); });

    int signatureBits = 0;
    int algorithmBits = 0;
    certificate.signature = &signatureBits;
    certificate.sigAlg = &algorithmBits;
    const void* signature = &Before;
    const void* signatureAlgorithm = &After;
    RejectsRuntime([&] { X509_get0_signature_nid_postfix(&signature, &signatureAlgorithm, badCertificate); });
    Require(signature == &Before && signatureAlgorithm == &After);
    RejectsRuntime([&] { X509_get0_signature_nid_postfix(badOut, &signatureAlgorithm, &certificate); });
    Require(signatureAlgorithm == &After);
    RejectsRuntime([&] { X509_get0_signature_nid_postfix(&signature, badOut, &certificate); });
    Require(signature == &Before);

    ReadOnlyPage page;
    auto* readOnlyOut = page.As<const void*>();
    *readOnlyOut = &Before;
    page.Protect();
    RejectsRuntime([&] { X509_get0_signature_nid_postfix(readOnlyOut, &signatureAlgorithm, &certificate); });
    Require(*readOnlyOut == &Before && signatureAlgorithm == &After);
    RejectsRuntime([&] { X509_get0_signature_nid_postfix(&signature, readOnlyOut, &certificate); });
    Require(signature == &Before && *readOnlyOut == &Before);
}

int main() {
    TestGetmNotBefore();
    TestGetmNotAfter();
    TestGet0NotBefore();
    TestGet0NotAfter();
    TestGet0Signature();
    TestGuestPointers();
    return 0;
}
