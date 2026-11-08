#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"

#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <stdexcept>

namespace {

using MethodBytes = std::array<unsigned char, 184>;

int checks = 0;
int cases = 0;
int queryCalls = 0;
int parameterCalls = 0;
int publicCalls = 0;
int queryResult = 0;
int parameterResult = 0;
int publicResult = 0;
const LibreSsl3EvpPkey* expectedFirst;
const LibreSsl3EvpPkey* expectedSecond;
const void* replacementMethod;

void Require(bool condition, const char* message) {
    ++checks;
    if (!condition) {
        std::fprintf(stderr, "%s\n", message);
        std::abort();
    }
}

template<class TCallback>
void SetCallback(MethodBytes& method, std::size_t offset, TCallback callback) {
    std::memcpy(method.data() + offset, &callback, sizeof(callback));
}

int APS5_VABI Query(const LibreSsl3EvpPkey* key) {
    Require(key == expectedFirst, "query key identity");
    ++queryCalls;
    return queryResult;
}

int APS5_VABI CompareParameters(const LibreSsl3EvpPkey* first, const LibreSsl3EvpPkey* second) {
    Require(first == expectedFirst && second == expectedSecond, "parameter operands");
    ++parameterCalls;
    if (replacementMethod)
        const_cast<LibreSsl3EvpPkey*>(first)->method = replacementMethod;
    return parameterResult;
}

int APS5_VABI ComparePublic(const LibreSsl3EvpPkey* first, const LibreSsl3EvpPkey* second) {
    Require(first == expectedFirst && second == expectedSecond, "public operands");
    ++publicCalls;
    return publicResult;
}

int APS5_VABI WrongQuery(const LibreSsl3EvpPkey*) {
    Require(false, "query dispatched neighboring callback");
    return 0;
}

int APS5_VABI WrongComparison(const LibreSsl3EvpPkey*, const LibreSsl3EvpPkey*) {
    Require(false, "comparison dispatched neighboring callback");
    return 0;
}

int APS5_VABI ThrowQuery(const LibreSsl3EvpPkey*) {
    throw std::runtime_error("query callback failure");
}

int APS5_VABI ThrowComparison(const LibreSsl3EvpPkey*, const LibreSsl3EvpPkey*) {
    throw std::runtime_error("comparison callback failure");
}

template<class TException, class TAction>
void RequireThrows(TAction action) {
    bool caught = false;
    try {
        action();
    } catch (const TException&) {
        caught = true;
    }
    Require(caught, "required exception missing");
}

template<class TObject>
auto Snapshot(const TObject& object) {
    std::array<unsigned char, sizeof(TObject)> bytes;
    std::memcpy(bytes.data(), &object, bytes.size());
    return bytes;
}

constexpr std::array Results{std::numeric_limits<int>::min(), -7, -2, -1, 0, 1, 2, 128, 256, 4096, std::numeric_limits<int>::max()};

void CheckQueries() {
    Require(EVP_PKEY_size_nid_postfix(nullptr) == 0, "null key size");
    Require(EVP_PKEY_security_bits_nid_postfix(nullptr) == 0, "null key security bits");
    std::array<unsigned char, 32> payload{};
    LibreSsl3EvpPkey key{6, 17, nullptr, payload.data(), -3};
    const auto original = Snapshot(key);
    Require(EVP_PKEY_size_nid_postfix(&key) == 0, "missing size method");
    Require(EVP_PKEY_security_bits_nid_postfix(&key) == -2, "missing security method");
    Require(Snapshot(key) == original, "missing-method queries mutated key");
    MethodBytes method;
    method.fill(0xa5);
    for (std::size_t offset : {88, 104, 120})
        SetCallback(method, offset, WrongQuery);
    key.method = method.data();
    expectedFirst = &key;
    for (int mask = 0; mask != 4; ++mask) {
        SetCallback(method, 96, (mask & 1) ? Query : nullptr);
        SetCallback(method, 112, (mask & 2) ? Query : nullptr);
        for (int result : Results) {
            ++cases;
            queryResult = result;
            const auto before = Snapshot(key);
            const auto methodBefore = method;
            const auto payloadBefore = payload;
            int calls = queryCalls;
            Require(EVP_PKEY_size_nid_postfix(&key) == ((mask & 1) ? result : 0), "size callback result");
            Require(queryCalls == calls + ((mask & 1) != 0), "size callback count");
            calls = queryCalls;
            Require(EVP_PKEY_security_bits_nid_postfix(&key) == ((mask & 2) ? result : -2), "security callback result");
            Require(queryCalls == calls + ((mask & 2) != 0), "security callback count");
            Require(Snapshot(key) == before && method == methodBefore && payload == payloadBefore, "query mutated input");
        }
    }
    SetCallback(method, 96, ThrowQuery);
    SetCallback(method, 112, ThrowQuery);
    RequireThrows<std::runtime_error>([&] { EVP_PKEY_size_nid_postfix(&key); });
    RequireThrows<std::runtime_error>([&] { EVP_PKEY_security_bits_nid_postfix(&key); });
}

void CheckComparison() {
    std::array<unsigned char, 32> firstPayload{};
    std::array<unsigned char, 32> secondPayload{};
    MethodBytes method;
    method.fill(0x5a);
    for (std::size_t offset : {48, 64, 152, 168})
        SetCallback(method, offset, WrongComparison);
    LibreSsl3EvpPkey first{6, 41, method.data(), firstPayload.data(), 1};
    LibreSsl3EvpPkey second{6, 73, nullptr, secondPayload.data(), 0};
    expectedFirst = &first;
    expectedSecond = &second;
    RequireThrows<std::invalid_argument>([&] { EVP_PKEY_cmp_nid_postfix(nullptr, &second); });
    RequireThrows<std::invalid_argument>([&] { EVP_PKEY_cmp_nid_postfix(&first, nullptr); });
    RequireThrows<std::invalid_argument>([&] { EVP_PKEY_cmp_nid_postfix(nullptr, nullptr); });
    for (int mask = 0; mask != 16; ++mask) {
        first.type = (mask & 1) ? 6 : 912;
        first.method = (mask & 2) ? method.data() : nullptr;
        SetCallback(method, 160, (mask & 4) ? CompareParameters : nullptr);
        SetCallback(method, 56, (mask & 8) ? ComparePublic : nullptr);
        for (int parameters : Results) {
            for (int publicKey : Results) {
                ++cases;
                parameterResult = parameters;
                publicResult = publicKey;
                const auto firstBefore = Snapshot(first);
                const auto secondBefore = Snapshot(second);
                const auto methodBefore = method;
                const auto payloadBefore = firstPayload;
                const auto otherPayloadBefore = secondPayload;
                const int parameterBefore = parameterCalls;
                const int publicBefore = publicCalls;
                const bool invokesParameters = (mask & 7) == 7;
                const bool invokesPublic = (mask & 11) == 11 && (!invokesParameters || parameters > 0);
                int expected = -2;
                if (!(mask & 1)) expected = -1;
                else if (invokesParameters && parameters <= 0) expected = parameters;
                else if (invokesPublic) expected = publicKey;
                Require(EVP_PKEY_cmp_nid_postfix(&first, &second) == expected, "comparison result");
                Require(parameterCalls == parameterBefore + invokesParameters, "parameter dispatch count");
                Require(publicCalls == publicBefore + invokesPublic, "public dispatch count");
                Require(Snapshot(first) == firstBefore && Snapshot(second) == secondBefore, "comparison mutated keys");
                Require(method == methodBefore && firstPayload == payloadBefore && secondPayload == otherPayloadBefore, "comparison mutated methods or payloads");
            }
        }
    }
    first.type = second.type;
    first.method = method.data();
    SetCallback(method, 160, ThrowComparison);
    const int calls = publicCalls;
    RequireThrows<std::runtime_error>([&] { EVP_PKEY_cmp_nid_postfix(&first, &second); });
    Require(publicCalls == calls, "public comparison after parameter exception");
    SetCallback(method, 160, static_cast<decltype(&CompareParameters)>(nullptr));
    SetCallback(method, 56, ThrowComparison);
    RequireThrows<std::runtime_error>([&] { EVP_PKEY_cmp_nid_postfix(&first, &second); });
    SetCallback(method, 56, ComparePublic);
    expectedSecond = &first;
    publicResult = -17;
    Require(EVP_PKEY_cmp_nid_postfix(&first, &first) == -17, "self comparison must dispatch");
    expectedSecond = &second;
    MethodBytes nextMethod{};
    SetCallback(nextMethod, 56, ComparePublic);
    SetCallback(method, 56, WrongComparison);
    SetCallback(method, 160, CompareParameters);
    parameterResult = 1;
    publicResult = 29;
    replacementMethod = nextMethod.data();
    Require(EVP_PKEY_cmp_nid_postfix(&first, &second) == 29, "public method refresh after parameter callback");
    replacementMethod = nullptr;
}

} // namespace

int main() {
    CheckQueries();
    CheckComparison();
    std::printf("PKEY queries: %d matrix cases, %d checks\n", cases, checks);
    return 0;
}
