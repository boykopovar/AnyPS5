#include "prx/libSceLibreSsl/include/Api.hpp"
#include "prx/libSceLibreSsl/include/Evp.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <source_location>
#include <stdexcept>
#include <vector>
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

using Bytes = std::vector<unsigned char>;
using CipherFactory = const LibreSslEvpCipher* (APS5_VABI*)();

constexpr int GcmGetTag = 0x10;
constexpr int GcmSetTag = 0x11;
constexpr int GcmSetIvFixed = 0x12;
constexpr int TlsAad = 0x16;

static void* APS5_VABI GuestHeapAllocate(std::size_t size) { return std::malloc(size); }
static void APS5_VABI GuestHeapFree(void* pointer) { std::free(pointer); }
static void* APS5_VABI GuestHeapCalloc(std::size_t count, std::size_t size) { return std::calloc(count, size); }
static void* APS5_VABI GuestHeapReallocate(void* pointer, std::size_t size) { return std::realloc(pointer, size); }
static void* APS5_VABI GuestHeapAlign(std::size_t, std::size_t) { throw std::logic_error("unexpected aligned allocation"); }
static void* APS5_VABI GuestHeapRealign(void*, std::size_t, std::size_t) { throw std::logic_error("unexpected aligned reallocation"); }
static int APS5_VABI GuestHeapPosixAlign(void**, std::size_t, std::size_t) { throw std::logic_error("unexpected aligned allocation"); }

static void InitializeHeap() {
    void* const api[10] {reinterpret_cast<void*>(GuestHeapAllocate), reinterpret_cast<void*>(GuestHeapFree), reinterpret_cast<void*>(GuestHeapCalloc),
        reinterpret_cast<void*>(GuestHeapReallocate), reinterpret_cast<void*>(GuestHeapAlign), reinterpret_cast<void*>(GuestHeapRealign),
        reinterpret_cast<void*>(GuestHeapPosixAlign)};
    ApplicationHeapRegister_nid_no_patch(api);
}

static void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "LibreSSL EVP AES guest pointer check failed at line %u\n", location.line());
    std::abort();
}

template<typename TAction> static void RequireRejected(TAction action, std::source_location location = std::source_location::current()) {
    errno = 71;
    bool rejected = false;
    try {
        action();
    } catch (const std::runtime_error&) {
        rejected = errno == 71;
    }
    Require(rejected, location);
}

