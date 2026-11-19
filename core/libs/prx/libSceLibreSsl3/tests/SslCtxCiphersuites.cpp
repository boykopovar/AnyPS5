#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <source_location>
#include <stdexcept>
#include <utility>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace {

std::size_t liveAllocations = 0;
bool failReallocation = false;

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "LibreSSL3 SSL_CTX_set_ciphersuites check failed at line %u\n", location.line());
    std::abort();
}

void* APS5_VABI Allocate_nid_no_patch(std::size_t size) {
    void* pointer = std::malloc(size);
    Require(pointer != nullptr);
    ++liveAllocations;
    return pointer;
}

void APS5_VABI Free_nid_no_patch(void* pointer) {
    Require(pointer != nullptr && liveAllocations != 0);
    --liveAllocations;
    std::free(pointer);
}

void* APS5_VABI Callocate_nid_no_patch(std::size_t count, std::size_t size) {
    void* pointer = std::calloc(count, size);
    Require(pointer != nullptr);
    ++liveAllocations;
    return pointer;
}

void* APS5_VABI Reallocate_nid_no_patch(void* pointer, std::size_t size) {
    if (failReallocation) return nullptr;
    if (!pointer) return Allocate_nid_no_patch(size);
    void* result = std::realloc(pointer, size);
    Require(result != nullptr);
    return result;
}

void* APS5_VABI UnexpectedAlign_nid_no_patch(std::size_t, std::size_t) { throw std::runtime_error("unexpected aligned allocation"); }
void* APS5_VABI UnexpectedRealign_nid_no_patch(void*, std::size_t, std::size_t) { throw std::runtime_error("unexpected aligned reallocation"); }
int APS5_VABI UnexpectedPosixAlign_nid_no_patch(void**, std::size_t, std::size_t) { throw std::runtime_error("unexpected posix_memalign"); }

constexpr std::array<std::pair<const char*, std::uint64_t>, 5> Suites{{
    {"TLS_AES_128_GCM_SHA256", 0x03001301u},
    {"TLS_AES_256_GCM_SHA384", 0x03001302u},
    {"TLS_CHACHA20_POLY1305_SHA256", 0x03001303u},
    {"TLS_AES_128_CCM_SHA256", 0x03001304u},
    {"TLS_AES_128_CCM_8_SHA256", 0x03001305u}
}};

LibreSslStack* Stored(const LibreSsl3SslCtx& context) {
    return static_cast<LibreSslStack*>(context.cipherListTls13);
}

std::uint64_t StoredId(const LibreSslStack* stack, int index) {
    Require(stack != nullptr && index < stack->num);
    return static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(stack->data[index]));
}

void SingleSuites() {
    LibreSsl3SslCtx context{};
    const std::size_t before = liveAllocations;
    for (const auto& [name, id] : Suites) {
        Require(SSL_CTX_set_ciphersuites_nid_postfix(&context, name) == 1);
        Require(liveAllocations == before + 2);
        const auto* stack = Stored(context);
        Require(stack != nullptr && stack->num == 1);
        Require(StoredId(stack, 0) == id);
    }
    sk_free_nid_postfix(Stored(context));
    Require(liveAllocations == before);
}

void MixedList() {
    LibreSsl3SslCtx context{};
    const std::size_t before = liveAllocations;
    Require(SSL_CTX_set_ciphersuites_nid_postfix(&context,
        "TLS_CHACHA20_POLY1305_SHA256:TLS_AES_128_CCM_8_SHA256:TLS_AES_128_GCM_SHA256") == 1);
    const auto* stack = Stored(context);
    Require(stack != nullptr && stack->num == 3);
    Require(StoredId(stack, 0) == 0x03001303u);
    Require(StoredId(stack, 1) == 0x03001305u);
    Require(StoredId(stack, 2) == 0x03001301u);
    Require(SSL_CTX_set_ciphersuites_nid_postfix(&context,
        "TLS_AES_256_GCM_SHA384:TLS_AES_256_GCM_SHA384") == 1);
    stack = Stored(context);
    Require(stack != nullptr && stack->num == 2);
    Require(StoredId(stack, 0) == 0x03001302u);
    Require(StoredId(stack, 1) == 0x03001302u);
    Require(SSL_CTX_set_ciphersuites_nid_postfix(&context,
        "TLS_AES_128_GCM_SHA256:TLS_AES_256_GCM_SHA384:TLS_CHACHA20_POLY1305_SHA256:"
        "TLS_AES_128_CCM_SHA256:TLS_AES_128_CCM_8_SHA256") == 1);
    stack = Stored(context);
    Require(stack != nullptr && stack->num == 5);
    for (int index = 0; index < stack->num; ++index) Require(StoredId(stack, index) == Suites[index].second);
    Require(liveAllocations == before + 2);
    sk_free_nid_postfix(Stored(context));
    Require(liveAllocations == before);
}

