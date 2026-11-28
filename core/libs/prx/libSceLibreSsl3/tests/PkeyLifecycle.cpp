#include <array>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <source_location>
#include <stdexcept>
#include <string>
#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#include <sys/mman.h>
#include <unistd.h>
#endif

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

template<typename TAction>
void RequireInvalidArgument(TAction action, std::source_location location = std::source_location::current()) {
    try {
        action();
    } catch (const std::invalid_argument&) {
        return;
    }
    Require(false, location);
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
    auto copyParameters = Resolve<int (*)(Pkey*, const Pkey*)>(module, "oa8shQdXY2A");
    Resolve<void (*)()>(module, "PFxpQ6lkQP0");
    auto getError = Resolve<std::uint64_t (*)()>(module, "DJg+qWX1rwY", false);
    auto clearErrors = Resolve<void (*)()>(module, "sieQ--PINWg", false);
    auto createEcGroup = Resolve<void* (*)(int)>(module, "47B8y-Mqy78", false);
    auto freeEcGroup = Resolve<void (*)(void*)>(module, "hMV6sxdqoCE", false);
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

    auto* badPkey = reinterpret_cast<Pkey*>(static_cast<std::uintptr_t>(1));
    errno = 41;
    RequireInvalidArgument([&] { release(badPkey); });
    Require(errno == 41);

    Pkey ecRollback{};
    std::memset(&ecRollback, 0xa5, sizeof(ecRollback));
    ecRollback.type = 408;
    ecRollback.references = 1;
    ecRollback.method = nullptr;
    ecRollback.key = reinterpret_cast<void*>(static_cast<std::uintptr_t>(1));
    errno = 42;
    RequireInvalidArgument([&] { release(&ecRollback); });
    Require(errno == 42);
    Require(ecRollback.references == 1 && ecRollback.key == reinterpret_cast<void*>(static_cast<std::uintptr_t>(1)));

    Pkey sentinelPkey{};
    std::memset(&sentinelPkey, 0xa5, sizeof(sentinelPkey));
    Pkey expectedSentinel{};
    std::memset(&expectedSentinel, 0xa5, sizeof(expectedSentinel));

    Require(copyParameters(nullptr, &sentinelPkey) == 0);
    Require(copyParameters(&sentinelPkey, nullptr) == 0);
    Require(std::memcmp(&sentinelPkey, &expectedSentinel, sizeof(sentinelPkey)) == 0);

    errno = 43;
    RequireInvalidArgument([&] { (void)copyParameters(badPkey, &sentinelPkey); });
    Require(errno == 43);
    Require(std::memcmp(&sentinelPkey, &expectedSentinel, sizeof(sentinelPkey)) == 0);

    errno = 44;
    RequireInvalidArgument([&] { (void)copyParameters(&sentinelPkey, badPkey); });
    Require(errno == 44);
    Require(std::memcmp(&sentinelPkey, &expectedSentinel, sizeof(sentinelPkey)) == 0);

    Pkey ecTo{};
    std::memset(&ecTo, 0xa5, sizeof(ecTo));
    ecTo.type = 408;
    ecTo.key = nullptr;
    Pkey ecToExpected = ecTo;

    Pkey ecBadFrom{};
    ecBadFrom.type = 408;
    ecBadFrom.key = reinterpret_cast<void*>(static_cast<std::uintptr_t>(1));
    errno = 45;
    RequireInvalidArgument([&] { (void)copyParameters(&ecTo, &ecBadFrom); });
    Require(errno == 45);
    Require(std::memcmp(&ecTo, &ecToExpected, sizeof(ecTo)) == 0);

    struct FakeEcKey {
        const void* method;
        std::int32_t version;
        void* group;
        void* publicKey;
        void* privateKey;
        std::uint32_t encodingFlags;
        std::int32_t conversionForm;
        std::int32_t references;
        std::int32_t flags;
        void* extraData;
    };
    static_assert(sizeof(FakeEcKey) == 64 && offsetof(FakeEcKey, group) == 16);
    FakeEcKey badGroupEc{};
    badGroupEc.group = reinterpret_cast<void*>(static_cast<std::uintptr_t>(1));
    ecBadFrom.key = &badGroupEc;
    ecTo.key = &badGroupEc;
    ecToExpected = ecTo;
    errno = 46;
    RequireInvalidArgument([&] { (void)copyParameters(&ecTo, &ecBadFrom); });
    Require(errno == 46);
    Require(std::memcmp(&ecTo, &ecToExpected, sizeof(ecTo)) == 0);

#ifdef _WIN32
    SYSTEM_INFO info{};
    GetSystemInfo(&info);
    const std::size_t page = info.dwPageSize;
    auto* roPage = static_cast<unsigned char*>(VirtualAlloc(nullptr, page, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
    Require(roPage != nullptr);
    DWORD previous = 0;
#else
    const std::size_t page = static_cast<std::size_t>(sysconf(_SC_PAGESIZE));
    auto* roPage = static_cast<unsigned char*>(mmap(nullptr, page, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
    Require(roPage != MAP_FAILED);
#endif

    std::memset(roPage, 0, page);
    std::memset(roPage, 0xa5, sizeof(Pkey));
    auto* roPkey = reinterpret_cast<Pkey*>(roPage);
    roPkey->type = 6;
    roPkey->references = 2;
    roPkey->method = nullptr;
    roPkey->key = nullptr;
    Pkey roExpected{};
    std::memcpy(&roExpected, roPkey, sizeof(roExpected));
    void* const roGroup = roPage + 64;

#ifdef _WIN32
    Require(VirtualProtect(roPage, page, PAGE_READONLY, &previous) != 0);
#else
    Require(mprotect(roPage, page, PROT_READ) == 0);
#endif

    errno = 47;
    RequireInvalidArgument([&] { release(roPkey); });
    Require(errno == 47);
    Require(std::memcmp(roPkey, &roExpected, sizeof(roExpected)) == 0);

    Pkey diffType{};
    diffType.type = 408;
    Require(copyParameters(roPkey, &diffType) == 0);
    Require(getError() != 0);
    clearErrors();
    Require(std::memcmp(roPkey, &roExpected, sizeof(roExpected)) == 0);

    void* const sourceGroup = createEcGroup(415);
    Require(sourceGroup != nullptr);
    FakeEcKey validSourceEc{};
    validSourceEc.group = sourceGroup;
    Pkey validEcFrom{};
    validEcFrom.type = 408;
    validEcFrom.key = &validSourceEc;

    int nextErrno = 48;
    for (void* FakeEcKey::* field : {&FakeEcKey::publicKey, &FakeEcKey::privateKey, &FakeEcKey::group}) {
        FakeEcKey badDestEc{};
        badDestEc.*field = reinterpret_cast<void*>(static_cast<std::uintptr_t>(1));
        FakeEcKey badDestExpected = badDestEc;
        ecTo.key = &badDestEc;
        ecToExpected = ecTo;
        errno = nextErrno;
        RequireInvalidArgument([&] { (void)copyParameters(&ecTo, &validEcFrom); });
        Require(errno == nextErrno);
        Require(std::memcmp(&badDestEc, &badDestExpected, sizeof(badDestEc)) == 0);
        Require(std::memcmp(&ecTo, &ecToExpected, sizeof(ecTo)) == 0);
        ++nextErrno;
    }

    FakeEcKey roGroupDestEc{};
    roGroupDestEc.group = roGroup;
    FakeEcKey roGroupDestExpected = roGroupDestEc;
    ecTo.key = &roGroupDestEc;
    ecToExpected = ecTo;
    errno = nextErrno;
    RequireInvalidArgument([&] { (void)copyParameters(&ecTo, &validEcFrom); });
    Require(errno == nextErrno);
    Require(std::memcmp(&roGroupDestEc, &roGroupDestExpected, sizeof(roGroupDestEc)) == 0);
    Require(std::memcmp(&ecTo, &ecToExpected, sizeof(ecTo)) == 0);

    freeEcGroup(sourceGroup);

#ifdef _WIN32
    Require(VirtualFree(roPage, 0, MEM_RELEASE) != 0);
#else
    Require(munmap(roPage, page) == 0);
#endif
    return 0;
}
