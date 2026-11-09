#include "prx/libSceLibreSsl/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <initializer_list>
#include <source_location>

struct TestPkey {
    int type;
    int saveType;
    int references;
    const void* ameth;
    void* engine;
    void* key;
    int saveParameters;
    void* attributes;
};

static_assert(sizeof(TestPkey) == 56 && offsetof(TestPkey, ameth) == 16 && offsetof(TestPkey, key) == 32);

constexpr int PkeyRsa = 6;
constexpr std::uint64_t ExpectingAnRsaKey = 0x06fff07f;
constexpr std::uint64_t DifferentKeyTypes = 0x06fff065;

static void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "LibreSSL EVP_PKEY RSA check failed at line %u\n", location.line());
    std::abort();
}

template<typename TAction> static void RequireThrows(TAction action, std::source_location location = std::source_location::current()) {
    try { action(); } catch (const std::exception&) { return; }
    Require(false, location);
}

static TestPkey* Fields(void* pkey) {
    return static_cast<TestPkey*>(pkey);
}

static void RequireRsaKey(void* pkey, const LibreSslRsa* rsa, std::source_location location = std::source_location::current()) {
    const auto* fields = Fields(pkey);
    Require(fields->type == PkeyRsa && fields->saveType == PkeyRsa && fields->references == 1, location);
    Require(fields->ameth != nullptr && fields->engine == nullptr && fields->key == rsa, location);
    Require(fields->saveParameters == 1 && fields->attributes == nullptr, location);
    Require(ERR_get_error_nid_postfix() == 0, location);
}

struct Finishes {
    int calls;
    LibreSslRsa* rsa;
};

static Finishes finishes;

static int APS5_VABI RecordFinish(LibreSslRsa* rsa) {
    ++finishes.calls;
    finishes.rsa = rsa;
    return 1;
}

static void TestSetAndGet() {
    void* pkey = EVP_PKEY_new_nid_postfix();
    auto* first = RSA_new_nid_postfix();
    auto* second = RSA_new_nid_postfix();
    Require(EVP_PKEY_set1_RSA_nid_postfix(pkey, first) == 1);
    RequireRsaKey(pkey, first);
    Require(first->references == 2);
    const void* method = Fields(pkey)->ameth;

    Require(EVP_PKEY_get1_RSA_nid_postfix(pkey) == first);
    Require(first->references == 3 && ERR_get_error_nid_postfix() == 0);
    RSA_free_nid_postfix(first);
    Require(first->references == 2);

    Require(EVP_PKEY_set1_RSA_nid_postfix(pkey, second) == 1);
    RequireRsaKey(pkey, second);
    Require(first->references == 1 && second->references == 2 && Fields(pkey)->ameth == method);

    Require(EVP_PKEY_set1_RSA_nid_postfix(pkey, second) == 1);
    RequireRsaKey(pkey, second);
    Require(second->references == 2 && Fields(pkey)->ameth == method);

    Fields(pkey)->type = 0;
    Require(EVP_PKEY_get1_RSA_nid_postfix(pkey) == nullptr && second->references == 2);
    Require(ERR_get_error_nid_postfix() == ExpectingAnRsaKey && ERR_get_error_nid_postfix() == 0);
    Fields(pkey)->type = PkeyRsa;

    Require(EVP_PKEY_set1_RSA_nid_postfix(pkey, nullptr) == 0);
    RequireRsaKey(pkey, nullptr);
    Require(second->references == 1 && Fields(pkey)->ameth == method);
    RequireThrows([&] { EVP_PKEY_get1_RSA_nid_postfix(pkey); });

    EVP_PKEY_free_nid_postfix(pkey);
    Require(second->references == 1);
    RSA_free_nid_postfix(first);
    RSA_free_nid_postfix(second);

    pkey = EVP_PKEY_new_nid_postfix();
    first = RSA_new_nid_postfix();
    Require(EVP_PKEY_set1_RSA_nid_postfix(pkey, first) == 1);
    RSA_free_nid_postfix(first);
    RequireThrows([&] { EVP_PKEY_set1_RSA_nid_postfix(pkey, first); });
    RequireRsaKey(pkey, first);
    Require(first->references == 1);
    EVP_PKEY_free_nid_postfix(pkey);

    pkey = EVP_PKEY_new_nid_postfix();
    Require(EVP_PKEY_set1_RSA_nid_postfix(pkey, nullptr) == 0);
    RequireRsaKey(pkey, nullptr);
    EVP_PKEY_free_nid_postfix(pkey);

    RequireThrows([] { EVP_PKEY_set1_RSA_nid_postfix(nullptr, nullptr); });
    RequireThrows([] { EVP_PKEY_get1_RSA_nid_postfix(nullptr); });
}

