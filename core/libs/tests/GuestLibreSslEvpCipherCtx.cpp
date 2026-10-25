#include "prx/libSceLibreSsl/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <source_location>
#include <stdexcept>
#include <vector>

constexpr std::size_t ContextBytes = 168;
constexpr std::size_t CipherOffset = 0;
constexpr std::size_t EngineOffset = 8;
constexpr std::size_t EncryptOffset = 16;
constexpr std::size_t BufLenOffset = 20;
constexpr std::size_t OivOffset = 24;
constexpr std::size_t IvOffset = 40;
constexpr std::size_t BufOffset = 56;
constexpr std::size_t NumOffset = 88;
constexpr std::size_t KeyLenOffset = 104;
constexpr std::size_t FlagsOffset = 112;
constexpr std::size_t CipherDataOffset = 120;
constexpr std::size_t FinalUsedOffset = 128;
constexpr std::size_t BlockMaskOffset = 132;
constexpr std::size_t FinalOffset = 136;
constexpr std::uint64_t ModeCbc = 2;
constexpr std::uint64_t ModeCfb = 3;
constexpr std::uint64_t ModeOfb = 4;
constexpr std::uint64_t ModeCtr = 5;
constexpr std::uint64_t ModeGcm = 6;
constexpr std::uint64_t FlagCustomIv = 0x10;
constexpr std::uint64_t FlagAlwaysCallInit = 0x20;
constexpr std::uint64_t FlagCtrlInit = 0x40;
constexpr std::uint64_t FlagCustomCipher = 0x100000;

struct Cipher {
    int nid;
    int blockSize;
    int keyLength;
    int ivLength;
    std::uint64_t flags;
    int (APS5_VABI* init)(unsigned char*, const unsigned char*, const unsigned char*, int);
    int (APS5_VABI* doCipher)(unsigned char*, unsigned char*, const unsigned char*, std::size_t);
    int (APS5_VABI* cleanup)(unsigned char*);
    int contextSize;
    void* setAsn1Parameters;
    void* getAsn1Parameters;
    int (APS5_VABI* control)(unsigned char*, int, int, void*);
    void* applicationData;
};

static_assert(sizeof(Cipher) == 88);

struct Calls {
    int inits;
    unsigned char* initContext;
    const unsigned char* initKey;
    const unsigned char* initIv;
    int initEncrypt;
    int initResult;
    int ciphers;
    unsigned char* cipherOut;
    const unsigned char* cipherIn;
    std::size_t cipherLength;
    int cipherResult;
    int cleanups;
    unsigned char* cleanupContext;
    int cleanupData;
    int cleanupResult;
    int controls;
    unsigned char* controlContext;
    int controlType;
    int controlArg;
    void* controlPointer;
    int controlResult;
};

static Calls calls;

template<typename T> static T Load(const unsigned char* context, std::size_t offset) {
    T value {};
    std::memcpy(&value, context + offset, sizeof(value));
    return value;
}

template<typename T> static void Store(unsigned char* context, std::size_t offset, T value) {
    std::memcpy(context + offset, &value, sizeof(value));
}

static std::uint64_t Flags(const unsigned char* context) {
    std::uint64_t flags = 0;
    std::memcpy(&flags, context + FlagsOffset, sizeof(flags));
    return flags;
}

static void SetFlags(unsigned char* context, std::uint64_t flags) {
    std::memcpy(context + FlagsOffset, &flags, sizeof(flags));
}

static void InitializeHeap() {
    void* const api[10] {};
    ApplicationHeapRegister_nid_no_patch(api);
}

static LibreSslEvpCipherCtx* Context(unsigned char* memory) {
    return reinterpret_cast<LibreSslEvpCipherCtx*>(memory);
}

static const LibreSslEvpCipher* EvpCipher(const void* cipher) {
    return static_cast<const LibreSslEvpCipher*>(cipher);
}

static void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "LibreSSL EVP_CIPHER_CTX check failed at line %u\n", location.line());
    std::abort();
}

template<typename TAction> static void RequireThrows(TAction action, std::source_location location = std::source_location::current()) {
    try { action(); } catch (const std::exception&) { return; }
    Require(false, location);
}

static void TestInit() {
    unsigned char memory[ContextBytes + 16];
    std::memset(memory, 0xAA, sizeof(memory));
    EVP_CIPHER_CTX_init_nid_postfix(Context(memory));
    for (std::size_t index = 0; index < ContextBytes; ++index) Require(memory[index] == 0);
    for (std::size_t index = ContextBytes; index < sizeof(memory); ++index) Require(memory[index] == 0xAA);
    RequireThrows([] { EVP_CIPHER_CTX_init_nid_postfix(nullptr); });
}

static void TestNew() {
    for (int round = 0; round < 4; ++round) {
        auto* context = reinterpret_cast<unsigned char*>(EVP_CIPHER_CTX_new_nid_postfix());
        Require(context != nullptr);
        for (std::size_t index = 0; index < ContextBytes; ++index) Require(context[index] == 0);
        std::memset(context, 0x5A, ContextBytes);
        free_nid_postfix(context);
    }
}

static void TestSetPadding() {
    unsigned char context[ContextBytes] {};
    Require(EVP_CIPHER_CTX_set_padding_nid_postfix(Context(context), 0) == 1);
    Require(Flags(context) == 0x100);
    Require(EVP_CIPHER_CTX_set_padding_nid_postfix(Context(context), 0) == 1);
    Require(Flags(context) == 0x100);
    Require(EVP_CIPHER_CTX_set_padding_nid_postfix(Context(context), 1) == 1);
    Require(Flags(context) == 0);
    SetFlags(context, UINT64_C(0xFFFFFFFF000000FF));
    Require(EVP_CIPHER_CTX_set_padding_nid_postfix(Context(context), 0) == 1);
    Require(Flags(context) == UINT64_C(0xFFFFFFFF000001FF));
    Require(EVP_CIPHER_CTX_set_padding_nid_postfix(Context(context), -5) == 1);
    Require(Flags(context) == UINT64_C(0xFFFFFFFF000000FF));
    for (std::size_t index = 0; index < ContextBytes; ++index) {
        if (index < FlagsOffset || index >= FlagsOffset + 8) Require(context[index] == 0);
    }
    RequireThrows([] { EVP_CIPHER_CTX_set_padding_nid_postfix(nullptr, 0); });
}

static int APS5_VABI RecordCleanup(unsigned char* context) {
    ++calls.cleanups;
    calls.cleanupContext = context;
    const auto* data = Load<const unsigned char*>(context, CipherDataOffset);
    calls.cleanupData = data ? data[0] : -1;
    return calls.cleanupResult;
}

static int APS5_VABI RecordControl(unsigned char* context, int type, int arg, void* pointer) {
    ++calls.controls;
    calls.controlContext = context;
    calls.controlType = type;
    calls.controlArg = arg;
    calls.controlPointer = pointer;
    return calls.controlResult;
}

static void RequireCleared(const unsigned char* context) {
    for (std::size_t index = 0; index < ContextBytes; ++index) Require(context[index] == 0);
}

static unsigned char* FilledData(std::size_t size) {
    auto* data = static_cast<unsigned char*>(malloc_nid_postfix(size));
    Require(data != nullptr);
    std::memset(data, 0x5A, size);
    return data;
}

static void PrepareContext(unsigned char* context, const Cipher* cipher, void* data) {
    std::memset(context, 0x33, ContextBytes);
    Store(context, CipherOffset, cipher);
    Store<void*>(context, EngineOffset, nullptr);
    Store(context, CipherDataOffset, data);
}

static void TestCleanup() {
    Cipher cipher {};
    cipher.cleanup = RecordCleanup;
    cipher.contextSize = 24;
    unsigned char context[ContextBytes];
    auto* data = FilledData(24);
    PrepareContext(context, &cipher, data);
    calls = {};
    Require(EVP_CIPHER_CTX_cleanup_nid_postfix(Context(context)) == 0);
    Require(calls.cleanups == 1 && calls.cleanupContext == context && calls.cleanupData == 0x5A);
    Require(Load<const Cipher*>(context, CipherOffset) == &cipher && Load<unsigned char*>(context, CipherDataOffset) == data);
    Require(context[ContextBytes - 1] == 0x33);
    calls.cleanupResult = 1;
    Require(EVP_CIPHER_CTX_cleanup_nid_postfix(Context(context)) == 1);
    Require(calls.cleanups == 2 && calls.cleanupData == 0x5A);
    RequireCleared(context);

    cipher.cleanup = nullptr;
    PrepareContext(context, &cipher, FilledData(24));
    Require(EVP_CIPHER_CTX_cleanup_nid_postfix(Context(context)) == 1);
    Require(calls.cleanups == 2);
    RequireCleared(context);

    PrepareContext(context, nullptr, FilledData(8));
    Require(EVP_CIPHER_CTX_cleanup_nid_postfix(Context(context)) == 1);
    RequireCleared(context);
    Require(EVP_CIPHER_CTX_cleanup_nid_postfix(Context(context)) == 1);
    RequireCleared(context);

    PrepareContext(context, &cipher, nullptr);
    Store<void*>(context, EngineOffset, &cipher);
    RequireThrows([&] { EVP_CIPHER_CTX_cleanup_nid_postfix(Context(context)); });
    Require(Load<void*>(context, EngineOffset) == &cipher && context[ContextBytes - 1] == 0x33);

    cipher.contextSize = -1;
    data = FilledData(8);
    PrepareContext(context, &cipher, data);
    RequireThrows([&] { EVP_CIPHER_CTX_cleanup_nid_postfix(Context(context)); });
    free_nid_postfix(data);
    RequireThrows([] { EVP_CIPHER_CTX_cleanup_nid_postfix(nullptr); });
}

