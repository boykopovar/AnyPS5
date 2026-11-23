#include "prx/libSceLibreSsl3/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <source_location>
#include <stdexcept>
#include <string>
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

static void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "LibreSSL3 X509 public-key parameter check failed at line %u\n", location.line());
    std::abort();
}

template<typename TAction> static void RequireThrows(TAction action, std::source_location location = std::source_location::current()) {
    try { action(); } catch (const std::invalid_argument&) { return; }
    Require(false, location);
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

template <typename TAction>
static void Rejects(TAction action, std::source_location location = std::source_location::current()) {
    errno = EINVAL;
    bool threw = false;
    try { action(); } catch (const std::runtime_error&) { threw = true; }
    Require(threw, location);
    Require(errno == EINVAL, location);
}

int main() {
    void* oid = reinterpret_cast<void*>(0x1234);
    LibreSsl3Asn1Type parameter{5, nullptr};
    unsigned char keyBytes[] = {0x30, 0x03, 0x02, 0x01, 0x01};
    LibreSsl3Asn1String bitString{5, 3, keyBytes, 0};
    LibreSsl3X509Algor algorithmInfo{oid, &parameter};
    LibreSsl3X509Pubkey pubkey{&algorithmInfo, &bitString, nullptr};

    void* returnedOid = nullptr;
    const unsigned char* returnedKey = nullptr;
    int returnedLength = -1;
    void* returnedAlgorithm = nullptr;
    Require(X509_PUBKEY_get0_param_nid_postfix(&returnedOid, &returnedKey, &returnedLength,
        &returnedAlgorithm, &pubkey) == 1);
    Require(returnedOid == oid && returnedKey == keyBytes && returnedLength == 5 && returnedAlgorithm == &algorithmInfo);

    returnedKey = nullptr;
    returnedLength = -1;
    Require(X509_PUBKEY_get0_param_nid_postfix(nullptr, &returnedKey, &returnedLength, nullptr, &pubkey) == 1);
    Require(returnedKey == keyBytes && returnedLength == 5);

    returnedLength = 91;
    Require(X509_PUBKEY_get0_param_nid_postfix(nullptr, nullptr, &returnedLength, nullptr, &pubkey) == 1);
    Require(returnedLength == 91);

    returnedOid = nullptr;
    Require(X509_PUBKEY_get0_param_nid_postfix(&returnedOid, nullptr, nullptr, nullptr, &pubkey) == 1);
    Require(returnedOid == oid);

    returnedAlgorithm = nullptr;
    Require(X509_PUBKEY_get0_param_nid_postfix(nullptr, nullptr, nullptr, &returnedAlgorithm, &pubkey) == 1);
    Require(returnedAlgorithm == &algorithmInfo);
    Require(X509_PUBKEY_get0_param_nid_postfix(nullptr, nullptr, nullptr, nullptr, nullptr) == 1);

    RequireThrows([&] { X509_PUBKEY_get0_param_nid_postfix(nullptr, &returnedKey, nullptr, nullptr, &pubkey); });
    RequireThrows([&] { X509_PUBKEY_get0_param_nid_postfix(&returnedOid, nullptr, nullptr, nullptr, nullptr); });
    RequireThrows([&] { X509_PUBKEY_get0_param_nid_postfix(nullptr, &returnedKey, &returnedLength, nullptr, nullptr); });
    LibreSsl3X509Pubkey noAlgorithm{nullptr, &bitString, nullptr};
    LibreSsl3X509Pubkey noPublicKey{&algorithmInfo, nullptr, nullptr};
    RequireThrows([&] { X509_PUBKEY_get0_param_nid_postfix(&returnedOid, nullptr, nullptr, nullptr, &noAlgorithm); });
    RequireThrows([&] { X509_PUBKEY_get0_param_nid_postfix(nullptr, &returnedKey, &returnedLength, nullptr, &noPublicKey); });

    {
        ReadOnlyPage algorPage;
        auto* roAlgor = algorPage.Set(algorithmInfo);
        algorPage.Protect();
        ReadOnlyPage bitStringPage;
        auto* roBitString = bitStringPage.Set(bitString);
        bitStringPage.Protect();
        ReadOnlyPage pubkeyPage;
        auto* roPubkey = pubkeyPage.Set(LibreSsl3X509Pubkey{roAlgor, roBitString, nullptr});
        pubkeyPage.Protect();

        returnedOid = nullptr;
        returnedKey = nullptr;
        returnedLength = -1;
        returnedAlgorithm = nullptr;
        errno = EINVAL;
        Require(X509_PUBKEY_get0_param_nid_postfix(&returnedOid, &returnedKey, &returnedLength,
            &returnedAlgorithm, roPubkey) == 1);
        Require(errno == EINVAL);
        Require(returnedOid == oid && returnedKey == keyBytes && returnedLength == 5 && returnedAlgorithm == roAlgor);

        auto* badVoidPtr = reinterpret_cast<void**>(0x10);
        auto* badPubkey = reinterpret_cast<const void*>(0x10);
        auto* badUcharPtr = reinterpret_cast<const unsigned char**>(0x10);
        auto* badInt = reinterpret_cast<int*>(0x10);

        errno = EINVAL;
        Require(X509_PUBKEY_get0_param_nid_postfix(nullptr, nullptr, badInt, nullptr, badPubkey) == 1);
        Require(errno == EINVAL);

        Rejects([&] { X509_PUBKEY_get0_param_nid_postfix(&returnedOid, nullptr, nullptr, nullptr, badPubkey); });
        Rejects([&] { X509_PUBKEY_get0_param_nid_postfix(nullptr, &returnedKey, &returnedLength, nullptr, badPubkey); });
        Rejects([&] { X509_PUBKEY_get0_param_nid_postfix(nullptr, nullptr, nullptr, &returnedAlgorithm, badPubkey); });

        LibreSsl3X509Pubkey badAlgorKey{reinterpret_cast<LibreSsl3X509Algor*>(0x10), roBitString, nullptr};
        Rejects([&] { X509_PUBKEY_get0_param_nid_postfix(&returnedOid, nullptr, nullptr, nullptr, &badAlgorKey); });

        LibreSsl3X509Pubkey badEncodedKey{roAlgor, reinterpret_cast<LibreSsl3Asn1String*>(0x10), nullptr};
        Rejects([&] { X509_PUBKEY_get0_param_nid_postfix(nullptr, &returnedKey, &returnedLength, nullptr, &badEncodedKey); });

        void* sentinelVoid = reinterpret_cast<void*>(0x1111);
        const unsigned char* sentinelBytes = reinterpret_cast<const unsigned char*>(0x2222);
        returnedOid = sentinelVoid;
        returnedKey = sentinelBytes;
        returnedLength = -77;
        returnedAlgorithm = sentinelVoid;

        Rejects([&] { X509_PUBKEY_get0_param_nid_postfix(badVoidPtr, &returnedKey, &returnedLength, &returnedAlgorithm, roPubkey); });
        Require(returnedKey == sentinelBytes && returnedLength == -77 && returnedAlgorithm == sentinelVoid);

        Rejects([&] { X509_PUBKEY_get0_param_nid_postfix(&returnedOid, badUcharPtr, &returnedLength, &returnedAlgorithm, roPubkey); });
        Require(returnedOid == sentinelVoid && returnedLength == -77 && returnedAlgorithm == sentinelVoid);

        Rejects([&] { X509_PUBKEY_get0_param_nid_postfix(&returnedOid, &returnedKey, badInt, &returnedAlgorithm, roPubkey); });
        Require(returnedOid == sentinelVoid && returnedKey == sentinelBytes && returnedAlgorithm == sentinelVoid);

        Rejects([&] { X509_PUBKEY_get0_param_nid_postfix(&returnedOid, &returnedKey, &returnedLength, badVoidPtr, roPubkey); });
        Require(returnedOid == sentinelVoid && returnedKey == sentinelBytes && returnedLength == -77);

        ReadOnlyPage roOutPage;
        auto* roOutVoid = roOutPage.Set<void*>(sentinelVoid);
        roOutPage.Protect();
        Rejects([&] { X509_PUBKEY_get0_param_nid_postfix(&returnedOid, &returnedKey, &returnedLength, roOutVoid, roPubkey); });
        Require(returnedOid == sentinelVoid && returnedKey == sentinelBytes && returnedLength == -77 && *roOutVoid == sentinelVoid);
    }

    std::puts("libSceLibreSsl3 X509 public-key parameter tests passed");
}
