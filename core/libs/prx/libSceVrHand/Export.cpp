#include "prx/libSceVrHand/include/Api.hpp"
#include <cstdint>
#include <cstddef>
#include <cstring>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

int APS5_VABI sceVrHandFinalize() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceVrHandGetMesh() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceVrHandGetMeshSize() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceVrHandGetResult() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceVrHandInitialize() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceVrHandQueryMemory(const void* param, void* result) {
    constexpr int errInvalidArg = static_cast<int>(0x8a870006);
    if (param == nullptr || result == nullptr) return errInvalidArg;
    const auto* input = static_cast<const std::uint8_t*>(param);
    std::uint32_t size = 0;
    std::memcpy(&size, input, sizeof(size));
    if (size != 0x20) return errInvalidArg;
    for (std::size_t index = 4; index < 0x20; ++index) if (input[index] != 0) return errInvalidArg;
    auto* output = static_cast<std::uint8_t*>(result);
    const std::uint32_t at0 = 0x3c00000;
    const std::uint32_t at4 = 0x10000;
    std::memcpy(output, &at0, sizeof(at0));
    std::memcpy(output + 4, &at4, sizeof(at4));
    std::memset(output + 8, 0, 0x14);
    return 0;
}

int APS5_VABI sceVrHandStart() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceVrHandStop() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

}