static void TestFree() {
    EVP_CIPHER_CTX_free_nid_postfix(nullptr);
    Cipher cipher {};
    cipher.cleanup = RecordCleanup;
    cipher.contextSize = 16;
    calls = {};
    for (int result : {1, 0}) {
        calls.cleanupResult = result;
        auto* context = reinterpret_cast<unsigned char*>(EVP_CIPHER_CTX_new_nid_postfix());
        Require(context != nullptr);
        Store(context, CipherOffset, &cipher);
        Store(context, CipherDataOffset, FilledData(16));
        EVP_CIPHER_CTX_free_nid_postfix(Context(context));
        Require(calls.cleanupContext == context && calls.cleanupData == 0x5A);
    }
    Require(calls.cleanups == 2);
}

static void TestCtrl() {
    unsigned char context[ContextBytes] {};
    int value = 0;
    Require(EVP_CIPHER_CTX_ctrl_nid_postfix(Context(context), 9, 12, &value) == 0);
    Cipher cipher {};
    Store(context, CipherOffset, &cipher);
    Require(EVP_CIPHER_CTX_ctrl_nid_postfix(Context(context), 9, 12, &value) == 0);
    cipher.control = RecordControl;
    calls = {};
    for (int result : {1, 16, 0, -1, -2}) {
        calls.controlResult = result;
        Require(EVP_CIPHER_CTX_ctrl_nid_postfix(Context(context), 0x11, -3, &value) == (result == -1 ? 0 : result));
    }
    Require(calls.controls == 5 && calls.controlContext == context && calls.controlType == 0x11);
    Require(calls.controlArg == -3 && calls.controlPointer == &value);
    RequireThrows([&] { EVP_CIPHER_CTX_ctrl_nid_postfix(nullptr, 0, 0, &value); });
}

using Update = int (APS5_VABI*)(LibreSslEvpCipherCtx*, unsigned char*, int*, const unsigned char*, int);
using Final = int (APS5_VABI*)(LibreSslEvpCipherCtx*, unsigned char*, int*);
using Bytes = std::vector<unsigned char>;

constexpr unsigned char XorKey[16] {0x2b, 0x7e, 0x15, 0x16, 0x28, 0xae, 0xd2, 0xa6, 0xab, 0xf7, 0x15, 0x88, 0x09, 0xcf, 0x4f, 0x3c};

static void ResetCalls() {
    calls = {};
    calls.initResult = 1;
    calls.cipherResult = 1;
    calls.cleanupResult = 1;
    calls.controlResult = 1;
}

static int APS5_VABI XorInit(unsigned char* context, const unsigned char* key, const unsigned char* iv, int encrypt) {
    ++calls.inits;
    calls.initContext = context;
    calls.initKey = key;
    calls.initIv = iv;
    calls.initEncrypt = encrypt;
    auto* data = Load<unsigned char*>(context, CipherDataOffset);
    if (key && data) std::memcpy(data, key, sizeof(XorKey));
    return calls.initResult;
}

static int APS5_VABI XorCipher(unsigned char* context, unsigned char* out, const unsigned char* in, std::size_t length) {
    ++calls.ciphers;
    calls.cipherOut = out;
    calls.cipherIn = in;
    calls.cipherLength = length;
    const auto* key = Load<const unsigned char*>(context, CipherDataOffset);
    for (std::size_t index = 0; index < length; ++index) out[index] = in[index] ^ key[index % sizeof(XorKey)];
    return calls.cipherResult;
}

static int APS5_VABI RecordCipher(unsigned char*, unsigned char* out, const unsigned char* in, std::size_t length) {
    ++calls.ciphers;
    calls.cipherOut = out;
    calls.cipherIn = in;
    calls.cipherLength = length;
    return calls.cipherResult;
}

static Cipher XorBlockCipher() {
    Cipher cipher {};
    cipher.nid = 1000;
    cipher.blockSize = 16;
    cipher.keyLength = 16;
    cipher.flags = 1;
    cipher.init = XorInit;
    cipher.doCipher = XorCipher;
    cipher.cleanup = RecordCleanup;
    cipher.contextSize = 16;
    return cipher;
}

static Bytes Pattern(std::size_t length, unsigned seed) {
    Bytes bytes(length);
    for (std::size_t index = 0; index < length; ++index) bytes[index] = static_cast<unsigned char>(seed + 37 * index);
    return bytes;
}

static Bytes Xor(Bytes bytes) {
    for (std::size_t index = 0; index < bytes.size(); ++index) bytes[index] ^= XorKey[index % sizeof(XorKey)];
    return bytes;
}

static Bytes Padded(Bytes bytes) {
    const std::size_t pad = 16 - bytes.size() % 16;
    bytes.insert(bytes.end(), pad, static_cast<unsigned char>(pad));
    return bytes;
}

static int Process(unsigned char* context, Update update, Final final, const Bytes& input, std::size_t chunk, Bytes& output) {
    output.assign(input.size() + 32, 0xEE);
    int total = 0;
    for (std::size_t offset = 0; offset < input.size(); offset += chunk) {
        const int length = static_cast<int>(input.size() - offset < chunk ? input.size() - offset : chunk);
        int written = -1;
        Require(update(Context(context), output.data() + total, &written, input.data() + offset, length) == 1);
        Require(written >= 0 && written <= length + 16);
        total += written;
    }
    int written = -1;
    const int result = final(Context(context), output.data() + total, &written);
    if (result) total += written;
    output.resize(static_cast<std::size_t>(total));
    return result;
}

static void TestPadding() {
    const auto cipher = XorBlockCipher();
    for (std::size_t length : {0, 1, 15, 16, 17, 31, 32, 33, 48}) {
        const auto input = Pattern(length, static_cast<unsigned>(length));
        const auto expected = Xor(Padded(input));
        for (std::size_t chunk : {1, 5, 16, 64}) {
            for (bool generic : {false, true}) {
                ResetCalls();
                auto* context = reinterpret_cast<unsigned char*>(EVP_CIPHER_CTX_new_nid_postfix());
                Bytes output;
                Require(generic ? EVP_CipherInit_ex_nid_postfix(Context(context), EvpCipher(&cipher), nullptr, XorKey, nullptr, 1) == 1
                                : EVP_EncryptInit_ex_nid_postfix(Context(context), EvpCipher(&cipher), nullptr, XorKey, nullptr) == 1);
                Require(calls.inits == 1 && calls.initKey == XorKey && calls.initIv == nullptr && calls.initEncrypt == 1);
                Require(Process(context, generic ? EVP_CipherUpdate_nid_postfix : EVP_EncryptUpdate_nid_postfix,
                    generic ? EVP_CipherFinal_ex_nid_postfix : EVP_EncryptFinal_ex_nid_postfix, input, chunk, output) == 1);
                Require(output == expected);
                Require(generic ? EVP_CipherInit_ex_nid_postfix(Context(context), EvpCipher(&cipher), nullptr, XorKey, nullptr, 0) == 1
                                : EVP_DecryptInit_ex_nid_postfix(Context(context), EvpCipher(&cipher), nullptr, XorKey, nullptr) == 1);
                Require(calls.cleanups == 1 && calls.inits == 2 && calls.initEncrypt == 0);
                Require(Process(context, generic ? EVP_CipherUpdate_nid_postfix : EVP_DecryptUpdate_nid_postfix,
                    generic ? EVP_CipherFinal_ex_nid_postfix : EVP_DecryptFinal_ex_nid_postfix, expected, chunk, output) == 1);
                Require(output == input);
                EVP_CIPHER_CTX_free_nid_postfix(Context(context));
                Require(calls.cleanups == 2);
            }
        }
    }
}

static void TestHeldBlock() {
    const auto cipher = XorBlockCipher();
    ResetCalls();
    unsigned char context[ContextBytes] {};
    const auto plain = Pattern(20, 3);
    const auto encrypted = Xor(Padded(plain));
    Require(EVP_DecryptInit_ex_nid_postfix(Context(context), EvpCipher(&cipher), nullptr, XorKey, nullptr) == 1);
    unsigned char out[64] {};
    int written = -1;
    Require(EVP_DecryptUpdate_nid_postfix(Context(context), out, &written, encrypted.data(), 16) == 1);
    Require(written == 0 && Load<int>(context, FinalUsedOffset) == 1 && Load<int>(context, BufLenOffset) == 0);
    Require(std::memcmp(context + FinalOffset, plain.data(), 16) == 0);
    Require(EVP_DecryptUpdate_nid_postfix(Context(context), out, &written, encrypted.data() + 16, 16) == 1);
    Require(written == 16 && std::memcmp(out, plain.data(), 16) == 0 && Load<int>(context, FinalUsedOffset) == 1);
    Require(EVP_DecryptFinal_ex_nid_postfix(Context(context), out + 16, &written) == 1);
    Require(written == 4 && std::memcmp(out + 16, plain.data() + 16, 4) == 0);
    Require(EVP_DecryptInit_ex_nid_postfix(Context(context), nullptr, nullptr, nullptr, nullptr) == 1);
    Require(EVP_DecryptUpdate_nid_postfix(Context(context), out, &written, encrypted.data(), 17) == 1);
    Require(written == 16 && std::memcmp(out, plain.data(), 16) == 0);
    Require(Load<int>(context, FinalUsedOffset) == 0 && Load<int>(context, BufLenOffset) == 1);
    Require(EVP_DecryptUpdate_nid_postfix(Context(context), out, &written, encrypted.data() + 17, 15) == 1);
    Require(written == 0 && Load<int>(context, FinalUsedOffset) == 1 && Load<int>(context, BufLenOffset) == 0);
    Require(std::memcmp(context + FinalOffset, Padded(plain).data() + 16, 16) == 0);
    Require(EVP_DecryptFinal_ex_nid_postfix(Context(context), out, &written) == 1);
    Require(written == 4 && std::memcmp(out, plain.data() + 16, 4) == 0);
    Require(EVP_CIPHER_CTX_cleanup_nid_postfix(Context(context)) == 1);
}

