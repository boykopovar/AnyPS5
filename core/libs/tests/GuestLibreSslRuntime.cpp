#include "prx/libc/include/Api.hpp"
#include "prx/libSceLibreSsl/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <source_location>
#include <stdexcept>

static void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "LibreSSL runtime check failed at line %u\n", location.line());
    std::abort();
}

template<typename TAction> static void RequireThrows(TAction action, std::source_location location = std::source_location::current()) {
    try { action(); } catch (const std::exception&) { return; }
    Require(false, location);
}

static void InitializeHeap() {
    void* const api[10] {};
    ApplicationHeapRegister_nid_no_patch(api);
}

int main() {
    InitializeHeap();
    std::array<unsigned char, 4099> first {};
    std::array<unsigned char, 4099> second {};
    first.fill(0xa5);
    Require(RAND_bytes_nid_postfix(first.data(), 0) == 1 && RAND_bytes_nid_postfix(first.data(), -5) == 1);
    Require(RAND_bytes_nid_postfix(nullptr, 0) == 1);
    Require(std::all_of(first.begin(), first.end(), [](unsigned char value) { return value == 0xa5; }));
    Require(RAND_bytes_nid_postfix(first.data(), 3) == 1);
    Require(std::all_of(first.begin() + 3, first.end(), [](unsigned char value) { return value == 0xa5; }));
    Require(RAND_bytes_nid_postfix(first.data(), static_cast<int>(first.size())) == 1);
    Require(RAND_bytes_nid_postfix(second.data(), static_cast<int>(second.size())) == 1);
    Require(first != second);
    std::array<std::size_t, 256> counts {};
    for (const auto value : first) ++counts[value];
    Require(*std::max_element(counts.begin(), counts.end()) < 64);
    RequireThrows([] { RAND_bytes_nid_postfix(nullptr, 1); });

    RAND_add_nid_postfix(first.data(), 16, 8.0);
    RAND_add_nid_postfix(nullptr, 0, 0.0);
    Require(RAND_status_nid_postfix() == 1);
    Require(RAND_load_file_nid_postfix("/nonexistent", -1) == 123456);
    Require(RAND_load_file_nid_postfix(nullptr, 1024) == 1024 && RAND_load_file_nid_postfix(nullptr, 0) == 0);
    Require(RAND_load_file_nid_postfix(nullptr, 0x100000005) == 5);

    char name[32];
    std::memset(name, 'x', sizeof(name));
    Require(RAND_file_name_nid_postfix(name, sizeof(name)) == name && !std::strcmp(name, "/dev/urandom"));
    std::memset(name, 'x', sizeof(name));
    Require(RAND_file_name_nid_postfix(name, 13) == name && !std::strcmp(name, "/dev/urandom"));
    std::memset(name, 'x', sizeof(name));
    Require(RAND_file_name_nid_postfix(name, 12) == nullptr && !std::strcmp(name, "/dev/urando"));
    std::memset(name, 'x', sizeof(name));
    Require(RAND_file_name_nid_postfix(name, 0) == nullptr && name[0] == 'x');
    Require(RAND_file_name_nid_postfix(nullptr, 0) == nullptr);
    RequireThrows([] { RAND_file_name_nid_postfix(nullptr, 1); });

    auto* block = static_cast<unsigned char*>(CRYPTO_malloc_nid_postfix(64, "file.c", 10));
    Require(block != nullptr);
    std::memset(block, 0x5a, 64);
    CRYPTO_free_nid_postfix(block);

    block = static_cast<unsigned char*>(CRYPTO_malloc_nid_postfix(32, nullptr, 0));
    Require(block != nullptr);
    free_nid_postfix(block);

    block = static_cast<unsigned char*>(malloc_nid_postfix(16));
    CRYPTO_free_nid_postfix(block);
    CRYPTO_free_nid_postfix(nullptr);

    Require(CRYPTO_malloc_nid_postfix(0, nullptr, 0) == nullptr);
    Require(CRYPTO_malloc_nid_postfix(-1, nullptr, 0) == nullptr);
    Require(CRYPTO_set_mem_functions_nid_postfix(reinterpret_cast<void*>(&std::malloc), reinterpret_cast<void*>(&std::realloc), reinterpret_cast<void*>(&std::free)) == 0);
    Require(CRYPTO_set_mem_functions_nid_postfix(nullptr, nullptr, nullptr) == 0);
    std::puts("libSceLibreSsl runtime tests passed");
}
