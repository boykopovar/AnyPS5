#include "prx/libSceJxr/libSceJxr.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "tests/CommandLineArgument.hpp"
#include <array>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <stdexcept>
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

void* Open(const std::filesystem::path& path) {
#ifdef _WIN32
    auto library = LoadLibraryW(path.c_str());
    if (!library) std::fprintf(stderr, "LoadLibrary error %lu: %s\n", GetLastError(), path.string().c_str());
#else
    auto library = dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);
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

void Invalid(Cleanup cleanup) {
    try {
        cleanup(nullptr);
        Require(false, "null metadata record accepted");
    } catch (const std::invalid_argument&) {}
    Jxr::MetadataVariant value{};
    value.type = 1;
    try {
        cleanup(&value);
        Require(false, "unsupported metadata tag accepted");
    } catch (const std::invalid_argument&) {}
    errno = 71;
    try {
        cleanup(reinterpret_cast<Jxr::MetadataVariant*>(std::uintptr_t{0x10}));
        Require(false, "unreadable metadata record accepted");
    } catch (const std::invalid_argument&) {
        Require(errno == 71, "errno preserved on unreadable metadata record");
    }
    static char readOnlyDummy[8] = {'a'};
    static const Jxr::MetadataVariant readOnly{.type = 30, .value = {.text = readOnlyDummy}};
    const unsigned count = frees;
    errno = 71;
    try {
        cleanup(const_cast<Jxr::MetadataVariant*>(&readOnly));
        Require(false, "read-only metadata record accepted");
    } catch (const std::invalid_argument&) {
        Require(errno == 71 && frees == count && readOnly.value.text == readOnlyDummy,
                "read-only metadata rejected before freeing or mutating");
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

} // namespace

int main(int argc, char** argv) {
    Require(argc == 4, "expected libc path, JXR path and export name");
    void* heap = Open(ArgumentPath(argv, 1));
    const std::array<void*, 10> callbacks{
        reinterpret_cast<void*>(&Allocate_nid_no_patch), reinterpret_cast<void*>(&Free_nid_no_patch), reinterpret_cast<void*>(&Calloc_nid_no_patch),
        reinterpret_cast<void*>(&Reallocate_nid_no_patch), reinterpret_cast<void*>(&Align_nid_no_patch), reinterpret_cast<void*>(&Realign_nid_no_patch),
        reinterpret_cast<void*>(&PosixAlign_nid_no_patch), nullptr, nullptr, nullptr};
    Symbol<decltype(&ApplicationHeapRegister_nid_no_patch)>(heap, "ApplicationHeapRegister_nid_no_patch")(callbacks.data());
    void* library = Open(ArgumentPath(argv, 2));
    const auto cleanup = Symbol<Cleanup>(library, argv[3]);
    try {
        Strings(cleanup);
        Scalars(cleanup);
        Invalid(cleanup);
        Retagged(cleanup);
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
    std::puts("JXR metadata: narrow/wide guest frees, repeated cleanup, scalar preservation passed");
}
