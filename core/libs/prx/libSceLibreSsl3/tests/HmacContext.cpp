#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include "prx/libSceLibreSsl/include/Api.hpp"
#include "prx/libc/libc.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <source_location>
#include <stdexcept>
#include <unordered_map>

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "LibreSSL3 HMAC_CTX check failed at line %u\n", location.line());
    std::abort();
}

std::unordered_map<void*, std::size_t> allocations;
std::size_t callocCalls = 0;
std::size_t freeCalls = 0;
std::size_t callocSize = 0;
bool failAllocate = false;

void* APS5_VABI Allocate_nid_no_patch(std::size_t size) {
    void* result = std::malloc(size ? size : 1);
    Require(result != nullptr);
    allocations.emplace(result, size);
    return result;
}

void* APS5_VABI Calloc_nid_no_patch(std::size_t count, std::size_t size) {
    ++callocCalls;
    callocSize = count * size;
    if (failAllocate) return nullptr;
    void* result = std::calloc(count ? count : 1, size ? size : 1);
    Require(result != nullptr);
    allocations.emplace(result, count * size);
    return result;
}

void APS5_VABI Free_nid_no_patch(void* pointer) {
    if (!pointer) return;
    Require(allocations.erase(pointer) == 1);
    ++freeCalls;
    std::free(pointer);
}

void* APS5_VABI UnexpectedReallocate_nid_no_patch(void*, std::size_t) { throw std::runtime_error("unexpected realloc"); }
void* APS5_VABI UnexpectedAlign_nid_no_patch(std::size_t, std::size_t) { throw std::runtime_error("unexpected aligned allocation"); }
void* APS5_VABI UnexpectedRealign_nid_no_patch(void*, std::size_t, std::size_t) { throw std::runtime_error("unexpected aligned reallocation"); }
int APS5_VABI UnexpectedPosixAlign_nid_no_patch(void**, std::size_t, std::size_t) { throw std::runtime_error("unexpected posix_memalign"); }

constexpr std::array<unsigned char, 32> Rfc4231Case1Sha256 = {
    0xb0, 0x34, 0x4c, 0x61, 0xd8, 0xdb, 0x38, 0x53, 0x5c, 0xa8, 0xaf, 0xce, 0xaf, 0x0b, 0xf1, 0x2b,
    0x88, 0x1d, 0xc2, 0x00, 0xc9, 0x83, 0x3d, 0xa7, 0x26, 0xe9, 0x37, 0x6c, 0x2e, 0x32, 0xcf, 0xf7};

constexpr std::array<unsigned char, 32> Rfc4231Case6Sha256 = {
    0x60, 0xe4, 0x31, 0x59, 0x1e, 0xe0, 0xb6, 0x7f, 0x0d, 0x8a, 0x26, 0xaa, 0xcb, 0xf5, 0xb7, 0x7f,
    0x8e, 0x0b, 0xc6, 0x21, 0x37, 0x28, 0xc5, 0x14, 0x05, 0x46, 0x04, 0x0f, 0x0e, 0xe3, 0x7f, 0x54};

template <std::size_t Size>
std::array<unsigned char, Size> FilledKey(unsigned char value) {
    std::array<unsigned char, Size> key;
    key.fill(value);
    return key;
}

constexpr unsigned char Message[] = {'H', 'i', ' ', 'T', 'h', 'e', 'r', 'e'};
constexpr unsigned char Case6Message[] = "Test Using Larger Than Block-Size Key - Hash Key First";

void VerifyFinal(HmacContext* context, const std::array<unsigned char, 32>& expected) {
    std::array<unsigned char, 64> digest {};
    std::uint32_t length = 0;
    Require(HMAC_Final_nid_postfix(context, digest.data(), &length) == 1);
    Require(length == expected.size() && std::memcmp(digest.data(), expected.data(), length) == 0);
}

void CreateAndFree() {
    callocCalls = 0;
    void* const context = HMAC_CTX_new_nid_postfix();
    Require(context != nullptr && callocCalls == 1);
    Require(allocations.size() == 1 && allocations.at(context) == sizeof(HmacContext) && callocSize == sizeof(HmacContext));
    void* const second = HMAC_CTX_new_nid_postfix();
    Require(second != nullptr && second != context && allocations.size() == 2);
    const std::size_t freesBefore = freeCalls;
    HMAC_CTX_free_nid_postfix(context);
    Require(freeCalls == freesBefore + 1 && allocations.size() == 1 && allocations.contains(second));
    HMAC_CTX_free_nid_postfix(second);
    Require(freeCalls == freesBefore + 2 && allocations.empty());
}

