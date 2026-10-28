#include "prx/libc/include/Api.hpp"
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
#include <source_location>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#elif defined(__linux__)
#include "prx/libc/include/specifics/linux/ElfTypes.hpp"
#include <dlfcn.h>
#include <sys/mman.h>
#include <unistd.h>
#elif defined(__APPLE__)
#include "ProcessParametersFixture.hpp"
#endif

extern "C" {

void* ApplicationHeapRealign_nid_no_patch(void*, std::size_t, std::size_t);

}

namespace {

alignas(64) std::array<std::byte, 256> storage{};
alignas(16) std::array<std::byte, 0x40> process{};
alignas(16) std::array<std::byte, 0x38> libc{};
alignas(16) std::array<std::byte, 0x78> replacement{};
#ifdef _WIN32
[[gnu::section(".proctst"), gnu::used]] alignas(4) std::uint32_t processParameterMetadata[2]{};
#endif
std::size_t lastSize = 0;
std::size_t lastAlignment = 0;
unsigned posixCalls = 0;
unsigned initializes = 0;
unsigned finalizes = 0;
unsigned frees = 0;
bool fail = false;
bool recurse = false;
bool nullPosixResult = false;

void require(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "application heap test failed at line %u\n", location.line());
        throw std::runtime_error("application heap test failed");
    }
}

void installProcessParameters() {
#ifdef _WIN32
    auto* image = reinterpret_cast<std::byte*>(GetModuleHandleW(nullptr));
    require(image != nullptr);
    auto* dos = reinterpret_cast<IMAGE_DOS_HEADER*>(image);
    auto* nt = reinterpret_cast<IMAGE_NT_HEADERS64*>(image + dos->e_lfanew);
    auto* sections = IMAGE_FIRST_SECTION(nt);
    IMAGE_SECTION_HEADER* target = nullptr;
    for (unsigned index = 0; index < nt->FileHeader.NumberOfSections; ++index) {
        if (std::memcmp(sections[index].Name, ".proctst", 8) == 0) {
            target = &sections[index];
            break;
        }
    }
    require(target != nullptr);
    processParameterMetadata[0] = static_cast<std::uint32_t>(process.data() - image);
    processParameterMetadata[1] = static_cast<std::uint32_t>(process.size());
    DWORD oldProtect = 0;
    require(VirtualProtect(target, sizeof(*target), PAGE_READWRITE, &oldProtect) != 0);
    std::memcpy(target->Name, ".procpar", 8);
    target->Misc.VirtualSize = sizeof(processParameterMetadata);
    DWORD restored = 0;
    VirtualProtect(target, sizeof(*target), oldProtect, &restored);
#elif defined(__linux__)
    bool installed = false;
    dl_iterate_phdr([](dl_phdr_info* image, std::size_t, void* data) {
        if (image->dlpi_name != nullptr && image->dlpi_name[0] != '\0') return 0;
        for (std::uint16_t index = 0; index < image->dlpi_phnum; ++index) {
            const auto& header = image->dlpi_phdr[index];
            if (header.p_type == PT_LOAD || header.p_type == PT_GNU_EH_FRAME || header.p_type < 0x60000000u) continue;
            auto* mutableHeader = const_cast<Elf64_Phdr*>(&header);
            const auto pageSize = static_cast<std::uintptr_t>(sysconf(_SC_PAGESIZE));
            const auto pageStart = reinterpret_cast<std::uintptr_t>(mutableHeader) & ~(pageSize - 1);
            const auto pageEnd = (reinterpret_cast<std::uintptr_t>(mutableHeader + 1) + pageSize - 1) & ~(pageSize - 1);
            if (mprotect(reinterpret_cast<void*>(pageStart), pageEnd - pageStart, PROT_READ | PROT_WRITE) != 0) return 1;
            mutableHeader->p_type = 0x61000001;
            mutableHeader->p_vaddr = reinterpret_cast<std::uintptr_t>(process.data()) - image->dlpi_addr;
            mutableHeader->p_paddr = mutableHeader->p_vaddr;
            mutableHeader->p_filesz = process.size();
            mutableHeader->p_memsz = process.size();
            *static_cast<bool*>(data) = true;
            return 1;
        }
        return 1;
    }, &installed);
    require(installed);
#elif defined(__APPLE__)
    require(ProcessParametersFixture::Install(process.data(), process.size()));
#endif
}

