#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include "prx/libSceLibreSsl/libSceLibreSsl.hpp"
#include "prx/libc/libc.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include <array>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <source_location>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "LibreSSL3 BIGNUM check failed at line %u\n", location.line());
    std::abort();
}

std::unordered_map<void*, std::size_t> allocations;

void* APS5_VABI Allocate_nid_no_patch(std::size_t size) {
    void* result = std::malloc(size);
    Require(result != nullptr);
    allocations.emplace(result, size);
    return result;
}

void APS5_VABI Free_nid_no_patch(void* pointer) {
    if (!pointer) return;
    Require(allocations.erase(pointer) == 1);
    std::free(pointer);
}

void* APS5_VABI Calloc_nid_no_patch(std::size_t count, std::size_t size) {
    void* result = std::calloc(count, size);
    Require(result != nullptr);
    allocations.emplace(result, count * size);
    return result;
}

void* APS5_VABI Reallocate_nid_no_patch(void* pointer, std::size_t size) {
    if (pointer) Require(allocations.erase(pointer) == 1);
    void* result = std::realloc(pointer, size);
    Require(result != nullptr);
    allocations.emplace(result, size);
    return result;
}

void* APS5_VABI UnexpectedAlign_nid_no_patch(std::size_t, std::size_t) { throw std::runtime_error("unexpected aligned allocation"); }
void* APS5_VABI UnexpectedRealign_nid_no_patch(void*, std::size_t, std::size_t) { throw std::runtime_error("unexpected aligned reallocation"); }
int APS5_VABI UnexpectedPosixAlign_nid_no_patch(void**, std::size_t, std::size_t) { throw std::runtime_error("unexpected posix_memalign"); }

LibreSsl3Bignum* Parse(const char* hex) {
    Bignum* value = nullptr;
    Require(BN_hex2bn_nid_postfix(&value, hex) > 0 && value != nullptr);
    return reinterpret_cast<LibreSsl3Bignum*>(value);
}

std::string Hex(const LibreSsl3Bignum* value) {
    const auto* number = reinterpret_cast<const Bignum*>(value);
    std::vector<unsigned char> bytes((BN_num_bits_nid_postfix(number) + 7) / 8);
    BN_bn2bin_nid_postfix(number, bytes.data());
    static constexpr char HexDigits[] = "0123456789ABCDEF";
    std::string text = value->negative ? "-" : "";
    bool started = false;
    for (const unsigned char byte : bytes) {
        for (const unsigned nibble : {byte >> 4, byte & 0x0f}) {
            if (!started && nibble == 0) continue;
            started = true;
            text += HexDigits[nibble];
        }
    }
    return started ? text : text + "0";
}

void Free(LibreSsl3Bignum* value) {
    BN_free_nid_postfix(reinterpret_cast<Bignum*>(value));
}

void RequireDivision(const char* dividend, const char* divisor, const char* quotient, const char* remainder) {
    auto* numerator = Parse(dividend);
    auto* denominator = Parse(divisor);
    auto* quotientValue = Parse("0");
    auto* remainderValue = Parse("0");
    Require(BN_div_nid_postfix(quotientValue, remainderValue, numerator, denominator, nullptr) == 1);
    Require(Hex(quotientValue) == quotient);
    Require(Hex(remainderValue) == remainder);
    Free(numerator);
    Free(denominator);
    Free(quotientValue);
    Free(remainderValue);
}

void TestDivision() {
    RequireDivision("64", "7", "E", "2");
    RequireDivision("-64", "7", "-E", "-2");
    RequireDivision("64", "-7", "-E", "2");
    RequireDivision("-64", "-7", "E", "-2");
    RequireDivision("5", "7", "0", "5");
    RequireDivision("FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF", "10000000000000000", "FFFFFFFFFFFFFFFF", "FFFFFFFFFFFFFFFF");

    auto* numerator = Parse("64");
    auto* denominator = Parse("7");
    auto* quotientValue = Parse("0");
    Require(BN_div_nid_postfix(quotientValue, nullptr, numerator, denominator, nullptr) == 1 && Hex(quotientValue) == "E");
    auto* remainderValue = Parse("0");
    Require(BN_div_nid_postfix(nullptr, remainderValue, numerator, denominator, nullptr) == 1 && Hex(remainderValue) == "2");
    auto* zero = Parse("0");
    Require(BN_div_nid_postfix(quotientValue, remainderValue, numerator, zero, nullptr) == 0);
    Free(numerator);
    Free(denominator);
    Free(quotientValue);
    Free(remainderValue);
    Free(zero);
}

void TestDuplicate() {
    Require(BN_dup_nid_postfix(nullptr) == nullptr);

    auto* original = Parse("-1F");
    auto* copy = BN_dup_nid_postfix(original);
    Require(copy != nullptr && copy != original && Hex(copy) == "-1F");
    Require(BN_set_word_nid_postfix(reinterpret_cast<Bignum*>(original), 5) == 1);
    Require(Hex(original) == "5" && Hex(copy) == "-1F");
    Free(copy);
    Free(original);

    auto* zero = Parse("0");
    auto* zeroCopy = BN_dup_nid_postfix(zero);
    Require(zeroCopy != nullptr && Hex(zeroCopy) == "0" && zeroCopy->negative == 0);
    Free(zeroCopy);
    Free(zero);

    auto* large = Parse("FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF0000000000000001");
    auto* largeCopy = BN_dup_nid_postfix(large);
    Require(largeCopy != nullptr && Hex(largeCopy) == "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF0000000000000001");
    Free(largeCopy);
    Free(large);
}

}  // namespace

int main() {
    std::array<void*, 7> api{
        reinterpret_cast<void*>(Allocate_nid_no_patch), reinterpret_cast<void*>(Free_nid_no_patch), reinterpret_cast<void*>(Calloc_nid_no_patch),
        reinterpret_cast<void*>(Reallocate_nid_no_patch), reinterpret_cast<void*>(UnexpectedAlign_nid_no_patch), reinterpret_cast<void*>(UnexpectedRealign_nid_no_patch),
        reinterpret_cast<void*>(UnexpectedPosixAlign_nid_no_patch)};
    ApplicationHeapRegister_nid_no_patch(api.data());
    TestDivision();
    TestDuplicate();
    Require(allocations.empty());
    std::puts("libSceLibreSsl3 BIGNUM tests passed");
}