static void TestBadPadding() {
    const auto cipher = XorBlockCipher();
    ResetCalls();
    unsigned char context[ContextBytes] {};
    unsigned char out[32] {};
    auto block = Pattern(16, 9);
    for (int last : {0, 17, 255}) {
        block[15] = static_cast<unsigned char>(last);
        const auto encrypted = Xor(block);
        int written = -1;
        Require(EVP_DecryptInit_ex_nid_postfix(Context(context), EvpCipher(&cipher), nullptr, XorKey, nullptr) == 1);
        Require(EVP_DecryptUpdate_nid_postfix(Context(context), out, &written, encrypted.data(), 16) == 1 && written == 0);
        written = -1;
        Require(EVP_DecryptFinal_ex_nid_postfix(Context(context), out, &written) == 0 && written == 0);
    }
    block[15] = 2;
    block[14] = 3;
    int written = -1;
    Require(EVP_DecryptInit_ex_nid_postfix(Context(context), EvpCipher(&cipher), nullptr, XorKey, nullptr) == 1);
    Require(EVP_DecryptUpdate_nid_postfix(Context(context), out, &written, Xor(block).data(), 16) == 1);
    Require(EVP_DecryptFinal_ex_nid_postfix(Context(context), out, &written) == 0 && written == 0);
    Bytes full(16, 16);
    Require(EVP_DecryptInit_ex_nid_postfix(Context(context), EvpCipher(&cipher), nullptr, XorKey, nullptr) == 1);
    Require(EVP_DecryptUpdate_nid_postfix(Context(context), out, &written, Xor(full).data(), 16) == 1);
    written = -1;
    Require(EVP_DecryptFinal_ex_nid_postfix(Context(context), out, &written) == 1 && written == 0);
    Require(EVP_DecryptInit_ex_nid_postfix(Context(context), EvpCipher(&cipher), nullptr, XorKey, nullptr) == 1);
    written = -1;
    Require(EVP_DecryptFinal_ex_nid_postfix(Context(context), out, &written) == 0 && written == 0);
    Require(EVP_DecryptUpdate_nid_postfix(Context(context), out, &written, Xor(full).data(), 15) == 1 && written == 0);
    written = -1;
    Require(EVP_DecryptFinal_ex_nid_postfix(Context(context), out, &written) == 0 && written == 0);
    Require(EVP_CIPHER_CTX_cleanup_nid_postfix(Context(context)) == 1);
}

static void TestNoPadding() {
    const auto cipher = XorBlockCipher();
    ResetCalls();
    unsigned char context[ContextBytes] {};
    Bytes output;
    const auto aligned = Pattern(32, 1);
    Require(EVP_EncryptInit_ex_nid_postfix(Context(context), EvpCipher(&cipher), nullptr, XorKey, nullptr) == 1);
    Require(EVP_CIPHER_CTX_set_padding_nid_postfix(Context(context), 0) == 1);
    Require(Process(context, EVP_EncryptUpdate_nid_postfix, EVP_EncryptFinal_ex_nid_postfix, aligned, 7, output) == 1);
    Require(output == Xor(aligned));
    Require(EVP_DecryptInit_ex_nid_postfix(Context(context), nullptr, nullptr, XorKey, nullptr) == 1);
    Require(Load<std::uint64_t>(context, FlagsOffset) == 0x100);
    unsigned char out[64] {};
    int written = -1;
    Require(EVP_DecryptUpdate_nid_postfix(Context(context), out, &written, Xor(aligned).data(), 32) == 1);
    Require(written == 32 && std::memcmp(out, aligned.data(), 32) == 0 && Load<int>(context, FinalUsedOffset) == 0);
    written = -1;
    Require(EVP_DecryptFinal_ex_nid_postfix(Context(context), out, &written) == 1 && written == 0);
    for (bool encrypt : {true, false}) {
        Require(EVP_CipherInit_ex_nid_postfix(Context(context), nullptr, nullptr, XorKey, nullptr, encrypt) == 1);
        Require(EVP_CipherUpdate_nid_postfix(Context(context), out, &written, aligned.data(), 17) == 1 && written == 16);
        written = -1;
        Require(EVP_CipherFinal_ex_nid_postfix(Context(context), out, &written) == 0);
        Require(written == (encrypt ? -1 : 0));
    }
    Require(EVP_EncryptInit_ex_nid_postfix(Context(context), EvpCipher(&cipher), nullptr, XorKey, nullptr) == 1);
    Require(Load<std::uint64_t>(context, FlagsOffset) == 0);
    Require(Process(context, EVP_EncryptUpdate_nid_postfix, EVP_EncryptFinal_ex_nid_postfix, aligned, 32, output) == 1);
    Require(output == Xor(Padded(aligned)));
    Require(EVP_CIPHER_CTX_cleanup_nid_postfix(Context(context)) == 1);
}

