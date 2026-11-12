#include "prx/libSceLibreSsl/libSceLibreSsl.hpp"
#include "prx/libSceLibreSsl/include/Error.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <limits>

struct alignas(8) Guest {
    std::array<std::byte, 32> basic;
    std::array<std::byte, 40> data;
    std::array<std::byte, 32> stack;
    std::array<std::byte, 40> decoyData;
    std::array<std::byte, 32> decoyStack;
    std::array<void*, 4> entries;
    std::array<void*, 8> decoyEntries;
};

static constexpr std::int32_t MinIndex = std::numeric_limits<std::int32_t>::min();
static constexpr std::int32_t MaxIndex = std::numeric_limits<std::int32_t>::max();
static constexpr std::int32_t Indices[] {0, 1, 2, 3, -1, MinIndex, MaxIndex};

static int first;
static int third;
static int beyond;

static void Require(bool value, const char* message) {
    if (value) return;
    std::fprintf(stderr, "failed: %s\n", message);
    std::abort();
}

template<typename TAction> static void RequireThrows(TAction action, const char* message) {
    try { action(); } catch (const std::exception&) { return; }
    Require(false, message);
}

template<std::size_t TSize, typename TValue> static void Store(std::array<std::byte, TSize>& object, std::size_t offset, TValue value) {
    Require(offset + sizeof(value) <= TSize, "store stays inside the object");
    std::memcpy(object.data() + offset, &value, sizeof(value));
}

static void RequireVerifyMissing(LibreSslOcspBasicResponse& response, LibreSslStack* certificates, std::uint64_t flags,
    std::uint64_t expectedPreviousError, bool seedError) {
    if (seedError) LibreSslPutError_nid_no_patch(7, 8, 9);
    const auto basicSnapshot = response;
    const auto dataSnapshot = *response.tbsResponseData;
    const auto responderSnapshot = *response.tbsResponseData->responderId;
    const auto responseStackSnapshot = response.certs ? *response.certs : LibreSslStack{};
    const auto inputStackSnapshot = certificates ? *certificates : LibreSslStack{};
    Require(OCSP_basic_verify_nid_postfix(&response, certificates, nullptr, flags) == 0, "missing signer returns zero");
    Require(std::memcmp(&response, &basicSnapshot, sizeof(response)) == 0, "verification leaves the basic response unchanged");
    Require(std::memcmp(response.tbsResponseData, &dataSnapshot, sizeof(dataSnapshot)) == 0, "verification leaves response data unchanged");
    const auto* const responder = response.tbsResponseData->responderId;
    const bool sameResponderValue = responderSnapshot.type == 0
        ? responder->value.byName == responderSnapshot.value.byName
        : responder->value.byKey == responderSnapshot.value.byKey;
    Require(responder->type == responderSnapshot.type && sameResponderValue, "verification leaves the responder id unchanged");
    if (response.certs) Require(std::memcmp(response.certs, &responseStackSnapshot, sizeof(responseStackSnapshot)) == 0,
        "verification leaves the internal certificate stack unchanged");
    if (certificates) Require(std::memcmp(certificates, &inputStackSnapshot, sizeof(inputStackSnapshot)) == 0,
        "verification leaves the external certificate stack unchanged");
    if (seedError) Require(ERR_get_error_nid_postfix() == expectedPreviousError, "pre-existing error remains first in the queue");
    Require(ERR_get_error_nid_postfix() == ((std::uint64_t{39} << 24) | (std::uint64_t{0xfff} << 12) | 118),
        "missing signer appends the pinned OCSP error code");
    Require(ERR_get_error_nid_postfix() == 0, "missing signer adds exactly one error");
}

