#include "prx/libSceLibreSsl/include/Api.hpp"
#include "prx/libSceLibreSsl/include/Error.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <source_location>
#include <stdexcept>

namespace {

struct TestBignum {
    std::uint64_t* d;
    int top;
    int dmax;
    int neg;
    int flags;
};

constexpr unsigned int UntouchedLength = 0xabcdef;
constexpr std::uint64_t Sentinel = (8u << 24) | (0xfffu << 12) | 123u;
int CallbackResult = 0;
int CallbackReason = 0;
int CallbackCalls = 0;

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "RSA_sign error check failed at line %u\n", location.line());
    std::abort();
}

std::uint64_t RsaError(int reason) {
    return (4u << 24) | (0xfffu << 12) | static_cast<unsigned int>(reason);
}

void SeedError() {
    Require(ERR_get_error_nid_postfix() == 0);
    LibreSslPutError_nid_no_patch(8, 0xfff, 123);
}

void CheckErrors(int reason, bool objectError = false) {
    Require(ERR_get_error_nid_postfix() == Sentinel);
    if (objectError) Require(ERR_get_error_nid_postfix() == ((8u << 24) | (0xfffu << 12) | 101u));
    const auto actual = ERR_get_error_nid_postfix();
    if (actual != (reason ? RsaError(reason) : 0))
        std::fprintf(stderr, "RSA_sign error 0x%llx expected RSA/fff/%d\n", static_cast<unsigned long long>(actual), reason);
    Require(actual == (reason ? RsaError(reason) : 0));
    Require(ERR_get_error_nid_postfix() == 0);
}

int APS5_VABI PrivateEncrypt(int length, const unsigned char* from, unsigned char* to, LibreSslRsa*, int padding) {
    ++CallbackCalls;
    Require(padding == 1);
    Require(length > 0 && from != nullptr);
    if (length == 51) {
        const unsigned char header[] {0x30, 0x31, 0x30, 0x0d, 0x06, 0x09, 0x60, 0x86, 0x48, 0x01, 0x65, 0x03, 0x04, 0x02, 0x01, 0x05, 0x00, 0x04, 0x20};
        Require(std::memcmp(from, header, sizeof(header)) == 0);
        for (int index = sizeof(header); index < length; ++index) Require(from[index] == 0);
    } else {
        Require(length == 36);
        for (int index = 0; index < length; ++index) Require(from[index] == 0);
    }
    if (CallbackReason) LibreSslPutError_nid_no_patch(4, 0xfff, CallbackReason);
    to[0] = 0x42;
    return CallbackResult;
}

int APS5_VABI Sign(int type, const unsigned char*, unsigned int length, unsigned char* to, unsigned int* written, const LibreSslRsa*) {
    ++CallbackCalls;
    Require(type == -1 && length == 1);
    if (CallbackReason) LibreSslPutError_nid_no_patch(4, 0xfff, CallbackReason);
    to[0] = 0x42;
    *written = 7;
    return CallbackResult;
}

void TestEarlyFailures(LibreSslRsa& rsa, int reason = 0) {
    const struct { int type; unsigned int length; int words; int reason; } cases[] {
        {114, 0, 8, 131}, {114, 35, 8, 131}, {114, 37, 8, 131},
        {-1, 32, 8, 117}, {-100, 32, 8, 117}, {0, 32, 8, 116},
        {672, 32, 7, 112}, {674, 64, 8, 112}, {114, 36, 5, 112},
    };
    for (const auto& scenario : cases) {
        if (reason && reason != scenario.reason) continue;
        static_cast<TestBignum*>(rsa.n)->top = scenario.words;
        std::array<unsigned char, 128> signature;
        signature.fill(0xa5);
        std::array<unsigned char, 64> digest {};
        unsigned int written = UntouchedLength;
        CallbackCalls = 0;
        SeedError();
        Require(RSA_sign_nid_postfix(scenario.type, digest.data(), scenario.length, signature.data(), &written, &rsa) == 0);
        Require(written == UntouchedLength);
        Require(std::all_of(signature.begin(), signature.end(), [](auto byte) { return byte == 0xa5; }));
        Require(CallbackCalls == 0);
        CheckErrors(scenario.reason);
    }
    static_cast<TestBignum*>(rsa.n)->top = 8;
}

