#include "prx/libSceCompanionHttpd/include/Api.hpp"
#include <cstdint>
#include <cstddef>
#include <cstring>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

namespace {

constexpr int CompanionHttpdErrorInvalidParam = static_cast<int>(0x80E40004u);
constexpr int CompanionHttpdErrorNoEvent = static_cast<int>(0x80E40008u);

static_assert(sizeof(CompanionHttpdOptParam) == 0x60);

}

extern "C" {

int APS5_VABI sceCompanionHttpdOptParamInitialize(CompanionHttpdOptParam* param) {
    if (param == nullptr) return CompanionHttpdErrorInvalidParam;
    std::memset(param, 0, sizeof(*param));
    return 0;
}

int APS5_VABI sceCompanionHttpdInitialize(const CompanionHttpdOptParam* param) {
    return 0;
}

int APS5_VABI sceCompanionHttpdTerminate(void) {
    return 0;
}

int APS5_VABI sceCompanionHttpdRegisterRequestCallback(CompanionHttpdRequestCallback function, void* argument) {
    return 0;
}

int APS5_VABI sceCompanionHttpdUnregisterRequestCallback(void) {
    return 0;
}

int APS5_VABI sceCompanionHttpdStart(void) {
    return 0;
}

int APS5_VABI sceCompanionHttpdStop(void) {
    return 0;
}

int APS5_VABI sceCompanionHttpdGetEvent(void* event) {
    return CompanionHttpdErrorNoEvent;
}

int APS5_VABI sceCompanionHttpdAddHeader(const char* key, const char* value, void* response) {
    return 0;
}

int APS5_VABI sceCompanionHttpdSetStatus(int status, void* response) {
    return 0;
}

int APS5_VABI sceCompanionHttpdSetBody(const char* body, std::uint64_t size, void* response) {
    return 0;
}

int APS5_VABI sceCompanionHttpdGet2ndScreenStatus(void) {
    return 0;
}

int APS5_VABI sceCompanionHttpdGetUserId(void) {
    return 0;
}

int APS5_VABI sceCompanionHttpdInitialize2(void) {
    return 0;
}

int APS5_VABI sceCompanionHttpdRegisterRequestBodyReceptionCallback(void) {
    return 0;
}

int APS5_VABI sceCompanionHttpdRegisterRequestCallback2(void) {
    return 0;
}

int APS5_VABI sceCompanionHttpdUnregisterRequestBodyReceptionCallback(void) {
    return 0;
}

}
