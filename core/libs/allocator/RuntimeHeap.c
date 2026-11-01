#include <windows.h>

#include <stdint.h>
#include <string.h>

void* __wrap_malloc(size_t bytes) {
    return HeapAlloc(GetProcessHeap(), 0, bytes != 0 ? bytes : 1);
}

void* __wrap_calloc(size_t count, size_t size) {
    if (size != 0 && count > SIZE_MAX / size) return NULL;
    const size_t bytes = count * size;
    return HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, bytes != 0 ? bytes : 1);
}

void __wrap_free(void* block) {
    if (block != NULL) HeapFree(GetProcessHeap(), 0, block);
}

void* __wrap_realloc(void* block, size_t bytes) {
    if (block == NULL) return __wrap_malloc(bytes);
    if (bytes == 0) {
        __wrap_free(block);
        return NULL;
    }
    return HeapReAlloc(GetProcessHeap(), 0, block, bytes);
}

char* __wrap__strdup(const char* text) {
    const size_t bytes = strlen(text) + 1;
    char* copy = __wrap_malloc(bytes);
    if (copy != NULL) memcpy(copy, text, bytes);
    return copy;
}

char* __wrap_strdup(const char* text) {
    return __wrap__strdup(text);
}
