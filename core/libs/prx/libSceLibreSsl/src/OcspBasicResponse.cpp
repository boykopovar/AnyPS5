#include <cstddef>
#include <cstdint>
#include <algorithm>
#include <atomic>
#include <limits>
#include "prx/libSceLibreSsl/libSceLibreSsl.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"

namespace {

struct Stack {
    std::int32_t num;
    void** data;
    std::int32_t sorted;
    std::int32_t numAlloc;
    void* comp;
};

struct ResponseData {
    void* version;
    void* responderId;
    void* producedAt;
    const Stack* responses;
    void* responseExtensions;
};

static_assert(sizeof(Stack) == 32);
static_assert(offsetof(Stack, data) == 8);
static_assert(sizeof(ResponseData) == 40);
static_assert(offsetof(ResponseData, responses) == 24);
static_assert(sizeof(LibreSslOcspBasicResponse) == 32);
static_assert(offsetof(LibreSslOcspBasicResponse, tbsResponseData) == 0);
static_assert(offsetof(LibreSslOcspBasicResponse, signatureAlgorithm) == 8);
static_assert(offsetof(LibreSslOcspBasicResponse, signature) == 16);
static_assert(offsetof(LibreSslOcspBasicResponse, certs) == 24);
static_assert(offsetof(LibreSslX509, references) == 28);

constexpr std::int32_t MinimumNodes = 4;

LibreSslStack* NewCertificateStack() {
    auto* const stack = static_cast<LibreSslStack*>(malloc_nid_postfix(sizeof(LibreSslStack)));
    if (!stack) return nullptr;
    stack->data = static_cast<char**>(realloc_nid_postfix(nullptr, MinimumNodes * sizeof(char*)));
    if (!stack->data) {
        free_nid_postfix(stack);
        return nullptr;
    }
    std::fill_n(stack->data, MinimumNodes, nullptr);
    stack->comp = nullptr;
    stack->numAlloc = MinimumNodes;
    stack->num = 0;
    stack->sorted = 0;
    return stack;
}

bool PushCertificate(LibreSslStack* stack, LibreSslX509* certificate) {
    if (stack->num < 0 || stack->num == std::numeric_limits<std::int32_t>::max() || stack->numAlloc <= 0)
        NotImplemented_nid_no_patch(__func__);
    if (stack->numAlloc <= stack->num + 1) {
        auto* const grown = static_cast<char**>(realloc_nid_postfix(stack->data, static_cast<std::size_t>(stack->numAlloc) * 2 * sizeof(char*)));
        if (!grown) return false;
        stack->data = grown;
        if (stack->numAlloc > std::numeric_limits<std::int32_t>::max() / 2) NotImplemented_nid_no_patch(__func__);
        stack->numAlloc *= 2;
    }
    stack->data[stack->num] = reinterpret_cast<char*>(certificate);
    ++stack->num;
    stack->sorted = 0;
    return true;
}

const Stack* Responses(const void* basicResponse, const char* function) {
    const ResponseData* data = static_cast<const ResponseData*>(static_cast<const void*>(static_cast<const LibreSslOcspBasicResponse*>(basicResponse)->tbsResponseData));
    if (!data) NotImplemented_nid_no_patch(function);
    return data->responses;
}

} // namespace

extern "C" {

int APS5_VABI OCSP_basic_add1_cert_nid_postfix(LibreSslOcspBasicResponse* response, LibreSslX509* certificate) {
    if (!response) NotImplemented_nid_no_patch(__func__);
    if (!response->certs) {
        response->certs = NewCertificateStack();
        if (!response->certs) return 0;
    }
    if (!PushCertificate(response->certs, certificate)) return 0;
    if (!certificate) NotImplemented_nid_no_patch(__func__);
    std::atomic_ref<std::int32_t>(certificate->references).fetch_add(1);
    return 1;
}

int APS5_VABI OCSP_resp_count_nid_postfix(const void* basicResponse) {
    if (!basicResponse) return -1;
    const Stack* responses = Responses(basicResponse, __func__);
    return responses ? responses->num : -1;
}

void* APS5_VABI OCSP_resp_get0_nid_postfix(const void* basicResponse, std::int32_t index) {
    if (!basicResponse) return nullptr;
    const Stack* responses = Responses(basicResponse, __func__);
    if (!responses || index < 0 || index >= responses->num) return nullptr;
    if (!responses->data) NotImplemented_nid_no_patch(__func__);
    return responses->data[index];
}

}
