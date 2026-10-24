#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "SceTypes.hpp"
#include <array>
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>

extern "C" {
char* APS5_VABI strndup_nid_postfix(const char*, std::size_t);
char* APS5_VABI strdup_nid_postfix(const char*);
int APS5_VABI asprintf_nid_postfix(char**, const char*, ...);
int APS5_VABI vasprintf_nid_postfix(char**, const char*, VaList*);
void APS5_VABI free_nid_postfix(void*);
int* APS5_VABI __error_nid_postfix();
}

namespace {

void Require(bool value) {
    if (!value) std::abort();
}

bool failAllocation = false;

void* APS5_VABI Allocate(std::size_t size) { return failAllocation ? nullptr : std::malloc(size); }
void APS5_VABI Free(void* ptr) { std::free(ptr); }
void* APS5_VABI Reallocate(void* ptr, std::size_t size) { return std::realloc(ptr, size); }
void* APS5_VABI Calloc(std::size_t count, std::size_t size) { return std::calloc(count, size); }
void* APS5_VABI Align(std::size_t, std::size_t size) { return std::malloc(size); }
void* APS5_VABI Realign(void* ptr, std::size_t size, std::size_t) { return std::realloc(ptr, size); }
int APS5_VABI PosixAlign(void** ptr, std::size_t, std::size_t size) {
    *ptr = std::malloc(size);
    return *ptr ? 0 : 12;
}

void InitializeHeap() {
    alignas(16) static std::array<std::byte, 0x40> process{};
    alignas(16) static std::array<std::byte, 0x38> libc{};
    alignas(16) static std::array<std::byte, 0x78> replacement{};
    const auto write = [](auto& data, std::size_t offset, auto value) {
        std::memcpy(data.data() + offset, &value, sizeof(value));
    };
    write(process, 0, std::uint64_t{0x40});
    write(process, 8, std::uint32_t{0x4942524f});
    write(process, 0x38, libc.data());
    write(libc, 0, std::uint64_t{0x38});
    write(libc, 0x30, replacement.data());
    write(replacement, 0, std::uint64_t{0x78});
    write(replacement, 8, std::uint64_t{2});
    write(replacement, 0x20, reinterpret_cast<void*>(&Allocate));
    write(replacement, 0x28, reinterpret_cast<void*>(&Free));
    write(replacement, 0x30, reinterpret_cast<void*>(&Calloc));
    write(replacement, 0x38, reinterpret_cast<void*>(&Reallocate));
    write(replacement, 0x40, reinterpret_cast<void*>(&Align));
    write(replacement, 0x48, reinterpret_cast<void*>(&Realign));
    write(replacement, 0x50, reinterpret_cast<void*>(&PosixAlign));
    ApplicationHeapInitialize_nid_no_patch(process.data());
}

int CallVasprintf(char** destination, const char* format, ...) {
#ifdef _WIN32
    __builtin_sysv_va_list args;
    __builtin_sysv_va_start(args, format);
#else
    std::va_list args;
    va_start(args, format);
#endif
    const int result = vasprintf_nid_postfix(destination, format, reinterpret_cast<VaList*>(args));
#ifdef _WIN32
    __builtin_sysv_va_end(args);
#else
    va_end(args);
#endif
    return result;
}

}

int main() {
    *__error_nid_postfix() = 0;
    Require(strndup_nid_postfix(nullptr, 0) == nullptr);
    Require(*__error_nid_postfix() == 22);

    *__error_nid_postfix() = 0;
    Require(strndup_nid_postfix(nullptr, 32) == nullptr);
    Require(*__error_nid_postfix() == 22);

    *__error_nid_postfix() = 0;
    Require(strdup_nid_postfix(nullptr) == nullptr);
    Require(*__error_nid_postfix() == 22);

    *__error_nid_postfix() = 0;
    Require(vasprintf_nid_postfix(nullptr, "format", nullptr) == -1);
    Require(*__error_nid_postfix() == 22);

    char* allocated = nullptr;
    *__error_nid_postfix() = 0;
    Require(CallVasprintf(&allocated, nullptr) == -1);
    Require(*__error_nid_postfix() == 22);
    Require(allocated == nullptr);

    *__error_nid_postfix() = 0;
    Require(vasprintf_nid_postfix(&allocated, "format", nullptr) == -1);
    Require(*__error_nid_postfix() == 22);
    Require(allocated == nullptr);

    InitializeHeap();

    char* dup1 = strndup_nid_postfix("hello world", 5);
    Require(dup1 != nullptr);
    Require(std::strcmp(dup1, "hello") == 0);
    free_nid_postfix(dup1);

    char* dup2 = strdup_nid_postfix("testing 1 2 3");
    Require(dup2 != nullptr);
    Require(std::strcmp(dup2, "testing 1 2 3") == 0);
    free_nid_postfix(dup2);

    allocated = nullptr;
    const int count = asprintf_nid_postfix(&allocated, "value=%d %s", 42, "ok");
    Require(count == 11);
    Require(allocated != nullptr);
    Require(std::strcmp(allocated, "value=42 ok") == 0);
    free_nid_postfix(allocated);

    failAllocation = true;
    allocated = nullptr;
    *__error_nid_postfix() = 0;
    Require(asprintf_nid_postfix(&allocated, "value=%d", 1) == -1);
    Require(*__error_nid_postfix() == 12);
    Require(allocated == nullptr);
    failAllocation = false;

    return 0;
}