static void TestGetWithoutRsa() {
    void* pkey = EVP_PKEY_new_nid_postfix();
    TestPkey before;
    std::memcpy(&before, pkey, sizeof(before));
    Require(EVP_PKEY_get1_RSA_nid_postfix(pkey) == nullptr);
    Require(ERR_get_error_nid_postfix() == ExpectingAnRsaKey && ERR_get_error_nid_postfix() == 0);
    Require(std::memcmp(&before, pkey, sizeof(before)) == 0);
    EVP_PKEY_free_nid_postfix(pkey);
}

static void RequireCopy(void* to, const void* from, std::uint64_t error, std::source_location location = std::source_location::current()) {
    TestPkey before;
    std::memcpy(&before, to, sizeof(before));
    Require(EVP_PKEY_copy_parameters_nid_postfix(to, from) == 0, location);
    Require(ERR_get_error_nid_postfix() == error && ERR_get_error_nid_postfix() == 0, location);
    Require(std::memcmp(&before, to, sizeof(before)) == 0, location);
}

static void TestCopyParameters() {
    auto* rsa = RSA_new_nid_postfix();
    void* first = EVP_PKEY_new_nid_postfix();
    void* second = EVP_PKEY_new_nid_postfix();
    void* fresh = EVP_PKEY_new_nid_postfix();
    void* other = EVP_PKEY_new_nid_postfix();
    Require(EVP_PKEY_set1_RSA_nid_postfix(first, rsa) == 1 && EVP_PKEY_set1_RSA_nid_postfix(second, rsa) == 1);
    RequireCopy(first, second, 0);
    RequireCopy(first, first, 0);
    RequireCopy(first, fresh, DifferentKeyTypes);
    RequireCopy(fresh, first, DifferentKeyTypes);
    RequireCopy(fresh, other, 0);
    RequireThrows([&] { EVP_PKEY_copy_parameters_nid_postfix(nullptr, first); });
    RequireThrows([&] { EVP_PKEY_copy_parameters_nid_postfix(first, nullptr); });
    for (void* pkey : {first, second, fresh, other}) EVP_PKEY_free_nid_postfix(pkey);
    RSA_free_nid_postfix(rsa);
}

static void TestFree() {
    auto* rsa = RSA_new_nid_postfix();
    LibreSslRsaMethod method = *rsa->meth;
    method.operations[7] = reinterpret_cast<void*>(RecordFinish);
    rsa->meth = &method;
    finishes = {};
    void* pkey = EVP_PKEY_new_nid_postfix();
    Require(EVP_PKEY_set1_RSA_nid_postfix(pkey, rsa) == 1);
    Fields(pkey)->references = 2;
    EVP_PKEY_free_nid_postfix(pkey);
    Require(Fields(pkey)->references == 1 && rsa->references == 2 && finishes.calls == 0);
    EVP_PKEY_free_nid_postfix(pkey);
    Require(rsa->references == 1 && finishes.calls == 0);
    RSA_free_nid_postfix(rsa);
    Require(finishes.calls == 1 && finishes.rsa == rsa);

    rsa = RSA_new_nid_postfix();
    rsa->meth = &method;
    finishes = {};
    pkey = EVP_PKEY_new_nid_postfix();
    Require(EVP_PKEY_set1_RSA_nid_postfix(pkey, rsa) == 1);
    RSA_free_nid_postfix(rsa);
    Require(rsa->references == 1 && finishes.calls == 0);
    EVP_PKEY_free_nid_postfix(pkey);
    Require(finishes.calls == 1 && finishes.rsa == rsa);
}

int main() {
    void* const api[10] {};
    ApplicationHeapRegister_nid_no_patch(api);
    TestSetAndGet();
    TestGetWithoutRsa();
    TestCopyParameters();
    TestFree();
    std::puts("libSceLibreSsl EVP_PKEY RSA tests passed");
}
