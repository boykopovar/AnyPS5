#include "prx/libSceAudiodec.native/include/Api.hpp"
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <stdexcept>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

namespace {

constexpr int AudiodecErrorInvalidType = static_cast<int>(0x807F0001u);
constexpr int AudiodecErrorArgument = static_cast<int>(0x807F0002u);
constexpr int AudiodecErrorInvalidParamSize = static_cast<int>(0x807F0004u);
constexpr int AudiodecErrorInvalidBsiInfoSize = static_cast<int>(0x807F0005u);
constexpr int AudiodecErrorInvalidAuInfoSize = static_cast<int>(0x807F0006u);
constexpr int AudiodecErrorInvalidPcmItemSize = static_cast<int>(0x807F0007u);
constexpr int AudiodecErrorInvalidCtrlPointer = static_cast<int>(0x807F0008u);
constexpr int AudiodecErrorInvalidParamPointer = static_cast<int>(0x807F0009u);
constexpr int AudiodecErrorInvalidBsiInfoPointer = static_cast<int>(0x807F000Au);
constexpr int AudiodecErrorInvalidAuInfoPointer = static_cast<int>(0x807F000Bu);
constexpr int AudiodecErrorInvalidPcmItemPointer = static_cast<int>(0x807F000Cu);
constexpr int AudiodecErrorInvalidAuPointer = static_cast<int>(0x807F000Du);
constexpr int AudiodecErrorInvalidPcmPointer = static_cast<int>(0x807F000Eu);
constexpr int AudiodecErrorInvalidHandle = static_cast<int>(0x807F000Fu);
constexpr int AudiodecErrorInvalidAuSize = static_cast<int>(0x807F0011u);
constexpr int AudiodecErrorInvalidPcmSize = static_cast<int>(0x807F0012u);
constexpr int AudiodecErrorInvalidPriority = static_cast<int>(0x807FF000u);
constexpr int AudiodecErrorAlreadyRegistered = static_cast<int>(0x80930009u);
constexpr int AudiodecErrorNotRegistered = static_cast<int>(0x8093000Au);

alignas(8) unsigned char g_moduleContext[32]{};

int DecodeInternal(void* context, std::uint32_t handle, AudiodecCtrl* ctrl, std::int32_t priority, std::uint32_t decode2) {
    if (context == nullptr) return AudiodecErrorArgument;
    if (ctrl == nullptr) return AudiodecErrorInvalidCtrlPointer;
    if (ctrl->pParam == nullptr) return AudiodecErrorInvalidParamPointer;
    if (ctrl->pBsiInfo == nullptr) return AudiodecErrorInvalidBsiInfoPointer;
    if (ctrl->pAuInfo == nullptr) return AudiodecErrorInvalidAuInfoPointer;
    if (ctrl->pAuInfo->p_au_addr == nullptr) return AudiodecErrorInvalidAuPointer;
    if (ctrl->pPcmItem == nullptr) return AudiodecErrorInvalidPcmItemPointer;
    if (ctrl->pPcmItem->p_pcm_addr == nullptr) return AudiodecErrorInvalidPcmPointer;
    if (ctrl->pAuInfo->ui_au_size == 0 || ctrl->pAuInfo->ui_au_size == 0xFFFFFFFFu) return AudiodecErrorInvalidAuSize;
    if (ctrl->pPcmItem->ui_pcm_size == 0 || ctrl->pPcmItem->ui_pcm_size == 0xFFFFFFFFu) return AudiodecErrorInvalidPcmSize;
    if (ctrl->pAuInfo->ui_size != 0x18) return AudiodecErrorInvalidAuInfoSize;
    if (ctrl->pPcmItem->ui_size != 0x18) return AudiodecErrorInvalidPcmItemSize;
    if (static_cast<std::uint32_t>(priority - 20) > 80u) return AudiodecErrorInvalidPriority;
    static_cast<void>(handle);
    static_cast<void>(decode2);
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

}

extern "C" {

int APS5_VABI sceAudiodecClearContextEx(void* context, std::uint32_t handle) {
    if (context == nullptr) return AudiodecErrorArgument;
    static_cast<void>(handle);
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAudiodecCreateDecoderEx(void* context, AudiodecCtrl* ctrl, std::uint32_t codecType) {
    if (context == nullptr) return AudiodecErrorArgument;
    if (ctrl == nullptr) return AudiodecErrorInvalidCtrlPointer;
    if (ctrl->pParam == nullptr) return AudiodecErrorInvalidParamPointer;
    if (ctrl->pBsiInfo == nullptr) return AudiodecErrorInvalidBsiInfoPointer;
    if (codecType == 0 || codecType == 5 || codecType == 7 || codecType == 8 || codecType == 9 || codecType > 12) {
        return AudiodecErrorInvalidType;
    }
    if (codecType == 1) {
        if (*static_cast<const std::uint32_t*>(ctrl->pParam) != 0xCu) {
            return AudiodecErrorInvalidParamSize;
        }
        if (*static_cast<const std::uint32_t*>(ctrl->pBsiInfo) != 0x24u) {
            return AudiodecErrorInvalidBsiInfoSize;
        }
    }
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAudiodecDecode2(std::uint32_t handle, AudiodecCtrl* ctrl) {
    return DecodeInternal(g_moduleContext, handle, ctrl, 40, 1);
}

int APS5_VABI sceAudiodecDecode2Ex(void* context, std::uint32_t handle, AudiodecCtrl* ctrl) {
    return DecodeInternal(context, handle, ctrl, 40, 1);
}

int APS5_VABI sceAudiodecDecode2WithPriority(std::uint32_t handle, AudiodecCtrl* ctrl, std::int32_t priority) {
    return DecodeInternal(g_moduleContext, handle, ctrl, priority, 1);
}

int APS5_VABI sceAudiodecDecode2WithPriorityEx(void* context, std::uint32_t handle, AudiodecCtrl* ctrl, std::int32_t priority) {
    return DecodeInternal(context, handle, ctrl, priority, 1);
}

int APS5_VABI sceAudiodecDecodeEx(void* context, std::uint32_t handle, AudiodecCtrl* ctrl) {
    return DecodeInternal(context, handle, ctrl, 40, 0);
}

int APS5_VABI sceAudiodecDecodeWithPriority(std::uint32_t handle, AudiodecCtrl* ctrl, std::int32_t priority) {
    return DecodeInternal(g_moduleContext, handle, ctrl, priority, 0);
}

int APS5_VABI sceAudiodecDecodeWithPriorityEx(void* context, std::uint32_t handle, AudiodecCtrl* ctrl, std::int32_t priority) {
    return DecodeInternal(context, handle, ctrl, priority, 0);
}

int APS5_VABI sceAudiodecDeleteDecoderEx(void* context, std::uint32_t handle) {
    if (context == nullptr) return AudiodecErrorArgument;
    static_cast<void>(handle);
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAudiodecGetAjmContext(const void* context, std::uint32_t* ajmiContext, std::uint32_t* ajmContext) {
    if (context == nullptr) return AudiodecErrorArgument;
    std::uint32_t ids[2];
    if (!GuestCopyIn_nid_no_patch(ids, context, sizeof(ids))) return AudiodecErrorArgument;
    if (ajmiContext != nullptr && !GuestCopyOut_nid_no_patch(ajmiContext, &ids[0], sizeof(*ajmiContext))) {
        return AudiodecErrorArgument;
    }
    if (ajmContext != nullptr && !GuestCopyOut_nid_no_patch(ajmContext, &ids[1], sizeof(*ajmContext))) {
        return AudiodecErrorArgument;
    }
    return 0;
}

int APS5_VABI sceAudiodecGetContext(void* out) {
    if (out == nullptr) return AudiodecErrorArgument;
    static_cast<void>(out);
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAudiodecInitialize(void* context) {
    if (context == nullptr) return AudiodecErrorArgument;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAudiodecRegisterCodec(void* context, std::uint32_t codecType) {
    if (context == nullptr) return AudiodecErrorArgument;
    switch (codecType) {
    case 1: case 2: case 3: case 4: case 6: case 10: case 11: case 12: break;
    default: return AudiodecErrorInvalidType;
    }
    if (codecType >= 11) {
        auto* bytes = static_cast<unsigned char*>(context);
        auto* lock = reinterpret_cast<unsigned char*>((reinterpret_cast<std::uintptr_t>(bytes) + 0xFu) & ~static_cast<std::uintptr_t>(7u));
        std::uint32_t selfFlag = 0;
        std::uint32_t otherFlag = 0;
        std::memcpy(&selfFlag, lock + (codecType == 11 ? 8 : 0xC), sizeof(selfFlag));
        std::memcpy(&otherFlag, lock + (codecType == 11 ? 0xC : 8), sizeof(otherFlag));
        if (selfFlag != 0) return AudiodecErrorAlreadyRegistered;
        if (otherFlag != 0) {
            selfFlag = 1;
            std::memcpy(lock + (codecType == 11 ? 8 : 0xC), &selfFlag, sizeof(selfFlag));
            return 0;
        }
    }
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAudiodecTerminate(void* context) {
    static_cast<void>(context);
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAudiodecUnregisterCodec(const void* context, std::uint32_t codecType) {
    if (context == nullptr) return AudiodecErrorArgument;
    switch (codecType) {
    case 1: case 2: case 3: case 4: case 6: case 10: case 11: case 12: break;
    default: return AudiodecErrorInvalidType;
    }
    if (codecType >= 11) {
        const auto bytes = static_cast<const unsigned char*>(context);
        const auto lock = reinterpret_cast<const unsigned char*>((reinterpret_cast<std::uintptr_t>(bytes) + 0xFu) & ~static_cast<std::uintptr_t>(7u));
        std::uint32_t flag;
        std::memcpy(&flag, lock + (codecType == 11 ? 8 : 0xC), sizeof(flag));
        if (flag == 0) return AudiodecErrorNotRegistered;
    }
    throw std::runtime_error("sceAudiodecUnregisterCodec: the Ajm service call for the accepted codec type");
}

}
