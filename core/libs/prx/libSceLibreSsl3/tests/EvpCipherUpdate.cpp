#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include "prx/libSceLibreSsl/include/Evp.hpp"

#include <algorithm>
#include <array>
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

namespace {

constexpr std::uint64_t CipherModeCcm = 0x7;
constexpr std::uint64_t BadBlockLengthError = 0x06fff000 | 136;
constexpr unsigned char XorByte = 0x5a;

struct Record {
    int calls = 0;
    const unsigned char* in = nullptr;
    std::size_t length = 0;
    int result = 1;
};

Record record;

int APS5_VABI BlockCipher(LibreSslEvpCipherCtx*, unsigned char* out, const unsigned char* in, std::size_t length) {
    ++record.calls;
    record.in = in;
    record.length = length;
    if (record.result <= 0) return record.result;
    for (std::size_t index = 0; index < length; ++index) out[index] = static_cast<unsigned char>(in[index] ^ XorByte);
    return 1;
}

int APS5_VABI CustomCipher(LibreSslEvpCipherCtx*, unsigned char*, const unsigned char* in, std::size_t length) {
    ++record.calls;
    record.in = in;
    record.length = length;
    return record.result < 0 ? record.result : static_cast<int>(length);
}

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "LibreSSL3 EVP_*Update check failed at line %u\n", location.line());
    std::abort();
}

void Reset(int result = 1) {
    record = {};
    record.result = result;
}

LibreSslEvpCipher Cipher(int blockSize, std::uint64_t flags, decltype(LibreSslEvpCipher::doCipher) doCipher) {
    LibreSslEvpCipher cipher{};
    cipher.blockSize = blockSize;
    cipher.flags = flags;
    cipher.doCipher = doCipher;
    return cipher;
}

LibreSslEvpCipherCtx Context(const LibreSslEvpCipher& cipher, int encrypt) {
    LibreSslEvpCipherCtx context{};
    context.cipher = &cipher;
    context.encrypt = encrypt;
    return context;
}

std::vector<unsigned char> Plain(std::size_t size) {
    std::vector<unsigned char> data(size);
    for (std::size_t index = 0; index < size; ++index) data[index] = static_cast<unsigned char>(index * 7 + 3);
    return data;
}

void CheckCustomCipher() {
    const auto custom = Cipher(1, CipherFlagCustomCipher, CustomCipher);
    auto context = Context(custom, 1);
    unsigned char out[4]{};
    int written = 77;
    Reset();
    Require(EVP_EncryptUpdate_nid_disambig1_nid_postfix(&context, out, &written, nullptr, 0) == 1 && written == 0 && record.calls == 0);
    written = 77;
    Require(EVP_DecryptUpdate_nid_disambig1_nid_postfix(&context, nullptr, &written, nullptr, 0) == 1 && written == 0 && record.calls == 0);
    written = 77;
    Require(EVP_EncryptUpdate_nid_disambig1_nid_postfix(&context, out, &written, out, -1) == 0 && written == 0 && record.calls == 0);
    written = 77;
    Require(EVP_DecryptUpdate_nid_disambig1_nid_postfix(&context, out, &written, out, -1) == 0 && written == 0 && record.calls == 0);
    Require(EVP_EncryptUpdate_nid_disambig1_nid_postfix(&context, out, &written, out, 3) == 1 && written == 3 && record.calls == 1 && record.in == out && record.length == 3);
    Reset(-1);
    written = 77;
    Require(EVP_DecryptUpdate_nid_disambig1_nid_postfix(&context, out, &written, out, 3) == 0 && written == 0 && record.calls == 1);

    const auto ccm = Cipher(1, CipherFlagCustomCipher | CipherModeCcm, CustomCipher);
    auto ccmContext = Context(ccm, 0);
    Reset();
    written = 77;
    Require(EVP_DecryptUpdate_nid_disambig1_nid_postfix(&ccmContext, nullptr, &written, nullptr, 0) == 1 && written == 0 && record.calls == 1 && !record.in && record.length == 0);
    written = 77;
    Require(EVP_EncryptUpdate_nid_disambig1_nid_postfix(&ccmContext, nullptr, &written, nullptr, 0) == 1 && written == 0 && record.calls == 2 && !record.in && record.length == 0);
}

void CheckEncryptBuffering() {
    const auto block = Cipher(16, CipherModeCbc, BlockCipher);
    auto context = Context(block, 1);
    const auto plain = Plain(37);
    std::array<unsigned char, 64> out{};
    int written = 77;
    Reset();
    Require(EVP_EncryptUpdate_nid_disambig1_nid_postfix(&context, out.data(), &written, plain.data(), 0) == 1 && written == 0 && context.bufLen == 0);
    Require(EVP_EncryptUpdate_nid_disambig1_nid_postfix(&context, out.data(), &written, plain.data(), 5) == 1 && written == 0 && context.bufLen == 5 && record.calls == 0);
    Require(EVP_EncryptUpdate_nid_disambig1_nid_postfix(&context, out.data(), &written, plain.data() + 5, 3) == 1 && written == 0 && context.bufLen == 8 && record.calls == 0);
    Require(EVP_EncryptUpdate_nid_disambig1_nid_postfix(&context, out.data(), &written, plain.data() + 8, 24) == 1 && written == 32 && context.bufLen == 0 && record.calls == 2);
    Require(EVP_CipherUpdate_nid_disambig1_nid_postfix(&context, out.data() + 32, &written, plain.data() + 32, 5) == 1 && written == 0 && context.bufLen == 5);
    for (std::size_t index = 0; index < 32; ++index) Require(out[index] == static_cast<unsigned char>(plain[index] ^ XorByte));

    Reset(0);
    written = 77;
    Require(EVP_EncryptUpdate_nid_disambig1_nid_postfix(&context, out.data(), &written, plain.data(), 16) == 0 && written == 0 && record.calls == 1 && context.bufLen == 5);
}

