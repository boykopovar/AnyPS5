#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

#include <algorithm>
#include <cerrno>
#include <climits>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <new>
#include <source_location>
#include <vector>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "LibreSSL3 BN_dup check failed at line %u\n", location.line());
    std::abort();
}

int liveBlocks = 0;
std::size_t largestZeroed = 0;
int zeroedCallsBeforeFailure = -1;

constexpr std::uint64_t MallocFailure = 0x03fff041;
constexpr std::uint64_t BignumTooLong = 0x03fff072;

void* APS5_VABI Allocate(std::size_t bytes) {
    ++liveBlocks;
    return std::malloc(bytes ? bytes : 1);
}

void APS5_VABI Release(void* block) {
    if (block) --liveBlocks;
    std::free(block);
}

void* APS5_VABI AllocateZeroed(std::size_t count, std::size_t bytes) {
    if (zeroedCallsBeforeFailure == 0) {
        zeroedCallsBeforeFailure = -1;
        return nullptr;
    }
    if (zeroedCallsBeforeFailure > 0) --zeroedCallsBeforeFailure;
    largestZeroed = std::max(largestZeroed, count * bytes);
    ++liveBlocks;
    return std::calloc(count ? count : 1, bytes ? bytes : 1);
}

void* APS5_VABI Reallocate(void*, std::size_t) { return nullptr; }
void* APS5_VABI Align(std::size_t, std::size_t) { return nullptr; }
void* APS5_VABI Realign(void*, std::size_t, std::size_t) { return nullptr; }
int APS5_VABI PosixAlign(void**, std::size_t, std::size_t) { return 12; }

void InitializeHeap() {
    void* const api[10] {
        reinterpret_cast<void*>(&Allocate), reinterpret_cast<void*>(&Release), reinterpret_cast<void*>(&AllocateZeroed),
        reinterpret_cast<void*>(&Reallocate), reinterpret_cast<void*>(&Align), reinterpret_cast<void*>(&Realign),
        reinterpret_cast<void*>(&PosixAlign),
    };
    ApplicationHeapRegister_nid_no_patch(api);
}

