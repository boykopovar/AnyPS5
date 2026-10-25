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
    return 0;
}
