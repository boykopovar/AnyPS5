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
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace {

constexpr std::uint64_t BadDecryptError = 0x06fff000 | 100;
constexpr std::uint64_t WrongFinalBlockLengthError = 0x06fff000 | 109;
constexpr std::uint64_t BadBlockLengthError = 0x06fff000 | 136;
constexpr std::uint64_t NotMultipleError = 0x06fff000 | 138;
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
    return record.result < 0 ? record.result : 7;
}

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "LibreSSL3 EVP_*Final_ex check failed at line %u\n", location.line());
    std::abort();
}

void Reset(int result = 1) {
    record = {};
    record.result = result;
    ERR_clear_error_nid_postfix();
}

void RequireQueue(std::uint64_t code) {
    Require(ERR_get_error_nid_postfix() == code && ERR_get_error_nid_postfix() == 0);
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

void CheckCustomCipher() {
    const auto custom = Cipher(1, CipherFlagCustomCipher, CustomCipher);
    auto context = Context(custom, 1);
    unsigned char out[8]{};
    int written = 77;
    Reset();
    Require(EVP_EncryptFinal_ex_nid_disambig1_nid_postfix(&context, out, &written) == 1 && written == 7 && record.calls == 1 && !record.in && record.length == 0);
    Reset(-1);
    written = 77;
    Require(EVP_DecryptFinal_ex_nid_disambig1_nid_postfix(&context, out, &written) == 0 && written == 0 && record.calls == 1);
}

void CheckEncrypt() {
    const auto block = Cipher(16, CipherModeCbc, BlockCipher);
    auto context = Context(block, 1);
    std::array<unsigned char, 16> out{};
    int written = 77;
    for (int index = 0; index < 5; ++index) context.buf[index] = static_cast<unsigned char>(index + 1);
    context.bufLen = 5;
    Reset();
    Require(EVP_CipherFinal_ex_nid_disambig1_nid_postfix(&context, out.data(), &written) == 1 && written == 16 && record.calls == 1 && record.length == 16);
    for (int index = 0; index < 16; ++index) {
        const unsigned char plain = index < 5 ? static_cast<unsigned char>(index + 1) : 11;
        Require(out[static_cast<std::size_t>(index)] == static_cast<unsigned char>(plain ^ XorByte));
    }

    auto empty = Context(block, 1);
    Reset();
    Require(EVP_EncryptFinal_ex_nid_disambig1_nid_postfix(&empty, out.data(), &written) == 1 && written == 16 && empty.buf[0] == 16 && empty.buf[15] == 16);

    Reset(0);
    written = 77;
    Require(EVP_EncryptFinal_ex_nid_disambig1_nid_postfix(&empty, out.data(), &written) == 0 && written == 0 && record.calls == 1);

    auto unpadded = Context(block, 1);
    unpadded.flags = CipherFlagNoPadding;
    unpadded.bufLen = 3;
    Reset();
    written = 77;
    Require(EVP_EncryptFinal_ex_nid_disambig1_nid_postfix(&unpadded, out.data(), &written) == 0 && written == 0 && record.calls == 0);
    RequireQueue(NotMultipleError);
    unpadded.bufLen = 0;
    Require(EVP_EncryptFinal_ex_nid_disambig1_nid_postfix(&unpadded, out.data(), &written) == 1 && written == 0 && record.calls == 0);

    auto corrupt = Context(block, 1);
    corrupt.bufLen = 16;
    Reset();
    written = 77;
    Require(EVP_EncryptFinal_ex_nid_disambig1_nid_postfix(&corrupt, out.data(), &written) == 0 && written == 0 && record.calls == 0);
    RequireQueue(BadBlockLengthError);

    const auto stream = Cipher(1, 0, BlockCipher);
    auto streamContext = Context(stream, 1);
    Reset();
    written = 77;
    Require(EVP_EncryptFinal_ex_nid_disambig1_nid_postfix(&streamContext, out.data(), &written) == 1 && written == 0 && record.calls == 0);
}

void Hold(LibreSslEvpCipherCtx& context, std::size_t plain, unsigned char pad) {
    for (std::size_t index = 0; index < 16; ++index) context.final[index] = index < plain ? static_cast<unsigned char>(0xa0 + index) : pad;
    context.finalUsed = 1;
    context.bufLen = 0;
}

void CheckDecrypt() {
    const auto block = Cipher(16, CipherModeCbc, BlockCipher);
    std::array<unsigned char, 16> out{};
    int written = 77;

    auto context = Context(block, 0);
    Hold(context, 10, 6);
    Reset();
    Require(EVP_CipherFinal_ex_nid_disambig1_nid_postfix(&context, out.data(), &written) == 1 && written == 10 && record.calls == 0);
    for (std::size_t index = 0; index < 10; ++index) Require(out[index] == static_cast<unsigned char>(0xa0 + index));

    Hold(context, 0, 16);
    Require(EVP_DecryptFinal_ex_nid_disambig1_nid_postfix(&context, nullptr, &written) == 1 && written == 0);

    Hold(context, 15, 0);
    Reset();
    Require(EVP_DecryptFinal_ex_nid_disambig1_nid_postfix(&context, out.data(), &written) == 0 && written == 0);
    RequireQueue(BadDecryptError);
    Hold(context, 0, 17);
    Require(EVP_DecryptFinal_ex_nid_disambig1_nid_postfix(&context, out.data(), &written) == 0);
    RequireQueue(BadDecryptError);
    Hold(context, 12, 4);
    context.final[13] = 5;
    Require(EVP_DecryptFinal_ex_nid_disambig1_nid_postfix(&context, out.data(), &written) == 0);
    RequireQueue(BadDecryptError);

    auto missing = Context(block, 0);
    written = 77;
    Require(EVP_DecryptFinal_ex_nid_disambig1_nid_postfix(&missing, out.data(), &written) == 0 && written == 0);
    RequireQueue(WrongFinalBlockLengthError);
    Hold(missing, 10, 6);
    missing.bufLen = 3;
    Require(EVP_DecryptFinal_ex_nid_disambig1_nid_postfix(&missing, out.data(), &written) == 0);
    RequireQueue(WrongFinalBlockLengthError);

    auto unpadded = Context(block, 0);
    unpadded.flags = CipherFlagNoPadding;
    Require(EVP_DecryptFinal_ex_nid_disambig1_nid_postfix(&unpadded, out.data(), &written) == 1 && written == 0);
    unpadded.bufLen = 2;
    Require(EVP_DecryptFinal_ex_nid_disambig1_nid_postfix(&unpadded, out.data(), &written) == 0);
    RequireQueue(NotMultipleError);

    const auto stream = Cipher(1, 0, BlockCipher);
    auto streamContext = Context(stream, 0);
    Require(EVP_DecryptFinal_ex_nid_disambig1_nid_postfix(&streamContext, out.data(), &written) == 1 && written == 0);
    Require(ERR_get_error_nid_postfix() == 0 && record.calls == 0);
}

void CheckInvalidArguments() {
    const auto block = Cipher(16, CipherModeCbc, BlockCipher);
    auto context = Context(block, 1);
    unsigned char out[16]{};
    int written = 0;
    bool threw = false;
    try {
        EVP_CipherFinal_ex_nid_disambig1_nid_postfix(nullptr, out, &written);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);
    threw = false;
    try {
        EVP_EncryptFinal_ex_nid_disambig1_nid_postfix(&context, out, nullptr);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);
}

void CheckGuestPointers() {
    struct GuardedPages {
        std::size_t pageSize = 0;
        std::uint8_t* base = nullptr;
        GuardedPages() {
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
        ~GuardedPages() {
            if (!base) return;
#ifdef _WIN32
            VirtualFree(base, 0, MEM_RELEASE);
#else
            munmap(base, 2 * pageSize);
#endif
        }
        void SetPage(std::size_t index, bool read, bool write) {
            auto* page = base + index * pageSize;
#ifdef _WIN32
            DWORD protect = PAGE_NOACCESS;
            if (read && write) protect = PAGE_READWRITE;
            else if (read) protect = PAGE_READONLY;
            DWORD previous = 0;
            Require(VirtualProtect(page, pageSize, protect, &previous) != 0);
#else
            int protect = PROT_NONE;
            if (read && write) protect = PROT_READ | PROT_WRITE;
            else if (read) protect = PROT_READ;
            Require(mprotect(page, pageSize, protect) == 0);
#endif
        }
    } pages;

    const auto block = Cipher(16, CipherModeCbc, BlockCipher);
    auto validEncrypt = Context(block, 1);
    for (int i = 0; i < 5; ++i) validEncrypt.buf[i] = static_cast<unsigned char>(i + 1);
    validEncrypt.bufLen = 5;
    auto validDecrypt = Context(block, 0);
    Hold(validDecrypt, 10, 6);
    int roWrittenValue = 0x5a5a5a5a;

    std::memcpy(pages.base, &block, sizeof(block));
    std::memcpy(pages.base + 256, &validEncrypt, sizeof(validEncrypt));
    std::memcpy(pages.base + 512, &validDecrypt, sizeof(validDecrypt));
    std::memset(pages.base + 768, 0xa5, 32);
    std::memcpy(pages.base + 1024, &roWrittenValue, sizeof(roWrittenValue));
    pages.SetPage(0, true, false);
    pages.SetPage(1, false, false);

    const auto* roCipher = reinterpret_cast<const LibreSslEvpCipher*>(pages.base);
    auto* roEncryptContext = reinterpret_cast<LibreSslEvpCipherCtx*>(pages.base + 256);
    auto* roDecryptContext = reinterpret_cast<LibreSslEvpCipherCtx*>(pages.base + 512);
    auto* roOut = reinterpret_cast<unsigned char*>(pages.base + 768);
    int* roWritten = reinterpret_cast<int*>(pages.base + 1024);
    auto* noAccess = pages.base + pages.pageSize;
    auto* unmapped = reinterpret_cast<void*>(std::uintptr_t{1});

    std::array<unsigned char, 16> out{};
    int written = 0;

    auto encRoCipher = Context(*roCipher, 1);
    Reset();
    Require(EVP_EncryptFinal_ex_nid_disambig1_nid_postfix(&encRoCipher, out.data(), &written) == 1 && written == 16 && record.calls == 1);

    auto decRoCipher = Context(*roCipher, 0);
    Hold(decRoCipher, 10, 6);
    Reset();
    Require(EVP_DecryptFinal_ex_nid_disambig1_nid_postfix(&decRoCipher, out.data(), &written) == 1 && written == 10 && record.calls == 0);

    auto cipRoCipher = Context(*roCipher, 1);
    Reset();
    Require(EVP_CipherFinal_ex_nid_disambig1_nid_postfix(&cipRoCipher, out.data(), &written) == 1 && written == 16 && record.calls == 1);

    auto expectInvalid = [&](auto&& fn, LibreSslEvpCipherCtx* watched = nullptr) {
        out.fill(0xa5);
        written = 0x5a5a5a5a;
        LibreSslEvpCipherCtx before{};
        if (watched) before = *watched;
        Reset();
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
        Require(record.calls == 0);
        Require(std::all_of(out.begin(), out.end(), [](unsigned char b) { return b == 0xa5; }));
        if (watched) Require(std::memcmp(&before, watched, sizeof(before)) == 0);
    };

    expectInvalid([&] { EVP_EncryptFinal_ex_nid_disambig1_nid_postfix(reinterpret_cast<LibreSslEvpCipherCtx*>(unmapped), out.data(), &written); });
    expectInvalid([&] { EVP_DecryptFinal_ex_nid_disambig1_nid_postfix(reinterpret_cast<LibreSslEvpCipherCtx*>(unmapped), out.data(), &written); });
    expectInvalid([&] { EVP_CipherFinal_ex_nid_disambig1_nid_postfix(reinterpret_cast<LibreSslEvpCipherCtx*>(unmapped), out.data(), &written); });
    expectInvalid([&] { EVP_EncryptFinal_ex_nid_disambig1_nid_postfix(reinterpret_cast<LibreSslEvpCipherCtx*>(noAccess), out.data(), &written); });
    expectInvalid([&] { EVP_DecryptFinal_ex_nid_disambig1_nid_postfix(reinterpret_cast<LibreSslEvpCipherCtx*>(noAccess), out.data(), &written); });
    expectInvalid([&] { EVP_CipherFinal_ex_nid_disambig1_nid_postfix(reinterpret_cast<LibreSslEvpCipherCtx*>(noAccess), out.data(), &written); });
    expectInvalid([&] { EVP_EncryptFinal_ex_nid_disambig1_nid_postfix(roEncryptContext, out.data(), &written); });
    expectInvalid([&] { EVP_DecryptFinal_ex_nid_disambig1_nid_postfix(roDecryptContext, out.data(), &written); });
    expectInvalid([&] { EVP_CipherFinal_ex_nid_disambig1_nid_postfix(roEncryptContext, out.data(), &written); });
    expectInvalid([&] { EVP_CipherFinal_ex_nid_disambig1_nid_postfix(roDecryptContext, out.data(), &written); });

    auto badCipherCtx = validEncrypt;
    badCipherCtx.cipher = nullptr;
    expectInvalid([&] { EVP_CipherFinal_ex_nid_disambig1_nid_postfix(&badCipherCtx, out.data(), &written); }, &badCipherCtx);
    badCipherCtx.cipher = reinterpret_cast<const LibreSslEvpCipher*>(unmapped);
    expectInvalid([&] { EVP_EncryptFinal_ex_nid_disambig1_nid_postfix(&badCipherCtx, out.data(), &written); }, &badCipherCtx);
    expectInvalid([&] { EVP_DecryptFinal_ex_nid_disambig1_nid_postfix(&badCipherCtx, out.data(), &written); }, &badCipherCtx);
    badCipherCtx.cipher = reinterpret_cast<const LibreSslEvpCipher*>(noAccess);
    expectInvalid([&] { EVP_CipherFinal_ex_nid_disambig1_nid_postfix(&badCipherCtx, out.data(), &written); }, &badCipherCtx);

    auto encCtx = validEncrypt;
    auto decCtx = validDecrypt;
    expectInvalid([&] { EVP_EncryptFinal_ex_nid_disambig1_nid_postfix(&encCtx, out.data(), nullptr); }, &encCtx);
    expectInvalid([&] { EVP_DecryptFinal_ex_nid_disambig1_nid_postfix(&decCtx, out.data(), nullptr); }, &decCtx);
    expectInvalid([&] { EVP_CipherFinal_ex_nid_disambig1_nid_postfix(&decCtx, out.data(), nullptr); }, &decCtx);
    expectInvalid([&] { EVP_EncryptFinal_ex_nid_disambig1_nid_postfix(&encCtx, out.data(), reinterpret_cast<int*>(unmapped)); }, &encCtx);
    expectInvalid([&] { EVP_DecryptFinal_ex_nid_disambig1_nid_postfix(&decCtx, out.data(), reinterpret_cast<int*>(noAccess)); }, &decCtx);
    expectInvalid([&] { EVP_EncryptFinal_ex_nid_disambig1_nid_postfix(&encCtx, out.data(), roWritten); }, &encCtx);
    expectInvalid([&] { EVP_DecryptFinal_ex_nid_disambig1_nid_postfix(&decCtx, out.data(), roWritten); }, &decCtx);
    expectInvalid([&] { EVP_CipherFinal_ex_nid_disambig1_nid_postfix(&encCtx, out.data(), roWritten); }, &encCtx);

    auto badEncryptState = encCtx;
    badEncryptState.bufLen = 16;
    expectInvalid([&] { EVP_EncryptFinal_ex_nid_disambig1_nid_postfix(&badEncryptState, out.data(), roWritten); }, &badEncryptState);
    auto badDecryptState = decCtx;
    badDecryptState.finalUsed = 0;
    expectInvalid([&] { EVP_DecryptFinal_ex_nid_disambig1_nid_postfix(&badDecryptState, out.data(), roWritten); }, &badDecryptState);

    expectInvalid([&] { EVP_EncryptFinal_ex_nid_disambig1_nid_postfix(&encCtx, nullptr, &written); }, &encCtx);
    expectInvalid([&] { EVP_DecryptFinal_ex_nid_disambig1_nid_postfix(&decCtx, nullptr, &written); }, &decCtx);
    expectInvalid([&] { EVP_EncryptFinal_ex_nid_disambig1_nid_postfix(&encCtx, reinterpret_cast<unsigned char*>(unmapped), &written); }, &encCtx);
    expectInvalid([&] { EVP_DecryptFinal_ex_nid_disambig1_nid_postfix(&decCtx, reinterpret_cast<unsigned char*>(unmapped), &written); }, &decCtx);
    expectInvalid([&] { EVP_EncryptFinal_ex_nid_disambig1_nid_postfix(&encCtx, noAccess, &written); }, &encCtx);
    expectInvalid([&] { EVP_DecryptFinal_ex_nid_disambig1_nid_postfix(&decCtx, noAccess, &written); }, &decCtx);
    expectInvalid([&] { EVP_EncryptFinal_ex_nid_disambig1_nid_postfix(&encCtx, roOut, &written); }, &encCtx);
    expectInvalid([&] { EVP_DecryptFinal_ex_nid_disambig1_nid_postfix(&decCtx, roOut, &written); }, &decCtx);
    expectInvalid([&] { EVP_CipherFinal_ex_nid_disambig1_nid_postfix(&encCtx, roOut, &written); }, &encCtx);

    pages.SetPage(0, true, true);
    const std::size_t page = pages.pageSize;

    std::memset(pages.base + page - 16, 0xa5, 16);
    auto fitEnc = validEncrypt;
    Reset();
    Require(EVP_EncryptFinal_ex_nid_disambig1_nid_postfix(&fitEnc, pages.base + page - 16, &written) == 1 && written == 16 && record.calls == 1);
    for (int i = 0; i < 16; ++i) {
        const unsigned char plain = i < 5 ? static_cast<unsigned char>(i + 1) : 11;
        Require(pages.base[page - 16 + static_cast<std::size_t>(i)] == static_cast<unsigned char>(plain ^ XorByte));
    }

    std::memset(pages.base + page - 8, 0xa5, 8);
    auto straddleEnc = validEncrypt;
    expectInvalid([&] { EVP_EncryptFinal_ex_nid_disambig1_nid_postfix(&straddleEnc, pages.base + page - 8, &written); }, &straddleEnc);
    for (std::size_t i = 0; i < 8; ++i) Require(pages.base[page - 8 + i] == 0xa5);

    std::memset(pages.base + page - 10, 0xa5, 10);
    auto fitDec = validDecrypt;
    Reset();
    Require(EVP_DecryptFinal_ex_nid_disambig1_nid_postfix(&fitDec, pages.base + page - 10, &written) == 1 && written == 10 && record.calls == 0);
    for (std::size_t i = 0; i < 10; ++i) Require(pages.base[page - 10 + i] == static_cast<unsigned char>(0xa0 + i));

    std::memset(pages.base + page - 4, 0xa5, 4);
    auto straddleDec = validDecrypt;
    expectInvalid([&] { EVP_DecryptFinal_ex_nid_disambig1_nid_postfix(&straddleDec, pages.base + page - 4, &written); }, &straddleDec);
    for (std::size_t i = 0; i < 4; ++i) Require(pages.base[page - 4 + i] == 0xa5);

    std::memset(pages.base + page - 2, 0xa5, 2);
    expectInvalid([&] { EVP_EncryptFinal_ex_nid_disambig1_nid_postfix(&straddleEnc, out.data(), reinterpret_cast<int*>(pages.base + page - 2)); }, &straddleEnc);
    for (std::size_t i = 0; i < 2; ++i) Require(pages.base[page - 2 + i] == 0xa5);

    pages.SetPage(1, true, false);
    std::memset(pages.base + page - 8, 0xa5, 8);
    expectInvalid([&] { EVP_EncryptFinal_ex_nid_disambig1_nid_postfix(&straddleEnc, pages.base + page - 8, &written); }, &straddleEnc);
    for (std::size_t i = 0; i < 8; ++i) Require(pages.base[page - 8 + i] == 0xa5);

    std::memset(pages.base + page - 4, 0xa5, 4);
    expectInvalid([&] { EVP_DecryptFinal_ex_nid_disambig1_nid_postfix(&straddleDec, pages.base + page - 4, &written); }, &straddleDec);
    for (std::size_t i = 0; i < 4; ++i) Require(pages.base[page - 4 + i] == 0xa5);

    std::memset(pages.base + page - 2, 0xa5, 2);
    expectInvalid([&] { EVP_DecryptFinal_ex_nid_disambig1_nid_postfix(&straddleDec, out.data(), reinterpret_cast<int*>(pages.base + page - 2)); }, &straddleDec);
    for (std::size_t i = 0; i < 2; ++i) Require(pages.base[page - 2 + i] == 0xa5);

    pages.SetPage(1, true, true);
    auto* straddleCtx = reinterpret_cast<LibreSslEvpCipherCtx*>(pages.base + page - 16);
    std::memcpy(straddleCtx, &validEncrypt, sizeof(validEncrypt));
    pages.SetPage(1, true, false);
    expectInvalid([&] { EVP_EncryptFinal_ex_nid_disambig1_nid_postfix(straddleCtx, out.data(), &written); });
    pages.SetPage(1, false, false);
    expectInvalid([&] { EVP_CipherFinal_ex_nid_disambig1_nid_postfix(straddleCtx, out.data(), &written); });

    pages.SetPage(1, true, true);
    auto* straddleCipher = reinterpret_cast<LibreSslEvpCipher*>(pages.base + page - 16);
    std::memcpy(straddleCipher, &block, sizeof(block));
    pages.SetPage(0, true, false);
    pages.SetPage(1, false, false);
    auto straddleCipherCtx = validEncrypt;
    straddleCipherCtx.cipher = straddleCipher;
    expectInvalid([&] { EVP_EncryptFinal_ex_nid_disambig1_nid_postfix(&straddleCipherCtx, out.data(), &written); }, &straddleCipherCtx);

    auto fullPadDecrypt = Context(block, 0);
    Hold(fullPadDecrypt, 0, 16);
    Reset();
    Require(EVP_DecryptFinal_ex_nid_disambig1_nid_postfix(&fullPadDecrypt, noAccess, &written) == 1 && written == 0 && record.calls == 0);

    const auto stream = Cipher(1, 0, BlockCipher);
    auto streamCtx = Context(stream, 1);
    Reset();
    Require(EVP_EncryptFinal_ex_nid_disambig1_nid_postfix(&streamCtx, noAccess, &written) == 1 && written == 0 && record.calls == 0);
    streamCtx.encrypt = 0;
    Require(EVP_CipherFinal_ex_nid_disambig1_nid_postfix(&streamCtx, noAccess, &written) == 1 && written == 0 && record.calls == 0);

    auto noPadCtx = Context(block, 0);
    noPadCtx.flags = CipherFlagNoPadding;
    Reset();
    Require(EVP_DecryptFinal_ex_nid_disambig1_nid_postfix(&noPadCtx, noAccess, &written) == 1 && written == 0 && record.calls == 0);
    noPadCtx.encrypt = 1;
    Require(EVP_CipherFinal_ex_nid_disambig1_nid_postfix(&noPadCtx, noAccess, &written) == 1 && written == 0 && record.calls == 0);
}

}

int main() {
    CheckCustomCipher();
    CheckEncrypt();
    CheckDecrypt();
    CheckInvalidArguments();
    CheckGuestPointers();
    std::puts("LibreSSL3 EVP_*Final_ex: ok");
}