static void TestInitFields() {
    ResetCalls();
    auto cipher = XorBlockCipher();
    cipher.flags = ModeCbc;
    cipher.ivLength = 16;
    cipher.keyLength = 24;
    const auto first = Pattern(16, 100);
    const auto second = Pattern(16, 200);
    unsigned char context[ContextBytes] {};
    Require(EVP_EncryptInit_ex_nid_postfix(Context(context), EvpCipher(&cipher), nullptr, XorKey, first.data()) == 1);
    Require(Load<const Cipher*>(context, CipherOffset) == &cipher && Load<void*>(context, EngineOffset) == nullptr);
    Require(Load<int>(context, EncryptOffset) == 1 && Load<int>(context, KeyLenOffset) == 24 && Load<int>(context, BlockMaskOffset) == 15);
    Require(Load<int>(context, BufLenOffset) == 0 && Load<int>(context, FinalUsedOffset) == 0 && Load<std::uint64_t>(context, FlagsOffset) == 0);
    Require(Load<void*>(context, CipherDataOffset) != nullptr && calls.initIv == first.data() && calls.initContext == context);
    Require(std::memcmp(context + OivOffset, first.data(), 16) == 0 && std::memcmp(context + IvOffset, first.data(), 16) == 0);
    std::memset(context + IvOffset, 0x44, 16);
    Require(EVP_EncryptInit_ex_nid_postfix(Context(context), nullptr, nullptr, nullptr, nullptr) == 1);
    Require(calls.inits == 1 && std::memcmp(context + IvOffset, first.data(), 16) == 0);
    Require(EVP_EncryptInit_ex_nid_postfix(Context(context), nullptr, nullptr, nullptr, second.data()) == 1);
    Require(calls.inits == 1 && std::memcmp(context + OivOffset, second.data(), 16) == 0 && std::memcmp(context + IvOffset, second.data(), 16) == 0);
    Require(EVP_DecryptInit_ex_nid_postfix(Context(context), nullptr, nullptr, XorKey, nullptr) == 1);
    Require(calls.inits == 2 && calls.initEncrypt == 0 && Load<int>(context, EncryptOffset) == 0);
    Require(EVP_CipherInit_ex_nid_postfix(Context(context), nullptr, nullptr, XorKey, nullptr, -1) == 1);
    Require(calls.initEncrypt == 0 && Load<int>(context, EncryptOffset) == 0);
    Require(EVP_CipherInit_ex_nid_postfix(Context(context), nullptr, nullptr, XorKey, nullptr, 7) == 1);
    Require(calls.initEncrypt == 1 && Load<int>(context, EncryptOffset) == 1);
    Require(EVP_CipherInit_ex_nid_postfix(Context(context), nullptr, nullptr, XorKey, nullptr, -1) == 1);
    Require(calls.initEncrypt == 1 && calls.cleanups == 0);
    for (std::uint64_t mode : {ModeCfb, ModeOfb, ModeCtr, std::uint64_t {0}, std::uint64_t {1}, ModeCbc | FlagCustomIv}) {
        cipher.flags = mode;
        Require(EVP_EncryptInit_ex_nid_postfix(Context(context), EvpCipher(&cipher), nullptr, XorKey, nullptr) == 1);
        std::memset(context + OivOffset, 0x77, 32);
        Store(context, NumOffset, 5);
        Require(EVP_EncryptInit_ex_nid_postfix(Context(context), nullptr, nullptr, nullptr, first.data()) == 1);
        const bool counter = mode == ModeCtr;
        const bool chained = mode == ModeCfb || mode == ModeOfb;
        Require(Load<int>(context, NumOffset) == (counter || chained ? 0 : 5));
        Require(context[OivOffset] == (chained ? first[0] : 0x77) && context[IvOffset] == (counter || chained ? first[0] : 0x77));
        std::memset(context + IvOffset, 0x55, 16);
        Require(EVP_EncryptInit_ex_nid_postfix(Context(context), nullptr, nullptr, nullptr, nullptr) == 1);
        Require(context[IvOffset] == (chained ? first[0] : 0x55));
    }
    cipher.flags = ModeGcm;
    Require(EVP_EncryptInit_ex_nid_postfix(Context(context), EvpCipher(&cipher), nullptr, XorKey, first.data()) == 0);
    cipher.flags = ModeCbc;
    cipher.ivLength = 17;
    Require(EVP_EncryptInit_ex_nid_postfix(Context(context), EvpCipher(&cipher), nullptr, XorKey, first.data()) == 0);
    cipher.flags = ModeCtr;
    Require(EVP_EncryptInit_ex_nid_postfix(Context(context), EvpCipher(&cipher), nullptr, XorKey, nullptr) == 1);
    RequireThrows([&] { EVP_EncryptInit_ex_nid_postfix(Context(context), EvpCipher(&cipher), nullptr, XorKey, first.data()); });
    cipher.ivLength = 16;
    cipher.blockSize = 4;
    Require(EVP_EncryptInit_ex_nid_postfix(Context(context), EvpCipher(&cipher), nullptr, XorKey, first.data()) == 0);
    Require(Load<const Cipher*>(context, CipherOffset) == &cipher);
    for (int block : {1, 8}) {
        cipher.blockSize = block;
        Require(EVP_EncryptInit_ex_nid_postfix(Context(context), EvpCipher(&cipher), nullptr, XorKey, first.data()) == 1);
        Require(Load<int>(context, BlockMaskOffset) == block - 1);
    }
    const int inits = calls.inits;
    cipher.flags = FlagAlwaysCallInit;
    Require(EVP_EncryptInit_ex_nid_postfix(Context(context), EvpCipher(&cipher), nullptr, nullptr, nullptr) == 1);
    Require(calls.inits == inits + 1 && calls.initKey == nullptr);
    calls.initResult = 0;
    Require(EVP_EncryptInit_ex_nid_postfix(Context(context), nullptr, nullptr, XorKey, nullptr) == 0);
    calls.initResult = 1;
    cipher.flags = FlagCtrlInit;
    cipher.control = RecordControl;
    cipher.contextSize = 0;
    calls.controls = 0;
    Require(EVP_EncryptInit_ex_nid_postfix(Context(context), EvpCipher(&cipher), nullptr, nullptr, nullptr) == 1);
    Require(calls.controls == 1 && calls.controlType == 0 && calls.controlArg == 0 && calls.controlPointer == nullptr);
    Require(Load<void*>(context, CipherDataOffset) == nullptr);
    calls.controlResult = 0;
    Require(EVP_EncryptInit_ex_nid_postfix(Context(context), EvpCipher(&cipher), nullptr, nullptr, nullptr) == 0);
    Require(EVP_CIPHER_CTX_cleanup_nid_postfix(Context(context)) == 1);
    Require(EVP_EncryptInit_ex_nid_postfix(Context(context), nullptr, nullptr, XorKey, nullptr) == 0);
    int engine = 0;
    RequireThrows([&] { EVP_EncryptInit_ex_nid_postfix(Context(context), EvpCipher(&cipher), &engine, XorKey, nullptr); });
    RequireThrows([] { EVP_CipherInit_ex_nid_postfix(nullptr, nullptr, nullptr, nullptr, nullptr, 1); });
}

static void TestStreamCipher() {
    auto cipher = XorBlockCipher();
    cipher.blockSize = 1;
    cipher.flags = 0;
    ResetCalls();
    unsigned char context[ContextBytes] {};
    const auto input = Pattern(37, 5);
    Bytes output;
    for (std::size_t chunk : {1, 4, 37}) {
        Require(EVP_EncryptInit_ex_nid_postfix(Context(context), EvpCipher(&cipher), nullptr, XorKey, nullptr) == 1);
        const int ciphers = calls.ciphers;
        Require(Process(context, EVP_EncryptUpdate_nid_postfix, EVP_EncryptFinal_ex_nid_postfix, input, chunk, output) == 1);
        Require(output.size() == input.size() && calls.ciphers == ciphers + static_cast<int>((input.size() + chunk - 1) / chunk));
        const auto encrypted = output;
        Require(EVP_DecryptInit_ex_nid_postfix(Context(context), EvpCipher(&cipher), nullptr, XorKey, nullptr) == 1);
        Require(Process(context, EVP_DecryptUpdate_nid_postfix, EVP_DecryptFinal_ex_nid_postfix, encrypted, chunk, output) == 1);
        Require(output == input);
    }
    Require(EVP_CIPHER_CTX_cleanup_nid_postfix(Context(context)) == 1);
}

static void TestCustomCipher() {
    Cipher cipher {};
    cipher.blockSize = 1;
    cipher.flags = FlagCustomCipher | FlagCustomIv;
    cipher.doCipher = RecordCipher;
    ResetCalls();
    unsigned char context[ContextBytes] {};
    unsigned char in[8] {};
    unsigned char out[8] {};
    int written = -1;
    Require(EVP_EncryptInit_ex_nid_postfix(Context(context), EvpCipher(&cipher), nullptr, nullptr, nullptr) == 1);
    calls.cipherResult = 7;
    Require(EVP_EncryptUpdate_nid_postfix(Context(context), out, &written, in, 3) == 1);
    Require(written == 7 && calls.cipherOut == out && calls.cipherIn == in && calls.cipherLength == 3);
    calls.cipherResult = 0;
    Require(EVP_EncryptUpdate_nid_postfix(Context(context), out, &written, in, 0) == 1 && written == 0 && calls.ciphers == 2);
    calls.cipherResult = 2;
    Require(EVP_DecryptUpdate_nid_postfix(Context(context), out, &written, in, -1) == 1);
    Require(written == 2 && calls.cipherLength == static_cast<std::size_t>(-1));
    calls.cipherResult = -1;
    written = -1;
    Require(EVP_EncryptUpdate_nid_postfix(Context(context), out, &written, in, 3) == 0 && written == -1);
    Require(EVP_DecryptUpdate_nid_postfix(Context(context), out, &written, in, 3) == 0 && written == 0);
    calls.cipherResult = 4;
    Require(EVP_EncryptFinal_ex_nid_postfix(Context(context), out, &written) == 1);
    Require(written == 4 && calls.cipherIn == nullptr && calls.cipherLength == 0 && calls.cipherOut == out);
    Require(EVP_DecryptFinal_ex_nid_postfix(Context(context), out, &written) == 1 && written == 4);
    calls.cipherResult = -1;
    written = -1;
    Require(EVP_EncryptFinal_ex_nid_postfix(Context(context), out, &written) == 0 && written == -1);
    Require(EVP_DecryptFinal_ex_nid_postfix(Context(context), out, &written) == 0 && written == 0);
    Require(calls.ciphers == 9);
}

static void TestUpdateArguments() {
    const auto cipher = XorBlockCipher();
    ResetCalls();
    unsigned char context[ContextBytes] {};
    unsigned char in[16] {};
    unsigned char out[32] {};
    int written = -1;
    Require(EVP_EncryptInit_ex_nid_postfix(Context(context), EvpCipher(&cipher), nullptr, XorKey, nullptr) == 1);
    for (Update update : {EVP_EncryptUpdate_nid_postfix, EVP_DecryptUpdate_nid_postfix, EVP_CipherUpdate_nid_postfix}) {
        written = -1;
        Require(update(Context(context), out, &written, in, 0) == 1 && written == 0);
        written = -1;
        Require(update(Context(context), out, &written, in, -1) == 0 && written == 0);
        RequireThrows([&] { update(Context(context), out, nullptr, in, 16); });
        RequireThrows([&] { update(Context(context), out, &written, nullptr, 3); });
        RequireThrows([&] { update(nullptr, out, &written, in, 16); });
    }
    Require(calls.ciphers == 3);
    for (Final final : {EVP_EncryptFinal_ex_nid_postfix, EVP_DecryptFinal_ex_nid_postfix, EVP_CipherFinal_ex_nid_postfix}) {
        RequireThrows([&] { final(Context(context), out, nullptr); });
        RequireThrows([&] { final(nullptr, out, &written); });
    }
    Require(EVP_CIPHER_CTX_cleanup_nid_postfix(Context(context)) == 1);
    RequireThrows([&] { EVP_EncryptUpdate_nid_postfix(Context(context), out, &written, in, 16); });
    RequireThrows([&] { EVP_DecryptFinal_ex_nid_postfix(Context(context), out, &written); });
}

using Descriptor = const LibreSslEvpCipher* (APS5_VABI*)(void);

constexpr const char* Sp80038aKey128 = "2b7e151628aed2a6abf7158809cf4f3c";
constexpr const char* Sp80038aKey192 = "8e73b0f7da0e6452c810f32b809079e562f8ead2522c6b7b";
constexpr const char* Sp80038aKey256 = "603deb1015ca71be2b73aef0857d77811f352c073b6108d72d9810a30914dff4";

