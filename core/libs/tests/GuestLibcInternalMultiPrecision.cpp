#include "prx/libSceLibcInternal/include/MultiPrecision.hpp"
#include <array>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace {

class GuardPages {
public:
    GuardPages() {
#ifdef _WIN32
        SYSTEM_INFO info{};
        GetSystemInfo(&info);
        page = info.dwPageSize;
        memory = static_cast<char*>(VirtualAlloc(nullptr, 3 * page, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
        if (!memory) std::abort();
#else
        const long size = sysconf(_SC_PAGESIZE);
        if (size <= 0) std::abort();
        page = static_cast<std::size_t>(size);
        memory = static_cast<char*>(mmap(nullptr, 3 * page, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
        if (memory == MAP_FAILED) std::abort();
#endif
    }

    ~GuardPages() {
#ifdef _WIN32
        VirtualFree(memory, 0, MEM_RELEASE);
#else
        munmap(memory, 3 * page);
#endif
    }

    GuardPages(const GuardPages&) = delete;
    GuardPages& operator=(const GuardPages&) = delete;

    void Protect() {
#ifdef _WIN32
        DWORD previous = 0;
        if (VirtualProtect(memory + page, page, PAGE_READONLY, &previous) == 0) std::abort();
        if (VirtualProtect(memory + 2 * page, page, PAGE_NOACCESS, &previous) == 0) std::abort();
#else
        if (mprotect(memory + page, page, PROT_READ) != 0) std::abort();
        if (mprotect(memory + 2 * page, page, PROT_NONE) != 0) std::abort();
#endif
    }

    char* ReadWrite() const { return memory; }
    char* ReadOnly() const { return memory + page; }
    char* NoAccess() const { return memory + 2 * page; }
    std::size_t PageSize() const { return page; }

private:
    char* memory = nullptr;
    std::size_t page = 0;
};

using Words = std::array<unsigned long long, 5>;

constexpr unsigned long long DigitMask = 0xFFFFFFFFull;

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "libc internal multiprecision: %s\n", message);
    std::abort();
}

template<typename TCall>
bool Rejects(const TCall& call) {
    try {
        call();
    } catch (const std::invalid_argument&) {
        return true;
    }
    return false;
}

Words Multiply(unsigned long long left, unsigned long long right) {
    Words product;
    product.fill(0xAAAAAAAAAAAAAAAAull);
    sceLibcInternalMultiPrecisionMultiply_nid_no_patch(product.data(), left, right);
    return product;
}

Words Add(Words value, unsigned long long addend) {
    sceLibcInternalMultiPrecisionAdd_nid_no_patch(value.data(), addend);
    return value;
}

Words Remainder(Words value, unsigned long long divisor) {
    sceLibcInternalMultiPrecisionRemainder_nid_no_patch(value.data(), divisor);
    return value;
}

Words SchoolbookProduct(unsigned long long left, unsigned long long right) {
    const unsigned long long low = (left & DigitMask) * (right & DigitMask);
    const unsigned long long crossLeft = (left >> 32) * (right & DigitMask);
    const unsigned long long crossRight = (left & DigitMask) * (right >> 32);
    const unsigned long long middle = (low >> 32) + (crossLeft & DigitMask) + (crossRight & DigitMask);
    const unsigned long long upper = (left >> 32) * (right >> 32) + (crossLeft >> 32) + (crossRight >> 32) + (middle >> 32);
    return {low & DigitMask, middle & DigitMask, upper & DigitMask, upper >> 32, 0};
}

unsigned long long AddModulo(unsigned long long left, unsigned long long right, unsigned long long modulus) {
    return left >= modulus - right ? left - (modulus - right) : left + right;
}

unsigned long long MultiplyModulo(unsigned long long left, unsigned long long right, unsigned long long modulus) {
    unsigned long long result = 0;
    left %= modulus;
    for (; right != 0; right >>= 1) {
        if (right & 1) result = AddModulo(result, left, modulus);
        left = AddModulo(left, left, modulus);
    }
    return result;
}

void CheckMultiply() {
    const unsigned long long values[] = {0, 1, DigitMask, DigitMask + 1, 1ull << 63, ~0ull, 0x0123456789ABCDEFull};
    for (const auto left : values) {
        for (const auto right : values) {
            Require(Multiply(left, right) == SchoolbookProduct(left, right), "_MP_Mul writes the 128-bit product as four digits and clears the fifth word");
        }
    }
}

void CheckGet() {
    Words value{5, 7, 9, 11, 13};
    Require(sceLibcInternalMultiPrecisionGet_nid_no_patch(value.data()) == (7ull << 32) + 5, "_MP_Get joins the two low digits");
    value = {1, 0x100000002ull, 0, 0, 0};
    Require(sceLibcInternalMultiPrecisionGet_nid_no_patch(value.data()) == 0x200000001ull, "_MP_Get wraps a second word wider than a digit");
}

void CheckAdd() {
    Require(Add({DigitMask, DigitMask, 0, 0, 0}, 1) == Words{0, 0, 1, 0, 0}, "_MP_Add carries into the third digit");
    Require(Add({DigitMask, DigitMask, DigitMask, DigitMask, 0}, 1) == Words{0, 0, 0, 0, 1}, "_MP_Add carries past 2^128 into the fifth word");
    Require(Add({DigitMask, DigitMask, DigitMask, DigitMask, DigitMask}, 1) == Words{}, "_MP_Add drops the carry out of the fifth word");
    Require(Add({0x1FFFFFFFFull, 0, 0, 0, 7}, 0) == Words{DigitMask, 1, 0, 0, 7}, "_MP_Add carries a first word wider than a digit");
    Require(Add({3, 0x100000000ull, 0, 0, 0}, 0xFFFFFFFF00000001ull) == Words{4, DigitMask, 1, 0, 0}, "_MP_Add adds both digits of the addend");
}

void CheckRemainder() {
    for (const auto modulus : {(1ull << 32) + 15, (1ull << 61) - 1, ~0ull - 58}) {
        const unsigned long long multiplier = 6364136223846793005ull;
        const unsigned long long increment = 1442695040888963407ull % modulus;
        unsigned long long state = 12345;
        for (int step = 0; step < 10000; ++step) {
            const unsigned long long expected = AddModulo(MultiplyModulo(multiplier, state, modulus), increment, modulus);
            Words value{};
            sceLibcInternalMultiPrecisionMultiply_nid_no_patch(value.data(), multiplier, state);
            sceLibcInternalMultiPrecisionAdd_nid_no_patch(value.data(), increment);
            sceLibcInternalMultiPrecisionRemainder_nid_no_patch(value.data(), modulus);
            Require(value[2] == 0 && value[3] == 0 && value[4] == 0, "_MP_Rem leaves the remainder in the two low digits");
            state = sceLibcInternalMultiPrecisionGet_nid_no_patch(value.data());
            Require(state == expected, "_MP_Rem gives the remainder of a linear congruential step");
        }
    }
    const Words wide{0x89ABCDEF, 0x01234567, 0xFEDCBA98, 0x76543210, 0};
    Require(Remainder(wide, ~0ull - 58) == Words{0x468AD530, 0x468ACF52, 0, 0, 0}, "_MP_Rem reduces a four-digit value");
    Require(Remainder(wide, (1ull << 61) - 1) == Words{0x8091A2CC, 0x13C4D5EF, 0, 0, 0}, "_MP_Rem normalizes a divisor with a small high digit");
    Require(Remainder(wide, 1ull << 32) == Words{0x89ABCDEF, 0, 0, 0, 0}, "_MP_Rem by 2^32 keeps the lowest digit");
    Require(Remainder({5, 0, 0, 0, 0}, 1ull << 32) == Words{5, 0, 0, 0, 0}, "_MP_Rem keeps a value below the divisor");
    Require(Remainder({0, 0, 1, 0, 0}, 1ull << 32) == Words{}, "_MP_Rem of a multiple is zero");
    Require(Remainder({0, 0, 0x7FFFFFFF, 0, 0}, 0x80000000FFFFFFFEull) == Words{0xFFFFFFF8, 5, 0, 0, 0}, "_MP_Rem corrects the trial quotient twice");
    Require(Remainder({7, 0, 0, 0, 0}, DigitMask) == Words{7, 0, 0, 0, 0}, "_MP_Rem keeps a digit below a one-digit divisor");
    Require(Remainder({5, 0, 0, 0, 0}, 7) == Words{5, 0, 0, 0, 0}, "_MP_Rem keeps a digit below a scaled one-digit divisor");
    Require(Remainder({0x123456789ull, 0xFFFFFFFFFull, 3, 0, 0}, 0x1FFFFFFFFull) == Words{0xA3456789, 1, 0, 0, 0}, "_MP_Rem scales words wider than a digit");
    Require(Remainder({~0ull, ~0ull, 0, 0, 0}, 0xFFFFFFFFFFFFFFC5ull) == Words{0xFFFFFF89, 0x39, 2, DigitMask, DigitMask}, "_MP_Rem propagates a borrow past the top digit");
}

void CheckRejections() {
    Words zero{};
    Require(Rejects([&] { sceLibcInternalMultiPrecisionRemainder_nid_no_patch(zero.data(), 1ull << 32); }) && zero == Words{}, "_MP_Rem rejects a zero value");
    Words fifth{1, 0, 0, 0, 1};
    Require(Rejects([&] { sceLibcInternalMultiPrecisionRemainder_nid_no_patch(fifth.data(), 1ull << 32); }) && fifth == Words{1, 0, 0, 0, 1}, "_MP_Rem rejects a nonzero fifth word");
    Words small{8, 0, 0, 0, 0};
    Require(Rejects([&] { sceLibcInternalMultiPrecisionRemainder_nid_no_patch(small.data(), 0); }), "_MP_Rem rejects a zero divisor");
    Require(Rejects([&] { sceLibcInternalMultiPrecisionRemainder_nid_no_patch(small.data(), 7); }) && small == Words{8, 0, 0, 0, 0}, "_MP_Rem rejects a nonzero quotient digit for a one-digit divisor");
    Require(Rejects([] { sceLibcInternalMultiPrecisionGet_nid_no_patch(nullptr); }), "_MP_Get rejects a null value");
    Require(Rejects([] { sceLibcInternalMultiPrecisionAdd_nid_no_patch(nullptr, 1); }), "_MP_Add rejects a null value");
    Require(Rejects([] { sceLibcInternalMultiPrecisionMultiply_nid_no_patch(nullptr, 1, 1); }), "_MP_Mul rejects a null product");
    Require(Rejects([] { sceLibcInternalMultiPrecisionRemainder_nid_no_patch(nullptr, 1ull << 32); }), "_MP_Rem rejects a null value");
}

void CheckGuestPointers() {
    auto* unmapped = reinterpret_cast<unsigned long long*>(std::uintptr_t{0x10});
    errno = 71;
    Require(Rejects([&] { sceLibcInternalMultiPrecisionGet_nid_no_patch(unmapped); }) && errno == 71, "_MP_Get rejects an unmapped value");
    Require(Rejects([&] { sceLibcInternalMultiPrecisionAdd_nid_no_patch(unmapped, 1); }) && errno == 71, "_MP_Add rejects an unmapped value");
    Require(Rejects([&] { sceLibcInternalMultiPrecisionMultiply_nid_no_patch(unmapped, 1, 1); }) && errno == 71, "_MP_Mul rejects an unmapped product");
    Require(Rejects([&] { sceLibcInternalMultiPrecisionRemainder_nid_no_patch(unmapped, 1ull << 32); }) && errno == 71, "_MP_Rem rejects an unmapped value");

    GuardPages pages;
    const Words initial{5, 7, 9, 11, 0};
    auto* readOnly = reinterpret_cast<unsigned long long*>(pages.ReadOnly()) + 5;
    std::memcpy(readOnly, initial.data(), sizeof(initial));
    auto* straddleReadOnly = reinterpret_cast<unsigned long long*>(pages.ReadOnly() - sizeof(unsigned long long));
    std::memcpy(straddleReadOnly, initial.data(), sizeof(initial));
    auto* straddleNoAccess = reinterpret_cast<unsigned long long*>(pages.NoAccess() - sizeof(unsigned long long));
    straddleNoAccess[0] = 5;
    pages.Protect();

    auto* noAccess = reinterpret_cast<unsigned long long*>(pages.NoAccess());
    errno = 71;
    Require(sceLibcInternalMultiPrecisionGet_nid_no_patch(readOnly) == (7ull << 32) + 5 && errno == 71, "_MP_Get reads a read-only value");
    Require(sceLibcInternalMultiPrecisionGet_nid_no_patch(straddleReadOnly) == (7ull << 32) + 5 && errno == 71, "_MP_Get reads words before a read-only boundary");
    Require(Rejects([&] { sceLibcInternalMultiPrecisionGet_nid_no_patch(noAccess); }) && errno == 71, "_MP_Get rejects a no-access page");
    Require(Rejects([&] { sceLibcInternalMultiPrecisionGet_nid_no_patch(straddleNoAccess); }) && errno == 71, "_MP_Get rejects words straddling into a no-access page");

    Require(Rejects([&] { sceLibcInternalMultiPrecisionAdd_nid_no_patch(readOnly, 1); }) && errno == 71, "_MP_Add rejects a read-only value");
    Require(Rejects([&] { sceLibcInternalMultiPrecisionMultiply_nid_no_patch(readOnly, 2, 3); }) && errno == 71, "_MP_Mul rejects a read-only product");
    Require(Rejects([&] { sceLibcInternalMultiPrecisionRemainder_nid_no_patch(readOnly, 1ull << 32); }) && errno == 71, "_MP_Rem rejects a read-only value");
    Require(std::memcmp(readOnly, initial.data(), sizeof(initial)) == 0, "read-only words remain untouched");

    Require(Rejects([&] { sceLibcInternalMultiPrecisionAdd_nid_no_patch(straddleReadOnly, 1); }) && errno == 71, "_MP_Add rejects words straddling into a read-only page");
    Require(Rejects([&] { sceLibcInternalMultiPrecisionMultiply_nid_no_patch(straddleReadOnly, 2, 3); }) && errno == 71, "_MP_Mul rejects a product straddling into a read-only page");
    Require(Rejects([&] { sceLibcInternalMultiPrecisionRemainder_nid_no_patch(straddleReadOnly, 1ull << 32); }) && errno == 71, "_MP_Rem rejects words straddling into a read-only page");
    Require(std::memcmp(straddleReadOnly, initial.data(), sizeof(initial)) == 0, "prefix words before a read-only boundary remain untouched");
}

}

int main() {
    CheckMultiply();
    CheckGet();
    CheckAdd();
    CheckRemainder();
    CheckRejections();
    CheckGuestPointers();
    std::puts("libc internal multiprecision checks passed");
    return 0;
}
