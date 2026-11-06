#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>

static void Check(bool condition, int line) {
    if (condition) return;
    std::fprintf(stderr, "std crt heap check failed at line %d\n", line);
    std::abort();
}
#define Require(condition) Check((condition), __LINE__)

static void Zeroed() {
    for (const std::size_t count : {std::size_t{1}, std::size_t{3}, std::size_t{64}, std::size_t{4096}}) {
        for (const std::size_t size : {std::size_t{1}, std::size_t{2}, std::size_t{7}, std::size_t{64}}) {
            auto* block = static_cast<unsigned char*>(__std_calloc_crt_nid_postfix(count, size));
            Require(block != nullptr);
            for (std::size_t index = 0; index < count * size; ++index) Require(block[index] == 0);
            std::memset(block, 0xA5, count * size);
            __std_free_crt_nid_postfix(block);
        }
    }
}

static void Recycled() {
    for (int round = 0; round < 64; ++round) {
        auto* block = static_cast<unsigned char*>(__std_calloc_crt_nid_postfix(16, 16));
        Require(block != nullptr);
        for (std::size_t index = 0; index < 256; ++index) Require(block[index] == 0);
        std::memset(block, 0xFF, 256);
        __std_free_crt_nid_postfix(block);
    }
}

static void Rejected() {
    Require(__std_calloc_crt_nid_postfix(std::numeric_limits<std::size_t>::max(), 2) == nullptr);
    Require(__std_calloc_crt_nid_postfix(2, std::numeric_limits<std::size_t>::max()) == nullptr);
}

static void NullReleased() {
    __std_free_crt_nid_postfix(nullptr);
}

int main() {
    void* api[10]{};
    ApplicationHeapRegister_nid_no_patch(api);
    Zeroed();
    Recycled();
    Rejected();
    NullReleased();
    std::puts("__std_calloc_crt zeroes its block and __std_free_crt releases it");
}
