#include "prx/libSceLibreSsl/libSceLibreSsl.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include <stdexcept>
#include <source_location>
#include <string>

#ifndef _WIN32
static std::size_t failHostAllocationOfSize = 0;
void* operator new(std::size_t bytes) {
    if (bytes != 0 && failHostAllocationOfSize == bytes) {
        failHostAllocationOfSize = 0;
        throw std::bad_alloc();
    }
    if (void* pointer = std::malloc(bytes ? bytes : 1)) return pointer;
    throw std::bad_alloc();
}
void operator delete(void* pointer) noexcept { std::free(pointer); }
void operator delete(void* pointer, std::size_t) noexcept { std::free(pointer); }
#endif

using Bn = Bignum;
using BnCtx = BignumContext;

static_assert(sizeof(Bn) == 24);
static_assert(offsetof(Bn, top) == 8);
static_assert(offsetof(Bn, neg) == 16);

static void Require(bool value, int line = __builtin_LINE()) {
    if (value) return;
    std::fprintf(stderr, "check failed at line %d\n", line);
    std::abort();
}

static Bn* FromHex(const char* text) {
    Bn* value = nullptr;
    Require(BN_hex2bn_nid_postfix(&value, text) > 0 && value);
    return value;
}

static std::string Hex(const Bn* value) {
    unsigned char bytes[64];
    const int length = BN_bn2bin_nid_postfix(value, bytes);
    Require(length >= 0 && length <= 64);
    static constexpr char digits[] = "0123456789ABCDEF";
    std::string text;
    for (int i = 0; i < length; i++) {
        text += digits[bytes[i] >> 4];
        text += digits[bytes[i] & 15];
    }
    while (text.size() > 1 && text[0] == '0') text.erase(0, 1);
    if (text.empty()) text = "0";
    return value->neg ? "-" + text : text;
}

static std::string Printed(const Bn* value, int expected = 1) {
    LibreSsl::Bio* bio = BIO_new_nid_postfix(BIO_s_mem_nid_postfix());
    Require(bio && BN_print_nid_postfix(bio, value) == expected);
    char* data = nullptr;
    const auto length = BIO_ctrl_nid_postfix(bio, 3, 0, &data);
    std::string text(data, static_cast<std::size_t>(length));
    BIO_free_nid_postfix(bio);
    return text;
}

static void Print() {
    const char* inputs[][2] = {{"0", "0"}, {"-1A", "-1A"}, {"00F", "F"}, {"10000000000000000", "10000000000000000"},
        {"123456789ABCDEF0FEDCBA9876543210", "123456789ABCDEF0FEDCBA9876543210"}};
    for (const auto& [input, output] : inputs) {
        Bn* value = FromHex(input);
        Require(Printed(value) == output);
        BN_free_nid_postfix(value);
    }
    std::uint64_t limbs[] = {0x2a, 0};
    Bn padded{limbs, 2, 2, 0, 2};
    Require(Printed(&padded) == "2A");
    Bn negativeZero{limbs, 0, 2, 1, 2};
    Require(Printed(&negativeZero) == "-0");
    Bn negative{limbs, 1, 2, 1, 2};
    LibreSsl::Bio* readOnly = BIO_new_mem_buf_nid_postfix("x", 1);
    Require(readOnly && BN_print_nid_postfix(readOnly, &negative) == 0);
    BIO_free_nid_postfix(readOnly);
    ERR_clear_error_nid_postfix();
    Require(BN_print_nid_postfix(nullptr, &negative) == 0);
}

static void InitializeHeap() {
    void* const api[10] {};
    ApplicationHeapRegister_nid_no_patch(api);
}

struct HeapBlock {
    void* pointer = nullptr;
    std::size_t bytes = 0;
    bool operator==(const HeapBlock&) const = default;
};

static std::array<HeapBlock, 16> heapBlocks{};
static std::array<void*, 4> allocationPointers{};
static std::array<void*, 4> releasePointers{};
static std::size_t allocationCount = 0;
static std::size_t releaseCount = 0;
static std::size_t callocCalls = 0;
static std::size_t faultCall = 0;
static std::size_t faultBytes = 0;
static bool throwAllocation = false;
static bool throwFree = false;

struct AllocationFailure {};
struct FreeFailure {};

static void ResetTracking() {
    allocationPointers = {};
    releasePointers = {};
    allocationCount = releaseCount = callocCalls = faultCall = faultBytes = 0;
    throwAllocation = throwFree = false;
}