static void VerifyMissingSigner() {
    constexpr std::uint64_t noIntern = 0x2;
    LibreSslOcspResponderId responderId{0, {nullptr}};
    LibreSslOcspResponseData data{nullptr, &responderId, nullptr, nullptr, nullptr};
    LibreSslStack internalStack{};
    LibreSslOcspBasicResponse response{&data, nullptr, nullptr, &internalStack};
    LibreSslStack externalStack{};
    ERR_clear_error_nid_postfix();
    RequireVerifyMissing(response, &externalStack, 0, 0, false);

    responderId.type = 1;
    Asn1String invalidHash{19, 4, nullptr, 0};
    responderId.value.byKey = &invalidHash;
    externalStack.num = 1;
    externalStack.data = nullptr;
    internalStack.num = 1;
    internalStack.data = nullptr;
    ERR_clear_error_nid_postfix();
    constexpr std::uint64_t previousError = (std::uint64_t{7} << 24) | (std::uint64_t{8} << 12) | 9;
    RequireVerifyMissing(response, &externalStack, 0, previousError, true);

    Asn1String validLengthHash{20, 4, nullptr, 0};
    responderId.value.byKey = &validLengthHash;
    ERR_clear_error_nid_postfix();
    RequireVerifyMissing(response, nullptr, noIntern, 0, false);

    ERR_clear_error_nid_postfix();
    LibreSslStack nonemptyExternal{1, nullptr, 0, 1, nullptr};
    RequireThrows([&] { OCSP_basic_verify_nid_postfix(&response, &nonemptyExternal, nullptr, noIntern); },
        "a nonempty external stack reaches unsupported signer matching");
    Require(ERR_peek_error_nid_postfix() == 0, "unsupported signer matching adds no fabricated error");

    LibreSslX509 candidate{};
    char* internalEntries[]{reinterpret_cast<char*>(&candidate)};
    internalStack = {1, internalEntries, 1, 1, nullptr};
    externalStack = {};
    RequireThrows([&] { OCSP_basic_verify_nid_postfix(&response, &externalStack, nullptr, 0); },
        "the internal stack is searched after the external stack misses");
    Require(ERR_peek_error_nid_postfix() == 0, "unsupported internal signer matching adds no fabricated error");

    data.responderId = nullptr;
    RequireThrows([&] { OCSP_basic_verify_nid_postfix(&response, nullptr, nullptr, noIntern); }, "a missing responder id throws");
    ERR_clear_error_nid_postfix();
}

static void Build(Guest& guest, std::int32_t count) {
    std::memset(&guest, 0xa5, sizeof(guest));
    guest.entries = {&first, nullptr, &third, &beyond};
    for (auto& entry : guest.decoyEntries) entry = &entry;
    Store(guest.decoyStack, 0, std::int32_t{7});
    Store(guest.decoyStack, 8, guest.decoyEntries.data());
    for (std::size_t offset = 0; offset < 40; offset += 8) Store(guest.decoyData, offset, guest.decoyStack.data());
    for (std::size_t offset = 8; offset < 32; offset += 8) Store(guest.basic, offset, guest.decoyData.data());
    Store(guest.basic, 0, guest.data.data());
    for (std::size_t offset = 0; offset < 40; offset += 8) Store(guest.data, offset, guest.decoyStack.data());
    Store(guest.data, 24, guest.stack.data());
    Store(guest.stack, 0, count);
    Store(guest.stack, 8, guest.entries.data());
    Store(guest.stack, 16, std::int32_t{5});
    Store(guest.stack, 20, std::int32_t{2});
    Store(guest.stack, 24, guest.decoyEntries.data());
}

static int allocationCalls;
static int reallocationCalls;
static int freeCalls;
static int failAllocationAt;
static int failReallocationAt;
static bool failReallocation;

static void* APS5_VABI Allocate_nid_no_patch(std::size_t size) {
    if (++allocationCalls == failAllocationAt) return nullptr;
    return std::malloc(size);
}

static void APS5_VABI Free_nid_no_patch(void* pointer) {
    if (pointer) ++freeCalls;
    std::free(pointer);
}

static void* APS5_VABI Calloc_nid_no_patch(std::size_t count, std::size_t size) { return std::calloc(count, size); }

static void* APS5_VABI Reallocate_nid_no_patch(void* pointer, std::size_t size) {
    if (++reallocationCalls == failReallocationAt || failReallocation) return nullptr;
    return std::realloc(pointer, size);
}

static void* APS5_VABI Align_nid_no_patch(std::size_t, std::size_t) { std::abort(); }
static void* APS5_VABI Realign_nid_no_patch(void*, std::size_t, std::size_t) { std::abort(); }
static int APS5_VABI PosixAlign_nid_no_patch(void**, std::size_t, std::size_t) { std::abort(); }

static void RegisterAllocator() {
    std::array<void*, 10> api{
        reinterpret_cast<void*>(Allocate_nid_no_patch), reinterpret_cast<void*>(Free_nid_no_patch), reinterpret_cast<void*>(Calloc_nid_no_patch),
        reinterpret_cast<void*>(Reallocate_nid_no_patch), reinterpret_cast<void*>(Align_nid_no_patch), reinterpret_cast<void*>(Realign_nid_no_patch),
        reinterpret_cast<void*>(PosixAlign_nid_no_patch)};
    ApplicationHeapRegister_nid_no_patch(api.data());
}

