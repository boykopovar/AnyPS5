#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <limits>
#include <new>
#include <stdexcept>

extern "C" {
int APS5_VABI posix_memalign_nid_postfix(void**, std::size_t, std::size_t);
void* APS5_VABI _Znwm_nid_postfix(std::size_t);
void* APS5_VABI _ZnwmRKSt9nothrow_t_nid_postfix(std::size_t, const void*) noexcept;
void* APS5_VABI _ZnamRKSt9nothrow_t_nid_postfix(std::size_t, const void*) noexcept;
void APS5_VABI _ZdlPv_nid_postfix(void*);
void APS5_VABI _ZdaPv_nid_postfix(void*);
void APS5_VABI _ZdlPvSt11align_val_t_nid_postfix(void*, std::size_t);
void* ApplicationHeapRealign_nid_no_patch(void*, std::size_t, std::size_t);
char* APS5_VABI strdup_nid_postfix(const char*);
char* APS5_VABI strndup_nid_postfix(const char*, std::size_t);
int* APS5_VABI __error_nid_postfix();
int APS5_VABI atexit_nid_postfix(void (APS5_VABI*)());
}

namespace {

alignas(64) std::array<std::byte, 256> storage{};
std::size_t lastSize = 0;
std::size_t lastAlignment = 0;
unsigned posixCalls = 0;
unsigned initializes = 0;
unsigned frees = 0;
bool fail = false;
bool recurse = false;
bool nullPosixResult = false;

void require(bool condition) {
    if (!condition) throw std::runtime_error("application heap test failed");
}

template<typename TAction>
void reject(TAction action) {
    bool rejected = false;
    try { action(); } catch (const std::exception&) { rejected = true; }
    require(rejected);
}

void APS5_VABI initialize() { ++initializes; }
void APS5_VABI finalize() { require(initializes == 1); }
void APS5_VABI exitCallbackAllocates() {
    if (ApplicationHeapAllocate_nid_no_patch(16) != storage.data()) throw std::runtime_error("application heap test failed");
}

void* APS5_VABI allocate(std::size_t bytes) {
    lastSize = bytes;
    if (recurse) return ApplicationHeapAllocate_nid_no_patch(bytes);
    return fail ? nullptr : storage.data();
}

void APS5_VABI release(void* pointer) {
    require(pointer == storage.data());
    ++frees;
}

void* APS5_VABI reallocate(void* pointer, std::size_t bytes) {
    require(pointer == storage.data());
    return allocate(bytes);
}

void* APS5_VABI allocateZeroed(std::size_t count, std::size_t bytes) {
    return allocate(count * bytes);
}

void* APS5_VABI align(std::size_t alignment, std::size_t bytes) {
    lastAlignment = alignment;
    return allocate(bytes);
}

void* APS5_VABI realign(void* pointer, std::size_t bytes, std::size_t alignment) {
    require(pointer == storage.data());
    lastAlignment = alignment;
    return allocate(bytes);
}

int APS5_VABI posixAlign(void** pointer, std::size_t alignment, std::size_t bytes) {
    ++posixCalls;
    if (fail) { *__error_nid_postfix() = 12; *pointer = nullptr; return 12; }
    if (nullPosixResult) { *pointer = nullptr; return 0; }
    *pointer = align(alignment, bytes);
    return 0;
}

template<typename TValue, std::size_t TSize>
void write(std::array<std::byte, TSize>& data, std::size_t offset, TValue value) {
    require(offset <= data.size() && sizeof(value) <= data.size() - offset);
    std::memcpy(data.data() + offset, &value, sizeof(value));
}

}

