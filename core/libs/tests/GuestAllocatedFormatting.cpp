#include "SceTypes.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include <array>
#include <cstdarg>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>

extern "C" {
int APS5_VABI vasprintf_nid_postfix(char**, const char*, VaList*);
int* APS5_VABI __error_nid_postfix();
void APS5_VABI free_nid_postfix(void*);
}

namespace {
bool failAllocation = false;
int allocations = 0;
int frees = 0;

void Require(bool condition) { if (!condition) std::abort(); }
void* APS5_VABI Allocate(std::size_t size) {
    if (failAllocation) return nullptr;
    ++allocations;
    return std::malloc(size);
}
void APS5_VABI Free(void* pointer) { ++frees; std::free(pointer); }
void* APS5_VABI Reallocate(void* pointer, std::size_t size) { return std::realloc(pointer, size); }
void* APS5_VABI Calloc(std::size_t count, std::size_t size) { return std::calloc(count, size); }
void* APS5_VABI Align(std::size_t, std::size_t) { return nullptr; }
int APS5_VABI PosixAlign(void**, std::size_t, std::size_t) { return 22; }

int APS5_VABI FormatList(char** output, const char* format, ...) {
    __builtin_sysv_va_list args;
    __builtin_sysv_va_start(args, format);
    VaList guestArgs;
    std::memcpy(&guestArgs, args, sizeof(guestArgs));
    const VaList original = guestArgs;
    const int result = vasprintf_nid_postfix(output, format, &guestArgs);
    Require(std::memcmp(&guestArgs, &original, sizeof(guestArgs)) == 0);
    __builtin_sysv_va_end(args);
    return result;
}
}

int main() {
    std::array<void*, 10> api{};
    api[0] = reinterpret_cast<void*>(&Allocate);
    api[1] = reinterpret_cast<void*>(&Free);
    api[2] = reinterpret_cast<void*>(&Calloc);
    api[3] = reinterpret_cast<void*>(&Reallocate);
    api[4] = reinterpret_cast<void*>(&Align);
    api[5] = reinterpret_cast<void*>(&Align);
    api[6] = reinterpret_cast<void*>(&PosixAlign);
    ApplicationHeapRegister_nid_no_patch(api.data());

    char* output = nullptr;
    Require(FormatList(&output, "Doom %d %.1f %s", 42, 1.5, "PS5") == 15);
    Require(output != nullptr && std::strcmp(output, "Doom 42 1.5 PS5") == 0);
    free_nid_postfix(output);
    Require(FormatList(&output, "%d %d %d %d %d %d %d %d", 1, 2, 3, 4, 5, 6, 7, 8) == 15);
    Require(std::strcmp(output, "1 2 3 4 5 6 7 8") == 0);
    free_nid_postfix(output);
    Require(FormatList(&output, "") == 0 && output != nullptr && *output == '\0');
    free_nid_postfix(output);

    *__error_nid_postfix() = 0;
    output = reinterpret_cast<char*>(0x1);
    failAllocation = true;
    Require(FormatList(&output, "test") == -1 && output == nullptr);
    Require(*__error_nid_postfix() == 12);
    failAllocation = false;
    Require(FormatList(&output, nullptr) == -1 && output == nullptr);
    Require(*__error_nid_postfix() == 22);
    Require(FormatList(nullptr, "test") == -1);
    Require(*__error_nid_postfix() == 22);
    Require(allocations == frees);
}