void FreeNull() {
    const std::size_t freesBefore = freeCalls;
    HMAC_CTX_free_nid_postfix(nullptr);
    Require(freeCalls == freesBefore);
}

void AllocationFailure() {
    failAllocate = true;
    Require(HMAC_CTX_new_nid_postfix() == nullptr && allocations.empty());
    failAllocate = false;
}

void RoundTrip() {
    auto* const context = static_cast<HmacContext*>(HMAC_CTX_new_nid_postfix());
    Require(context != nullptr);
    const auto key = FilledKey<20>(0x0b);
    Require(HMAC_Init_ex_nid_postfix(context, key.data(), static_cast<int>(key.size()), EVP_sha256_nid_postfix(), nullptr) == 1);
    Require(HMAC_Update_nid_postfix(context, Message, 3) == 1);
    Require(HMAC_Update_nid_postfix(context, Message + 3, sizeof(Message) - 3) == 1);
    VerifyFinal(context, Rfc4231Case1Sha256);
    Require(HMAC_Init_ex_nid_postfix(context, nullptr, 0, nullptr, nullptr) == 1);
    Require(HMAC_Update_nid_postfix(context, Message, sizeof(Message)) == 1);
    VerifyFinal(context, Rfc4231Case1Sha256);
    HMAC_CTX_free_nid_postfix(context);
    Require(allocations.empty());
}

void RekeyFromLongToShortKey() {
    auto* const context = static_cast<HmacContext*>(HMAC_CTX_new_nid_postfix());
    Require(context != nullptr);
    const auto longKey = FilledKey<131>(0xaa);
    Require(HMAC_Init_ex_nid_postfix(context, longKey.data(), static_cast<int>(longKey.size()), EVP_sha256_nid_postfix(), nullptr) == 1);
    Require(HMAC_Update_nid_postfix(context, Case6Message, sizeof(Case6Message) - 1) == 1);
    VerifyFinal(context, Rfc4231Case6Sha256);
    Require(HMAC_Init_ex_nid_postfix(context, nullptr, 0, nullptr, nullptr) == 1);
    Require(HMAC_Update_nid_postfix(context, Case6Message, sizeof(Case6Message) - 1) == 1);
    VerifyFinal(context, Rfc4231Case6Sha256);
    const auto shortKey = FilledKey<20>(0x0b);
    Require(HMAC_Init_ex_nid_postfix(context, shortKey.data(), static_cast<int>(shortKey.size()), EVP_sha256_nid_postfix(), nullptr) == 1);
    Require(HMAC_Update_nid_postfix(context, Message, 3) == 1);
    Require(HMAC_Update_nid_postfix(context, Message + 3, sizeof(Message) - 3) == 1);
    VerifyFinal(context, Rfc4231Case1Sha256);
    HMAC_CTX_free_nid_postfix(context);
    Require(allocations.empty());
}

void FreeKeyedContext() {
    auto* const context = static_cast<HmacContext*>(HMAC_CTX_new_nid_postfix());
    Require(context != nullptr);
    const auto key = FilledKey<20>(0x0b);
    Require(HMAC_Init_ex_nid_postfix(context, key.data(), static_cast<int>(key.size()), EVP_sha256_nid_postfix(), nullptr) == 1);
    Require(HMAC_Update_nid_postfix(context, Message, sizeof(Message)) == 1);
    Require(allocations.size() > 1);
    HMAC_CTX_free_nid_postfix(context);
    Require(allocations.empty());
}

}

int main() {
    std::array<void*, 10> api{
        reinterpret_cast<void*>(Allocate_nid_no_patch), reinterpret_cast<void*>(Free_nid_no_patch), reinterpret_cast<void*>(Calloc_nid_no_patch),
        reinterpret_cast<void*>(UnexpectedReallocate_nid_no_patch), reinterpret_cast<void*>(UnexpectedAlign_nid_no_patch), reinterpret_cast<void*>(UnexpectedRealign_nid_no_patch),
        reinterpret_cast<void*>(UnexpectedPosixAlign_nid_no_patch)};
    ApplicationHeapRegister_nid_no_patch(api.data());
    CreateAndFree();
    FreeNull();
    AllocationFailure();
    RoundTrip();
    RekeyFromLongToShortKey();
    FreeKeyedContext();
    std::puts("libSceLibreSsl3 HMAC_CTX tests passed");
}