int main(int argc, char** argv) {
    reject([] { ApplicationHeapAllocate_nid_no_patch(64); });
    reject([] { ApplicationHeapRegister_nid_no_patch(nullptr); });
    std::array<std::byte, 0x40> process{};
    std::array<std::byte, 0x38> libc{};
    std::array<std::byte, 0x78> replacement{};
    write(process, 0, std::uint64_t{0x40});
    write(process, 8, std::uint32_t{0x4942524f});
    write(process, 0x38, libc.data());
    write(libc, 0, std::uint64_t{0x38});
    write(libc, 0x30, replacement.data());
    write(replacement, 0, std::uint64_t{0x78});
    write(replacement, 8, std::uint64_t{2});
    write(replacement, 0x10, &initialize);
    write(replacement, 0x18, &finalize);
    write(replacement, 0x20, &allocate);
    write(replacement, 0x28, &release);
    write(replacement, 0x30, &allocateZeroed);
    write(replacement, 0x38, &reallocate);
    write(replacement, 0x40, &align);
    write(replacement, 0x48, &realign);
    write(replacement, 0x50, &posixAlign);
    if (argc > 1 && std::strcmp(argv[1], "exit-order") == 0) {
        require(atexit_nid_postfix(exitCallbackAllocates) == 0);
        ApplicationHeapInitialize_nid_no_patch(process.data());
        require(initializes == 1);
        return 0;
    }
    if (argc > 1 && std::strcmp(argv[1], "default") == 0) {
        std::array<void*, 10> partial{};
        partial[0] = reinterpret_cast<void*>(&allocate);
        reject([&] { ApplicationHeapRegister_nid_no_patch(partial.data()); });
        std::memset(replacement.data() + 0x20, 0, sizeof(partial));
        ApplicationHeapInitialize_nid_no_patch(process.data());
        require(initializes == 1);
        auto* pointer = static_cast<unsigned char*>(ApplicationHeapCalloc_nid_no_patch(7, 9));
        for (unsigned i = 0; i < 63; ++i) require(pointer[i] == 0);
        std::memset(pointer, 0x5a, 63);
        pointer = static_cast<unsigned char*>(ApplicationHeapReallocate_nid_no_patch(pointer, 150));
        for (unsigned i = 0; i < 63; ++i) require(pointer[i] == 0x5a);
        pointer = static_cast<unsigned char*>(ApplicationHeapReallocate_nid_no_patch(pointer, 11));
        for (unsigned i = 0; i < 11; ++i) require(pointer[i] == 0x5a);
        require(ApplicationHeapReallocate_nid_no_patch(pointer, 0) == nullptr);
        pointer = static_cast<unsigned char*>(ApplicationHeapReallocate_nid_no_patch(nullptr, 32));
        require(pointer != nullptr);
        ApplicationHeapFree_nid_no_patch(pointer);
        for (std::size_t alignment : {16, 64, 4096}) {
            auto* aligned = ApplicationHeapAlign_nid_no_patch(alignment, 37);
            require(reinterpret_cast<std::uintptr_t>(aligned) % alignment == 0);
            _ZdlPvSt11align_val_t_nid_postfix(aligned, alignment);
        }
        void* aligned = nullptr;
        require(ApplicationHeapPosixAlign_nid_no_patch(&aligned, 256, 99) == 0);
        require(reinterpret_cast<std::uintptr_t>(aligned) % 256 == 0);
        ApplicationHeapFree_nid_no_patch(aligned);
        ApplicationHeapFree_nid_no_patch(nullptr);
        reject([] { ApplicationHeapCalloc_nid_no_patch(SIZE_MAX, 2); });
        reject([] { ApplicationHeapAlign_nid_no_patch(3, 16); });
        ApplicationHeapInitialize_nid_no_patch(process.data());
        require(initializes == 1);
        return 0;
    }
    if (argc > 1) {
        write(replacement, 8, std::uint64_t{99});
        reject([&] { ApplicationHeapInitialize_nid_no_patch(process.data()); });
        write(replacement, 8, std::uint64_t{2});
        reject([&] { ApplicationHeapInitialize_nid_no_patch(process.data()); });
        reject([] { ApplicationHeapAlign_nid_no_patch(4, 64); });
        require(initializes == 0);
        return 0;
    }
    ApplicationHeapInitialize_nid_no_patch(process.data());
    ApplicationHeapInitialize_nid_no_patch(process.data());
    require(initializes == 1);
    void* pointer = ApplicationHeapAlign_nid_no_patch(4, 64);
    require(pointer == storage.data() && lastAlignment == 4 && lastSize == 64);
    release(pointer);
    require(frees == 1);
    pointer = ApplicationHeapAllocate_nid_no_patch(32);
    require(pointer == storage.data() && lastSize == 32);
    require(ApplicationHeapReallocate_nid_no_patch(pointer, 96) == storage.data() && lastSize == 96);
    ApplicationHeapFree_nid_no_patch(pointer);
    require(frees == 2);
    require(ApplicationHeapCalloc_nid_no_patch(3, 16) == storage.data() && lastSize == 48);
    require(ApplicationHeapPosixAlign_nid_no_patch(&pointer, 64, 128) == 0 && pointer == storage.data() && lastAlignment == 64);
    reject([] { ApplicationHeapAlign_nid_no_patch(3, 64); });
    reject([] { ApplicationHeapCalloc_nid_no_patch(2, std::numeric_limits<std::size_t>::max()); });
    require(_Znwm_nid_postfix(0) == storage.data() && lastSize == 1);
    _ZdlPv_nid_postfix(storage.data());
    _ZdlPv_nid_postfix(nullptr);
    require(frees == 3);
    require(_ZnamRKSt9nothrow_t_nid_postfix(24, nullptr) == storage.data() && lastSize == 24);
    _ZdaPv_nid_postfix(storage.data());
    require(frees == 4);
    require(ApplicationHeapRealign_nid_no_patch(storage.data(), 48, 32) == storage.data() && lastSize == 48 && lastAlignment == 32);
    reject([] { ApplicationHeapRealign_nid_no_patch(storage.data(), 16, 4096); });
    pointer = ApplicationHeapAlign_nid_no_patch(64, 37);
    _ZdlPvSt11align_val_t_nid_postfix(pointer, 64);
    require(frees == 5);
    _ZdlPvSt11align_val_t_nid_postfix(nullptr, 64);
    require(frees == 5);
    fail = true;
    require(_ZnwmRKSt9nothrow_t_nid_postfix(8, nullptr) == nullptr);
    require(_ZnamRKSt9nothrow_t_nid_postfix(8, nullptr) == nullptr);
    reject([] { _Znwm_nid_postfix(8); });
    reject([] { ApplicationHeapAlign_nid_no_patch(4, 64); });
    reject([] { ApplicationHeapAllocate_nid_no_patch(64); });
    void* unchanged = storage.data();
    *__error_nid_postfix() = 77;
    try {
        const int error = posix_memalign_nid_postfix(&unchanged, 64, 64);
        if (error != 12) { std::fprintf(stderr, "posix_memalign callback: expected ENOMEM 12, received %d\n", error); return 1; }
    } catch (const std::exception&) {
        std::fputs("posix_memalign callback: expected ENOMEM 12, received exception\n", stderr);
        return 1;
    }
    require(*__error_nid_postfix() == 77);
    require(unchanged == storage.data());
    fail = false;
    nullPosixResult = true;
    bool rejectedNull = false;
    try {
        const int error = posix_memalign_nid_postfix(&unchanged, 64, 64);
        std::fprintf(stderr, "posix_memalign callback: success with null pointer must throw, received %d\n", error);
        return 1;
    } catch (const std::bad_alloc&) { rejectedNull = true; }
    require(rejectedNull && unchanged == storage.data());
    nullPosixResult = false;
    const auto beforeInvalid = posixCalls;
    for (const std::size_t alignment : {std::size_t{0}, std::size_t{1}, std::size_t{4}, std::size_t{24}}) {
        require(posix_memalign_nid_postfix(&unchanged, alignment, 32) == 22);
        require(unchanged == storage.data() && *__error_nid_postfix() == 77);
    }
    require(posix_memalign_nid_postfix(nullptr, 64, 32) == 22 && posixCalls == beforeInvalid);
    require(posix_memalign_nid_postfix(&unchanged, 64, 31) == 0 && unchanged == storage.data());
    require(lastSize == 31 && lastAlignment == 64 && *__error_nid_postfix() == 77);
    require(_ZnwmRKSt9nothrow_t_nid_postfix(16, nullptr) == storage.data() && lastSize == 16);
    require(_ZnwmRKSt9nothrow_t_nid_postfix(0, nullptr) == storage.data() && lastSize == 1);
    require(_ZnamRKSt9nothrow_t_nid_postfix(0, nullptr) == storage.data() && lastSize == 1);
    recurse = true;
    reject([] { ApplicationHeapAllocate_nid_no_patch(64); });
    recurse = false;
    require(ApplicationHeapAllocate_nid_no_patch(64) == storage.data());
    const char text[] = "guest string";
    char* copy = strdup_nid_postfix(text);
    require(copy == reinterpret_cast<char*>(storage.data()) && lastSize == sizeof(text));
    require(std::strcmp(copy, text) == 0 && copy != text);
    ApplicationHeapFree_nid_no_patch(copy);
    copy = strdup_nid_postfix("");
    require(lastSize == 1 && copy[0] == '\0');
    ApplicationHeapFree_nid_no_patch(copy);
    copy = strndup_nid_postfix(text, 5);
    require(lastSize == 6 && std::strcmp(copy, "guest") == 0);
    ApplicationHeapFree_nid_no_patch(copy);
    fail = true;
    *__error_nid_postfix() = 0;
    require(strdup_nid_postfix(text) == nullptr && *__error_nid_postfix() == 12);
    *__error_nid_postfix() = 0;
    require(strdup_nid_postfix("") == nullptr && *__error_nid_postfix() == 12);
    *__error_nid_postfix() = 0;
    require(strndup_nid_postfix(text, 5) == nullptr && *__error_nid_postfix() == 12);
    fail = false;
}
