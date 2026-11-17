#include "prx/libSceLibreSsl3/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "Testing.hpp"
#include <cerrno>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <source_location>
#include <stdexcept>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

static int Number[12];

static const void* const Unset = &Number[0];

static LibreSsl3Rsa FilledRsa() {
    LibreSsl3Rsa rsa {};
    rsa.method = &Number[1];
    rsa.n = &Number[2];
    rsa.e = &Number[3];
    rsa.d = &Number[4];
    rsa.p = &Number[5];
    rsa.q = &Number[6];
    rsa.dmp1 = &Number[7];
    rsa.dmq1 = &Number[8];
    rsa.iqmp = &Number[9];
    return rsa;
}

static LibreSsl3Dh FilledDh() {
    LibreSsl3Dh dh {};
    dh.p = &Number[1];
    dh.g = &Number[2];
    dh.pubKey = &Number[3];
    dh.privKey = &Number[4];
    dh.methodMontP = &Number[5];
    dh.q = &Number[6];
    return dh;
}

static LibreSsl3Dsa FilledDsa() {
    LibreSsl3Dsa dsa {};
    dsa.p = &Number[1];
    dsa.q = &Number[2];
    dsa.g = &Number[3];
    dsa.pubKey = &Number[4];
    dsa.privKey = &Number[5];
    return dsa;
}

