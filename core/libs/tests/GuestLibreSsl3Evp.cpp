#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include "prx/libc/libc.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <source_location>
#include <stdexcept>
#include <string>

using LibreSsl3::DigestContext;
using LibreSsl3::DigestMethod;


static void Require(bool value, std::source_location location = std::source_location::current()) {
    if (value) return;
    std::fprintf(stderr, "LibreSSL3 EVP check failed at line %u\n", location.line());
    std::abort();
}

template<typename TAction> static void Throws(TAction action) {
    try { action(); } catch (const std::exception&) { return; }
    Require(false);
}

static bool failAllocation;
static std::array<std::pair<void*, std::size_t>, 2> expectedCleared{};
static unsigned clearedCount;
static unsigned initCount;
static unsigned cleanupCount;
static std::uint64_t cleanupFlags;
static bool clearKeyContext;

static int APS5_VABI CustomInit(DigestContext* context) {
    ++initCount;
    Require(context->flags == 0 && !context->keyContext);
    if (!context->data) return 19;
    for (int index = 0; index < context->digest->contextSize; ++index) Require(static_cast<unsigned char*>(context->data)[index] == 0);
    std::memset(context->data, 0x5b, context->digest->contextSize);
    return 0;
}

static int APS5_VABI CustomCleanup(DigestContext* context) {
    ++cleanupCount;
    context->flags |= cleanupFlags;
    if (clearKeyContext) context->keyContext = nullptr;
    return 0;
}

static void* APS5_VABI Allocate(std::size_t size) { return std::malloc(size); }
static void APS5_VABI Free(void* memory) {
    for (auto& [pointer, size] : expectedCleared) {
        if (!pointer || pointer != memory) continue;
        for (std::size_t index = 0; index < size; ++index) Require(static_cast<unsigned char*>(memory)[index] == 0);
        pointer = nullptr;
        ++clearedCount;
    }
    std::free(memory);
}
static void* APS5_VABI Calloc(std::size_t count, std::size_t size) { return failAllocation ? nullptr : std::calloc(count, size); }
static void* APS5_VABI Reallocate(void* memory, std::size_t size) { return std::realloc(memory, size); }
static void* APS5_VABI Align(std::size_t, std::size_t size) { return std::malloc(size); }
static void* APS5_VABI Realign(void* memory, std::size_t size, std::size_t) { return std::realloc(memory, size); }
static int APS5_VABI PosixAlign(void** memory, std::size_t, std::size_t size) {
    *memory = std::malloc(size);
    return *memory ? 0 : 12;
}

static std::string Hash(const DigestMethod* method, const std::string& input, std::size_t stride) {
    auto* context = EVP_MD_CTX_new_nid_postfix();
    Require(context != nullptr);
    const DigestContext empty{};
    Require(std::memcmp(context, &empty, sizeof(empty)) == 0);
    Require(EVP_DigestInit_nid_postfix(context, method) == 1);
    Require(context->digest == method && context->flags == 0 && !context->keyContext && context->update == method->update);
    Require(context->update(context, nullptr, 0) == 1);
    for (std::size_t offset = 0; offset < input.size();) {
        const std::size_t count = std::min(stride, input.size() - offset);
        Require(context->update(context, input.data() + offset, count) == 1);
        offset += count;
    }
    std::array<unsigned char, 64> digest;
    digest.fill(0xa5);
    Require(method->finalize(context, digest.data()) == 1);
    Require(digest[method->digestSize] == 0xa5);
    std::string result;
    for (int index = 0; index < method->digestSize; ++index) {
        result += "0123456789abcdef"[digest[index] >> 4];
        result += "0123456789abcdef"[digest[index] & 15];
    }
    EVP_MD_CTX_free_nid_postfix(context);
    return result;
}

