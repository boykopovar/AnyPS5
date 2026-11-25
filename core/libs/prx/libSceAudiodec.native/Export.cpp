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

template <typename T>
bool ReadGuest(T& value, const T* source) {
    const bool copied = GuestCopyIn_nid_no_patch(&value, source, sizeof(T));
    return copied;
}

template <typename T>
bool WriteGuest(T* destination, const T& value) {
    const bool copied = GuestCopyOut_nid_no_patch(destination, &value, sizeof(T));
    return copied;
}

int DecodeInternal(void* context, std::uint32_t handle, AudiodecCtrl* ctrl, std::int32_t priority, std::uint32_t decode2) {
    if (context == nullptr) return AudiodecErrorArgument;
    if (ctrl == nullptr) return AudiodecErrorInvalidCtrlPointer;
    AudiodecCtrl localCtrl{};
    if (!ReadGuest(localCtrl, ctrl)) return AudiodecErrorInvalidCtrlPointer;
    if (localCtrl.pParam == nullptr) return AudiodecErrorInvalidParamPointer;
    if (localCtrl.pBsiInfo == nullptr) return AudiodecErrorInvalidBsiInfoPointer;
    if (localCtrl.pAuInfo == nullptr) return AudiodecErrorInvalidAuInfoPointer;
    AudiodecAuInfo auInfo{};
    if (!ReadGuest(auInfo, localCtrl.pAuInfo)) return AudiodecErrorInvalidAuInfoPointer;
    if (auInfo.p_au_addr == nullptr) return AudiodecErrorInvalidAuPointer;
    if (localCtrl.pPcmItem == nullptr) return AudiodecErrorInvalidPcmItemPointer;
    AudiodecPcmItem pcmItem{};
    if (!ReadGuest(pcmItem, localCtrl.pPcmItem)) return AudiodecErrorInvalidPcmItemPointer;
    if (pcmItem.p_pcm_addr == nullptr) return AudiodecErrorInvalidPcmPointer;
    if (auInfo.ui_au_size == 0 || auInfo.ui_au_size == 0xFFFFFFFFu) return AudiodecErrorInvalidAuSize;
    if (pcmItem.ui_pcm_size == 0 || pcmItem.ui_pcm_size == 0xFFFFFFFFu) return AudiodecErrorInvalidPcmSize;
    if (auInfo.ui_size != 0x18) return AudiodecErrorInvalidAuInfoSize;
    if (pcmItem.ui_size != 0x18) return AudiodecErrorInvalidPcmItemSize;
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
    AudiodecCtrl localCtrl{};
    if (!ReadGuest(localCtrl, ctrl)) return AudiodecErrorInvalidCtrlPointer;
    if (localCtrl.pParam == nullptr) return AudiodecErrorInvalidParamPointer;
    if (localCtrl.pBsiInfo == nullptr) return AudiodecErrorInvalidBsiInfoPointer;
    if (codecType == 0 || codecType == 5 || codecType == 7 || codecType == 8 || codecType == 9 || codecType > 12) {
        return AudiodecErrorInvalidType;
    }
    if (codecType == 1) {
        std::uint32_t paramSize = 0;
        if (!ReadGuest(paramSize, static_cast<const std::uint32_t*>(localCtrl.pParam))) {
            return AudiodecErrorInvalidParamPointer;
        }
        if (paramSize != 0xCu) {
            return AudiodecErrorInvalidParamSize;
        }
        std::uint32_t bsiInfoSize = 0;
        if (!ReadGuest(bsiInfoSize, static_cast<const std::uint32_t*>(localCtrl.pBsiInfo))) {
            return AudiodecErrorInvalidBsiInfoPointer;
        }
        if (bsiInfoSize != 0x24u) {
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
    const bool copied = GuestCopyIn_nid_no_patch(ids, context, sizeof(ids));
    if (!copied) return AudiodecErrorArgument;
    if (ajmiContext != nullptr && !WriteGuest(ajmiContext, ids[0])) {
        return AudiodecErrorArgument;
    }
    if (ajmContext != nullptr && !WriteGuest(ajmContext, ids[1])) {
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
        const auto address = reinterpret_cast<std::uintptr_t>(context);
        if (address > UINTPTR_MAX - 0x1Fu) return AudiodecErrorArgument;
        const auto lock = (address + 0xFu) & ~static_cast<std::uintptr_t>(7u);
        auto* const selfAddress = reinterpret_cast<std::uint32_t*>(lock + (codecType == 11 ? 8u : 0xCu));
        const auto* const otherAddress = reinterpret_cast<const std::uint32_t*>(lock + (codecType == 11 ? 0xCu : 8u));
        std::uint32_t selfFlag = 0;
        std::uint32_t otherFlag = 0;
        if (!ReadGuest(selfFlag, selfAddress) || !ReadGuest(otherFlag, otherAddress)) {
            return AudiodecErrorArgument;
        }
        if (selfFlag != 0) return AudiodecErrorAlreadyRegistered;
        if (otherFlag != 0) {
            selfFlag = 1;
            if (!WriteGuest(selfAddress, selfFlag)) {
                return AudiodecErrorArgument;
            }
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
        const auto address = reinterpret_cast<std::uintptr_t>(context);
        if (address > UINTPTR_MAX - 0x1Fu) return AudiodecErrorArgument;
        const auto lock = (address + 0xFu) & ~static_cast<std::uintptr_t>(7u);
        const auto* const flagAddress = reinterpret_cast<const std::uint32_t*>(lock + (codecType == 11 ? 8u : 0xCu));
        std::uint32_t flag = 0;
        if (!ReadGuest(flag, flagAddress)) return AudiodecErrorArgument;
        if (flag == 0) return AudiodecErrorNotRegistered;
    }
    throw std::runtime_error("sceAudiodecUnregisterCodec: the Ajm service call for the accepted codec type");
}

}