void CheckDecryptHeldBlock() {
    const auto block = Cipher(16, CipherModeCbc, BlockCipher);
    auto context = Context(block, 0);
    const auto plain = Plain(48);
    std::array<unsigned char, 64> out{};
    int written = 77;
    Reset();
    Require(EVP_CipherUpdate_nid_disambig1_nid_postfix(&context, out.data(), &written, plain.data(), 32) == 1 && written == 16 && context.finalUsed == 1);
    Require(EVP_DecryptUpdate_nid_disambig1_nid_postfix(&context, out.data() + 16, &written, plain.data() + 32, 16) == 1 && written == 16 && context.finalUsed == 1);
    for (std::size_t index = 0; index < 32; ++index) Require(out[index] == static_cast<unsigned char>(plain[index] ^ XorByte));
    for (std::size_t index = 0; index < 16; ++index) Require(context.final[index] == static_cast<unsigned char>(plain[32 + index] ^ XorByte));

    Reset(0);
    written = 77;
    Require(EVP_DecryptUpdate_nid_disambig1_nid_postfix(&context, out.data(), &written, plain.data(), 16) == 0 && written == 0 && context.finalUsed == 0);

    auto unpadded = Context(block, 0);
    unpadded.flags = CipherFlagNoPadding;
    Reset();
    Require(EVP_DecryptUpdate_nid_disambig1_nid_postfix(&unpadded, out.data(), &written, plain.data(), 32) == 1 && written == 32 && unpadded.finalUsed == 0);
}

void CheckInvalidState() {
    const auto odd = Cipher(12, CipherModeCbc, BlockCipher);
    auto oddContext = Context(odd, 1);
    const auto plain = Plain(16);
    std::array<unsigned char, 32> out{};
    int written = 77;
    Reset();
    Require(EVP_EncryptUpdate_nid_disambig1_nid_postfix(&oddContext, out.data(), &written, plain.data(), 12) == 0 && written == 0 && record.calls == 0);

    const auto block = Cipher(16, CipherModeCbc, BlockCipher);
    auto corrupt = Context(block, 1);
    corrupt.bufLen = 16;
    ERR_clear_error_nid_postfix();
    written = 77;
    Require(EVP_EncryptUpdate_nid_disambig1_nid_postfix(&corrupt, out.data(), &written, plain.data(), 3) == 0 && written == 0 && record.calls == 0);
    Require(ERR_get_error_nid_postfix() == BadBlockLengthError && ERR_get_error_nid_postfix() == 0);

    bool threw = false;
    try {
        EVP_EncryptUpdate_nid_disambig1_nid_postfix(nullptr, out.data(), &written, plain.data(), 1);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);
    threw = false;
    auto context = Context(block, 1);
    try {
        EVP_DecryptUpdate_nid_disambig1_nid_postfix(&context, out.data(), nullptr, plain.data(), 1);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);
}