template<typename TAction>
void reject(TAction action) {
    bool rejected = false;
    try { action(); } catch (const std::exception&) { rejected = true; }
    require(rejected);
}

void APS5_VABI initialize() { ++initializes; }
void APS5_VABI finalize() { require(initializes == 1 && ++finalizes == 1); }
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

#ifdef __linux__
extern "C" void HeapTestSdkInitialize() { initialize(); }
extern "C" void HeapTestSdkFinalize() { finalize(); }
extern "C" void* HeapTestAllocatorEntry(unsigned index) {
    require(index < 7);
    void* result;
    std::memcpy(&result, replacement.data() + 0x20 + index * sizeof(result), sizeof(result));
    return result;
}
#endif

int main(int argc, char** argv) {
    reject([] { ApplicationHeapAllocate_nid_no_patch(64); });
    reject([] { ApplicationHeapRegister_nid_no_patch(nullptr); });
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
#ifdef __linux__
    if (argc > 1 && (std::strcmp(argv[1], "sdk") == 0 || std::strcmp(argv[1], "sdk-partial") == 0)) {
        require(argc == 3);
        void* sdk = dlopen(argv[2], RTLD_NOW | RTLD_GLOBAL);
        require(sdk != nullptr && initializes == 1 && finalizes == 0);
        if (std::strcmp(argv[1], "sdk-partial") == 0) {
            reject([] { ApplicationHeapInitialize_nid_no_patch(process.data()); });
            require(initializes == 1);
            require(dlclose(sdk) == 0);
            return 0;
        }
        const auto sdkAllocate = reinterpret_cast<void* (*)(std::size_t)>(dlsym(sdk, "gQX+4GDQjpM#guest"));
        require(sdkAllocate != nullptr);
        auto* live = static_cast<unsigned char*>(sdkAllocate(32));
        require(live == reinterpret_cast<unsigned char*>(storage.data()));
        std::memset(live, 0x5a, 32);
        ApplicationHeapInitialize_nid_no_patch(process.data());
        require(initializes == 1);
        for (unsigned index = 0; index < 32; ++index) require(live[index] == 0x5a);
        ApplicationHeapFree_nid_no_patch(live);
        require(frees == 1);
        require(ApplicationHeapAlign_nid_no_patch(64, 48) == storage.data());
        require(lastSize == 48 && lastAlignment == 64);
        ApplicationHeapFree_nid_no_patch(storage.data());
        require(frees == 2);
        require(dlclose(sdk) == 0 && finalizes == 0);
        require(ApplicationHeapAllocate_nid_no_patch(24) == storage.data() && lastSize == 24);
        ApplicationHeapFree_nid_no_patch(storage.data());
        require(frees == 3 && initializes == 1);
        return 0;
    }
    if (argc > 1 && std::strcmp(argv[1], "sdk-default") == 0) {
        require(argc == 3);
        void* sdk = dlopen(argv[2], RTLD_NOW | RTLD_GLOBAL);
        require(sdk != nullptr && initializes == 1);
        std::memset(replacement.data() + 0x10, 0, replacement.size() - 0x10);
        ApplicationHeapInitialize_nid_no_patch(process.data());
        void* pointer = ApplicationHeapAllocate_nid_no_patch(24);
        require(pointer != nullptr && pointer != storage.data() && lastSize == 0);
        ApplicationHeapFree_nid_no_patch(pointer);
        require(frees == 0 && dlclose(sdk) == 0 && finalizes == 1);
        return 0;
    }
#endif
    if (argc > 1 && std::strcmp(argv[1], "lazy") == 0) {
        installProcessParameters();
        require(initializes == 0);
        require(_Znwm_nid_postfix(40) == storage.data() && lastSize == 40 && initializes == 1);
        _ZdlPv_nid_postfix(storage.data());

        require(frees == 1);
        _init_env_nid_postfix();

        require(initializes == 1);
        return 0;
    }
    if (argc > 1 && std::strcmp(argv[1], "scripting") == 0) {
        installProcessParameters();
        constexpr std::size_t blockBytes = 0x40000;
        auto* first = static_cast<std::byte*>(scriptingGetMem_nid_postfix(0x1000, blockBytes));
        require(initializes == 0 && lastSize == 0);
        auto* second = static_cast<std::byte*>(scriptingGetMem_nid_postfix(0x1000, blockBytes));
        require(initializes == 0 && lastSize == 0);
        require(first != nullptr && second != nullptr && reinterpret_cast<std::uintptr_t>(first) % 0x1000 == 0 && reinterpret_cast<std::uintptr_t>(second) % 0x1000 == 0);
        require(first + blockBytes <= second || second + blockBytes <= first);
        std::memset(first, 0x5a, blockBytes);
        std::memset(second, 0xa5, blockBytes);
        require(first[blockBytes - 1] == std::byte{0x5a} && second[0] == std::byte{0xa5});
        scriptingFreeMem_nid_postfix(first);
        scriptingFreeMem_nid_postfix(second);

        require(initializes == 0 && frees == 0);
        _init_env_nid_postfix();

        require(initializes == 1);
        require(_Znwm_nid_postfix(40) == storage.data() && lastSize == 40);
        _ZdlPv_nid_postfix(storage.data());

        require(frees == 1);
        return 0;
    }
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
        require(ApplicationHeapReallocate_nid_no_patch(pointer, 0) == pointer && ApplicationHeapUsableSize_nid_no_patch(pointer) == 0);
        ApplicationHeapFree_nid_no_patch(pointer);
        pointer = static_cast<unsigned char*>(ApplicationHeapReallocate_nid_no_patch(nullptr, 32));
        require(pointer != nullptr);
        ApplicationHeapFree_nid_no_patch(pointer);
        pointer = static_cast<unsigned char*>(reallocf_nid_postfix(nullptr, 24));
        require(pointer != nullptr && ApplicationHeapUsableSize_nid_no_patch(pointer) == 24);
        std::memset(pointer, 0x3c, 24);
        pointer = static_cast<unsigned char*>(reallocf_nid_postfix(pointer, 200));
        require(pointer != nullptr && ApplicationHeapUsableSize_nid_no_patch(pointer) == 200);
        for (unsigned i = 0; i < 24; ++i) require(pointer[i] == 0x3c);
        pointer = static_cast<unsigned char*>(reallocf_nid_postfix(pointer, 9));
        require(pointer != nullptr && ApplicationHeapUsableSize_nid_no_patch(pointer) == 9);
        for (unsigned i = 0; i < 9; ++i) require(pointer[i] == 0x3c);
        *__error_nid_postfix() = 0;
        require(reallocf_nid_postfix(pointer, 0) == pointer && *__error_nid_postfix() == 0 && ApplicationHeapUsableSize_nid_no_patch(pointer) == 0);
        ApplicationHeapFree_nid_no_patch(pointer);
        reject([pointer] { ApplicationHeapUsableSize_nid_no_patch(pointer); });
        pointer = static_cast<unsigned char*>(reallocf_nid_postfix(nullptr, 40));
        require(pointer != nullptr);
        *__error_nid_postfix() = 77;
        require(reallocf_nid_postfix(pointer, SIZE_MAX - 4096) == nullptr && *__error_nid_postfix() == 77);
        reject([pointer] { ApplicationHeapUsableSize_nid_no_patch(pointer); });
        pointer = static_cast<unsigned char*>(reallocf_nid_postfix(nullptr, 40));
        require(pointer != nullptr);
        require(reallocf_nid_postfix(pointer, SIZE_MAX - 0x1008f) == nullptr && *__error_nid_postfix() == 22);
        reject([pointer] { ApplicationHeapUsableSize_nid_no_patch(pointer); });
        *__error_nid_postfix() = 77;
        require(reallocf_nid_postfix(nullptr, SIZE_MAX) == nullptr && *__error_nid_postfix() == 77);
        void* empty = ApplicationHeapAllocate_nid_no_patch(0);
        require(empty != nullptr);
        ApplicationHeapFree_nid_no_patch(empty);
        empty = ApplicationHeapReallocate_nid_no_patch(nullptr, 0);
        require(empty != nullptr);
        ApplicationHeapFree_nid_no_patch(empty);
        pointer = static_cast<unsigned char*>(reallocarray_nid_postfix(nullptr, 4, 10));
        require(pointer != nullptr && ApplicationHeapUsableSize_nid_no_patch(pointer) == 40);
        std::memset(pointer, 0x5a, 40);
        pointer = static_cast<unsigned char*>(reallocarray_nid_postfix(pointer, 5, 20));
        require(pointer != nullptr && ApplicationHeapUsableSize_nid_no_patch(pointer) == 100);
        for (unsigned i = 0; i < 40; ++i) require(pointer[i] == 0x5a);
        *__error_nid_postfix() = 0;
        require(reallocarray_nid_postfix(pointer, 2, SIZE_MAX) == nullptr && *__error_nid_postfix() == 12);
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
        void* refused = nullptr;
        require(ApplicationHeapPosixAlign_nid_no_patch(&refused, 256, SIZE_MAX) == 12 && refused == nullptr);
        require(posix_memalign_nid_postfix(&refused, 4096, SIZE_MAX - 4096) == 12 && refused == nullptr);
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
    pointer = ApplicationHeapAlign_nid_no_patch(64, 37);
    _ZdlPvmSt11align_val_t_nid_postfix(pointer, 37, 64);

    require(frees == 6);
    _ZdlPvmSt11align_val_t_nid_postfix(nullptr, 37, 64);

    require(frees == 6);
    require(reallocf_nid_postfix(storage.data(), 80) == storage.data() && lastSize == 80 && frees == 6);
    require(reallocf_nid_postfix(storage.data(), 16) == storage.data() && lastSize == 16 && frees == 6);
    require(reallocf_nid_postfix(storage.data(), 0) == nullptr && lastSize == 16 && frees == 7);
    fail = true;
    *__error_nid_postfix() = 0;
    require(reallocf_nid_postfix(storage.data(), 48) == nullptr && lastSize == 48 && frees == 8 && *__error_nid_postfix() == 12);
    reject([] { ApplicationHeapReallocate_nid_no_patch(storage.data(), 48); });
    require(frees == 8);
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
    char* directory = getcwd_nid_postfix(nullptr, 0);
    require(directory == reinterpret_cast<char*>(storage.data()) && directory[0] == '/' && lastSize == std::strlen(directory) + 1);
    ApplicationHeapFree_nid_no_patch(directory);
    directory = getcwd_nid_postfix(nullptr, 200);
    require(directory == reinterpret_cast<char*>(storage.data()) && lastSize == std::strlen(directory) + 1);
    ApplicationHeapFree_nid_no_patch(directory);
    fail = true;
    *__error_nid_postfix() = 0;
    require(strdup_nid_postfix(text) == nullptr && *__error_nid_postfix() == 12);
    *__error_nid_postfix() = 0;
    require(strdup_nid_postfix("") == nullptr && *__error_nid_postfix() == 12);
    *__error_nid_postfix() = 0;
    require(strndup_nid_postfix(text, 5) == nullptr && *__error_nid_postfix() == 12);
    *__error_nid_postfix() = 0;
    require(getcwd_nid_postfix(nullptr, 0) == nullptr && *__error_nid_postfix() == 12);
    fail = false;
    const auto beforeScripting = frees;
    const auto sizeBeforeScripting = lastSize;
    scriptingFreeMem_nid_postfix(nullptr);

    auto* small = static_cast<std::byte*>(scriptingGetMem_nid_postfix(0, 31));
    auto* aligned = static_cast<std::byte*>(scriptingGetMem_nid_postfix(64, 37));
    require(small != nullptr && small != storage.data() && reinterpret_cast<std::uintptr_t>(small) % 16 == 0);
    require(aligned != nullptr && aligned != storage.data() && reinterpret_cast<std::uintptr_t>(aligned) % 64 == 0);
    std::memset(small, 0x5a, 31);
    std::memset(aligned, 0xa5, 37);
    require(small + 31 <= aligned || aligned + 37 <= small);
    scriptingFreeMem_nid_postfix(small);
    scriptingFreeMem_nid_postfix(aligned);

    require(scriptingGetMem_nid_postfix(24, 80) == nullptr);
    require(scriptingGetMem_nid_postfix(16, std::numeric_limits<std::size_t>::max()) == nullptr);
    fail = true;
    auto* unaffected = scriptingGetMem_nid_postfix(16, 64);
    require(unaffected != nullptr);
    scriptingFreeMem_nid_postfix(unaffected);

    fail = false;
    require(frees == beforeScripting && lastSize == sizeBeforeScripting);
}
