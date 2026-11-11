#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <new>

namespace {

int failures = 0;

void report(const char* what, const char* detail) {
    std::fprintf(stderr, "%s: %s\n", what, detail);
    ++failures;
}

bool allocates(std::size_t bytes) {
    void* block = nullptr;
    try {
        block = malloc_nid_postfix(bytes);
    } catch (const std::exception& error) {
        std::fprintf(stderr, "malloc(%zu) threw %s\n", bytes, error.what());
        ++failures;
        return false;
    } catch (...) {
        report("malloc", "threw a non-standard exception");
        return false;
    }
    if (block == nullptr) {
        report("malloc", "returned NULL");
        return false;
    }
    free_nid_postfix(block);
    return true;
}

bool allocationThrows(std::size_t bytes) {
    try {
        void* block = malloc_nid_postfix(bytes);
        if (block != nullptr) free_nid_postfix(block);
    } catch (const std::exception&) {
        return true;
    } catch (...) {
        return true;
    }
    report("malloc after malloc_finalize", "returned a block");
    return false;
}

}

int main() {
    std::array<void*, 10> none{};
    ApplicationHeapRegister_nid_no_patch(none.data());
    malloc_initialize_nid_postfix();
    malloc_initialize_nid_postfix();
    if (!ApplicationHeapDefault_nid_no_patch()) report("malloc_initialize", "the default allocator is not active");
    if (!allocates(64)) return failures == 0 ? 0 : 1;
    if (!allocates(0x10000)) return failures == 0 ? 0 : 1;

    malloc_finalize_nid_postfix();
    try {
        malloc_finalize_nid_postfix();
    } catch (const std::exception& error) {
        std::fprintf(stderr, "the second malloc_finalize threw %s\n", error.what());
        ++failures;
    }
    if (!allocationThrows(64)) return failures == 0 ? 0 : 1;

    int result = -1;
    try {
        result = _malloc_fini_nid_postfix();
    } catch (const std::exception& error) {
        std::fprintf(stderr, "_malloc_fini threw %s\n", error.what());
        ++failures;
    }
    if (result != 0) {
        std::fprintf(stderr, "_malloc_fini returned %d, expected 0\n", result);
        ++failures;
    }
    return failures == 0 ? 0 : 1;
}
