#include <cstddef>
#include <cstdint>
#include "prx/libSceLibreSsl/libSceLibreSsl.hpp"
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

struct BasicResponse {
    const ResponseData* tbsResponseData;
    void* signatureAlgorithm;
    void* signature;
    void* certs;
};

static_assert(sizeof(Stack) == 32);
static_assert(offsetof(Stack, data) == 8);
static_assert(sizeof(ResponseData) == 40);
static_assert(offsetof(ResponseData, responses) == 24);
static_assert(sizeof(BasicResponse) == 32);

const Stack* Responses(const void* basicResponse, const char* function) {
    const ResponseData* data = static_cast<const BasicResponse*>(basicResponse)->tbsResponseData;
    if (!data) NotImplemented_nid_no_patch(function);
    return data->responses;
}

} // namespace

extern "C" {

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