class ReadOnlyPage {
public:
    explicit ReadOnlyPage(const void* initial = nullptr, std::size_t size = 0) {
#ifdef _WIN32
        SYSTEM_INFO info{};
        GetSystemInfo(&info);
        _size = info.dwPageSize != 0 ? info.dwPageSize : 4096u;
        _page = VirtualAlloc(nullptr, _size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
        Require(_page != nullptr);
        if (initial && size > 0) std::memcpy(_page, initial, size);
        DWORD oldProtect = 0;
        Require(VirtualProtect(_page, _size, PAGE_READONLY, &oldProtect) != 0);
#else
        const long pageSize = sysconf(_SC_PAGESIZE);
        _size = pageSize > 0 ? static_cast<std::size_t>(pageSize) : 4096u;
        _page = mmap(nullptr, _size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        Require(_page != MAP_FAILED);
        if (initial && size > 0) std::memcpy(_page, initial, size);
        Require(mprotect(_page, _size, PROT_READ) == 0);
#endif
    }

    ~ReadOnlyPage() {
        if (!_page) return;
#ifdef _WIN32
        VirtualFree(_page, 0, MEM_RELEASE);
#else
        munmap(_page, _size);
#endif
    }

    ReadOnlyPage(const ReadOnlyPage&) = delete;
    ReadOnlyPage& operator=(const ReadOnlyPage&) = delete;

    template<typename T> T* As() const { return static_cast<T*>(_page); }

private:
    void* _page = nullptr;
    std::size_t _size = 0;
};

static void TestRsaKey() {
    LibreSsl3Rsa key = FilledRsa();
    const void* first = Unset;
    const void* second = Unset;
    const void* third = Unset;
    RSA_get0_key_nid_postfix(&key, &first, &second, &third);
    Require(first == &Number[2] && second == &Number[3] && third == &Number[4]);
    first = second = third = Unset;
    RSA_get0_key_nid_postfix(&key, nullptr, &second, nullptr);
    Require(first == Unset && second == &Number[3] && third == Unset);
    RSA_get0_key_nid_postfix(&key, nullptr, nullptr, nullptr);
    key.n = nullptr;
    key.d = nullptr;
    RSA_get0_key_nid_postfix(&key, &first, nullptr, &third);
    Require(first == nullptr && third == nullptr);
    RSA_get0_key_nid_postfix(nullptr, nullptr, nullptr, nullptr);
    ExpectErr([&] { RSA_get0_key_nid_postfix(nullptr, &first, &second, &third); });
    ExpectErr([&] { RSA_get0_key_nid_postfix(nullptr, nullptr, nullptr, &third); });
}

static void TestDhPqg() {
    LibreSsl3Dh key = FilledDh();
    const void* first = Unset;
    const void* second = Unset;
    const void* third = Unset;
    DH_get0_pqg_nid_postfix(&key, &first, &second, &third);
    Require(first == &Number[1] && second == &Number[6] && third == &Number[2]);
    first = second = third = Unset;
    DH_get0_pqg_nid_postfix(&key, nullptr, &second, nullptr);
    Require(first == Unset && second == &Number[6] && third == Unset);
    DH_get0_pqg_nid_postfix(&key, nullptr, nullptr, nullptr);
    key.p = nullptr;
    key.g = nullptr;
    DH_get0_pqg_nid_postfix(&key, &first, nullptr, &third);
    Require(first == nullptr && third == nullptr);
    DH_get0_pqg_nid_postfix(nullptr, nullptr, nullptr, nullptr);
    ExpectErr([&] { DH_get0_pqg_nid_postfix(nullptr, &first, &second, &third); });
    ExpectErr([&] { DH_get0_pqg_nid_postfix(nullptr, nullptr, &second, nullptr); });
}

static void TestDhKey() {
    LibreSsl3Dh key = FilledDh();
    const void* first = Unset;
    const void* second = Unset;
    DH_get0_key_nid_postfix(&key, &first, &second);
    Require(first == &Number[3] && second == &Number[4]);
    first = second = Unset;
    DH_get0_key_nid_postfix(&key, nullptr, &second);
    Require(first == Unset && second == &Number[4]);
    second = Unset;
    DH_get0_key_nid_postfix(&key, &first, nullptr);
    Require(first == &Number[3] && second == Unset);
    key.privKey = nullptr;
    DH_get0_key_nid_postfix(&key, &first, &second);
    Require(first == &Number[3] && second == nullptr);
    DH_get0_key_nid_postfix(nullptr, nullptr, nullptr);
    ExpectErr([&] { DH_get0_key_nid_postfix(nullptr, &first, &second); });
    ExpectErr([&] { DH_get0_key_nid_postfix(nullptr, &first, nullptr); });
}

static void TestDsaPqg() {
    LibreSsl3Dsa key = FilledDsa();
    const void* first = Unset;
    const void* second = Unset;
    const void* third = Unset;
    DSA_get0_pqg_nid_postfix(&key, &first, &second, &third);
    Require(first == &Number[1] && second == &Number[2] && third == &Number[3]);
    first = second = third = Unset;
    DSA_get0_pqg_nid_postfix(&key, nullptr, &second, nullptr);
    Require(first == Unset && second == &Number[2] && third == Unset);
    DSA_get0_pqg_nid_postfix(&key, nullptr, nullptr, nullptr);
    key.p = nullptr;
    key.g = nullptr;
    DSA_get0_pqg_nid_postfix(&key, &first, nullptr, &third);
    Require(first == nullptr && third == nullptr);
    DSA_get0_pqg_nid_postfix(nullptr, nullptr, nullptr, nullptr);
    ExpectErr([&] { DSA_get0_pqg_nid_postfix(nullptr, &first, &second, &third); });
    ExpectErr([&] { DSA_get0_pqg_nid_postfix(nullptr, &first, nullptr, nullptr); });
}

static void TestDsaKey() {
    LibreSsl3Dsa key = FilledDsa();
    const void* first = Unset;
    const void* second = Unset;
    DSA_get0_key_nid_postfix(&key, &first, &second);
    Require(first == &Number[4] && second == &Number[5]);
    first = second = Unset;
    DSA_get0_key_nid_postfix(&key, nullptr, &second);
    Require(first == Unset && second == &Number[5]);
    second = Unset;
    DSA_get0_key_nid_postfix(&key, &first, nullptr);
    Require(first == &Number[4] && second == Unset);
    key.privKey = nullptr;
    DSA_get0_key_nid_postfix(&key, &first, &second);
    Require(first == &Number[4] && second == nullptr);
    DSA_get0_key_nid_postfix(nullptr, nullptr, nullptr);
    ExpectErr([&] { DSA_get0_key_nid_postfix(nullptr, &first, &second); });
    ExpectErr([&] { DSA_get0_key_nid_postfix(nullptr, nullptr, &second); });
}

static void TestGuestPointers() {
    ReadOnlyPage roOut(&Unset, sizeof(Unset));
    const void** const badPtr = roOut.As<const void*>();

    const LibreSsl3Rsa rsa = FilledRsa();
    const LibreSsl3Dh dh = FilledDh();
    const LibreSsl3Dsa dsa = FilledDsa();

    const void* first = Unset;
    const void* second = Unset;
    const void* third = Unset;

    errno = 73;
    ExpectErr<std::runtime_error>([&] { RSA_get0_key_nid_postfix(reinterpret_cast<const LibreSsl3Rsa*>(0x10), &first, &second, &third); });
    Require(errno == 73 && first == Unset && second == Unset && third == Unset);
    errno = 73;
    ExpectErr<std::runtime_error>([&] { RSA_get0_key_nid_postfix(&rsa, badPtr, &second, &third); });
    Require(errno == 73 && second == Unset && third == Unset);
    errno = 73;
    ExpectErr<std::runtime_error>([&] { RSA_get0_key_nid_postfix(&rsa, &first, badPtr, &third); });
    Require(errno == 73 && first == Unset && third == Unset);
    errno = 73;
    ExpectErr<std::runtime_error>([&] { RSA_get0_key_nid_postfix(&rsa, &first, &second, badPtr); });
    Require(errno == 73 && first == Unset && second == Unset);
    const void* shared = Unset;
    errno = 73;
    ExpectErr<std::runtime_error>([&] { RSA_get0_key_nid_postfix(&rsa, &shared, &shared, badPtr); });
    Require(errno == 73 && shared == Unset);

    errno = 73;
    ExpectErr<std::runtime_error>([&] { DH_get0_pqg_nid_postfix(reinterpret_cast<const LibreSsl3Dh*>(0x10), &first, &second, &third); });
    Require(errno == 73 && first == Unset && second == Unset && third == Unset);
    errno = 73;
    ExpectErr<std::runtime_error>([&] { DH_get0_pqg_nid_postfix(&dh, badPtr, &second, &third); });
    Require(errno == 73 && second == Unset && third == Unset);
    errno = 73;
    ExpectErr<std::runtime_error>([&] { DH_get0_pqg_nid_postfix(&dh, &first, badPtr, &third); });
    Require(errno == 73 && first == Unset && third == Unset);
    errno = 73;
    ExpectErr<std::runtime_error>([&] { DH_get0_pqg_nid_postfix(&dh, &first, &second, badPtr); });
    Require(errno == 73 && first == Unset && second == Unset);

    errno = 73;
    ExpectErr<std::runtime_error>([&] { DH_get0_key_nid_postfix(reinterpret_cast<const LibreSsl3Dh*>(0x10), &first, &second); });
    Require(errno == 73 && first == Unset && second == Unset);
    errno = 73;
    ExpectErr<std::runtime_error>([&] { DH_get0_key_nid_postfix(&dh, badPtr, &second); });
    Require(errno == 73 && second == Unset);
    errno = 73;
    ExpectErr<std::runtime_error>([&] { DH_get0_key_nid_postfix(&dh, &first, badPtr); });
    Require(errno == 73 && first == Unset);

    errno = 73;
    ExpectErr<std::runtime_error>([&] { DSA_get0_pqg_nid_postfix(reinterpret_cast<const LibreSsl3Dsa*>(0x10), &first, &second, &third); });
    Require(errno == 73 && first == Unset && second == Unset && third == Unset);
    errno = 73;
    ExpectErr<std::runtime_error>([&] { DSA_get0_pqg_nid_postfix(&dsa, badPtr, &second, &third); });
    Require(errno == 73 && second == Unset && third == Unset);
    errno = 73;
    ExpectErr<std::runtime_error>([&] { DSA_get0_pqg_nid_postfix(&dsa, &first, badPtr, &third); });
    Require(errno == 73 && first == Unset && third == Unset);
    errno = 73;
    ExpectErr<std::runtime_error>([&] { DSA_get0_pqg_nid_postfix(&dsa, &first, &second, badPtr); });
    Require(errno == 73 && first == Unset && second == Unset);

    errno = 73;
    ExpectErr<std::runtime_error>([&] { DSA_get0_key_nid_postfix(reinterpret_cast<const LibreSsl3Dsa*>(0x10), &first, &second); });
    Require(errno == 73 && first == Unset && second == Unset);
    errno = 73;
    ExpectErr<std::runtime_error>([&] { DSA_get0_key_nid_postfix(&dsa, badPtr, &second); });
    Require(errno == 73 && second == Unset);
    errno = 73;
    ExpectErr<std::runtime_error>([&] { DSA_get0_key_nid_postfix(&dsa, &first, badPtr); });
    Require(errno == 73 && first == Unset);
}

int main() {
    TestRsaKey();
    TestDhPqg();
    TestDhKey();
    TestDsaPqg();
    TestDsaKey();
    TestGuestPointers();
    return 0;
}
