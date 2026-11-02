#include "prx/libSceShellUIUtil/include/Api.hpp"
#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

struct ShellUIUtilLaunchParameter {
    std::uint32_t size;
    std::uint32_t flags;
};

constexpr int SHELLUIUTIL_ERROR_INVALID_ARGUMENT = static_cast<int>(0x80B8000Au);

extern "C" {

int APS5_VABI sceShellUIUtilAbortTask() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShellUIUtilCreateTaskToken() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShellUIUtilGetAppUrl(const char* application, const char* parameter, char* output, std::uint32_t capacity, std::uint32_t* outputSize) {
    if (application == nullptr || output == nullptr || capacity == 0) return SHELLUIUTIL_ERROR_INVALID_ARGUMENT;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShellUIUtilInitialize() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShellUIUtilLaunchByUri(const char* uri, const ShellUIUtilLaunchParameter* parameter) {
    if (uri == nullptr || parameter == nullptr || parameter->size < 8) return SHELLUIUTIL_ERROR_INVALID_ARGUMENT;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShellUIUtilLaunchTask(std::int32_t taskToken, const char* uri, char* output, std::uint32_t capacity) {
    if (taskToken < 0 || uri == nullptr) return SHELLUIUTIL_ERROR_INVALID_ARGUMENT;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShellUIUtilTerminate() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

}