class GuardedPage {
public:
    GuardedPage() {
#ifdef _WIN32
        SYSTEM_INFO info{};
        GetSystemInfo(&info);
        size = info.dwPageSize;
        base = VirtualAlloc(nullptr, 2 * size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
        Require(base != nullptr);
        DWORD previous{};
        Require(VirtualProtect(static_cast<char*>(base) + size, size, PAGE_NOACCESS, &previous) != 0);
#else
        size = static_cast<std::size_t>(sysconf(_SC_PAGESIZE));
        base = mmap(nullptr, 2 * size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        Require(base != MAP_FAILED);
        Require(mprotect(static_cast<char*>(base) + size, size, PROT_NONE) == 0);
#endif
    }

    ~GuardedPage() {
#ifdef _WIN32
        VirtualFree(base, 0, MEM_RELEASE);
#else
        munmap(base, 2 * size);
#endif
    }

    void* Get() const { return base; }

    void MakeReadOnly() {
#ifdef _WIN32
        DWORD previous{};
        Require(VirtualProtect(base, size, PAGE_READONLY, &previous) != 0);
#else
        Require(mprotect(base, size, PROT_READ) == 0);
#endif
    }

private:
    void* base = nullptr;
    std::size_t size = 0;
};

template<class TAction>
bool Throws(TAction action) {
    try {
        action();
    } catch (const std::bad_alloc&) {
        return false;
    } catch (const std::exception&) {
        return true;
    }
    return false;
}

Bignum* FromHex(const char* hex) {
    Bignum* value = nullptr;
    Require(BN_hex2bn_nid_postfix(&value, hex) > 0);
    return value;
}

std::vector<unsigned char> Bytes(const Bignum* value) {
    std::vector<unsigned char> bytes((BN_num_bits_nid_postfix(value) + 7) / 8);
    Require(BN_bn2bin_nid_postfix(value, bytes.data()) == static_cast<int>(bytes.size()));
    return bytes;
}

}

int main() {
    InitializeHeap();
    Require(BN_dup_nid_postfix(nullptr) == nullptr);

    Bignum* source = FromHex("-1234567890ABCDEF0FEDCBA0987654321");
    Bignum* copy = BN_dup_nid_postfix(source);
    Require(copy != nullptr && copy != source && copy->d != source->d);
    Require(copy->top == source->top && copy->dmax == source->top && copy->neg == 1);
    for (int i = 0; i < source->top; ++i) Require(copy->d[i] == source->d[i]);
    Require(Bytes(copy) == Bytes(source));
    Require(BN_add_word_nid_postfix(copy, 1) == 1);
    Require(Bytes(copy) != Bytes(source));
    BN_free_nid_postfix(copy);

    source->flags |= 0x04;
    copy = BN_dup_nid_postfix(source);
    Require((copy->flags & 0x04) != 0);
    BN_free_nid_postfix(copy);
    BN_free_nid_postfix(source);

    Bignum* zero = BN_new_nid_postfix();
    Bignum* zeroCopy = BN_dup_nid_postfix(zero);
    Require(zeroCopy != nullptr && zeroCopy->top == 0 && zeroCopy->neg == 0 && zeroCopy->d == nullptr && zeroCopy->dmax == 0);
    Require((zeroCopy->flags & 0x04) == 0);
    BN_free_nid_postfix(zeroCopy);
    BN_free_nid_postfix(zero);
    Require(ERR_get_error_nid_postfix() == 0);

    std::uint64_t word = 1;
    Bignum negative{};
    negative.d = &word;
    negative.top = -1;
    Require(BN_dup_nid_postfix(&negative) == nullptr);
    Require(ERR_get_error_nid_postfix() == 0);

    Bignum tooLong{};
    tooLong.d = &word;
    tooLong.top = INT_MAX / 256 + 1;
    Require(BN_dup_nid_postfix(&tooLong) == nullptr);
    Require(ERR_get_error_nid_postfix() == BignumTooLong);
    Require(ERR_get_error_nid_postfix() == 0);

    source = FromHex("123456789ABCDEF0123456789ABCDEF");
    zeroedCallsBeforeFailure = 0;
    Require(BN_dup_nid_postfix(source) == nullptr);
    Require(ERR_get_error_nid_postfix() == MallocFailure);
    Require(ERR_get_error_nid_postfix() == 0);

    zeroedCallsBeforeFailure = 1;
    Require(BN_dup_nid_postfix(source) == nullptr);
    Require(ERR_get_error_nid_postfix() == MallocFailure);
    Require(ERR_get_error_nid_postfix() == 0);
    BN_free_nid_postfix(source);
    Require(liveBlocks == 0);

    errno = 71;
    Require(Throws([] { BN_dup_nid_postfix(reinterpret_cast<const Bignum*>(std::uintptr_t{0x10})); }));
    Bignum unmappedWords{};
    unmappedWords.d = reinterpret_cast<std::uint64_t*>(std::uintptr_t{0x10});
    unmappedWords.top = 1;
    Require(Throws([&] { BN_dup_nid_postfix(&unmappedWords); }));
    GuardedPage page;
    std::memset(page.Get(), 0x5a, sizeof(std::uint64_t));
    Bignum oversized{};
    oversized.d = static_cast<std::uint64_t*>(page.Get());
    oversized.top = INT_MAX / 256;
    largestZeroed = 0;
    Require(Throws([&] { BN_dup_nid_postfix(&oversized); }));
    Require(largestZeroed <= sizeof(Bignum) && liveBlocks == 0 && errno == 71);
    oversized.top = 1;
    copy = BN_dup_nid_postfix(&oversized);
    Require(copy != nullptr && copy->top == 1 && copy->d[0] == 0x5a5a5a5a5a5a5a5aULL);
    BN_free_nid_postfix(copy);
    Require(liveBlocks == 0 && ERR_get_error_nid_postfix() == 0);
}
