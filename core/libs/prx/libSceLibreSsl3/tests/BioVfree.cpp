#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include "prx/libSceLibreSsl/include/Api.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <source_location>

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "LibreSSL3 BIO_vfree check failed at line %u\n", location.line());
    std::abort();
}

void CheckReleasesAReference() {
    auto* bio = BIO_new_mem_buf_nid_postfix("abc", 3);
    Require(bio != nullptr);
    Require(bio->references == 1);
    bio->references = 2;

    BIO_vfree_nid_postfix(bio);
    Require(bio->references == 1);

    BIO_vfree_nid_postfix(bio);
}

void CheckAcceptsNull() {
    BIO_vfree_nid_postfix(nullptr);
}

}

namespace {

void* APS5_VABI HeapAllocate(std::size_t bytes) { return std::malloc(bytes); }
void APS5_VABI HeapFree(void* pointer) { std::free(pointer); }
void* APS5_VABI HeapCallocate(std::size_t count, std::size_t bytes) { return std::calloc(count, bytes); }
void* APS5_VABI HeapReallocate(void* pointer, std::size_t bytes) { return std::realloc(pointer, bytes); }
void* APS5_VABI UnexpectedAlign(std::size_t, std::size_t) { std::abort(); }
void* APS5_VABI UnexpectedRealign(void*, std::size_t, std::size_t) { std::abort(); }
int APS5_VABI UnexpectedPosixAlign(void**, std::size_t, std::size_t) { std::abort(); }

std::array<void*, 10> HeapApi() {
    return {reinterpret_cast<void*>(HeapAllocate), reinterpret_cast<void*>(HeapFree),
        reinterpret_cast<void*>(HeapCallocate), reinterpret_cast<void*>(HeapReallocate),
        reinterpret_cast<void*>(UnexpectedAlign), reinterpret_cast<void*>(UnexpectedRealign),
        reinterpret_cast<void*>(UnexpectedPosixAlign)};
}

}

int main() {
    std::array<void*, 10> heapApi = HeapApi();
    ApplicationHeapRegister_nid_no_patch(heapApi.data());
    CheckAcceptsNull();
    CheckReleasesAReference();
    std::printf("libSceLibreSsl3 BIO_vfree tests passed\n");
    return 0;
}