void UnmatchedLists() {
    LibreSsl3SslCtx context{};
    const std::size_t before = liveAllocations;
    Require(SSL_CTX_set_ciphersuites_nid_postfix(&context, "TLS_AES_128_GCM_SHA256") == 1);
    const auto* stack = Stored(context);
    Require(stack != nullptr && stack->num == 1 && StoredId(stack, 0) == 0x03001301u);
    for (const char* list : {":", "::", "TLS_AES_128_SHA256", "TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256",
                             "junk", "TLS_AES_128_GCM_SHA256:junk", "junk:TLS_AES_128_GCM_SHA256",
                             "TLS_AES_128_GCM_SHA256_", "tls_aes_128_gcm_sha256", " TLS_AES_128_GCM_SHA256",
                             "TLS_AES_128_GCM_SHA256 ", "TLS_AES_128_GCM_SHA256;TLS_AES_256_GCM_SHA384"}) {
        Require(SSL_CTX_set_ciphersuites_nid_postfix(&context, list) == 0);
        Require(Stored(context) == stack && stack->num == 1 && StoredId(stack, 0) == 0x03001301u);
        Require(liveAllocations == before + 2);
    }
    sk_free_nid_postfix(Stored(context));
    Require(liveAllocations == before);
}

void EmptyList() {
    LibreSsl3SslCtx context{};
    Require(SSL_CTX_set_ciphersuites_nid_postfix(&context, "TLS_AES_128_GCM_SHA256") == 1);
    const std::size_t before = liveAllocations;
    Require(SSL_CTX_set_ciphersuites_nid_postfix(&context, "") == 1);
    const auto* stack = Stored(context);
    Require(stack != nullptr && stack->num == 0);
    Require(liveAllocations == before);
    sk_free_nid_postfix(Stored(context));
    Require(liveAllocations == 0);
}

void AliasNames() {
    LibreSsl3SslCtx context{};
    const std::size_t before = liveAllocations;
    constexpr std::array<std::pair<const char*, std::uint64_t>, 5> aliases{{
        {"AEAD-AES128-GCM-SHA256", 0x03001301u},
        {"AEAD-AES256-GCM-SHA384", 0x03001302u},
        {"AEAD-CHACHA20-POLY1305-SHA256", 0x03001303u},
        {"AEAD-AES128-CCM-SHA256", 0x03001304u},
        {"AEAD-AES128-CCM-8-SHA256", 0x03001305u}
    }};
    for (const auto& [name, id] : aliases) {
        Require(SSL_CTX_set_ciphersuites_nid_postfix(&context, name) == 1);
        const auto* stack = Stored(context);
        Require(stack != nullptr && stack->num == 1);
        Require(StoredId(stack, 0) == id);
    }
    Require(liveAllocations == before + 2);
    sk_free_nid_postfix(Stored(context));
    Require(liveAllocations == 0);
}

void NullArguments() {
    LibreSsl3SslCtx context{};
    Require(SSL_CTX_set_ciphersuites_nid_postfix(nullptr, "TLS_AES_128_GCM_SHA256") == 0);
    Require(SSL_CTX_set_ciphersuites_nid_postfix(&context, nullptr) == 0);
    Require(SSL_CTX_set_ciphersuites_nid_postfix(nullptr, nullptr) == 0);
    Require(context.cipherListTls13 == nullptr);
    Require(liveAllocations == 0);
}

void ReallocationFailure() {
    LibreSsl3SslCtx context{};
    Require(SSL_CTX_set_ciphersuites_nid_postfix(&context, "TLS_AES_128_GCM_SHA256") == 1);
    auto* stack = Stored(context);
    const std::size_t before = liveAllocations;
    failReallocation = true;
    Require(SSL_CTX_set_ciphersuites_nid_postfix(&context,
        "TLS_AES_128_GCM_SHA256:TLS_AES_256_GCM_SHA384:TLS_CHACHA20_POLY1305_SHA256:"
        "TLS_AES_128_CCM_SHA256:TLS_AES_128_CCM_8_SHA256") == 0);
    failReallocation = false;
    Require(Stored(context) == stack && stack->num == 1 && StoredId(stack, 0) == 0x03001301u);
    Require(liveAllocations == before);
    sk_free_nid_postfix(Stored(context));
    Require(liveAllocations == 0);
}