class GuardedPage {
public:
    GuardedPage() {
#ifdef _WIN32
        SYSTEM_INFO info{};
        GetSystemInfo(&info);
        bytes = info.dwPageSize;
        data = static_cast<unsigned char*>(VirtualAlloc(nullptr, bytes * 2, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
        Require(data != nullptr);
        DWORD previous{};
        Require(VirtualProtect(data + bytes, bytes, PAGE_NOACCESS, &previous) != 0);
#else
        bytes = static_cast<std::size_t>(sysconf(_SC_PAGESIZE));
        void* mapped = mmap(nullptr, bytes * 2, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        Require(mapped != MAP_FAILED);
        data = static_cast<unsigned char*>(mapped);
        Require(mprotect(data + bytes, bytes, PROT_NONE) == 0);
#endif
    }

    ~GuardedPage() {
#ifdef _WIN32
        VirtualFree(data, 0, MEM_RELEASE);
#else
        munmap(data, bytes * 2);
#endif
    }

    GuardedPage(const GuardedPage&) = delete;
    GuardedPage& operator=(const GuardedPage&) = delete;

    unsigned char* Tail(const unsigned char* value, std::size_t size) {
        std::memcpy(data + bytes - size, value, size);
        return data + bytes - size;
    }

    unsigned char* Tail(std::size_t size) {
        std::memset(data + bytes - size, 0x5a, size);
        return data + bytes - size;
    }

private:
    unsigned char* data = nullptr;
    std::size_t bytes = 0;
};

class Context {
public:
    Context() : ctx(EVP_CIPHER_CTX_new_nid_postfix()) { Require(ctx != nullptr); }
    ~Context() { EVP_CIPHER_CTX_free_nid_postfix(ctx); }
    Context(const Context&) = delete;
    Context& operator=(const Context&) = delete;
    LibreSslEvpCipherCtx* get() const { return ctx; }

private:
    LibreSslEvpCipherCtx* ctx;
};

static Bytes Pattern(std::size_t size, unsigned char seed) {
    Bytes bytes(size);
    for (std::size_t index = 0; index < size; ++index) bytes[index] = static_cast<unsigned char>(seed + index * 7);
    return bytes;
}

static unsigned char* const Unmapped = reinterpret_cast<unsigned char*>(0x10);

static Bytes Encrypt(CipherFactory factory, const Bytes& key, const Bytes& iv, const Bytes& plain) {
    Context context;
    Require(EVP_EncryptInit_ex_nid_postfix(context.get(), factory(), nullptr, key.data(), iv.data()) == 1);
    Bytes output(plain.size());
    int written = -1;
    Require(EVP_EncryptUpdate_nid_postfix(context.get(), output.data(), &written, plain.data(), static_cast<int>(plain.size())) == 1);
    Require(written == static_cast<int>(plain.size()));
    return output;
}

static void TestStreamCiphers() {
    const auto key = Pattern(32, 1);
    const auto iv = Pattern(16, 2);
    const auto plain = Pattern(40, 3);
    for (const CipherFactory factory : {EVP_aes_128_cfb8_nid_postfix, EVP_aes_256_cfb8_nid_postfix, EVP_aes_128_ctr_nid_postfix, EVP_aes_256_ctr_nid_postfix}) {
        const auto keyLength = static_cast<std::size_t>(factory()->keyLength);
        const Bytes cipherKey(key.begin(), key.begin() + static_cast<std::ptrdiff_t>(keyLength));
        const auto expected = Encrypt(factory, cipherKey, iv, plain);
        GuardedPage page;
        {
            Context context;
            RequireRejected([&] { EVP_EncryptInit_ex_nid_postfix(context.get(), factory(), nullptr, Unmapped, iv.data()); });
        }
        {
            Context context;
            RequireRejected([&] { EVP_EncryptInit_ex_nid_postfix(context.get(), factory(), nullptr, page.Tail(cipherKey.data(), keyLength - 1), iv.data()); });
        }
        Context context;
        Require(EVP_EncryptInit_ex_nid_postfix(context.get(), factory(), nullptr, page.Tail(cipherKey.data(), keyLength), iv.data()) == 1);
        Bytes output(plain.size());
        int written = -1;
        RequireRejected([&] { EVP_EncryptUpdate_nid_postfix(context.get(), output.data(), &written, Unmapped, 8); });
        RequireRejected([&] { EVP_EncryptUpdate_nid_postfix(context.get(), Unmapped, &written, plain.data(), 8); });
        Require(EVP_EncryptInit_ex_nid_postfix(context.get(), nullptr, nullptr, nullptr, iv.data()) == 1);
        auto* input = page.Tail(plain.data(), plain.size());
        Require(EVP_EncryptUpdate_nid_postfix(context.get(), output.data(), &written, input, static_cast<int>(plain.size())) == 1);
        Require(written == static_cast<int>(plain.size()) && output == expected);
        Require(EVP_EncryptInit_ex_nid_postfix(context.get(), nullptr, nullptr, nullptr, iv.data()) == 1);
        auto* tail = page.Tail(plain.size());
        Require(EVP_EncryptUpdate_nid_postfix(context.get(), tail, &written, plain.data(), static_cast<int>(plain.size())) == 1);
        Require(Bytes(tail, tail + plain.size()) == expected);
        RequireRejected([&] { EVP_EncryptUpdate_nid_postfix(context.get(), page.Tail(8), &written, plain.data(), 9); });
    }
}

static void TestGcm() {
    const auto key = Pattern(16, 4);
    const auto iv = Pattern(12, 5);
    const auto plain = Pattern(37, 6);
    const auto aad = Pattern(20, 7);
    Bytes expected(plain.size());
    unsigned char expectedTag[16] {};
    int written = -1;
    {
        Context context;
        Require(EVP_EncryptInit_ex_nid_postfix(context.get(), EVP_aes_128_gcm_nid_postfix(), nullptr, key.data(), iv.data()) == 1);
        Require(EVP_EncryptUpdate_nid_postfix(context.get(), nullptr, &written, aad.data(), static_cast<int>(aad.size())) == 1);
        Require(EVP_EncryptUpdate_nid_postfix(context.get(), expected.data(), &written, plain.data(), static_cast<int>(plain.size())) == 1);
        Require(EVP_EncryptFinal_ex_nid_postfix(context.get(), expected.data(), &written) == 1);
        Require(EVP_CIPHER_CTX_ctrl_nid_postfix(context.get(), GcmGetTag, 16, expectedTag) == 1);
    }
    GuardedPage page;
    {
        Context context;
        RequireRejected([&] { EVP_EncryptInit_ex_nid_postfix(context.get(), EVP_aes_128_gcm_nid_postfix(), nullptr, Unmapped, iv.data()); });
    }
    {
        Context context;
        RequireRejected([&] { EVP_EncryptInit_ex_nid_postfix(context.get(), EVP_aes_128_gcm_nid_postfix(), nullptr, key.data(), Unmapped); });
    }
    {
        Context context;
        RequireRejected([&] { EVP_EncryptInit_ex_nid_postfix(context.get(), EVP_aes_128_gcm_nid_postfix(), nullptr, key.data(), page.Tail(iv.data(), 11)); });
    }
    Context context;
    Require(EVP_EncryptInit_ex_nid_postfix(context.get(), EVP_aes_128_gcm_nid_postfix(), nullptr, page.Tail(key.data(), key.size()), nullptr) == 1);
    Require(EVP_EncryptInit_ex_nid_postfix(context.get(), nullptr, nullptr, nullptr, page.Tail(iv.data(), iv.size())) == 1);
    RequireRejected([&] { EVP_EncryptUpdate_nid_postfix(context.get(), nullptr, &written, Unmapped, 4); });
    Require(EVP_EncryptUpdate_nid_postfix(context.get(), nullptr, &written, page.Tail(aad.data(), aad.size()), static_cast<int>(aad.size())) == 1);
    Bytes output(plain.size());
    Require(EVP_EncryptUpdate_nid_postfix(context.get(), output.data(), &written, page.Tail(plain.data(), plain.size()), static_cast<int>(plain.size())) == 1);
    Require(written == static_cast<int>(plain.size()) && output == expected);
    Require(EVP_EncryptFinal_ex_nid_postfix(context.get(), output.data(), &written) == 1);
    RequireRejected([&] { EVP_CIPHER_CTX_ctrl_nid_postfix(context.get(), GcmGetTag, 16, Unmapped); });
    auto* tag = page.Tail(16);
    Require(EVP_CIPHER_CTX_ctrl_nid_postfix(context.get(), GcmGetTag, 16, tag) == 1 && std::memcmp(tag, expectedTag, 16) == 0);
    RequireRejected([&] { EVP_CIPHER_CTX_ctrl_nid_postfix(context.get(), GcmGetTag, 16, page.Tail(15)); });
    RequireRejected([&] { EVP_CIPHER_CTX_ctrl_nid_postfix(context.get(), GcmSetIvFixed, 4, Unmapped); });
    RequireRejected([&] { EVP_CIPHER_CTX_ctrl_nid_postfix(context.get(), TlsAad, 13, Unmapped); });
    Require(EVP_EncryptInit_ex_nid_postfix(context.get(), nullptr, nullptr, nullptr, iv.data()) == 1);
    RequireRejected([&] { EVP_EncryptUpdate_nid_postfix(context.get(), Unmapped, &written, plain.data(), 4); });
    RequireRejected([&] { EVP_EncryptUpdate_nid_postfix(context.get(), page.Tail(4), &written, plain.data(), 5); });

    Context decrypt;
    Require(EVP_DecryptInit_ex_nid_postfix(decrypt.get(), EVP_aes_128_gcm_nid_postfix(), nullptr, key.data(), iv.data()) == 1);
    RequireRejected([&] { EVP_CIPHER_CTX_ctrl_nid_postfix(decrypt.get(), GcmSetTag, 16, Unmapped); });
    Require(EVP_CIPHER_CTX_ctrl_nid_postfix(decrypt.get(), GcmSetTag, 16, page.Tail(expectedTag, 16)) == 1);
    Require(EVP_DecryptUpdate_nid_postfix(decrypt.get(), nullptr, &written, aad.data(), static_cast<int>(aad.size())) == 1);
    auto* plainOut = page.Tail(plain.size());
    Require(EVP_DecryptUpdate_nid_postfix(decrypt.get(), plainOut, &written, expected.data(), static_cast<int>(expected.size())) == 1);
    Require(Bytes(plainOut, plainOut + plain.size()) == plain);
    Require(EVP_DecryptFinal_ex_nid_postfix(decrypt.get(), output.data(), &written) == 1);

    Context tls;
    const auto fixed = Pattern(4, 8);
    Require(EVP_EncryptInit_ex_nid_postfix(tls.get(), EVP_aes_128_gcm_nid_postfix(), nullptr, key.data(), nullptr) == 1);
    Require(EVP_CIPHER_CTX_ctrl_nid_postfix(tls.get(), GcmSetIvFixed, 4, const_cast<unsigned char*>(fixed.data())) == 1);
    Bytes header {0, 0, 0, 0, 0, 0, 0, 1, 23, 3, 3, 0, 45};
    Require(EVP_CIPHER_CTX_ctrl_nid_postfix(tls.get(), TlsAad, 13, header.data()) == 16);
    RequireRejected([&] { EVP_EncryptUpdate_nid_postfix(tls.get(), Unmapped, &written, Unmapped, 45); });
    Require(EVP_CIPHER_CTX_ctrl_nid_postfix(tls.get(), TlsAad, 13, header.data()) == 16);
    auto* record = page.Tail(45);
    Require(EVP_EncryptUpdate_nid_postfix(tls.get(), record, &written, record, 45) == 1 && written == 45);
}

static void TestWrap() {
    const auto kek = Pattern(16, 9);
    const auto key = Pattern(16, 10);
    Bytes expected(24);
    int written = -1;
    {
        Context context;
        context.get()->flags = CipherCtxFlagWrapAllow;
        Require(EVP_EncryptInit_ex_nid_postfix(context.get(), EVP_aes_128_wrap_nid_postfix(), nullptr, kek.data(), nullptr) == 1);
        Require(EVP_EncryptUpdate_nid_postfix(context.get(), expected.data(), &written, key.data(), 16) == 1 && written == 24);
    }
    GuardedPage page;
    {
        Context context;
        context.get()->flags = CipherCtxFlagWrapAllow;
        RequireRejected([&] { EVP_EncryptInit_ex_nid_postfix(context.get(), EVP_aes_128_wrap_nid_postfix(), nullptr, Unmapped, nullptr); });
    }
    {
        Context context;
        context.get()->flags = CipherCtxFlagWrapAllow;
        RequireRejected([&] { EVP_EncryptInit_ex_nid_postfix(context.get(), EVP_aes_128_wrap_nid_postfix(), nullptr, kek.data(), Unmapped); });
    }
    Context context;
    context.get()->flags = CipherCtxFlagWrapAllow;
    Require(EVP_EncryptInit_ex_nid_postfix(context.get(), EVP_aes_128_wrap_nid_postfix(), nullptr, page.Tail(kek.data(), kek.size()), nullptr) == 1);
    Bytes output(24);
    RequireRejected([&] { EVP_EncryptUpdate_nid_postfix(context.get(), output.data(), &written, Unmapped, 16); });
    RequireRejected([&] { EVP_EncryptUpdate_nid_postfix(context.get(), output.data(), &written, page.Tail(key.data(), 15), 16); });
    RequireRejected([&] { EVP_EncryptUpdate_nid_postfix(context.get(), Unmapped, &written, key.data(), 16); });
    RequireRejected([&] { EVP_EncryptUpdate_nid_postfix(context.get(), page.Tail(23), &written, key.data(), 16); });
    Require(EVP_EncryptUpdate_nid_postfix(context.get(), output.data(), &written, page.Tail(key.data(), key.size()), 16) == 1);
    Require(written == 24 && output == expected);
    auto* wrapped = page.Tail(24);
    Require(EVP_EncryptUpdate_nid_postfix(context.get(), wrapped, &written, key.data(), 16) == 1);
    Require(Bytes(wrapped, wrapped + 24) == expected);

    Context decrypt;
    decrypt.get()->flags = CipherCtxFlagWrapAllow;
    Require(EVP_DecryptInit_ex_nid_postfix(decrypt.get(), EVP_aes_128_wrap_nid_postfix(), nullptr, kek.data(), nullptr) == 1);
    auto* unwrapped = page.Tail(16);
    Require(EVP_DecryptUpdate_nid_postfix(decrypt.get(), unwrapped, &written, expected.data(), 24) == 1 && written == 16);
    Require(Bytes(unwrapped, unwrapped + 16) == key);
    auto tampered = expected;
    tampered[3] ^= 1;
    unwrapped = page.Tail(16);
    Require(EVP_DecryptUpdate_nid_postfix(decrypt.get(), unwrapped, &written, tampered.data(), 24) == 0);
    Require(Bytes(unwrapped, unwrapped + 16) == Bytes(16, 0));
}

int main() {
    InitializeHeap();
    TestStreamCiphers();
    TestGcm();
    TestWrap();
    std::puts("libSceLibreSsl EVP AES guest pointer tests passed");
}
