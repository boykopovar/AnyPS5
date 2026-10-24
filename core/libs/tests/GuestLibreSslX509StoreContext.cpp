#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <source_location>
#include <stdexcept>

extern "C" {
int APS5_VABI X509_STORE_CTX_get_error_nid_postfix(void*);
void APS5_VABI X509_STORE_CTX_set_error_nid_postfix(void*, int);
int APS5_VABI X509_STORE_CTX_get_error_depth_nid_postfix(void*);
}

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
constexpr std::size_t ErrorDepthOffset = 180;
constexpr std::size_t ErrorOffset = 184;

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

static bool OnlyChanged(const TestContext& context, std::size_t offset, std::size_t size) {
    for (std::size_t index = 0; index < ContextSize; ++index)
        if ((index < offset || index >= offset + size) && context.bytes[index] != 0xAA) return false;
    return true;
}

static void TestGetError() {
    TestContext context;
    context.Write<std::int32_t>(ErrorOffset, 0);
    Require(X509_STORE_CTX_get_error_nid_postfix(&context) == 0);
    context.Write<std::int32_t>(ErrorOffset, 20);
    Require(X509_STORE_CTX_get_error_nid_postfix(&context) == 20);
    context.Write<std::int32_t>(ErrorOffset, -7);
    Require(X509_STORE_CTX_get_error_nid_postfix(&context) == -7);
    Require(OnlyChanged(context, ErrorOffset, 4));
    RequireThrows([] { X509_STORE_CTX_get_error_nid_postfix(nullptr); });
}

static void TestSetError() {
    TestContext context;
    X509_STORE_CTX_set_error_nid_postfix(&context, 10);
    Require(context.Read<std::int32_t>(ErrorOffset) == 10);
    Require(X509_STORE_CTX_get_error_nid_postfix(&context) == 10);
    X509_STORE_CTX_set_error_nid_postfix(&context, -1);
    Require(context.Read<std::int32_t>(ErrorOffset) == -1);
    X509_STORE_CTX_set_error_nid_postfix(&context, 0);
    Require(context.Read<std::int32_t>(ErrorOffset) == 0);
    Require(OnlyChanged(context, ErrorOffset, 4));
    RequireThrows([] { X509_STORE_CTX_set_error_nid_postfix(nullptr, 1); });
}

static void TestGetErrorDepth() {
    TestContext context;
    context.Write<std::int32_t>(ErrorDepthOffset, 0);
    Require(X509_STORE_CTX_get_error_depth_nid_postfix(&context) == 0);
    context.Write<std::int32_t>(ErrorDepthOffset, 3);
    Require(X509_STORE_CTX_get_error_depth_nid_postfix(&context) == 3);
    context.Write<std::int32_t>(ErrorOffset, 9);
    Require(X509_STORE_CTX_get_error_depth_nid_postfix(&context) == 3);
    X509_STORE_CTX_set_error_nid_postfix(&context, 11);
    Require(X509_STORE_CTX_get_error_depth_nid_postfix(&context) == 3);
    RequireThrows([] { X509_STORE_CTX_get_error_depth_nid_postfix(nullptr); });
}

int main() {
    TestGetError();
    TestSetError();
    TestGetErrorDepth();
    std::puts("libSceLibreSsl X509_STORE_CTX tests passed");
}
