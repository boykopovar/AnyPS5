#include "prx/libSceS3da/include/Api.hpp"
#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

int APS5_VABI sceS3daClose(std::uint32_t port) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceS3daGetInfo(std::uint32_t port, S3daInfo* info) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceS3daOpen(std::uint32_t port) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceS3daSendData(std::uint32_t port, const void* data, std::uint32_t size) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

}
