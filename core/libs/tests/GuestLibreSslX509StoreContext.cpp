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

static void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "LibreSSL X509_STORE_CTX check failed at line %u\n", location.line());
    std::abort();
}

template<typename TAction> static void RequireThrows(TAction action, std::source_location location = std::source_location::current()) {
    try { action(); } catch (const std::exception&) { return; }
    Require(false, location);
}

constexpr std::size_t ContextSize = 240;
constexpr std::size_t ChainOffset = 160;
constexpr std::size_t ErrorDepthOffset = 180;
constexpr std::size_t ErrorOffset = 184;
constexpr std::size_t CurrentCertOffset = 192;
constexpr std::size_t ExDataOffset = 232;

struct alignas(8) TestContext {
    unsigned char bytes[ContextSize];

    TestContext() { std::memset(bytes, 0xAA, sizeof(bytes)); }

    template<typename TValue> void Write(std::size_t offset, TValue value) { std::memcpy(bytes + offset, &value, sizeof(value)); }

    template<typename TValue> TValue Read(std::size_t offset) const {
        TValue value;
        std::memcpy(&value, bytes + offset, sizeof(value));
        return value;
    }
};

static LibreSslX509StoreContext* Context(TestContext& context) {
    return reinterpret_cast<LibreSslX509StoreContext*>(context.bytes);
}

static bool OnlyChanged(const TestContext& context, std::size_t offset, std::size_t size) {
    for (std::size_t index = 0; index < ContextSize; ++index)
        if ((index < offset || index >= offset + size) && context.bytes[index] != 0xAA) return false;
    return true;
}

static void TestGetError() {
    TestContext context;
    context.Write<std::int32_t>(ErrorOffset, 0);
    Require(X509_STORE_CTX_get_error_nid_postfix(Context(context)) == 0);
    context.Write<std::int32_t>(ErrorOffset, 20);
    Require(X509_STORE_CTX_get_error_nid_postfix(Context(context)) == 20);
    context.Write<std::int32_t>(ErrorOffset, -7);
    Require(X509_STORE_CTX_get_error_nid_postfix(Context(context)) == -7);
    Require(OnlyChanged(context, ErrorOffset, 4));
    RequireThrows([] { X509_STORE_CTX_get_error_nid_postfix(nullptr); });
}

static void TestSetError() {
    TestContext context;
    X509_STORE_CTX_set_error_nid_postfix(Context(context), 10);
    Require(context.Read<std::int32_t>(ErrorOffset) == 10);
    Require(X509_STORE_CTX_get_error_nid_postfix(Context(context)) == 10);
    X509_STORE_CTX_set_error_nid_postfix(Context(context), -1);
    Require(context.Read<std::int32_t>(ErrorOffset) == -1);
    X509_STORE_CTX_set_error_nid_postfix(Context(context), 0);
    Require(context.Read<std::int32_t>(ErrorOffset) == 0);
    Require(OnlyChanged(context, ErrorOffset, 4));
    RequireThrows([] { X509_STORE_CTX_set_error_nid_postfix(nullptr, 1); });
}

static void TestGetErrorDepth() {
    TestContext context;
    context.Write<std::int32_t>(ErrorDepthOffset, 0);
    Require(X509_STORE_CTX_get_error_depth_nid_postfix(Context(context)) == 0);
    context.Write<std::int32_t>(ErrorDepthOffset, 3);
    Require(X509_STORE_CTX_get_error_depth_nid_postfix(Context(context)) == 3);
    context.Write<std::int32_t>(ErrorOffset, 9);
    Require(X509_STORE_CTX_get_error_depth_nid_postfix(Context(context)) == 3);
    X509_STORE_CTX_set_error_nid_postfix(Context(context), 11);
    Require(X509_STORE_CTX_get_error_depth_nid_postfix(Context(context)) == 3);
    RequireThrows([] { X509_STORE_CTX_get_error_depth_nid_postfix(nullptr); });
}

static void TestGetCurrentCert() {
    TestContext context;
    int certificate = 0;
    context.Write<void*>(CurrentCertOffset, nullptr);
    Require(X509_STORE_CTX_get_current_cert_nid_postfix(Context(context)) == nullptr);
    context.Write<void*>(CurrentCertOffset, &certificate);
    Require(X509_STORE_CTX_get_current_cert_nid_postfix(Context(context)) == &certificate);
    Require(OnlyChanged(context, CurrentCertOffset, 8));
    RequireThrows([] { X509_STORE_CTX_get_current_cert_nid_postfix(nullptr); });
}