class ReadOnlyPage {
public:
    explicit ReadOnlyPage(const void* initial, std::size_t size) {
#ifdef _WIN32
        SYSTEM_INFO info{};
        GetSystemInfo(&info);
        _size = info.dwPageSize != 0 ? info.dwPageSize : 4096u;
        _page = VirtualAlloc(nullptr, _size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
        Require(_page != nullptr);
        if (initial && size > 0) std::memcpy(_page, initial, size);
        DWORD oldProtect = 0;
        Require(VirtualProtect(_page, _size, PAGE_READONLY, &oldProtect) != 0);
#else
        const long pageSize = sysconf(_SC_PAGESIZE);
        _size = pageSize > 0 ? static_cast<std::size_t>(pageSize) : 4096u;
        _page = mmap(nullptr, _size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        Require(_page != MAP_FAILED);
        if (initial && size > 0) std::memcpy(_page, initial, size);
        Require(mprotect(_page, _size, PROT_READ) == 0);
#endif
    }

    ~ReadOnlyPage() {
        if (!_page) return;
#ifdef _WIN32
        VirtualFree(_page, 0, MEM_RELEASE);
#else
        munmap(_page, _size);
#endif
    }

    ReadOnlyPage(const ReadOnlyPage&) = delete;
    ReadOnlyPage& operator=(const ReadOnlyPage&) = delete;

    template<typename T> T* As() const { return static_cast<T*>(_page); }

private:
    void* _page = nullptr;
    std::size_t _size = 0;
};

void UnreadableList() {
    LibreSsl3SslCtx context{};
    errno = 71;
    bool thrown = false;
    try {
        SSL_CTX_set_ciphersuites_nid_postfix(&context, reinterpret_cast<const char*>(0x10));
    } catch (const std::runtime_error&) {
        thrown = errno == 71;
    }
    Require(thrown);
    Require(context.cipherListTls13 == nullptr);
    Require(liveAllocations == 0);
}

void UnreadableAndReadOnlyContext() {
    errno = 71;
    bool thrown = false;
    try {
        SSL_CTX_set_ciphersuites_nid_postfix(reinterpret_cast<LibreSsl3SslCtx*>(std::uintptr_t{1}), "TLS_AES_128_GCM_SHA256");
    } catch (const std::runtime_error&) {
        thrown = errno == 71;
    }
    Require(thrown);
    Require(liveAllocations == 0);

    LibreSsl3SslCtx sentinelPrototype{};
    std::memset(&sentinelPrototype, 0xa5, sizeof(sentinelPrototype));
    const ReadOnlyPage sentinelPage(&sentinelPrototype, sizeof(sentinelPrototype));
    errno = 71;
    thrown = false;
    try {
        SSL_CTX_set_ciphersuites_nid_postfix(sentinelPage.As<LibreSsl3SslCtx>(), "TLS_AES_128_GCM_SHA256");
    } catch (const std::runtime_error&) {
        thrown = errno == 71;
    }
    Require(thrown);
    Require(std::memcmp(sentinelPage.As<void>(), &sentinelPrototype, sizeof(sentinelPrototype)) == 0);
    Require(liveAllocations == 0);

    auto* const existingStack = sk_new_nid_postfix(nullptr);
    Require(existingStack != nullptr);
    const std::size_t before = liveAllocations;
    LibreSsl3SslCtx ownedPrototype{};
    ownedPrototype.cipherListTls13 = existingStack;
    const ReadOnlyPage ownedPage(&ownedPrototype, sizeof(ownedPrototype));
    errno = 71;
    thrown = false;
    try {
        SSL_CTX_set_ciphersuites_nid_postfix(ownedPage.As<LibreSsl3SslCtx>(), "TLS_AES_128_GCM_SHA256");
    } catch (const std::runtime_error&) {
        thrown = errno == 71;
    }
    Require(thrown);
    Require(liveAllocations == before);
    Require(ownedPage.As<LibreSsl3SslCtx>()->cipherListTls13 == existingStack);
    sk_free_nid_postfix(existingStack);
    Require(liveAllocations == 0);
}

}

int main() {
    std::array<void*, 10> api{
        reinterpret_cast<void*>(Allocate_nid_no_patch), reinterpret_cast<void*>(Free_nid_no_patch),
        reinterpret_cast<void*>(Callocate_nid_no_patch), reinterpret_cast<void*>(Reallocate_nid_no_patch),
        reinterpret_cast<void*>(UnexpectedAlign_nid_no_patch), reinterpret_cast<void*>(UnexpectedRealign_nid_no_patch),
        reinterpret_cast<void*>(UnexpectedPosixAlign_nid_no_patch)};
    ApplicationHeapRegister_nid_no_patch(api.data());
    SingleSuites();
    MixedList();
    UnmatchedLists();
    EmptyList();
    AliasNames();
    NullArguments();
    ReallocationFailure();
    UnreadableList();
    UnreadableAndReadOnlyContext();
    Require(liveAllocations == 0);
    std::puts("libSceLibreSsl3 SSL_CTX_set_ciphersuites tests passed");
}
