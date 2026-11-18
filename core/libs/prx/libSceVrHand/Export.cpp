#include "prx/libSceVrHand/include/Api.hpp"
#include <array>
#include <cerrno>
#include <cstdint>
#include <cstddef>
#include <cstring>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

namespace {

constexpr int ErrorNotInitialized = static_cast<int>(0x8a870001);
constexpr int ErrorAlreadyInitialized = static_cast<int>(0x8a870002);
constexpr int ErrorInvalidArgument = static_cast<int>(0x8a870006);

int g_vrHandState = 0;

struct VrHandInitializeParam {
    std::uint32_t size;
    std::uint32_t unk04;
    std::uint32_t unk08;
    std::uint32_t pad0c;
    std::uint64_t unk10;
    std::uint32_t unk18;
    std::uint32_t unk1c;
    void* workMemory;
    std::uint32_t memorySize;
    std::uint32_t memoryAlignment;
    std::uint32_t unk30;
    std::uint32_t unk34;
    std::uint32_t unk38;
    std::uint8_t unk3c;
    std::uint8_t unk3d;
    std::uint8_t pad3e[2];
};
static_assert(sizeof(VrHandInitializeParam) == 0x40);

struct VrHandStartParam {
    std::uint32_t size;
    std::uint32_t mode;
    std::uint8_t reserved[8];
};
static_assert(sizeof(VrHandStartParam) == 0x10);

}

extern "C" {

int APS5_VABI sceVrHandFinalize(void) {
    if (g_vrHandState == 0) {
        return 0;
    }
    g_vrHandState = 0;
    return 0;
}

int APS5_VABI sceVrHandGetMesh(void) {
    if (g_vrHandState == 0) {
        return ErrorNotInitialized;
    }
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceVrHandGetMeshSize(void) {
    if (g_vrHandState == 0) {
        return ErrorNotInitialized;
    }
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceVrHandGetResult(void) {
    if (g_vrHandState == 0) {
        return ErrorNotInitialized;
    }
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceVrHandInitialize(const void* param) {
    if (g_vrHandState != 0) {
        return ErrorAlreadyInitialized;
    }
    if (param == nullptr) {
        return ErrorInvalidArgument;
    }
    const int savedErrno = errno;
    VrHandInitializeParam localParam{};
    if (!GuestCopyIn_nid_no_patch(&localParam, param, sizeof(localParam))) {
        errno = savedErrno;
        return ErrorInvalidArgument;
    }
    errno = savedErrno;
    if (localParam.size != sizeof(VrHandInitializeParam) ||
        localParam.unk04 != 0 ||
        localParam.unk08 < 0x100 || localParam.unk08 > 0x2ff ||
        localParam.unk10 > 0x1fff ||
        localParam.unk18 > 6 ||
        localParam.unk1c > 7 ||
        localParam.workMemory == nullptr ||
        localParam.memorySize < 0x3c00000 ||
        localParam.memoryAlignment < 0x10000 ||
        localParam.unk30 > 1 ||
        (localParam.unk34 != 0 && localParam.unk34 != 1 && localParam.unk34 != 32) ||
        localParam.unk38 != 0 ||
        localParam.unk3c != 0 || localParam.unk3d != 0) {
        return ErrorInvalidArgument;
    }
    g_vrHandState = 1;
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

int APS5_VABI sceVrHandStart(const void* param) {
    if (g_vrHandState == 0) {
        return ErrorNotInitialized;
    }
    if (param == nullptr) {
        return ErrorInvalidArgument;
    }
    const int savedErrno = errno;
    VrHandStartParam localParam{};
    if (!GuestCopyIn_nid_no_patch(&localParam, param, sizeof(localParam))) {
        errno = savedErrno;
        return ErrorInvalidArgument;
    }
    errno = savedErrno;
    if (localParam.size != sizeof(VrHandStartParam) || localParam.mode > 2) {
        return ErrorInvalidArgument;
    }
    g_vrHandState = 2;
    return 0;
}

int APS5_VABI sceVrHandStop(void) {
    if (g_vrHandState == 0) {
        return ErrorNotInitialized;
    }
    if (g_vrHandState == 2) {
        g_vrHandState = 1;
    }
    return 0;
}

}