static void AddCertificates() {
    LibreSslOcspBasicResponse response{};
    response.tbsResponseData = reinterpret_cast<LibreSslOcspResponseData*>(0x1111);
    response.signatureAlgorithm = reinterpret_cast<void*>(0x2222);
    response.signature = reinterpret_cast<Asn1String*>(0x3333);
    std::array<LibreSslX509, 4> certificates{};
    for (auto& certificate : certificates) certificate.references = 7;
    for (auto& certificate : certificates) Require(OCSP_basic_add1_cert_nid_postfix(&response, &certificate) == 1, "a certificate is added");
    Require(response.tbsResponseData == reinterpret_cast<LibreSslOcspResponseData*>(0x1111) && response.signatureAlgorithm == reinterpret_cast<void*>(0x2222)
        && response.signature == reinterpret_cast<Asn1String*>(0x3333), "the response's other fields are unchanged");
    Require(response.certs && response.certs->num == 4 && response.certs->numAlloc == 8 && response.certs->sorted == 0
        && response.certs->comp == nullptr, "the stack follows LibreSSL's four-slot allocation and doubling rule");
    for (std::size_t index = 0; index < certificates.size(); ++index) {
        Require(response.certs->data[index] == reinterpret_cast<char*>(&certificates[index]), "certificates are appended in order");
        Require(certificates[index].references == 8, "each pushed certificate gains one reference");
    }
    free_nid_postfix(response.certs->data);
    free_nid_postfix(response.certs);
}

static void AllocationFailures() {
    LibreSslX509 certificate{};
    certificate.references = 9;
    LibreSslOcspBasicResponse response{};
    failAllocationAt = allocationCalls + 1;
    Require(OCSP_basic_add1_cert_nid_postfix(&response, &certificate) == 0 && !response.certs && certificate.references == 9,
        "a failed stack allocation leaves the response and reference count unchanged");
    failAllocationAt = 0;
    failReallocationAt = reallocationCalls + 1;
    const int beforeFrees = freeCalls;
    Require(OCSP_basic_add1_cert_nid_postfix(&response, &certificate) == 0 && !response.certs && certificate.references == 9,
        "a failed element-array allocation leaves the response and reference count unchanged");
    Require(freeCalls == beforeFrees + 1, "the failed element-array allocation frees the partial stack");
    failReallocationAt = 0;

    LibreSslOcspBasicResponse nullCertResponse{};
    failAllocationAt = allocationCalls + 1;
    Require(OCSP_basic_add1_cert_nid_postfix(&nullCertResponse, nullptr) == 0 && !nullCertResponse.certs,
        "a failed stack allocation returns before a null certificate is dereferenced");
    failAllocationAt = 0;
    nullCertResponse = {};
    failReallocation = true;
    Require(OCSP_basic_add1_cert_nid_postfix(&nullCertResponse, nullptr) == 0 && !nullCertResponse.certs,
        "a failed element-array allocation returns before a null certificate is dereferenced");
    failReallocation = false;

    Require(OCSP_basic_add1_cert_nid_postfix(&response, &certificate) == 1, "the stack is created before a later push failure");
    response.certs->num = 3;
    response.certs->data[1] = reinterpret_cast<char*>(&certificate);
    response.certs->data[2] = reinterpret_cast<char*>(&certificate);
    auto* const stack = response.certs;
    auto* const elements = stack->data;
    const int beforeReferences = certificate.references;
    failReallocation = true;
    Require(OCSP_basic_add1_cert_nid_postfix(&response, &certificate) == 0, "a failed stack growth fails the push");
    Require(OCSP_basic_add1_cert_nid_postfix(&response, nullptr) == 0, "a failed stack growth returns before a null certificate is dereferenced");
    failReallocation = false;
    Require(response.certs == stack && stack->data == elements && stack->num == 3 && stack->numAlloc == 4
        && stack->data[0] == reinterpret_cast<char*>(&certificate) && stack->data[1] == reinterpret_cast<char*>(&certificate)
        && stack->data[2] == reinterpret_cast<char*>(&certificate) && certificate.references == beforeReferences,
        "failed growth preserves the response stack and does not add a reference");
    free_nid_postfix(stack->data);
    free_nid_postfix(stack);

    LibreSslOcspBasicResponse pushedNull{};
    bool threw = false;
    try {
        OCSP_basic_add1_cert_nid_postfix(&pushedNull, nullptr);
    } catch (const std::exception&) {
        threw = true;
    }
    Require(threw && pushedNull.certs && pushedNull.certs->num == 1 && pushedNull.certs->data[0] == nullptr,
        "after a successful null push, the source's certificate dereference throws with the append preserved");
    free_nid_postfix(pushedNull.certs->data);
    free_nid_postfix(pushedNull.certs);

    std::array<char*, 4> malformedElements{};
    LibreSslStack malformedStack{std::numeric_limits<std::int32_t>::max(), malformedElements.data(), 0, 4, nullptr};
    LibreSslOcspBasicResponse malformedResponse{};
    malformedResponse.certs = &malformedStack;
    bool malformedThrew = false;
    try {
        OCSP_basic_add1_cert_nid_postfix(&malformedResponse, &certificate);
    } catch (const std::exception&) {
        malformedThrew = true;
    }
    Require(malformedThrew && malformedStack.num == std::numeric_limits<std::int32_t>::max() && certificate.references == beforeReferences,
        "the signed-overflow stack state throws before modifying the certificate");

    std::array<char*, 1> largeElements{};
    LibreSslStack largeStack{std::numeric_limits<std::int32_t>::max() - 1, largeElements.data(), 0,
        std::numeric_limits<std::int32_t>::max(), nullptr};
    LibreSslOcspBasicResponse largeResponse{};
    largeResponse.certs = &largeStack;
    failReallocation = true;
    Require(OCSP_basic_add1_cert_nid_postfix(&largeResponse, &certificate) == 0, "a failed huge stack growth returns zero before the signed capacity update");
    failReallocation = false;
    Require(largeStack.data == largeElements.data() && largeStack.num == std::numeric_limits<std::int32_t>::max() - 1
        && largeStack.numAlloc == std::numeric_limits<std::int32_t>::max() && certificate.references == beforeReferences,
        "a failed huge growth preserves its pointer, count, capacity and certificate reference");
}