static void Methods() {
    const auto* sha1 = EVP_sha1_nid_postfix();
    const auto* sha224 = EVP_sha224_nid_postfix();
    Require(sha1 == EVP_sha1_nid_postfix() && sha224 == EVP_sha224_nid_postfix() && sha1 != sha224);
    Require(sha1->type == 64 && sha1->keyType == 65 && sha1->digestSize == 20 && sha1->contextSize == 104);
    Require(sha224->type == 675 && sha224->keyType == 671 && sha224->digestSize == 28 && sha224->contextSize == 120);
    for (const auto* method : {sha1, sha224}) {
        Require(method->flags == 8 && method->blockSize == 64);
        Require(!method->copy && !method->cleanup && !method->control);
        Require(!method->sign && !method->verify && method->requiredKeyTypes[0] == 6 && method->requiredKeyTypes[1] == 19);
        Throws([&] { method->init(nullptr); });
        DigestContext noData{};
        noData.digest = method;
        noData.flags = 0x1234;
        noData.keyContext = &noData;
        noData.update = method->update;
        const auto saved = noData;
        Require(method->update(&noData, nullptr, 0) == 1);
        Require(std::memcmp(&noData, &saved, sizeof(saved)) == 0);
        Throws([&] { method->update(nullptr, nullptr, 0); });
        Throws([&] { method->update(&noData, "a", 1); });
    }
    Require(Hash(sha1, "", 1) == "da39a3ee5e6b4b0d3255bfef95601890afd80709");
    Require(Hash(sha1, "abc", 1) == "a9993e364706816aba3e25717850c26c9cd0d89d");
    Require(Hash(sha224, "", 1) == "d14a028c2a3a2bc9476102bb288234c415a2b01f828ea62ac5b3e42f");
    Require(Hash(sha224, "abc", 2) == "23097d223405d8228642a477bda255b32aadbce4bda0b3f7e36c9da7");
    Require(Hash(sha1, std::string(1000000, 'a'), 63) == "34aa973cd4c4daa4f61eeb2bdbad27316534016f");
    Require(Hash(sha224, std::string(1000000, 'a'), 65) == "20794655980c91d8bbb4c1ea97618a4bf03f42581948b2ee4ee7ad67");
    for (std::size_t length : {55, 56, 63, 64, 65, 119, 120, 127, 128, 129}) {
        const std::string data(length, static_cast<char>(length));
        Require(Hash(sha1, data, 1) == Hash(sha1, data, 70));
        Require(Hash(sha224, data, 1) == Hash(sha224, data, 70));
    }
}

static void Ownership() {
    EVP_MD_CTX_free_nid_postfix(nullptr);
    auto* context = EVP_MD_CTX_new_nid_postfix();
    Require(EVP_DigestInit_nid_postfix(context, EVP_sha1_nid_postfix()) == 1);
    void* oldData = context->data;
    context->flags = 0xffff;
    context->keyContext = context;
    Require(EVP_DigestInit_nid_postfix(context, EVP_sha224_nid_postfix()) == 1);
    Require(context->data != oldData && context->flags == 0 && !context->keyContext);
    free_nid_postfix(oldData);
    context->flags = 4 | 0x400;
    context->keyContext = context;
    auto* retained = static_cast<unsigned char*>(context->data);
    std::memset(retained, 0x5a, 120);
    EVP_MD_CTX_free_nid_postfix(context);
    for (int index = 0; index < 120; ++index) Require(retained[index] == 0x5a);
    free_nid_postfix(retained);

    context = EVP_MD_CTX_new_nid_postfix();
    DigestMethod foreign{};
    foreign.contextSize = -1;
    context->digest = &foreign;
    context->flags = 0x123;
    DigestContext saved = *context;
    Throws([&] { EVP_DigestInit_nid_postfix(context, &foreign); });
    Require(std::memcmp(context, &saved, sizeof(saved)) == 0);
    EVP_MD_CTX_free_nid_postfix(context);
    context = EVP_MD_CTX_new_nid_postfix();
    context->digest = &foreign;
    context->data = malloc_nid_postfix(1);
    saved = *context;
    Throws([&] { EVP_MD_CTX_free_nid_postfix(context); });
    Require(std::memcmp(context, &saved, sizeof(saved)) == 0);
    free_nid_postfix(context->data);
    context->data = nullptr;
    context->digest = nullptr;
    context->flags = 0;
    EVP_MD_CTX_free_nid_postfix(context);
    Throws([&] { EVP_DigestInit_nid_postfix(nullptr, EVP_sha1_nid_postfix()); });
    DigestContext stack{};
    Throws([&] { EVP_DigestInit_nid_postfix(&stack, nullptr); });
}