static Bytes Hex(const char* text) {
    const auto nibble = [](char digit) { return static_cast<unsigned char>(digit <= '9' ? digit - '0' : digit - 'a' + 10); };
    Bytes bytes;
    for (; text[0] && text[1]; text += 2) bytes.push_back(static_cast<unsigned char>(nibble(text[0]) << 4 | nibble(text[1])));
    return bytes;
}

static void RequireDescriptor(const Cipher* cipher, int nid, std::size_t keyLength, int ivLength, std::uint64_t flags, int contextSize) {
    Require(cipher != nullptr && cipher->nid == nid && cipher->blockSize == 1 && cipher->keyLength == static_cast<int>(keyLength));
    Require(cipher->ivLength == ivLength && cipher->flags == flags && cipher->contextSize == contextSize);
    Require(cipher->init != nullptr && cipher->doCipher != nullptr && cipher->applicationData == nullptr);
    Require(cipher->setAsn1Parameters == nullptr && cipher->getAsn1Parameters == nullptr);
}

static void RequireThreeForms(const Cipher* cipher, const Bytes& key, const Bytes& iv, const Bytes& plain, const Bytes& encrypted) {
    unsigned char context[ContextBytes] {};
    Bytes output;
    for (std::size_t chunk : {1, 7, 64}) {
        Require(EVP_EncryptInit_ex_nid_postfix(Context(context), EvpCipher(cipher), nullptr, key.data(), iv.data()) == 1);
        Require(Process(context, EVP_EncryptUpdate_nid_postfix, EVP_EncryptFinal_ex_nid_postfix, plain, chunk, output) == 1);
        Require(output == encrypted);
        Require(EVP_DecryptInit_ex_nid_postfix(Context(context), EvpCipher(cipher), nullptr, key.data(), iv.data()) == 1);
        Require(Process(context, EVP_DecryptUpdate_nid_postfix, EVP_DecryptFinal_ex_nid_postfix, encrypted, chunk, output) == 1);
        Require(output == plain);
        for (int encrypt : {1, 0}) {
            Require(EVP_CipherInit_ex_nid_postfix(Context(context), EvpCipher(cipher), nullptr, key.data(), iv.data(), encrypt) == 1);
            Require(Process(context, EVP_CipherUpdate_nid_postfix, EVP_CipherFinal_ex_nid_postfix, encrypt ? plain : encrypted, chunk, output) == 1);
            Require(output == (encrypt ? encrypted : plain));
        }
    }
    Require(EVP_CIPHER_CTX_set_padding_nid_postfix(Context(context), 0) == 1);
    Require(EVP_EncryptInit_ex_nid_postfix(Context(context), nullptr, nullptr, key.data(), iv.data()) == 1);
    Require(Process(context, EVP_EncryptUpdate_nid_postfix, EVP_EncryptFinal_ex_nid_postfix, plain, 5, output) == 1);
    Require(output == encrypted);
    Require(EVP_CIPHER_CTX_cleanup_nid_postfix(Context(context)) == 1);
}

static void TestAesCfb8() {
    const struct {
        Descriptor descriptor;
        int nid;
        const char* key;
        const char* encrypted;
    } cases[] {
        {EVP_aes_128_cfb8_nid_postfix, 653, Sp80038aKey128, "3b79424c9c0dd436bace9e0ed4586a4f32b9"},
        {EVP_aes_192_cfb8_nid_postfix, 654, Sp80038aKey192, "cda2521ef0a905ca44cd057cbf0d47a0678a"},
        {EVP_aes_256_cfb8_nid_postfix, 655, Sp80038aKey256, "dc1f1a8520a64db55fcc8ac554844e889700"},
    };
    const auto iv = Hex("000102030405060708090a0b0c0d0e0f");
    const auto plain = Hex("6bc1bee22e409f96e93d7e117393172aae2d");
    for (const auto& test : cases) {
        const auto* cipher = reinterpret_cast<const Cipher*>(test.descriptor());
        const auto key = Hex(test.key);
        const auto encrypted = Hex(test.encrypted);
        Require(EvpCipher(cipher) == test.descriptor());
        RequireDescriptor(cipher, test.nid, key.size(), 16, 0x4003, 264);
        Require(cipher->cleanup == nullptr && cipher->control == nullptr);
        RequireThreeForms(cipher, key, iv, plain, encrypted);
        unsigned char context[ContextBytes] {};
        Require(EVP_EncryptInit_ex_nid_postfix(Context(context), EvpCipher(cipher), nullptr, key.data(), iv.data()) == 1);
        Store(context, NumOffset, 9);
        auto buffer = plain;
        int written = -1;
        Require(EVP_EncryptUpdate_nid_postfix(Context(context), buffer.data(), &written, buffer.data(), 18) == 1);
        Require(written == 18 && buffer == encrypted && Load<int>(context, NumOffset) == 9);
        Require(std::memcmp(context + IvOffset, encrypted.data() + 2, 16) == 0);
        RequireThrows([&] { EVP_EncryptUpdate_nid_postfix(Context(context), nullptr, &written, buffer.data(), 3); });
        Require(EVP_CIPHER_CTX_cleanup_nid_postfix(Context(context)) == 1);
    }
}

static void TestAesCtr() {
    const struct {
        Descriptor descriptor;
        int nid;
        const char* key;
        const char* encrypted;
    } cases[] {
        {EVP_aes_128_ctr_nid_postfix, 904, Sp80038aKey128, "874d6191b620e3261bef6864990db6ce9806f66b7970fdff8617187bb9fffdff5ae4df3edbd5d35e5b4f09020db03eab1e031dda2fbe03d1792170a0f3009cee"},
        {EVP_aes_192_ctr_nid_postfix, 905, Sp80038aKey192, "1abc932417521ca24f2b0459fe7e6e0b090339ec0aa6faefd5ccc2c6f4ce8e941e36b26bd1ebc670d1bd1d665620abf74f78a7f6d29809585a97daec58c6b050"},
        {EVP_aes_256_ctr_nid_postfix, 906, Sp80038aKey256, "601ec313775789a5b7a7f504bbf3d228f443e3ca4d62b59aca84e990cacaf5c52b0930daa23de94ce87017ba2d84988ddfc9c58db67aada613c2dd08457941a6"},
    };
    const auto counter = Hex("f0f1f2f3f4f5f6f7f8f9fafbfcfdfeff");
    const auto plain = Hex("6bc1bee22e409f96e93d7e117393172aae2d8a571e03ac9c9eb76fac45af8e5130c81c46a35ce411e5fbc1191a0a52eff69f2445df4f9b17ad2b417be66c3710");
    for (const auto& test : cases) {
        const auto* cipher = reinterpret_cast<const Cipher*>(test.descriptor());
        const auto key = Hex(test.key);
        const auto encrypted = Hex(test.encrypted);
        Require(EvpCipher(cipher) == test.descriptor());
        RequireDescriptor(cipher, test.nid, key.size(), 16, 0x4005, 264);
        Require(cipher->cleanup == nullptr && cipher->control == nullptr);
        RequireThreeForms(cipher, key, counter, plain, encrypted);
        unsigned char context[ContextBytes] {};
        unsigned char out[64] {};
        int written = -1;
        Require(EVP_EncryptInit_ex_nid_postfix(Context(context), EvpCipher(cipher), nullptr, key.data(), counter.data()) == 1);
        Require(EVP_EncryptUpdate_nid_postfix(Context(context), out, &written, plain.data(), 18) == 1 && written == 18);
        Require(Load<int>(context, NumOffset) == 2 && context[IvOffset + 15] == 0x01 && context[IvOffset + 14] == 0xff);
        for (std::size_t index = 0; index < 16; ++index) Require(context[BufOffset + index] == (plain[16 + index] ^ encrypted[16 + index]));
        Require(EVP_EncryptInit_ex_nid_postfix(Context(context), nullptr, nullptr, nullptr, nullptr) == 1);
        Require(Load<int>(context, NumOffset) == 0);
        Require(EVP_EncryptUpdate_nid_postfix(Context(context), out, &written, plain.data() + 32, 32) == 1 && written == 32);
        Require(std::memcmp(out, encrypted.data() + 32, 32) == 0);
        Require(EVP_CIPHER_CTX_cleanup_nid_postfix(Context(context)) == 1);
    }
    const auto* cipher = reinterpret_cast<const Cipher*>(EVP_aes_128_ctr_nid_postfix());
    const auto key = Hex(Sp80038aKey128);
    const Bytes wrap(16, 0xff);
    unsigned char context[ContextBytes] {};
    unsigned char out[17] {};
    int written = -1;
    Require(EVP_EncryptInit_ex_nid_postfix(Context(context), EvpCipher(cipher), nullptr, key.data(), wrap.data()) == 1);
    Require(EVP_EncryptUpdate_nid_postfix(Context(context), out, &written, plain.data(), 17) == 1 && written == 17);
    Require(Bytes(out, out + 17) == Hex("e13338e36cb71962e00d020b4cedbd86d3"));
    for (std::size_t index = 0; index < 16; ++index) Require(context[IvOffset + index] == (index == 15 ? 1 : 0));
    Store(context, KeyLenOffset, 20);
    Require(EVP_EncryptInit_ex_nid_postfix(Context(context), nullptr, nullptr, key.data(), nullptr) == 0);
    Require(cipher->init(context, nullptr, nullptr, 1) == 0);
    Store(context, NumOffset, 16);
    RequireThrows([&] { EVP_EncryptUpdate_nid_postfix(Context(context), out, &written, plain.data(), 1); });
    Require(EVP_CIPHER_CTX_cleanup_nid_postfix(Context(context)) == 1);
    RequireThrows([&] { cipher->doCipher(context, out, plain.data(), 1); });
}

