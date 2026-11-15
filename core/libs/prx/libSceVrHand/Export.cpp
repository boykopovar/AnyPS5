#include "prx/libSceVrHand/include/Api.hpp"
#include <array>
#include <cerrno>
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
    const int savedErrno = errno;
    std::uint32_t size = 0;
    const bool sizeCopied = GuestCopyIn_nid_no_patch(&size, param, sizeof(size));
    errno = savedErrno;
    if (!sizeCopied || size != 0x20) return errInvalidArg;
    std::array<std::uint8_t, 0x20> input{};
    const bool inputCopied = GuestCopyIn_nid_no_patch(input.data(), param, sizeof(input));
    errno = savedErrno;
    if (!inputCopied) return errInvalidArg;
    for (std::size_t index = 4; index < 0x20; ++index) if (input[index] != 0) return errInvalidArg;
    struct MemoryInfo {
        std::uint32_t memorySize{0x3c00000};
        std::uint32_t alignment{0x10000};
        std::uint8_t reserved[0x14]{};
    };
    static_assert(sizeof(MemoryInfo) == 28);
    const MemoryInfo info{};
    const bool resultCopied = GuestCopyOut_nid_no_patch(result, &info, sizeof(info));
    errno = savedErrno;
    if (!resultCopied) return errInvalidArg;
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