static void* TrackAllocation(void* pointer, std::size_t bytes) {
    if (!pointer) return nullptr;
    Require(allocationCount < allocationPointers.size());
    allocationPointers[allocationCount++] = pointer;
    for (auto& block : heapBlocks) {
        if (!block.pointer) {
            block = {pointer, bytes};
            return pointer;
        }
    }
    std::abort();
}

static void* APS5_VABI TestAllocate(std::size_t bytes) {
    return TrackAllocation(std::malloc(bytes ? bytes : 1), bytes);
}

static void APS5_VABI TestFree(void* pointer) {
    if (!pointer) return;
    HeapBlock* found = nullptr;
    for (auto& block : heapBlocks) if (block.pointer == pointer) found = &block;
    Require(found && releaseCount < releasePointers.size());
    *found = {};
    releasePointers[releaseCount++] = pointer;
    std::free(pointer);
    if (throwFree) throw FreeFailure{};
}

static void* APS5_VABI TestCalloc(std::size_t count, std::size_t bytes) {
    if (++callocCalls == faultCall) {
        faultBytes = count * bytes;
        if (throwAllocation) throw AllocationFailure{};
        return nullptr;
    }
    return TrackAllocation(std::calloc(count ? count : 1, bytes ? bytes : 1), count * bytes);
}

static void* APS5_VABI TestReallocate(void*, std::size_t) { throw std::runtime_error("unexpected realloc"); }
static void* APS5_VABI TestAlign(std::size_t, std::size_t) { throw std::runtime_error("unexpected aligned allocation"); }
static void* APS5_VABI TestRealign(void*, std::size_t, std::size_t) { throw std::runtime_error("unexpected aligned reallocation"); }
static int APS5_VABI TestPosixAlign(void**, std::size_t, std::size_t) { throw std::runtime_error("unexpected posix_memalign"); }

enum class Operation { Binary, Hexadecimal, Inverse };

static Bn* Convert(Operation operation, Bn*& output) {
    Bn* result = nullptr;
    if (operation == Operation::Binary) {
        const unsigned char bytes[]{1, 2, 3, 4, 5, 6, 7, 8};
        result = BN_bin2bn_nid_postfix(bytes, sizeof(bytes), output);
    } else if (operation == Operation::Hexadecimal) {
        const int consumed = BN_hex2bn_nid_postfix(&output, "102030405060708");
        Require(consumed == 0 || consumed == 15);
        return consumed ? output : nullptr;
    } else {
        std::uint64_t value = 3;
        std::uint64_t modulus = 11;
        Bn a{&value, 1, 1, 0, 2};
        Bn n{&modulus, 1, 1, 0, 2};
        result = BN_mod_inverse_nid_postfix(output, &a, &n, nullptr);
    }
    if (result) output = result;
    return result;
}

static void CheckReleases(Operation operation, bool provided, std::size_t stage, std::size_t expected) {
    if (allocationCount != expected || releaseCount != (provided ? 0 : expected)) {
        std::fprintf(stderr, "BIGNUM cleanup failed: operation=%d provided=%d stage=%zu allocations=%zu releases=%zu expected=%zu\n",
            static_cast<int>(operation), provided, stage, allocationCount, releaseCount, expected);
        std::abort();
    }
    if (!provided && expected) Require(releasePointers[0] == allocationPointers[0]);
}

static void CheckFault(Operation operation, bool provided, std::size_t stage, bool allocationThrows, bool freeThrows) {
    ResetTracking();
    Bn* const original = provided ? BN_new_nid_postfix() : nullptr;
    Require(!provided || original);
    Bn* output = original;
    const auto before = heapBlocks;
    ResetTracking();
    faultCall = stage;
    throwAllocation = allocationThrows;
    throwFree = freeThrows;
    Bn* result = nullptr;
    int caught = 0;
    try {
        result = Convert(operation, output);
    } catch (const AllocationFailure&) {
        caught = 1;
    } catch (const FreeFailure&) {
        caught = 2;
    } catch (...) {
        caught = 3;
    }
    faultCall = 0;
    throwAllocation = throwFree = false;
    const auto expected = !provided && stage == 2 ? 1u : 0u;
    CheckReleases(operation, provided, stage, expected);
    Require(callocCalls == stage && faultBytes == (!provided && stage == 1 ? sizeof(Bn) : sizeof(std::uint64_t)));
    Require(caught == (allocationThrows ? 1 : (freeThrows ? 2 : 0)));
    Require(!result && output == original && heapBlocks == before);
    if (provided) Require(original->d == nullptr && original->top == 0 && original->flags == 1);
    Require(ERR_get_error_nid_postfix() == 0);
    BN_free_nid_postfix(original);
    Require(heapBlocks == std::array<HeapBlock, 16>{});
}