constexpr int ControlCopy = 0x8;
constexpr int GcmSetIvLength = 0x9;
constexpr int GcmGetTag = 0x10;
constexpr int GcmSetTag = 0x11;
constexpr int GcmSetIvFixed = 0x12;
constexpr int GcmIvGenerate = 0x13;
constexpr int TlsAad = 0x16;
constexpr int GcmSetIvInvocation = 0x18;
constexpr std::size_t GcmContextBytes = 680;
constexpr const char* Gcm3Key = "feffe9928665731c6d6a8f9467308308";
constexpr const char* Gcm3Iv = "cafebabefacedbaddecaf888";
constexpr const char* Gcm3Plain = "d9313225f88406e5a55909c5aff5269a86a7a9531534f7da2e4c303d8a318a721c3c0c95956809532fcf0e2449a6b525b16aedf5aa0de657ba637b391aafd255";
constexpr const char* Gcm4Plain = "d9313225f88406e5a55909c5aff5269a86a7a9531534f7da2e4c303d8a318a721c3c0c95956809532fcf0e2449a6b525b16aedf5aa0de657ba637b39";
constexpr const char* Gcm4Aad = "feedfacedeadbeeffeedfacedeadbeefabaddad2";
constexpr const char* Gcm6Iv = "9313225df88406e555909c5aff5269aa6a7a9538534f7da1e4c303d2a318a728c3c0c95156809539fcf0e2429a6b525416aedbf5a0de6a57a637b39b";

struct GcmVector {
    Descriptor descriptor;
    const char* key;
    const char* iv;
    const char* plain;
    const char* aad;
    const char* encrypted;
    const char* tag;
};

const GcmVector GcmVectors[] {
    {EVP_aes_128_gcm_nid_postfix, "00000000000000000000000000000000", "000000000000000000000000", "", "", "", "58e2fccefa7e3061367f1d57a4e7455a"},
    {EVP_aes_128_gcm_nid_postfix, "00000000000000000000000000000000", "000000000000000000000000", "00000000000000000000000000000000", "",
        "0388dace60b6a392f328c2b971b2fe78", "ab6e47d42cec13bdf53a67b21257bddf"},
    {EVP_aes_128_gcm_nid_postfix, Gcm3Key, Gcm3Iv, Gcm3Plain, "",
        "42831ec2217774244b7221b784d0d49ce3aa212f2c02a4e035c17e2329aca12e21d514b25466931c7d8f6a5aac84aa051ba30b396a0aac973d58e091473f5985",
        "4d5c2af327cd64a62cf35abd2ba6fab4"},
    {EVP_aes_128_gcm_nid_postfix, Gcm3Key, Gcm3Iv, Gcm4Plain, Gcm4Aad,
        "42831ec2217774244b7221b784d0d49ce3aa212f2c02a4e035c17e2329aca12e21d514b25466931c7d8f6a5aac84aa051ba30b396a0aac973d58e091",
        "5bc94fbc3221a5db94fae95ae7121a47"},
    {EVP_aes_128_gcm_nid_postfix, Gcm3Key, "cafebabefacedbad", Gcm4Plain, Gcm4Aad,
        "61353b4c2806934a777ff51fa22a4755699b2a714fcdc6f83766e5f97b6c742373806900e49f24b22b097544d4896b424989b5e1ebac0f07c23f4598",
        "3612d2e79e3b0785561be14aaca2fccb"},
    {EVP_aes_128_gcm_nid_postfix, Gcm3Key, Gcm6Iv, Gcm4Plain, Gcm4Aad,
        "8ce24998625615b603a033aca13fb894be9112a5c3a211a8ba262a3cca7e2ca701e4a9a4fba43c90ccdcb281d48c7c6fd62875d2aca417034c34aee5",
        "619cc5aefffe0bfa462af43c1699d050"},
    {EVP_aes_192_gcm_nid_postfix, "000000000000000000000000000000000000000000000000", "000000000000000000000000", "", "", "", "cd33b28ac773f74ba00ed1f312572435"},
    {EVP_aes_192_gcm_nid_postfix, "000000000000000000000000000000000000000000000000", "000000000000000000000000", "00000000000000000000000000000000", "",
        "98e7247c07f0fe411c267e4384b0f600", "2ff58d80033927ab8ef4d4587514f0fb"},
    {EVP_aes_256_gcm_nid_postfix, "0000000000000000000000000000000000000000000000000000000000000000", "000000000000000000000000", "", "", "",
        "530f8afbc74536b9a963b4f1c4cb738b"},
    {EVP_aes_256_gcm_nid_postfix, "0000000000000000000000000000000000000000000000000000000000000000", "000000000000000000000000",
        "00000000000000000000000000000000", "", "cea7403d4d606b6e074ec5d3baf39d18", "d0d1c8a799996bf0265b98b5d48ab919"},
};

static bool InitForm(unsigned char* context, const void* cipher, const unsigned char* key, const unsigned char* iv, int encrypt, bool generic) {
    if (generic) return EVP_CipherInit_ex_nid_postfix(Context(context), EvpCipher(cipher), nullptr, key, iv, encrypt) == 1;
    if (encrypt) return EVP_EncryptInit_ex_nid_postfix(Context(context), EvpCipher(cipher), nullptr, key, iv) == 1;
    return EVP_DecryptInit_ex_nid_postfix(Context(context), EvpCipher(cipher), nullptr, key, iv) == 1;
}

static Bytes RunGcm(unsigned char* context, const GcmVector& test, int encrypt, bool generic, std::size_t chunk) {
    const auto key = Hex(test.key);
    const auto iv = Hex(test.iv);
    const auto aad = Hex(test.aad);
    const auto input = Hex(encrypt ? test.plain : test.encrypted);
    const Update update = generic ? EVP_CipherUpdate_nid_postfix : encrypt ? EVP_EncryptUpdate_nid_postfix : EVP_DecryptUpdate_nid_postfix;
    Require(InitForm(context, test.descriptor(), nullptr, nullptr, encrypt, generic));
    Require(EVP_CIPHER_CTX_ctrl_nid_postfix(Context(context), GcmSetIvLength, static_cast<int>(iv.size()), nullptr) == 1);
    Require(InitForm(context, nullptr, key.data(), iv.data(), encrypt, generic));
    int written = -1;
    if (!aad.empty()) Require(update(Context(context), nullptr, &written, aad.data(), static_cast<int>(aad.size())) == 1 && written == static_cast<int>(aad.size()));
    Bytes output(input.size(), 0xEE);
    for (std::size_t offset = 0; offset < input.size(); offset += chunk) {
        const int length = static_cast<int>(input.size() - offset < chunk ? input.size() - offset : chunk);
        Require(update(Context(context), output.data() + offset, &written, input.data() + offset, length) == 1 && written == length);
    }
    return output;
}

