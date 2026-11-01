#include "prx/libSceShellUIUtil/include/Api.hpp"
#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

int APS5_VABI sceShellUIUtilAbortTask() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceShellUIUtilCreateTaskToken() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShellUIUtilGetAppUrl(const char* application, const char* parameter, char* output, std::uint32_t capacity, std::uint32_t* outputSize) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShellUIUtilInitialize() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShellUIUtilLaunchByUri(const char* uri, const ShellUIUtilLaunchParameter* parameter) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShellUIUtilLaunchTask(std::int32_t taskToken, const char* uri, char* output, std::uint32_t capacity) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShellUIUtilTerminate() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

}