void TestObjectTable(LibreSslRsa& rsa) {
    Require(OBJ_create_nid_postfix("1.3.6.1.4.1.55555.12", "rsaSignTest", "RSA sign test") >= 956);
    for (int type : {-1, 0}) {
        unsigned char signature[128];
        std::memset(signature, 0xa5, sizeof(signature));
        unsigned int written = UntouchedLength;
        SeedError();
        Require(RSA_sign_nid_postfix(type, nullptr, 0, signature, &written, &rsa) == 0);
        Require(written == UntouchedLength);
        for (auto byte : signature) Require(byte == 0xa5);
        CheckErrors(type < 0 ? 117 : 116, type < 0);
    }
    OBJ_cleanup_nid_postfix();
}

void TestPrivateEncrypt(LibreSslRsa& rsa) {
    for (int type : {114, 672}) {
        for (int result : {-1, 0, 64}) {
            for (int reason : {0, 104}) {
                std::array<unsigned char, 128> signature;
                signature.fill(0xa5);
                std::array<unsigned char, 36> digest {};
                unsigned int written = UntouchedLength;
                CallbackResult = result;
                CallbackReason = reason;
                CallbackCalls = 0;
                SeedError();
                Require(RSA_sign_nid_postfix(type, digest.data(), type == 114 ? 36 : 32, signature.data(), &written, &rsa) == (result > 0));
                Require(written == (result > 0 ? 64 : UntouchedLength));
                Require(signature[0] == 0x42);
                Require(std::all_of(signature.begin() + 1, signature.end(), [](auto byte) { return byte == 0xa5; }));
                Require(CallbackCalls == 1);
                CheckErrors(reason);
            }
        }
    }
}

void TestCustomSign(LibreSslRsa& rsa) {
    rsa.flags = 0x40;
    for (int result : {0, 1, 7}) {
        for (int reason : {0, 104}) {
            unsigned char signature[2] {0xa5, 0xa5};
            unsigned int written = UntouchedLength;
            CallbackResult = result;
            CallbackReason = reason;
            CallbackCalls = 0;
            SeedError();
            Require(RSA_sign_nid_postfix(-1, nullptr, 1, signature, &written, &rsa) == result);
            Require(written == 7 && signature[0] == 0x42 && signature[1] == 0xa5);
            Require(CallbackCalls == 1);
            CheckErrors(reason);
        }
    }
    rsa.flags = 0;
}

void TestUnsupported(LibreSslRsa& rsa) {
    unsigned char signature[128];
    std::memset(signature, 0xa5, sizeof(signature));
    unsigned int written = UntouchedLength;
    SeedError();
    bool threw = false;
    try {
        RSA_sign_nid_postfix(1, nullptr, 0, signature, &written, &rsa);
    } catch (const std::exception&) {
        threw = true;
    }
    Require(threw);
    Require(written == UntouchedLength);
    for (auto byte : signature) Require(byte == 0xa5);
    CheckErrors(0);
}

} // namespace

int main(int argc, char** argv) {
    std::uint64_t words[8];
    std::fill(std::begin(words), std::end(words), UINT64_MAX);
    TestBignum modulus {words, 8, 8, 0, 2};
    LibreSslRsaMethod method {};
    method.operations[2] = reinterpret_cast<void*>(PrivateEncrypt);
    method.sign = reinterpret_cast<void*>(Sign);
    LibreSslRsa rsa {};
    rsa.meth = &method;
    rsa.n = &modulus;
    if (argc == 2) {
        TestEarlyFailures(rsa, std::atoi(argv[1]));
        return 0;
    }
    TestEarlyFailures(rsa);
    TestObjectTable(rsa);
    TestEarlyFailures(rsa);
    TestPrivateEncrypt(rsa);
    TestCustomSign(rsa);
    TestUnsupported(rsa);
}