static void TestAesGcmVectors() {
    for (const auto& test : GcmVectors) {
        const auto* cipher = reinterpret_cast<const Cipher*>(test.descriptor());
        const auto key = Hex(test.key);
        const int nid = key.size() == 16 ? 895 : key.size() == 24 ? 898 : 901;
        Require(EvpCipher(cipher) == test.descriptor());
        RequireDescriptor(cipher, nid, key.size(), 12, 0x305476, 680);
        Require(cipher->cleanup != nullptr && cipher->control != nullptr);
        const auto tag = Hex(test.tag);
        for (std::size_t chunk : {1, 7, 64}) {
            for (bool generic : {false, true}) {
                unsigned char context[ContextBytes] {};
                unsigned char computed[16] {};
                int written = -1;
                Require(RunGcm(context, test, 1, generic, chunk) == Hex(test.encrypted));
                const Final encryptFinal = generic ? EVP_CipherFinal_ex_nid_postfix : EVP_EncryptFinal_ex_nid_postfix;
                Require(encryptFinal(Context(context), computed, &written) == 1 && written == 0);
                Require(EVP_CIPHER_CTX_ctrl_nid_postfix(Context(context), GcmGetTag, 16, computed) == 1 && Bytes(computed, computed + 16) == tag);
                std::memset(computed, 0, sizeof(computed));
                Require(EVP_CIPHER_CTX_ctrl_nid_postfix(Context(context), GcmGetTag, 12, computed) == 1 && std::memcmp(computed, tag.data(), 12) == 0 && computed[12] == 0);
                Require(encryptFinal(Context(context), computed, &written) == 0);
                Require(RunGcm(context, test, 0, generic, chunk) == Hex(test.plain));
                const Final decryptFinal = generic ? EVP_CipherFinal_ex_nid_postfix : EVP_DecryptFinal_ex_nid_postfix;
                Require(decryptFinal(Context(context), computed, &written) == 0 && written == 0);
                Require(EVP_CIPHER_CTX_ctrl_nid_postfix(Context(context), GcmSetTag, 16, const_cast<unsigned char*>(tag.data())) == 1);
                Require(decryptFinal(Context(context), computed, &written) == 1 && written == 0);
                Require(RunGcm(context, test, 0, generic, chunk) == Hex(test.plain));
                Require(EVP_CIPHER_CTX_ctrl_nid_postfix(Context(context), GcmSetTag, 16, const_cast<unsigned char*>(tag.data())) == 1);
                written = -1;
                Require(decryptFinal(Context(context), computed, &written) == 1 && written == 0);
                auto wrong = tag;
                wrong[5] ^= 0x20;
                Require(RunGcm(context, test, 0, generic, chunk) == Hex(test.plain));
                Require(EVP_CIPHER_CTX_ctrl_nid_postfix(Context(context), GcmSetTag, 16, wrong.data()) == 1);
                Require(decryptFinal(Context(context), computed, &written) == 0 && written == 0);
                Require(RunGcm(context, test, 0, generic, chunk) == Hex(test.plain));
                Require(EVP_CIPHER_CTX_ctrl_nid_postfix(Context(context), GcmSetTag, 12, wrong.data()) == 1);
                Require(decryptFinal(Context(context), computed, &written) == 0);
                Require(RunGcm(context, test, 0, generic, chunk) == Hex(test.plain));
                Require(EVP_CIPHER_CTX_ctrl_nid_postfix(Context(context), GcmSetTag, 5, const_cast<unsigned char*>(tag.data())) == 1);
                Require(decryptFinal(Context(context), computed, &written) == 1);
                Require(EVP_CIPHER_CTX_cleanup_nid_postfix(Context(context)) == 1);
                RequireCleared(context);
            }
        }
        for (std::size_t index = 0; index < tag.size(); ++index) {
            unsigned char context[ContextBytes] {};
            unsigned char computed[16] {};
            int written = -1;
            auto wrong = tag;
            wrong[index] ^= 0x01;
            Require(RunGcm(context, test, 0, false, 64) == Hex(test.plain));
            Require(EVP_CIPHER_CTX_ctrl_nid_postfix(Context(context), GcmSetTag, 16, wrong.data()) == 1);
            Require(EVP_DecryptFinal_ex_nid_postfix(Context(context), computed, &written) == 0);
            Require(EVP_CIPHER_CTX_cleanup_nid_postfix(Context(context)) == 1);
        }
    }
}

static void TestAesGcmControl() {
    const auto* cipher = reinterpret_cast<const Cipher*>(EVP_aes_128_gcm_nid_postfix());
    const auto key = Hex(Gcm3Key);
    const auto iv = Hex(Gcm3Iv);
    const auto plain = Hex(Gcm3Plain);
    unsigned char context[ContextBytes] {};
    unsigned char tag[17] {};
    unsigned char out[64] {};
    int written = -1;
    Require(EVP_EncryptInit_ex_nid_postfix(Context(context), EvpCipher(cipher), nullptr, nullptr, nullptr) == 1);
    Require(Load<void*>(context, CipherDataOffset) != nullptr);
    Require(EVP_CIPHER_CTX_ctrl_nid_postfix(Context(context), GcmSetIvLength, 0, nullptr) == 0);
    Require(EVP_CIPHER_CTX_ctrl_nid_postfix(Context(context), GcmSetIvLength, -3, nullptr) == 0);
    Require(EVP_CIPHER_CTX_ctrl_nid_postfix(Context(context), GcmSetTag, 16, tag) == 0);
    Require(EVP_CIPHER_CTX_ctrl_nid_postfix(Context(context), GcmGetTag, 16, tag) == 0);
    Require(EVP_CIPHER_CTX_ctrl_nid_postfix(Context(context), 0x7777, 0, nullptr) == 0);
    Require(EVP_CIPHER_CTX_ctrl_nid_postfix(Context(context), GcmIvGenerate, 8, tag) == 0);
    Require(EVP_EncryptUpdate_nid_postfix(Context(context), out, &written, plain.data(), 16) == 0 && written == -1);
    Require(EVP_EncryptFinal_ex_nid_postfix(Context(context), out, &written) == 0);
    Require(EVP_EncryptInit_ex_nid_postfix(Context(context), nullptr, nullptr, key.data(), nullptr) == 1);
    Require(EVP_EncryptUpdate_nid_postfix(Context(context), out, &written, plain.data(), 16) == 0);
    Require(EVP_EncryptInit_ex_nid_postfix(Context(context), nullptr, nullptr, nullptr, iv.data()) == 1);
    Require(EVP_EncryptUpdate_nid_postfix(Context(context), out, &written, plain.data(), 16) == 1 && written == 16);
    Require(std::memcmp(out, Hex(GcmVectors[2].encrypted).data(), 16) == 0);
    Require(EVP_EncryptUpdate_nid_postfix(Context(context), nullptr, &written, plain.data(), 4) == 0 && written == 16);
    Require(EVP_DecryptUpdate_nid_postfix(Context(context), nullptr, &written, plain.data(), 4) == 0 && written == 0);
    Require(EVP_EncryptUpdate_nid_postfix(Context(context), out, &written, plain.data() + 16, 48) == 1 && written == 48);
    Require(EVP_EncryptFinal_ex_nid_postfix(Context(context), out, &written) == 1 && written == 0);
    Require(EVP_CIPHER_CTX_ctrl_nid_postfix(Context(context), GcmGetTag, 17, tag) == 0);
    Require(EVP_CIPHER_CTX_ctrl_nid_postfix(Context(context), GcmGetTag, 0, tag) == 0);
    Require(EVP_CIPHER_CTX_ctrl_nid_postfix(Context(context), GcmGetTag, 16, tag) == 1);
    Require(Bytes(tag, tag + 16) == Hex(GcmVectors[2].tag));
    Require(EVP_DecryptInit_ex_nid_postfix(Context(context), nullptr, nullptr, key.data(), iv.data()) == 1);
    Require(EVP_CIPHER_CTX_ctrl_nid_postfix(Context(context), GcmGetTag, 16, tag) == 0);
    Require(EVP_CIPHER_CTX_ctrl_nid_postfix(Context(context), GcmSetTag, 17, tag) == 0);
    Require(EVP_CIPHER_CTX_ctrl_nid_postfix(Context(context), GcmSetTag, 0, tag) == 0);
    Require(EVP_DecryptInit_ex_nid_postfix(Context(context), EvpCipher(cipher), nullptr, key.data(), iv.data()) == 1);
    Require(EVP_DecryptFinal_ex_nid_postfix(Context(context), out, &written) == 0);
    Require(EVP_CIPHER_CTX_cleanup_nid_postfix(Context(context)) == 1);

    const auto& longIv = GcmVectors[5];
    Require(EVP_EncryptInit_ex_nid_postfix(Context(context), EvpCipher(cipher), nullptr, nullptr, nullptr) == 1);
    Require(EVP_CIPHER_CTX_ctrl_nid_postfix(Context(context), GcmSetIvLength, 60, nullptr) == 1);
    Require(EVP_EncryptInit_ex_nid_postfix(Context(context), nullptr, nullptr, nullptr, Hex(longIv.iv).data()) == 1);
    for (std::size_t index = 0; index < 16; ++index) Require(context[IvOffset + index] == 0);
    Require(EVP_EncryptInit_ex_nid_postfix(Context(context), nullptr, nullptr, key.data(), nullptr) == 1);
    const auto aad = Hex(longIv.aad);
    Require(EVP_EncryptUpdate_nid_postfix(Context(context), nullptr, &written, aad.data(), static_cast<int>(aad.size())) == 1);
    unsigned char copy[ContextBytes];
    std::memcpy(copy, context, ContextBytes);
    auto* data = malloc_nid_postfix(GcmContextBytes);
    Require(data != nullptr);
    std::memcpy(data, Load<void*>(context, CipherDataOffset), GcmContextBytes);
    Store(copy, CipherDataOffset, data);
    Require(EVP_CIPHER_CTX_ctrl_nid_postfix(Context(context), ControlCopy, 0, copy) == 1);
    for (unsigned char* each : {context, copy}) {
        const auto message = Hex(longIv.plain);
        Require(EVP_EncryptUpdate_nid_postfix(Context(each), out, &written, message.data(), static_cast<int>(message.size())) == 1);
        Require(Bytes(out, out + written) == Hex(longIv.encrypted));
        Require(EVP_EncryptFinal_ex_nid_postfix(Context(each), out, &written) == 1);
        Require(EVP_CIPHER_CTX_ctrl_nid_postfix(Context(each), GcmGetTag, 16, tag) == 1 && Bytes(tag, tag + 16) == Hex(longIv.tag));
        Require(EVP_CIPHER_CTX_cleanup_nid_postfix(Context(each)) == 1);
    }

    Require(EVP_EncryptInit_ex_nid_postfix(Context(context), EvpCipher(cipher), nullptr, nullptr, nullptr) == 1);
    std::memcpy(copy, context, ContextBytes);
    data = malloc_nid_postfix(GcmContextBytes);
    Require(data != nullptr);
    std::memcpy(data, Load<void*>(context, CipherDataOffset), GcmContextBytes);
    Store(copy, CipherDataOffset, data);
    Require(EVP_CIPHER_CTX_ctrl_nid_postfix(Context(context), ControlCopy, 0, copy) == 1);
    Require(EVP_EncryptInit_ex_nid_postfix(Context(copy), nullptr, nullptr, nullptr, iv.data()) == 1);
    Require(std::memcmp(copy + IvOffset, iv.data(), 12) == 0);
    for (std::size_t index = 0; index < 16; ++index) Require(context[IvOffset + index] == 0);
    Require(EVP_EncryptInit_ex_nid_postfix(Context(copy), nullptr, nullptr, key.data(), nullptr) == 1);
    Require(EVP_EncryptUpdate_nid_postfix(Context(copy), out, &written, plain.data(), 64) == 1);
    Require(Bytes(out, out + 64) == Hex(GcmVectors[2].encrypted));
    Require(EVP_CIPHER_CTX_cleanup_nid_postfix(Context(copy)) == 1);
    Require(EVP_CIPHER_CTX_cleanup_nid_postfix(Context(context)) == 1);
    Require(EVP_CIPHER_CTX_ctrl_nid_postfix(Context(context), GcmSetIvLength, 12, nullptr) == 0);
    RequireThrows([&] { cipher->control(context, GcmSetIvLength, 12, nullptr); });
    RequireThrows([&] { cipher->doCipher(context, out, plain.data(), 16); });
}

