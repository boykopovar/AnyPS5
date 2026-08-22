#include <cstddef>
#include <cstdio>
#include <cstdlib>

extern "C" {

FILE* fopen_nid_postfix(const char* filename, const char* mode) {
    return std::fopen(filename, mode);
}

int fclose_nid_postfix(FILE* stream) {
    return std::fclose(stream);
}

size_t fread_nid_postfix(void* buffer, size_t size, size_t count, FILE* stream) {
    return std::fread(buffer, size, count, stream);
}

size_t fwrite_nid_postfix(const void* buffer, size_t size, size_t count, FILE* stream) {
    return std::fwrite(buffer, size, count, stream);
}

int fseek_nid_postfix(FILE* stream, long offset, int origin) {
    return std::fseek(stream, offset, origin);
}

long ftell_nid_postfix(FILE* stream) {
    return std::ftell(stream);
}

int fputs_nid_postfix(const char* str, FILE* stream) {
    return std::fputs(str, stream);
}

int fflush_nid_postfix(FILE* stream) {
    return std::fflush(stream);
}

void* malloc_nid_postfix(size_t size) {
    return std::malloc(size);
}

void free_nid_postfix(void* ptr) {
    std::free(ptr);
}

void* realloc_nid_postfix(void* ptr, size_t newSize) {
    return std::realloc(ptr, newSize);
}

void* memalign_nid_postfix(size_t alignment, size_t size) {
    return std::aligned_alloc(alignment, size);
}

void qsort_nid_postfix(void* base, size_t count, size_t size, int (*compare)(const void*, const void*)) {
    std::qsort(base, count, size, compare);
}

}
