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

constexpr std::size_t ContextBytes = 168;
constexpr std::size_t CipherOffset = 0;
constexpr std::size_t EngineOffset = 8;
constexpr std::size_t FlagsOffset = 112;
constexpr std::size_t CipherDataOffset = 120;

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

int main() {
    InitializeHeap();
    TestInit();
    TestNew();
    TestSetPadding();
    TestCleanup();
    TestFree();
    TestCtrl();
    return 0;
}