static void TestGetChain() {
    TestContext context;
    int chain = 0;
    context.Write<void*>(ChainOffset, nullptr);
    Require(X509_STORE_CTX_get_chain_nid_postfix(Context(context)) == nullptr);
    context.Write<void*>(ChainOffset, &chain);
    Require(X509_STORE_CTX_get_chain_nid_postfix(Context(context)) == &chain);
    Require(OnlyChanged(context, ChainOffset, 8));
    RequireThrows([] { X509_STORE_CTX_get_chain_nid_postfix(nullptr); });
}

static void TestGetExData() {
    TestContext context;
    int first = 0;
    int second = 0;
    char* entries[2] {reinterpret_cast<char*>(&first), reinterpret_cast<char*>(&second)};
    LibreSslStack data {2, entries, 0, 2, nullptr};
    context.Write<void*>(ExDataOffset, nullptr);
    Require(X509_STORE_CTX_get_ex_data_nid_postfix(Context(context), 0) == nullptr);
    Require(X509_STORE_CTX_get_ex_data_nid_postfix(Context(context), 1) == nullptr);
    context.Write<void*>(ExDataOffset, &data);
    Require(X509_STORE_CTX_get_ex_data_nid_postfix(Context(context), 1) == &second);
    Require(X509_STORE_CTX_get_ex_data_nid_postfix(Context(context), 0) == &first);
    Require(X509_STORE_CTX_get_ex_data_nid_postfix(Context(context), 2) == nullptr);
    Require(X509_STORE_CTX_get_ex_data_nid_postfix(Context(context), -1) == nullptr);
    Require(OnlyChanged(context, ExDataOffset, 8));
    Require(data.num == 2 && data.data == entries && entries[0] == reinterpret_cast<char*>(&first) && entries[1] == reinterpret_cast<char*>(&second));
}

static int APS5_VABI CompareCertificates(const void*, const void*) { return 0; }

static void TestGet1Chain() {
    TestContext context;
    context.Write<void*>(ChainOffset, nullptr);
    Require(X509_STORE_CTX_get1_chain_nid_postfix(Context(context)) == nullptr);
    Require(OnlyChanged(context, ChainOffset, 8));

    LibreSslX509 leaf {};
    LibreSslX509 intermediate {};
    LibreSslX509 root {};
    leaf.references = 1;
    intermediate.references = 2;
    root.references = 5;
    char* certificates[6] {reinterpret_cast<char*>(&leaf), reinterpret_cast<char*>(&intermediate), reinterpret_cast<char*>(&root)};
    LibreSslStack chain {3, certificates, 1, 6, CompareCertificates};
    context.Write<void*>(ChainOffset, &chain);
    LibreSslStack* copy = X509_STORE_CTX_get1_chain_nid_postfix(Context(context));
    Require(copy != nullptr && copy != &chain && copy->data != nullptr && copy->data != certificates);
    Require(copy->num == 3 && copy->numAlloc == 6 && copy->sorted == 1 && copy->comp == CompareCertificates);
    Require(copy->data[0] == certificates[0] && copy->data[1] == certificates[1] && copy->data[2] == certificates[2]);
    Require(leaf.references == 2 && intermediate.references == 3 && root.references == 6);
    Require(chain.num == 3 && chain.data == certificates && chain.numAlloc == 6);
    Require(X509_STORE_CTX_get_chain_nid_postfix(Context(context)) == &chain);
    Require(OnlyChanged(context, ChainOffset, 8));
    free_nid_postfix(copy->data);
    free_nid_postfix(copy);

    char* none[4] {};
    LibreSslStack empty {0, none, 0, 4, nullptr};
    context.Write<void*>(ChainOffset, &empty);
    copy = X509_STORE_CTX_get1_chain_nid_postfix(Context(context));
    Require(copy != nullptr && copy != &empty && copy->num == 0 && copy->numAlloc == 4 && copy->comp == nullptr);
    free_nid_postfix(copy->data);
    free_nid_postfix(copy);
    Require(leaf.references == 2 && intermediate.references == 3 && root.references == 6);
}

int main() {
    void* const heap[10] {};
    ApplicationHeapRegister_nid_no_patch(heap);
    TestGetError();
    TestSetError();
    TestGetErrorDepth();
    TestGetCurrentCert();
    TestGetChain();
    TestGetExData();
    TestGet1Chain();
    std::puts("libSceLibreSsl X509_STORE_CTX tests passed");
}
