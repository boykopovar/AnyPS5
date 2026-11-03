#include "prx/libSceJxr/libSceJxr.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace {

using Cleanup = decltype(&FreeDescMetadata_nid_postfix);
void* lastFreed = nullptr;
unsigned frees = 0;
Jxr::MetadataVariant* retagTarget = nullptr;

void Require(bool value, const char* message) {
    if (value) return;
    std::fprintf(stderr, "JXR metadata: %s\n", message);
    std::exit(1);
}

void* APS5_VABI Allocate_nid_no_patch(std::size_t size) { return std::malloc(size); }
void APS5_VABI Free_nid_no_patch(void* pointer) {
    if (pointer) {
        lastFreed = pointer;
        ++frees;
        if (retagTarget) retagTarget->type = 19;
    }
    std::free(pointer);
}
void* APS5_VABI Calloc_nid_no_patch(std::size_t count, std::size_t size) { return std::calloc(count, size); }
void* APS5_VABI Reallocate_nid_no_patch(void* pointer, std::size_t size) { return std::realloc(pointer, size); }
void* APS5_VABI Align_nid_no_patch(std::size_t, std::size_t size) { return std::malloc(size); }
void* APS5_VABI Realign_nid_no_patch(void* pointer, std::size_t size, std::size_t) { return std::realloc(pointer, size); }
int APS5_VABI PosixAlign_nid_no_patch(void** pointer, std::size_t, std::size_t size) {
    *pointer = std::malloc(size);
    return *pointer ? 0 : 12;
}

template<typename TFunction>
TFunction Symbol(void* library, const char* name) {
#ifdef _WIN32
    auto address = GetProcAddress(static_cast<HMODULE>(library), name);
#else
    auto address = dlsym(library, name);
#endif
    Require(address != nullptr, name);
    return reinterpret_cast<TFunction>(address);
}

void* Open(const char* path) {
#ifdef _WIN32
    auto library = LoadLibraryA(path);
    if (!library) std::fprintf(stderr, "LoadLibrary error %lu: %s\n", GetLastError(), path);
#else
    auto library = dlopen(path, RTLD_NOW | RTLD_LOCAL);
    if (!library) std::fprintf(stderr, "%s\n", dlerror());
#endif
    Require(library != nullptr, "library load");
    return library;
}

struct GuardedVariant {
    std::uint64_t before;
    Jxr::MetadataVariant variant;
    std::uint64_t after;
};

void Strings(Cleanup cleanup) {
    for (const std::int32_t type : {30, 31}) {
        GuardedVariant record;
        std::memset(&record, 0xa5, sizeof(record));
        record.variant.type = type;
        void* memory = std::malloc(64);
        Require(memory != nullptr, "string allocation");
        std::memset(memory, 0x5a, 64);
        if (type == 30) record.variant.value.text = static_cast<char*>(memory);
        else record.variant.value.wideText = static_cast<std::uint16_t*>(memory);
        const auto original = record;
        const unsigned count = frees;
        cleanup(&record.variant);
        Require(frees == count + 1 && lastFreed == memory, "string released through the guest free callback");
        if (type == 30) Require(record.variant.value.text == nullptr, "narrow pointer cleared");
        else Require(record.variant.value.wideText == nullptr, "wide pointer cleared");
        Require(std::memcmp(&record.variant, &original.variant, offsetof(Jxr::MetadataVariant, value)) == 0,
                "string tag and padding preserved");
        Require(record.before == original.before && record.after == original.after, "cleanup stays inside the record");
        const auto empty = record;
        cleanup(&record.variant);
        Require(frees == count + 1 && std::memcmp(&record, &empty, sizeof(record)) == 0, "repeated cleanup is harmless");
    }
}

void Scalars(Cleanup cleanup) {
    for (const std::int32_t type : {0, 18, 19}) {
        GuardedVariant record;
        std::memset(&record, 0xa5, sizeof(record));
        record.variant.type = type;
        const auto original = record;
        const unsigned count = frees;
        cleanup(&record.variant);
        Require(frees == count && std::memcmp(&record, &original, sizeof(record)) == 0, "scalar variant left unchanged");
    }
}

void Retagged(Cleanup cleanup) {
    Jxr::MetadataVariant value{};
    value.type = 30;
    value.value.text = static_cast<char*>(std::malloc(16));
    Require(value.value.text != nullptr, "retag allocation");
    const unsigned count = frees;
    retagTarget = &value;
    cleanup(&value);
    retagTarget = nullptr;
    Require(frees == count + 1 && value.type == 19 && value.value.text == nullptr,
            "callback may retag the record without preventing pointer cleanup");
}

void Unsupported(Cleanup cleanup) {
    for (const std::int32_t type : {17, 0x4011, -1, 0x7fffffff}) {
        GuardedVariant record;
        std::memset(&record, 0xa5, sizeof(record));
        record.variant.type = type;
        const auto original = record;
        const unsigned count = frees;
        bool threw = false;
        try { cleanup(&record.variant); }
        catch (const std::exception&) { threw = true; }
        Require(threw && frees == count && std::memcmp(&record, &original, sizeof(record)) == 0,
                "unsupported tag throws before mutation or release");
    }
    bool threw = false;
    try { cleanup(nullptr); }
    catch (const std::exception&) { threw = true; }
    Require(threw, "null record throws");
}

} // namespace

int main(int argc, char** argv) {
    Require(argc == 4, "expected libc path, JXR path and export name");
    void* heap = Open(argv[1]);
    const std::array<void*, 10> callbacks{
        reinterpret_cast<void*>(&Allocate_nid_no_patch), reinterpret_cast<void*>(&Free_nid_no_patch), reinterpret_cast<void*>(&Calloc_nid_no_patch),
        reinterpret_cast<void*>(&Reallocate_nid_no_patch), reinterpret_cast<void*>(&Align_nid_no_patch), reinterpret_cast<void*>(&Realign_nid_no_patch),
        reinterpret_cast<void*>(&PosixAlign_nid_no_patch), nullptr, nullptr, nullptr};
    Symbol<decltype(&ApplicationHeapRegister_nid_no_patch)>(heap, "ApplicationHeapRegister_nid_no_patch")(callbacks.data());
    void* library = Open(argv[2]);
    const auto cleanup = Symbol<Cleanup>(library, argv[3]);
    try {
        Strings(cleanup);
        Scalars(cleanup);
        Retagged(cleanup);
        Unsupported(cleanup);
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
    std::puts("JXR metadata: narrow/wide guest frees, repeated cleanup, scalar preservation and unsupported inputs passed");
}