static int Count(Guest& guest) {
    return OCSP_resp_count_nid_postfix(guest.basic.data());
}

static void* Get(Guest& guest, std::int32_t index) {
    return OCSP_resp_get0_nid_postfix(guest.basic.data(), index);
}

int main() {
    RegisterAllocator();
    AddCertificates();
    AllocationFailures();
    Require(OCSP_resp_count_nid_postfix(nullptr) == -1, "a null basic response counts -1");
    for (const auto index : Indices)
        Require(OCSP_resp_get0_nid_postfix(nullptr, index) == nullptr, "a null basic response has no entries");

    Guest guest;
    Build(guest, 3);
    const Guest snapshot = guest;
    Require(Count(guest) == 3, "the count is the 32-bit size of the stack at offset 24 of the response data");
    Require(Get(guest, 0) == &first, "index 0 returns the first entry");
    Require(Get(guest, 1) == nullptr, "a null entry is returned as stored");
    Require(Get(guest, 2) == &third, "the last index returns the last entry");
    Require(Get(guest, 3) == nullptr, "an index equal to the count returns null");
    Require(Get(guest, -1) == nullptr, "a negative index returns null");
    Require(Get(guest, MinIndex) == nullptr, "the minimum index returns null");
    Require(Get(guest, MaxIndex) == nullptr, "the maximum index returns null");
    Require(std::memcmp(&guest, &snapshot, sizeof(guest)) == 0, "queries do not write the objects");

    Store(guest.data, 24, static_cast<const void*>(nullptr));
    Require(Count(guest) == -1, "a null responses stack counts -1");
    for (const auto index : Indices) Require(Get(guest, index) == nullptr, "a null responses stack has no entries");

    Build(guest, 0);
    Require(Count(guest) == 0, "an empty stack counts 0");
    for (const auto index : Indices) Require(Get(guest, index) == nullptr, "an empty stack has no entries");

    Build(guest, -2);
    Require(Count(guest) == -2, "a negative stack size is returned unchanged");
    for (const auto index : Indices) Require(Get(guest, index) == nullptr, "a negative stack size has no entries");

    Build(guest, MaxIndex);
    Require(Count(guest) == MaxIndex, "the maximum stack size is returned unchanged");
    Require(Get(guest, 0) == &first, "index 0 is below the maximum stack size");
    Require(Get(guest, 3) == &beyond, "the stack size alone bounds the index");
    Require(Get(guest, MaxIndex) == nullptr, "an index equal to the maximum stack size returns null");
    Require(Get(guest, -1) == nullptr, "a negative index returns null for the maximum stack size");
    Require(Get(guest, MinIndex) == nullptr, "the minimum index returns null for the maximum stack size");

    Build(guest, 3);
    Store(guest.stack, 8, static_cast<const void*>(nullptr));
    Require(Count(guest) == 3, "the count does not read the entry array");
    Require(Get(guest, 3) == nullptr, "an index equal to the count returns null before the entry array is read");
    Require(Get(guest, -1) == nullptr, "a negative index returns null before the entry array is read");
    RequireThrows([&] { Get(guest, 0); }, "a null entry array with index 0 throws");
    RequireThrows([&] { Get(guest, 2); }, "a null entry array with the last index throws");

    Build(guest, 3);
    Store(guest.basic, 0, static_cast<const void*>(nullptr));
    RequireThrows([&] { Count(guest); }, "a null response data pointer throws in the count");
    for (const auto index : Indices)
        RequireThrows([&] { Get(guest, index); }, "a null response data pointer throws for every index");
    VerifyMissingSigner();
    return 0;
}
