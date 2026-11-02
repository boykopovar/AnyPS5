#include "prx/libSceEditMp4/include/Api.hpp"
#include <cstdint>
#include <cstddef>
#include <cstring>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

int APS5_VABI sceEditMp4Abort() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceEditMp4Edit() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceEditMp4GetStatus() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceEditMp4Initialize() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

void APS5_VABI sceEditMp4InitializeParam(void* param) {
    if (param == nullptr) return;
    auto* bytes = static_cast<std::uint8_t*>(param);
    const std::uint64_t at10 = 0x3f;
    const std::uint32_t at18 = 700;
    std::memset(bytes, 0, 0x10);
    std::memcpy(bytes + 0x10, &at10, sizeof(at10));
    std::memcpy(bytes + 0x18, &at18, sizeof(at18));
}

std::uint64_t APS5_VABI sceEditMp4QueryMemSize() {
    return 0x387d30;
}

int APS5_VABI sceEditMp4SetFinishCallback() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceEditMp4Terminate() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceEditMp4Unite() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

}