void CheckGuestPointerValidation() {
    struct ProtectedPages {
        std::size_t pageSize = 0;
        std::uint8_t* base = nullptr;
        ProtectedPages() {
#ifdef _WIN32
            SYSTEM_INFO info{};
            GetSystemInfo(&info);
            pageSize = info.dwPageSize;
            base = static_cast<std::uint8_t*>(VirtualAlloc(nullptr, 2 * pageSize, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
            Require(base != nullptr);
#else
            const long size = sysconf(_SC_PAGESIZE);
            pageSize = size > 0 ? static_cast<std::size_t>(size) : 4096u;
            void* mapped = mmap(nullptr, 2 * pageSize, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
            Require(mapped != MAP_FAILED);
            base = static_cast<std::uint8_t*>(mapped);
#endif
        }
        ~ProtectedPages() {
            if (!base) return;
#ifdef _WIN32
            VirtualFree(base, 0, MEM_RELEASE);
#else
            munmap(base, 2 * pageSize);
#endif
        }
        void ProtectReadOnly() {
#ifdef _WIN32
            DWORD previous = 0;
            Require(VirtualProtect(base, pageSize, PAGE_READONLY, &previous) != 0);
            Require(VirtualProtect(base + pageSize, pageSize, PAGE_NOACCESS, &previous) != 0);
#else
            Require(mprotect(base, pageSize, PROT_READ) == 0);
            Require(mprotect(base + pageSize, pageSize, PROT_NONE) == 0);
#endif
        }
    } pages;

    const auto block = Cipher(16, CipherModeCbc, BlockCipher);
    const auto plain = Plain(16);
    auto validContext = Context(block, 1);

    std::memcpy(pages.base, &block, sizeof(block));
    std::memcpy(pages.base + 256, &validContext, sizeof(validContext));
    std::memcpy(pages.base + 512, plain.data(), plain.size());
    std::memset(pages.base + 768, 0xa5, 32);
    int roWritten = 0x5a5a5a5a;
    std::memcpy(pages.base + 1024, &roWritten, sizeof(roWritten));
    pages.ProtectReadOnly();

    const auto* roCipher = reinterpret_cast<const LibreSslEvpCipher*>(pages.base);
    auto* roContext = reinterpret_cast<LibreSslEvpCipherCtx*>(pages.base + 256);
    const auto* roIn = reinterpret_cast<const unsigned char*>(pages.base + 512);
    auto* roOut = reinterpret_cast<unsigned char*>(pages.base + 768);
    int* roWrittenPtr = reinterpret_cast<int*>(pages.base + 1024);
    auto* noAccess = pages.base + pages.pageSize;
    auto* unmapped = reinterpret_cast<void*>(std::uintptr_t{1});

    std::array<unsigned char, 32> out{};
    out.fill(0xa5);
    int written = 0x5a5a5a5a;

    auto ctxWithRoCipher = Context(*roCipher, 1);
    Reset();
    Require(EVP_CipherUpdate_nid_disambig1_nid_postfix(&ctxWithRoCipher, out.data(), &written, roIn, 16) == 1);
    Require(written == 16 && record.calls == 1);
    for (std::size_t i = 0; i < 16; ++i) Require(out[i] == static_cast<unsigned char>(plain[i] ^ XorByte));

    auto expectInvalid = [&](auto&& fn) {
        out.fill(0xa5);
        written = 0x5a5a5a5a;
        errno = E2BIG;
        bool threw = false;
        try {
            fn();
        } catch (const std::invalid_argument&) {
            threw = true;
        }
        Require(threw);
        Require(errno == E2BIG);
        Require(written == 0x5a5a5a5a);
        Require(std::all_of(out.begin(), out.end(), [](unsigned char b) { return b == 0xa5; }));
    };

    expectInvalid([&] { EVP_CipherUpdate_nid_disambig1_nid_postfix(reinterpret_cast<LibreSslEvpCipherCtx*>(unmapped), out.data(), &written, roIn, 16); });
    expectInvalid([&] { EVP_EncryptUpdate_nid_disambig1_nid_postfix(reinterpret_cast<LibreSslEvpCipherCtx*>(noAccess), out.data(), &written, roIn, 16); });
    expectInvalid([&] { EVP_DecryptUpdate_nid_disambig1_nid_postfix(roContext, out.data(), &written, roIn, 16); });

    auto badCipherCtx = Context(block, 1);
    badCipherCtx.cipher = reinterpret_cast<const LibreSslEvpCipher*>(unmapped);
    expectInvalid([&] { EVP_EncryptUpdate_nid_disambig1_nid_postfix(&badCipherCtx, out.data(), &written, roIn, 16); });
    badCipherCtx.cipher = reinterpret_cast<const LibreSslEvpCipher*>(noAccess);
    expectInvalid([&] { EVP_DecryptUpdate_nid_disambig1_nid_postfix(&badCipherCtx, out.data(), &written, roIn, 16); });

    auto ctx = Context(block, 1);
    expectInvalid([&] { EVP_EncryptUpdate_nid_disambig1_nid_postfix(&ctx, out.data(), reinterpret_cast<int*>(unmapped), roIn, 16); });
    expectInvalid([&] { EVP_DecryptUpdate_nid_disambig1_nid_postfix(&ctx, out.data(), reinterpret_cast<int*>(noAccess), roIn, 16); });
    expectInvalid([&] { EVP_CipherUpdate_nid_disambig1_nid_postfix(&ctx, out.data(), roWrittenPtr, roIn, 16); });

    expectInvalid([&] { EVP_EncryptUpdate_nid_disambig1_nid_postfix(&ctx, out.data(), &written, reinterpret_cast<const unsigned char*>(unmapped), 16); });
    expectInvalid([&] { EVP_DecryptUpdate_nid_disambig1_nid_postfix(&ctx, out.data(), &written, noAccess, 16); });

    expectInvalid([&] { EVP_EncryptUpdate_nid_disambig1_nid_postfix(&ctx, reinterpret_cast<unsigned char*>(unmapped), &written, roIn, 16); });
    expectInvalid([&] { EVP_DecryptUpdate_nid_disambig1_nid_postfix(&ctx, noAccess, &written, roIn, 16); });
    expectInvalid([&] { EVP_CipherUpdate_nid_disambig1_nid_postfix(&ctx, roOut, &written, roIn, 16); });
}

}

int main() {
    CheckCustomCipher();
    CheckEncryptBuffering();
    CheckDecryptHeldBlock();
    CheckInvalidState();
    CheckGuestPointerValidation();
    std::puts("LibreSSL3 EVP_*Update: ok");
}
