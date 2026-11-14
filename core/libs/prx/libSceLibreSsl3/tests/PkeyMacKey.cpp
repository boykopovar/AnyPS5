#include <dlfcn.h>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
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

struct Octets {
    std::int32_t length;
    std::int32_t type;
    unsigned char* data;
    std::int64_t flags;
};

static_assert(sizeof(Octets) == 24);
static_assert(offsetof(Octets, type) == 4);
static_assert(offsetof(Octets, data) == 8);
static_assert(offsetof(Octets, flags) == 16);

int outstanding = 0;
int allocations = 0;
int failOn = 0;

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "LibreSSL3 PKEY MAC key failed at line %u\n", location.line());
    std::exit(1);
}

void* Allocate(std::size_t bytes) {
    ++allocations;
    if (allocations == failOn) return nullptr;
    void* block = std::malloc(bytes);
    Require(block != nullptr);
    ++outstanding;
    return block;
}

void* AllocateZeroed(std::size_t count, std::size_t bytes) {
    ++allocations;
    if (allocations == failOn) return nullptr;
    void* block = std::calloc(count, bytes);
    Require(block != nullptr);
    ++outstanding;
    return block;
}

void* Reallocate(void*, std::size_t) { Require(false); return nullptr; }
void* Align(std::size_t, std::size_t) { Require(false); return nullptr; }
void* Realign(void*, std::size_t, std::size_t) { Require(false); return nullptr; }
int PosixAlign(void**, std::size_t, std::size_t) { Require(false); return 0; }

void Release(void* pointer) {
    if (pointer == nullptr) return;
    --outstanding;
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
    Require(argc == 2);
    void* module = dlopen((std::string(argv[1]) + "/libSceLibreSsl3.prx").c_str(), RTLD_NOW | RTLD_LOCAL);
    if (!module) std::fprintf(stderr, "%s\n", dlerror());
    Require(module != nullptr);
    auto registerHeap = reinterpret_cast<void (*)(void* const*)>(dlsym(module, "ApplicationHeapRegister_nid_no_patch"));
    Require(registerHeap != nullptr);
    void* const heap[10] {reinterpret_cast<void*>(Allocate), reinterpret_cast<void*>(Release), reinterpret_cast<void*>(AllocateZeroed),
                         reinterpret_cast<void*>(Reallocate), reinterpret_cast<void*>(Align), reinterpret_cast<void*>(Realign),
                         reinterpret_cast<void*>(PosixAlign)};
    registerHeap(heap);
    auto release = Resolve<void (*)(Pkey*)>(module, "S7L4OYSnC0Q");
    auto newMacKey = Resolve<Pkey* (*)(int, void*, const unsigned char*, int)>(module, "m0bTFPN3yzk");
    auto keySize = Resolve<int (*)(Pkey*)>(module, "uoC6WmyO4Dg");
    auto compareKeys = Resolve<int (*)(Pkey*, Pkey*)>(module, "vLuIwBfrxrk");
    const unsigned char secret[4] = {1, 2, 3, 4};
    Pkey* key = newMacKey(855, nullptr, secret, 4);
    Require(key != nullptr);
    Require(key->type == 855);
    Require(key->references == 1);
    Require(key->method != nullptr);
    Require(key->key != nullptr);
    Require(outstanding == 3);
    auto* octets = static_cast<Octets*>(key->key);
    Require(octets->length == 4);
    Require(octets->type == 4);
    Require(std::memcmp(octets->data, secret, 4) == 0);
    Require(octets->data[4] == 0);
    Require(keySize(key) == 64);
    auto same = newMacKey(855, nullptr, secret, 4);
    Require(same != nullptr);
    Require(compareKeys(key, same) == 1);
    const unsigned char other[4] = {5, 6, 7, 8};
    auto different = newMacKey(855, nullptr, other, 4);
    Require(different != nullptr);
    Require(compareKeys(key, different) == 0);
    release(key);
    release(same);
    release(different);
    Require(outstanding == 0);
    Require(newMacKey(6, nullptr, secret, 4) == nullptr);
    Require(newMacKey(12345, nullptr, secret, 4) == nullptr);
    Require(newMacKey(855, nullptr, nullptr, 5) == nullptr);
    Require(newMacKey(855, nullptr, secret, -2) == nullptr);
    Require(newMacKey(855, nullptr, nullptr, -1) == nullptr);
    Require(outstanding == 0);
    return 0;
}