#ifndef _WIN32
static void CheckHostAllocationFault(bool provided, bool freeThrows) {
    ResetTracking();
    Bn* const original = provided ? BN_new_nid_postfix() : nullptr;
    Require(!provided || original);
    Bn* output = original;
    const auto before = heapBlocks;
    ResetTracking();
    throwFree = freeThrows;
    failHostAllocationOfSize = sizeof(std::uint64_t);
    bool caught = false;
    try {
        Convert(Operation::Binary, output);
    } catch (const std::bad_alloc&) {
        caught = true;
    } catch (...) {
        Require(false);
    }
    const bool consumed = failHostAllocationOfSize == 0;
    failHostAllocationOfSize = 0;
    throwFree = false;
    CheckReleases(Operation::Binary, provided, 0, provided ? 0 : 1);
    Require(caught && consumed && callocCalls == (provided ? 0 : 1));
    Require(output == original && heapBlocks == before);
    BN_free_nid_postfix(original);
    Require(heapBlocks == std::array<HeapBlock, 16>{});
}
#endif

static void CleanupFailures() {
    const std::array<void*, 10> api{
        reinterpret_cast<void*>(TestAllocate), reinterpret_cast<void*>(TestFree), reinterpret_cast<void*>(TestCalloc),
        reinterpret_cast<void*>(TestReallocate), reinterpret_cast<void*>(TestAlign), reinterpret_cast<void*>(TestRealign),
        reinterpret_cast<void*>(TestPosixAlign)};
    ApplicationHeapRegister_nid_no_patch(api.data());
#ifndef _WIN32
    CheckHostAllocationFault(false, false);
    CheckHostAllocationFault(false, true);
    CheckHostAllocationFault(true, false);
#endif
    for (Operation operation : {Operation::Binary, Operation::Hexadecimal, Operation::Inverse}) {
        for (bool throwing : {false, true}) {
            CheckFault(operation, false, 1, throwing, false);
            CheckFault(operation, true, 1, throwing, false);
            for (bool freeThrows : {false, true}) CheckFault(operation, false, 2, throwing, freeThrows);
        }
        for (bool provided : {false, true}) {
            ResetTracking();
            Bn* const original = provided ? BN_new_nid_postfix() : nullptr;
            Bn* output = original;
            Bn* const result = Convert(operation, output);
            Require(result == output && output);
            Require(!provided || output == original);
            Require(Hex(output) == (operation == Operation::Inverse ? "4" : "102030405060708"));
            BN_free_nid_postfix(output);
            Require(heapBlocks == std::array<HeapBlock, 16>{});
        }
    }
}

