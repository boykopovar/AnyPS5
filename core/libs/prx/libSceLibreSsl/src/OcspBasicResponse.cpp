#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <algorithm>
#include <atomic>
#include <limits>
#include <stdexcept>
#include "prx/libSceLibreSsl/libSceLibreSsl.hpp"
#include "prx/libSceLibreSsl/include/OcspResponse.hpp"
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

template <typename T>
T Load(const T* source, const char* function) {
    T value{};
    const int savedErrno = errno;
    const bool ok = GuestCopyIn_nid_no_patch(&value, source, sizeof(T));
    errno = savedErrno;
    if (!ok) NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    return value;
}

template <typename T>
void Store(T* destination, const T& value, const char* function) {
    const int savedErrno = errno;
    const bool ok = GuestCopyOut_nid_no_patch(destination, &value, sizeof(T));
    errno = savedErrno;
    if (!ok) NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
}

bool PushCertificate(LibreSslStack* stack, LibreSslX509* certificate, const char* function) {
    LibreSslStack value = Load(stack, function);
    if (value.num < 0 || value.num == std::numeric_limits<std::int32_t>::max() || value.numAlloc <= 0)
        throw std::invalid_argument("sk_push: corrupt stack");
    if (value.numAlloc <= value.num + 1) {
        auto* const grown = static_cast<char**>(realloc_nid_postfix(value.data, static_cast<std::size_t>(value.numAlloc) * 2 * sizeof(char*)));
        if (!grown) return false;
        value.data = grown;
        if (value.numAlloc > std::numeric_limits<std::int32_t>::max() / 2) {
            Store(stack, value, function);
            throw std::invalid_argument("sk_push: corrupt stack");
        }
        value.numAlloc *= 2;
    }
    Store(value.data + value.num, reinterpret_cast<char*>(certificate), function);
    ++value.num;
    value.sorted = 0;
    Store(stack, value, function);
    return true;
}

const Stack* Responses(const void* basicResponse, const char* function) {
    const ResponseData* data = static_cast<const ResponseData*>(static_cast<const void*>(static_cast<const LibreSslOcspBasicResponse*>(basicResponse)->tbsResponseData));
    if (!data) throw std::invalid_argument(function);
    return data->responses;
}

} // namespace

LibreSslStack* LibreSslStackNew_nid_no_patch() {
    return NewCertificateStack();
}

bool LibreSslStackPush_nid_no_patch(LibreSslStack* stack, void* element) {
    return PushCertificate(stack, static_cast<LibreSslX509*>(element), __func__);
}

extern "C" {

int APS5_VABI OCSP_basic_add1_cert_nid_postfix(LibreSslOcspBasicResponse* response, LibreSslX509* certificate) {
    if (!response) throw std::invalid_argument("OCSP_basic_add1_cert: null response");
    auto* const references = certificate ? reinterpret_cast<std::int32_t*>(reinterpret_cast<std::byte*>(certificate) + offsetof(LibreSslX509, references)) : nullptr;
    if (references) {
        Load(references, __func__);
        const int savedErrno = errno;
        const bool writable = GuestWritable_nid_no_patch(references, sizeof(*references));
        errno = savedErrno;
        if (!writable) NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
    }
    LibreSslOcspBasicResponse value = Load(response, __func__);
    if (!value.certs) {
        value.certs = NewCertificateStack();
        if (!value.certs) return 0;
        Store(response, value, __func__);
    }
    if (!PushCertificate(value.certs, certificate, __func__)) return 0;
    if (!references) throw std::invalid_argument("OCSP_basic_add1_cert: null certificate");
    std::atomic_ref<std::int32_t>(*references).fetch_add(1);
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
    if (!responses->data) throw std::invalid_argument("OCSP_resp_get0: null response array");
    return responses->data[index];
}

}
