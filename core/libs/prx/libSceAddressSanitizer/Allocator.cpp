#include "prx/libSceAddressSanitizer/include/Api.hpp"
#include "prx/libc/include/Api.hpp"

#include <cstddef>

extern "C" {

void* APS5_VABI __asan_malloc(std::size_t size) {
    return malloc_nid_postfix(size);
}

void* APS5_VABI __asan_calloc(std::size_t count, std::size_t size) {
    return calloc_nid_postfix(count, size);
}

void* APS5_VABI __asan_realloc(void* pointer, std::size_t size) {
    return realloc_nid_postfix(pointer, size);
}

void* APS5_VABI __asan_reallocalign(void* pointer, std::size_t size, std::size_t alignment) {
    return reallocalign_nid_postfix(pointer, size, alignment);
}

void APS5_VABI __asan_free(void* pointer) {
    free_nid_postfix(pointer);
}

void* APS5_VABI __asan_memalign(std::size_t alignment, std::size_t size) {
    return memalign_nid_postfix(alignment, size);
}

void* APS5_VABI __asan_aligned_alloc(std::size_t alignment, std::size_t size) {
    return aligned_alloc_nid_postfix(alignment, size);
}

int APS5_VABI __asan_posix_memalign(void** pointer, std::size_t alignment, std::size_t size) {
    return posix_memalign_nid_postfix(pointer, alignment, size);
}

std::size_t APS5_VABI __asan_malloc_usable_size(void* pointer) {
    return pointer != nullptr ? malloc_usable_size_nid_postfix(pointer) : 0;
}

void APS5_VABI __asan_malloc_stats() {
}

void APS5_VABI __asan_print_accumulated_stats() {
}

std::size_t APS5_VABI __asan_load_cxx_array_cookie(void* pointer) {
    return pointer != nullptr ? *static_cast<const std::size_t*>(pointer) : 0;
}

void* APS5_VABI __asan_poison_cxx_array_cookie(void* pointer) {
    return pointer;
}

}