static void AllocationAndClearing() {
    auto* context = EVP_MD_CTX_new_nid_postfix();
    Require(EVP_DigestInit_nid_postfix(context, EVP_sha224_nid_postfix()) == 1);
    std::memset(context->data, 0xa5, 120);
    expectedCleared = {{{context->data, 120}, {context, sizeof(*context)}}};
    EVP_MD_CTX_free_nid_postfix(context);
    Require(clearedCount == 2);
    context = EVP_MD_CTX_new_nid_postfix();
    failAllocation = true;
    Require(EVP_MD_CTX_new_nid_postfix() == nullptr);
    Throws([&] { EVP_DigestInit_nid_postfix(context, EVP_sha1_nid_postfix()); });
    Require(context->digest == EVP_sha1_nid_postfix() && !context->data && !context->keyContext);
    Require(context->update == context->digest->update && context->flags == 0);
    failAllocation = false;
    EVP_MD_CTX_free_nid_postfix(context);
}

static void CustomMethods() {
    DigestMethod method{999, 0, 7, 0, CustomInit, nullptr, nullptr, nullptr, CustomCleanup, nullptr, nullptr, {}, 8, 17, nullptr};
    auto* context = EVP_MD_CTX_new_nid_postfix();
    Require(EVP_DigestInit_nid_postfix(context, &method) == 0 && initCount == 1);
    expectedCleared = {{{context->data, 17}, {context, sizeof(*context)}}};
    EVP_MD_CTX_free_nid_postfix(context);
    Require(cleanupCount == 1 && clearedCount == 4);

    context = EVP_MD_CTX_new_nid_postfix();
    Require(EVP_DigestInit_nid_postfix(context, &method) == 0);
    context->flags = 2;
    EVP_MD_CTX_free_nid_postfix(context);
    Require(cleanupCount == 1);

    context = EVP_MD_CTX_new_nid_postfix();
    Require(EVP_DigestInit_nid_postfix(context, &method) == 0);
    auto* retained = static_cast<unsigned char*>(context->data);
    cleanupFlags = 4 | 0x400;
    context->keyContext = context;
    EVP_MD_CTX_free_nid_postfix(context);
    Require(cleanupCount == 2);
    for (int index = 0; index < 17; ++index) Require(retained[index] == 0x5b);
    free_nid_postfix(retained);
    cleanupFlags = 0;

    method.contextSize = 0;
    method.update = EVP_sha1_nid_postfix()->update;
    context = EVP_MD_CTX_new_nid_postfix();
    Require(EVP_DigestInit_nid_postfix(context, &method) == 19 && !context->data && !context->update);
    EVP_MD_CTX_free_nid_postfix(context);
    Require(cleanupCount == 3);

    context = EVP_MD_CTX_new_nid_postfix();
    Require(EVP_DigestInit_nid_postfix(context, &method) == 19);
    context->keyContext = context;
    clearKeyContext = true;
    EVP_MD_CTX_free_nid_postfix(context);
    Require(cleanupCount == 4);
    clearKeyContext = false;
}

int main() {
    void* api[10]{reinterpret_cast<void*>(&Allocate), reinterpret_cast<void*>(&Free), reinterpret_cast<void*>(&Calloc),
        reinterpret_cast<void*>(&Reallocate), reinterpret_cast<void*>(&Align), reinterpret_cast<void*>(&Realign), reinterpret_cast<void*>(&PosixAlign)};
    ApplicationHeapRegister_nid_no_patch(api);
    Methods();
    Ownership();
    AllocationAndClearing();
    CustomMethods();
    std::puts("LibreSSL3 EVP tests passed");
}
