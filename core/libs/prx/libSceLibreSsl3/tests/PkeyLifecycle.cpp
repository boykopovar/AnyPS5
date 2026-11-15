#include <dlfcn.h>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <source_location>
#include <string>

namespace {

struct Pkey {
    std::int32_t type;
    std::int32_t references;
    const void* method;
    void* key;
    std::int32_t saveParameters;
};

static_assert(sizeof(Pkey) == 32);
static_assert(offsetof(Pkey, references) == 4);
static_assert(offsetof(Pkey, method) == 8);
static_assert(offsetof(Pkey, key) == 16);
static_assert(offsetof(Pkey, saveParameters) == 24);

void* live;
std::size_t allocated;
unsigned releases;
bool wiped;
bool failAllocation;

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "LibreSSL3 PKEY lifecycle failed at line %u\n", location.line());
    std::exit(1);
}

void* Allocate(std::size_t bytes) {
    Require(!live);
    live = std::malloc(bytes);
    allocated = bytes;
    Require(live != nullptr);
    std::memset(live, 0xa5, bytes);
    return live;
}

void* AllocateZeroed(std::size_t count, std::size_t bytes) {
    if (failAllocation) return nullptr;
    void* result = Allocate(count * bytes);
    std::memset(result, 0, count * bytes);
    return result;
}

void* Reallocate(void*, std::size_t) { Require(false); return nullptr; }
void* Align(std::size_t, std::size_t) { Require(false); return nullptr; }
void* Realign(void*, std::size_t, std::size_t) { Require(false); return nullptr; }
int PosixAlign(void**, std::size_t, std::size_t) { Require(false); return 0; }

void Release(void* pointer) {
    if (!pointer) return;
    Require(pointer == live);
    wiped = true;
    for (std::size_t i = 0; i < allocated; ++i)
        wiped &= static_cast<unsigned char*>(pointer)[i] == 0;
    live = nullptr;
    ++releases;
    std::free(pointer);
}

template<typename TFunction> TFunction Resolve(void* module, const char* nid, bool requireLocal = true) {
    void* address = dlsym(module, nid);
    Require(address != nullptr);
    if (requireLocal) {
        Dl_info info{};
        Require(dladdr(address, &info) != 0 && info.dli_fname != nullptr);
        const std::string file = info.dli_fname;
        Require(file.ends_with("/libSceLibreSsl3.prx"));
    }
    return reinterpret_cast<TFunction>(address);
}

}

int main(int argc, char** argv) {
    Require(argc == 2 || argc == 3);
    const bool constructorOnly = argc == 3 && std::strcmp(argv[2], "--constructor-only") == 0;
    void* module = dlopen((std::string(argv[1]) + "/libSceLibreSsl3.prx").c_str(), RTLD_NOW | RTLD_LOCAL);
    if (!module) std::fprintf(stderr, "%s\n", dlerror());
    Require(module != nullptr);
    auto registerHeap = reinterpret_cast<void (*)(void* const*)>(dlsym(module, "ApplicationHeapRegister_nid_no_patch"));
    Require(registerHeap != nullptr);
    void* const heap[10] {reinterpret_cast<void*>(Allocate), reinterpret_cast<void*>(Release), reinterpret_cast<void*>(AllocateZeroed),
                         reinterpret_cast<void*>(Reallocate), reinterpret_cast<void*>(Align), reinterpret_cast<void*>(Realign),
                         reinterpret_cast<void*>(PosixAlign)};
    registerHeap(heap);
    auto create = Resolve<Pkey* (*)()>(module, "LiZK5PkEeMY", !constructorOnly);
    Pkey* key = create();
    Require(key != nullptr && live == key);
    if (allocated != 32) std::fprintf(stderr, "constructor allocation: expected 32, got %zu\n", allocated);
    Require(allocated == 32);
    const std::array<unsigned char, 32> initial = {0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                                                0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0};
    Require(std::memcmp(key, initial.data(), initial.size()) == 0);
    if (constructorOnly) return 0;
    auto retain = Resolve<int (*)(Pkey*)>(module, "GT3wfuoOO5o");
    auto release = Resolve<void (*)(Pkey*)>(module, "S7L4OYSnC0Q");
    Resolve<void (*)()>(module, "5RBpaKVkWRQ");
    Resolve<void (*)()>(module, "Ek3df14vjDk");
    Resolve<void (*)()>(module, "oa8shQdXY2A");
    Resolve<void (*)()>(module, "PFxpQ6lkQP0");
    auto getError = Resolve<std::uint64_t (*)()>(module, "DJg+qWX1rwY", false);
    auto clearErrors = Resolve<void (*)()>(module, "sieQ--PINWg", false);
    Require(retain(key) == 1);
    auto retained = initial;
    retained[4] = 2;
    Require(std::memcmp(key, retained.data(), retained.size()) == 0);
    release(key);
    Require(live == key && releases == 0 && std::memcmp(key, initial.data(), initial.size()) == 0);
    release(key);
    Require(live == nullptr && releases == 1 && wiped);
    release(nullptr);
    Require(releases == 1);
    for (int references : {1, 0, -1}) {
        key = create();
        key->type = 6;
        key->references = references;
        release(key);
        Require(live == nullptr && wiped);
    }
    Require(releases == 4);
    clearErrors();
    Require(getError() == 0);
    failAllocation = true;
    Require(create() == nullptr && live == nullptr && releases == 4);
    Require(getError() == 0x06fff041 && getError() == 0);
    return 0;
}