static Bytes TlsAadFor(int sequence, int length) {
    Bytes aad(13, 0);
    aad[7] = static_cast<unsigned char>(sequence);
    aad[8] = 0x17;
    aad[9] = 0x03;
    aad[10] = 0x03;
    aad[11] = static_cast<unsigned char>(length >> 8);
    aad[12] = static_cast<unsigned char>(length);
    return aad;
}

static void TestAesGcmTls() {
    const auto* cipher = reinterpret_cast<const Cipher*>(EVP_aes_128_gcm_nid_postfix());
    const auto key = Hex(Gcm3Key);
    const auto iv = Hex(Gcm3Iv);
    const auto message = Hex(Gcm3Plain);
    const Bytes plain(message.begin(), message.begin() + 37);
    const Bytes records[] {
        Hex("facedbaddecaf88842831ec2217774244b7221b784d0d49ce3aa212f2c02a4e035c17e2329aca12e21d514b2544c778d019b1b98643bb3d0d3467c192e"),
        Hex("facedbaddecaf8895c21c68aa9787c7294b2d7a47a6e8e4d8adafeea894bf504323d55f62afe5ba118a028444dd9c8388aa114d2b38f3541e5513447cc"),
    };
    unsigned char context[ContextBytes] {};
    int written = -1;
    Require(EVP_EncryptInit_ex_nid_postfix(Context(context), EvpCipher(cipher), nullptr, key.data(), nullptr) == 1);
    Require(EVP_CIPHER_CTX_ctrl_nid_postfix(Context(context), GcmSetIvInvocation, 8, const_cast<unsigned char*>(iv.data())) == 0);
    Require(EVP_CIPHER_CTX_ctrl_nid_postfix(Context(context), GcmSetIvFixed, -1, const_cast<unsigned char*>(iv.data())) == 1);
    for (int sequence : {1, 2}) {
        auto aad = TlsAadFor(sequence, 8 + 37);
        Require(EVP_CIPHER_CTX_ctrl_nid_postfix(Context(context), TlsAad, 13, aad.data()) == 16);
        Require(context[BufOffset + 11] == 0 && context[BufOffset + 12] == 37);
        Bytes record(61, 0xAA);
        std::copy(plain.begin(), plain.end(), record.begin() + 8);
        Require(EVP_EncryptUpdate_nid_postfix(Context(context), record.data(), &written, record.data(), 61) == 1 && written == 61);
        Require(record == records[sequence - 1]);
    }
    auto aad = TlsAadFor(3, 7);
    Require(EVP_CIPHER_CTX_ctrl_nid_postfix(Context(context), TlsAad, 13, aad.data()) == 0);
    Require(EVP_CIPHER_CTX_ctrl_nid_postfix(Context(context), TlsAad, 12, aad.data()) == 0);
    aad = TlsAadFor(3, 8 + 37);
    Require(EVP_CIPHER_CTX_ctrl_nid_postfix(Context(context), TlsAad, 13, aad.data()) == 16);
    Bytes record(61, 0);
    Bytes other(61, 0);
    Require(EVP_EncryptUpdate_nid_postfix(Context(context), other.data(), &written, record.data(), 61) == 0);
    Require(EVP_EncryptUpdate_nid_postfix(Context(context), record.data(), &written, record.data(), 23) == 0);
    Require(EVP_EncryptUpdate_nid_postfix(Context(context), record.data(), &written, record.data(), 61) == 1 && written == 61);
    Require(EVP_EncryptUpdate_nid_postfix(Context(context), record.data(), &written, record.data(), 61) == 0);
    Require(EVP_CIPHER_CTX_cleanup_nid_postfix(Context(context)) == 1);

    Require(EVP_DecryptInit_ex_nid_postfix(Context(context), EvpCipher(cipher), nullptr, key.data(), nullptr) == 1);
    Require(EVP_CIPHER_CTX_ctrl_nid_postfix(Context(context), GcmSetIvFixed, 3, const_cast<unsigned char*>(iv.data())) == 0);
    Require(EVP_CIPHER_CTX_ctrl_nid_postfix(Context(context), GcmSetIvFixed, 5, const_cast<unsigned char*>(iv.data())) == 0);
    Require(EVP_CIPHER_CTX_ctrl_nid_postfix(Context(context), GcmSetIvFixed, 4, const_cast<unsigned char*>(iv.data())) == 1);
    aad = TlsAadFor(1, 8 + 15);
    Require(EVP_CIPHER_CTX_ctrl_nid_postfix(Context(context), TlsAad, 13, aad.data()) == 0);
    for (int sequence : {1, 2}) {
        aad = TlsAadFor(sequence, 61);
        Require(EVP_CIPHER_CTX_ctrl_nid_postfix(Context(context), TlsAad, 13, aad.data()) == 16);
        Require(context[BufOffset + 11] == 0 && context[BufOffset + 12] == 37);
        record = records[sequence - 1];
        Require(EVP_DecryptUpdate_nid_postfix(Context(context), record.data(), &written, record.data(), 61) == 1 && written == 37);
        Require(Bytes(record.begin() + 8, record.begin() + 45) == plain);
    }
    aad = TlsAadFor(2, 61);
    Require(EVP_CIPHER_CTX_ctrl_nid_postfix(Context(context), TlsAad, 13, aad.data()) == 16);
    record = records[1];
    record[60] ^= 1;
    Require(EVP_DecryptUpdate_nid_postfix(Context(context), record.data(), &written, record.data(), 61) == 0 && written == 0);
    for (std::size_t index = 8; index < 45; ++index) Require(record[index] == 0);
    Require(EVP_CIPHER_CTX_cleanup_nid_postfix(Context(context)) == 1);
    for (std::size_t index = 45; index < 61; ++index) {
        Require(EVP_DecryptInit_ex_nid_postfix(Context(context), EvpCipher(cipher), nullptr, key.data(), nullptr) == 1);
        Require(EVP_CIPHER_CTX_ctrl_nid_postfix(Context(context), GcmSetIvFixed, 4, const_cast<unsigned char*>(iv.data())) == 1);
        aad = TlsAadFor(1, 61);
        Require(EVP_CIPHER_CTX_ctrl_nid_postfix(Context(context), TlsAad, 13, aad.data()) == 16);
        record = records[0];
        record[index] ^= 0x01;
        Require(EVP_DecryptUpdate_nid_postfix(Context(context), record.data(), &written, record.data(), 61) == 0 && written == 0);
        Require(EVP_CIPHER_CTX_cleanup_nid_postfix(Context(context)) == 1);
    }
}

static void TestEvpCleanup() {
    const auto* cipher = EVP_aes_128_ctr_nid_postfix();
    EVP_cleanup_nid_postfix();
    EVP_cleanup_nid_postfix();
    Require(EVP_aes_128_ctr_nid_postfix() == cipher);
    const auto key = Hex(Sp80038aKey128);
    const auto counter = Hex("f0f1f2f3f4f5f6f7f8f9fafbfcfdfeff");
    const auto plain = Hex("6bc1bee22e409f96e93d7e117393172a");
    unsigned char context[ContextBytes] {};
    unsigned char out[16] {};
    int written = -1;
    Require(EVP_EncryptInit_ex_nid_postfix(Context(context), EvpCipher(cipher), nullptr, key.data(), counter.data()) == 1);
    EVP_cleanup_nid_postfix();
    Require(EVP_EncryptUpdate_nid_postfix(Context(context), out, &written, plain.data(), 16) == 1 && written == 16);
    Require(Bytes(out, out + 16) == Hex("874d6191b620e3261bef6864990db6ce"));
    Require(EVP_CIPHER_CTX_cleanup_nid_postfix(Context(context)) == 1);
}

int main() {
    InitializeHeap();
    TestInit();
    TestNew();
    TestSetPadding();
    TestCleanup();
    TestFree();
    TestCtrl();
    TestPadding();
    TestHeldBlock();
    TestBadPadding();
    TestNoPadding();
    TestInitFields();
    TestStreamCipher();
    TestCustomCipher();
    TestUpdateArguments();
    TestAesCfb8();
    TestAesCtr();
    TestAesGcmVectors();
    TestAesGcmControl();
    TestAesGcmTls();
    TestEvpCleanup();
    return 0;
}