int main(int argc, char** argv) {
    if (argc == 2 && std::strcmp(argv[1], "--allocator") == 0) {
        CleanupFailures();
        return 0;
    }
    InitializeHeap();
    Bn* fresh = BN_new_nid_postfix();
    Require(fresh && fresh->top == 0 && fresh->neg == 0 && BN_num_bits_nid_postfix(fresh) == 0);
    BN_free_nid_postfix(fresh);
    BN_free_nid_postfix(nullptr);

    BnCtx* ctx = BN_CTX_new_nid_postfix();
    Require(ctx);
    BN_CTX_start_nid_postfix(ctx);
    BN_CTX_end_nid_postfix(ctx);
    BN_CTX_free_nid_postfix(ctx);
    BN_CTX_free_nid_postfix(nullptr);
    ctx = BN_CTX_new_nid_postfix();
    Require(ctx);

    {
        Bn* a = FromHex("FFFFFFFFFFFFFFFF");
        Bn* b = FromHex("FFFFFFFFFFFFFFFF");
        Bn* r = BN_new_nid_postfix();
        Require(BN_mul_nid_postfix(r, a, b, ctx) == 1);
        Require(Hex(r) == "FFFFFFFFFFFFFFFE0000000000000001");
        Require(r->top == 2 && r->neg == 0);
        BN_free_nid_postfix(a);
        BN_free_nid_postfix(b);
        BN_free_nid_postfix(r);
    }
    {
        Bn* a = FromHex("0123456789ABCDEFFEDCBA98765432100F1E2D3C4B5A6978");
        Bn* b = FromHex("00FF00FF00FF00FF00FF00FF00FF00FF00FF");
        Bn* r = BN_new_nid_postfix();
        Require(BN_mul_nid_postfix(r, a, b, ctx) == 1);
        Require(Hex(r) == "12223444566678876665444322210000F0F1CFC09E8F6D55CB34DD56FF79219B43BB52CA61D970E88");
        Require(BN_mul_nid_postfix(a, a, a, ctx) == 1);
        Require(Hex(a) == "14B66DC33F6ACDEE9D609EBE253A6424AD884E9A04269A9706601BB096443363E2CE44634913DF9C3B9FAA3D3A840");
        Bn* c = FromHex("-3");
        Bn* d = FromHex("5");
        Require(BN_mul_nid_postfix(d, c, d, ctx) == 1 && Hex(d) == "-F" && d->neg == 1);
        Require(BN_mul_nid_postfix(c, c, c, ctx) == 1 && Hex(c) == "9" && c->neg == 0);
        Bn* zero = FromHex("0");
        Require(BN_mul_nid_postfix(d, d, zero, ctx) == 1 && d->top == 0 && d->neg == 0);
        BN_free_nid_postfix(a);
        BN_free_nid_postfix(b);
        BN_free_nid_postfix(c);
        BN_free_nid_postfix(d);
        BN_free_nid_postfix(r);
        BN_free_nid_postfix(zero);
    }
    {
        const unsigned char bytes[] = {0x00, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09};
        Bn* value = BN_bin2bn_nid_postfix(bytes, sizeof(bytes), nullptr);
        Require(value && value->neg == 0 && value->top == 2);
        Require(Hex(value) == "10203040506070809");
        unsigned char out[16];
        std::memset(out, 0xEE, sizeof(out));
        Require(BN_bn2bin_nid_postfix(value, out) == 9);
        Require(std::memcmp(out, bytes + 2, 9) == 0 && out[9] == 0xEE);
        Require(BN_bin2bn_nid_postfix(bytes, 4, value) == value && Hex(value) == "102" && value->top == 1);
        const unsigned char zeros[] = {0, 0, 0};
        Require(BN_bin2bn_nid_postfix(zeros, 3, value) == value && value->top == 0);
        std::memset(out, 0xEE, sizeof(out));
        Require(BN_bn2bin_nid_postfix(value, out) == 0 && out[0] == 0xEE);
        Require(BN_bin2bn_nid_postfix(bytes, 0, value) == value && value->top == 0);
        Require(BN_bin2bn_nid_postfix(bytes, -1, value) == nullptr);
        BN_free_nid_postfix(value);
    }
    {
        Bn* value = BN_new_nid_postfix();
        Require(BN_num_bits_nid_postfix(value) == 0);
        Require(BN_set_word_nid_postfix(value, 1) == 1 && BN_num_bits_nid_postfix(value) == 1);
        Require(BN_set_word_nid_postfix(value, 0x100) == 1 && BN_num_bits_nid_postfix(value) == 9);
        Bn* big = FromHex("10000000000000000");
        Require(BN_num_bits_nid_postfix(big) == 65 && big->top == 2);
        Require(BN_set_word_nid_postfix(big, 0) == 1 && BN_num_bits_nid_postfix(big) == 0 && big->top == 0 && big->neg == 0);
        Require(BN_set_word_nid_postfix(value, 0xFFFFFFFFFFFFFFFFull) == 1 && BN_num_bits_nid_postfix(value) == 64);
        BN_free_nid_postfix(value);
        BN_free_nid_postfix(big);
    }
    {
        Require(BN_hex2bn_nid_postfix(nullptr, nullptr) == 0);
        Require(BN_hex2bn_nid_postfix(nullptr, "") == 0);
        Require(BN_hex2bn_nid_postfix(nullptr, "z") == 0);
        Require(BN_hex2bn_nid_postfix(nullptr, "-") == 1);
        Require(BN_hex2bn_nid_postfix(nullptr, "-z") == 1);
        Require(BN_hex2bn_nid_postfix(nullptr, "-0") == 2);
        Require(BN_hex2bn_nid_postfix(nullptr, "12G") == 2);
        Require(BN_hex2bn_nid_postfix(nullptr, "-12G") == 3);
        Require(BN_hex2bn_nid_postfix(nullptr, "1234xyz") == 4);
        Require(BN_hex2bn_nid_postfix(nullptr, "-1234xyz") == 5);

        Bn* fresh = nullptr;
        Require(BN_hex2bn_nid_postfix(&fresh, "") == 0 && fresh == nullptr);
        Require(BN_hex2bn_nid_postfix(&fresh, nullptr) == 0 && fresh == nullptr);
        Require(BN_hex2bn_nid_postfix(&fresh, "z") == 0 && fresh && fresh->top == 0 && fresh->neg == 0);
        BN_free_nid_postfix(fresh);
        fresh = nullptr;
        Require(BN_hex2bn_nid_postfix(&fresh, "-") == 1 && fresh && fresh->top == 0 && fresh->neg == 1);
        BN_free_nid_postfix(fresh);
        fresh = nullptr;
        Require(BN_hex2bn_nid_postfix(&fresh, "-z") == 1 && fresh && fresh->top == 0 && fresh->neg == 1);
        BN_free_nid_postfix(fresh);
        fresh = nullptr;

        Bn* value = nullptr;
        Require(BN_hex2bn_nid_postfix(&value, "-1F") == 3 && value);
        Require(value->neg == 1 && value->top == 1 && value->d[0] == 0x1F);
        Bn* same = value;
        Require(BN_hex2bn_nid_postfix(&value, "ABC") == 3 && value == same);
        Require(value->neg == 0 && value->d[0] == 0xABC);
        Require(BN_hex2bn_nid_postfix(&value, "FFzz") == 2 && value->d[0] == 0xFF && value->top == 1 && value->neg == 0);
        Require(BN_hex2bn_nid_postfix(&value, "") == 0 && value->d[0] == 0xFF && value->top == 1 && value->neg == 0);
        Require(BN_hex2bn_nid_postfix(&value, nullptr) == 0 && value->d[0] == 0xFF && value->top == 1 && value->neg == 0);
        Require(BN_hex2bn_nid_postfix(&value, "zz") == 0 && value->top == 0 && value->neg == 0);
        Require(BN_hex2bn_nid_postfix(&value, "ABC") == 3 && value->d[0] == 0xABC && value->top == 1 && value->neg == 0);
        Require(BN_hex2bn_nid_postfix(&value, "-") == 1 && value->top == 0 && value->neg == 1);
        Require(BN_hex2bn_nid_postfix(&value, "ABC") == 3 && value->d[0] == 0xABC && value->top == 1 && value->neg == 0);
        Require(BN_hex2bn_nid_postfix(&value, "-z") == 1 && value->top == 0 && value->neg == 1);
        Require(BN_hex2bn_nid_postfix(&value, "-0") == 2 && value->top == 0 && value->neg == 1);
        Require(BN_hex2bn_nid_postfix(&value, "1234xyz") == 4 && value->d[0] == 0x1234 && value->top == 1 && value->neg == 0);
        Require(BN_hex2bn_nid_postfix(&value, "-1234xyz") == 5 && value->d[0] == 0x1234 && value->top == 1 && value->neg == 1);
        Require(BN_hex2bn_nid_postfix(&value, "deadBEEF0123456789abcdef") == 24);
        Require(Hex(value) == "DEADBEEF0123456789ABCDEF" && value->top == 2);
        BN_free_nid_postfix(value);
    }
    {
        Bn* value = FromHex("FFFFFFFFFFFFFFFF");
        Require(BN_add_word_nid_postfix(value, 1) == 1);
        Require(value->top == 2 && value->d[0] == 0 && value->d[1] == 1 && Hex(value) == "10000000000000000");
        Require(BN_sub_word_nid_postfix(value, 1) == 1);
        Require(value->top == 1 && value->d[0] == 0xFFFFFFFFFFFFFFFFull && value->neg == 0);
        Require(BN_set_word_nid_postfix(value, 3) == 1);
        Require(BN_sub_word_nid_postfix(value, 5) == 1 && value->neg == 1 && value->top == 1 && value->d[0] == 2);
        Require(BN_add_word_nid_postfix(value, 2) == 1 && value->top == 0 && value->neg == 0);
        Require(BN_sub_word_nid_postfix(value, 7) == 1 && value->neg == 1 && Hex(value) == "-7");
        Require(BN_add_word_nid_postfix(value, 10) == 1 && value->neg == 0 && Hex(value) == "3");
        Require(BN_sub_word_nid_postfix(value, 0) == 1 && Hex(value) == "3");
        Bn* wide = FromHex("-10000000000000000");
        Require(BN_sub_word_nid_postfix(wide, 1) == 1 && wide->neg == 1 && Hex(wide) == "-10000000000000001");
        Require(BN_add_word_nid_postfix(wide, 2) == 1 && wide->neg == 1 && Hex(wide) == "-FFFFFFFFFFFFFFFF" && wide->top == 1);
        Bn* chain = FromHex("FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF");
        Require(BN_add_word_nid_postfix(chain, 1) == 1 && chain->top == 3 && Hex(chain) == "100000000000000000000000000000000");
        Require(BN_sub_word_nid_postfix(chain, 1) == 1 && chain->top == 2 && Hex(chain) == "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF");
        BN_free_nid_postfix(value);
        BN_free_nid_postfix(wide);
        BN_free_nid_postfix(chain);
    }
    {
        Bn* r = BN_new_nid_postfix();
        Require(ERR_get_error_nid_postfix() == 0);
        Bn* three = FromHex("3");
        Bn* eleven = FromHex("B");
        Require(BN_mod_inverse_nid_postfix(r, three, eleven, ctx) == r && Hex(r) == "4");
        Bn* seven = FromHex("7");
        Bn* forty = FromHex("28");
        Require(BN_mod_inverse_nid_postfix(r, seven, forty, ctx) == r && Hex(r) == "17");
        Require(ERR_get_error_nid_postfix() == 0);
        Bn* four = FromHex("4");
        Bn* eight = FromHex("8");
        Require(BN_mod_inverse_nid_postfix(r, four, eight, ctx) == nullptr);
        Require(Hex(r) == "17");
        constexpr std::uint64_t noInverse = ((3u & 0xffu) << 24) | ((0xfffu & 0xfffu) << 12) | (108u & 0xfffu);
        Require(ERR_get_error_nid_postfix() == noInverse);
        Require(ERR_get_error_nid_postfix() == 0);
        const char* operands[][2] = {{"2", "4"}, {"6", "9"}, {"0", "B"}, {"2", "-4"}, {"-6", "-9"}};
        for (const auto& pair : operands) {
            Bn* operand = FromHex(pair[0]);
            Bn* modulus = FromHex(pair[1]);
            for (bool supplied : {false, true}) {
                Require(BN_set_word_nid_postfix(r, 0x1234) == 1);
                Require(BN_mod_inverse_nid_postfix(supplied ? r : nullptr, operand, modulus, ctx) == nullptr);
                Require(Hex(r) == "1234");
                Require(ERR_get_error_nid_postfix() == noInverse);
                Require(ERR_get_error_nid_postfix() == 0);
            }
            BN_free_nid_postfix(operand);
            BN_free_nid_postfix(modulus);
        }
        Bn* allocated = BN_mod_inverse_nid_postfix(nullptr, three, eleven, ctx);
        Require(allocated && Hex(allocated) == "4");
        Bn* minusThree = FromHex("-3");
        Require(BN_mod_inverse_nid_postfix(r, minusThree, eleven, ctx) == r && Hex(r) == "7");
        Bn* fifteen = FromHex("F");
        Require(BN_mod_inverse_nid_postfix(r, fifteen, eleven, ctx) == r && Hex(r) == "3");
        Bn* inPlace = FromHex("3");
        Require(BN_mod_inverse_nid_postfix(inPlace, inPlace, eleven, ctx) == inPlace && Hex(inPlace) == "4");

        Bn* prime = FromHex("7FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF");
        Bn* a = FromHex("123456789ABCDEF0FEDCBA9876543210");
        Bn* inverse = BN_mod_inverse_nid_postfix(nullptr, a, prime, ctx);
        Require(inverse && Hex(inverse) == "62CFEFE6EB41053969967BD711C5169F");
        Require(ERR_get_error_nid_postfix() == 0);
        Bn* product = BN_new_nid_postfix();
        Require(BN_mul_nid_postfix(product, a, inverse, ctx) == 1);
        Bn* quotient = FromHex("E0DA4F6BE78BFD74B8C9F95AF178811");
        Bn* check = BN_new_nid_postfix();
        Require(BN_mul_nid_postfix(check, quotient, prime, ctx) == 1);
        Require(BN_add_word_nid_postfix(check, 1) == 1);
        Require(Hex(check) == Hex(product));

        Bn* all[] = {r, three, eleven, seven, forty, four, eight, allocated, minusThree, fifteen, inPlace, prime, a, inverse, product, quotient, check};
        for (Bn* value : all) BN_free_nid_postfix(value);
    }
    BN_CTX_free_nid_postfix(ctx);
    Print();
    return 0;
}
