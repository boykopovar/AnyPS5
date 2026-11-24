#include "prx/libSceAudiodec.native/include/Api.hpp"
#include <array>
#include <cerrno>
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <stdexcept>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libSceAjm.native/include/Api.hpp"
#include "prx/libSceAjmi/include/InventoryExports.hpp"
#include "prx/libkernel/Pthread/include/Rwlock.hpp"
#include "prx/libkernel/Time/include/Time.hpp"

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
constexpr int AjmRetry = static_cast<int>(0x80931007u);
constexpr int AjmInvalidParameter = static_cast<int>(0x80930008u);

constexpr int AjmRetry = static_cast<int>(0x80931007u);
constexpr int AjmInvalidInstance = static_cast<int>(0x80930003u);
using ContextIds = std::array<std::uint32_t, 2>;

template <typename Operation>
int RetryService(Operation operation) {
    int status;
    while ((status = operation()) == AjmRetry) sceKernelUsleep_nid_postfix(1000);
    return status;
}

struct ModuleStorage {
    std::uint32_t prefix;
    ContextIds ids;
    std::uint32_t padding;
    PthreadRwlock lock;
    std::uint32_t flags[2];
    std::uint32_t tail;
};

static_assert(offsetof(ModuleStorage, ids) == 4);
static_assert(offsetof(ModuleStorage, lock) == 16);
static_assert(offsetof(ModuleStorage, tail) == 32);

struct ModuleContext {
    ModuleStorage storage{};
    int status;

    ModuleContext() {
        status = RetryService([&] { return sceAjmiInitialize(0, &storage.ids[0]); });
        if (status < 0) return;
        status = RetryService([&] { return sceAjmInitialize(0, &storage.ids[1]); });
        if (status < 0) return;
        status = scePthreadRwlockInit(&storage.lock, nullptr, nullptr);
    }

    ~ModuleContext() {
        if (storage.lock) scePthreadRwlockDestroy(&storage.lock);
        if (storage.ids[0]) RetryService([&] { return sceAjmFinalize(storage.ids[0]); });
        if (storage.ids[1]) RetryService([&] { return sceAjmFinalize(storage.ids[1]); });
    }

    const unsigned char* data() const {
        return reinterpret_cast<const unsigned char*>(&storage) + offsetof(ModuleStorage, ids);
    }
};

ModuleContext& Module() {
    const int savedErrno = errno;
    static ModuleContext module;
    errno = savedErrno;
    return module;
}

template <typename Operation>
int RetryService(Operation operation) {
    int status;
    while ((status = operation()) == AjmRetry) sceKernelUsleep_nid_postfix(1000);
    return status;
}

template <typename T>
bool ReadGuest(T& value, const T* source) {
    const int savedErrno = errno;
    const bool copied = GuestCopyIn_nid_no_patch(&value, source, sizeof(T));
    errno = savedErrno;
    return copied;
}

template <typename T>
bool WriteGuest(T* destination, const T& value) {
    const int savedErrno = errno;
    const bool copied = GuestCopyOut_nid_no_patch(destination, &value, sizeof(T));
    errno = savedErrno;
    return copied;
}

int DecodeInternal(const void* context, std::uint32_t handle, AudiodecCtrl* ctrl, std::int32_t priority, std::uint32_t decode2) {
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

int DecodeOnModule(std::uint32_t handle, AudiodecCtrl* ctrl, std::int32_t priority, std::uint32_t decode2) {
    auto& module = Module();
    if (module.status < 0) return module.status;
    return DecodeInternal(module.data(), handle, ctrl, priority, decode2);
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
    return DecodeOnModule(handle, ctrl, 40, 1);
}

int APS5_VABI sceAudiodecDecode2Ex(void* context, std::uint32_t handle, AudiodecCtrl* ctrl) {
    return DecodeInternal(context, handle, ctrl, 40, 1);
}

int APS5_VABI sceAudiodecDecode2WithPriority(std::uint32_t handle, AudiodecCtrl* ctrl, std::int32_t priority) {
    return DecodeOnModule(handle, ctrl, priority, 1);
}

int APS5_VABI sceAudiodecDecode2WithPriorityEx(void* context, std::uint32_t handle, AudiodecCtrl* ctrl, std::int32_t priority) {
    return DecodeInternal(context, handle, ctrl, priority, 1);
}

int APS5_VABI sceAudiodecDecodeEx(void* context, std::uint32_t handle, AudiodecCtrl* ctrl) {
    return DecodeInternal(context, handle, ctrl, 40, 0);
}

int APS5_VABI sceAudiodecDecodeWithPriority(std::uint32_t handle, AudiodecCtrl* ctrl, std::int32_t priority) {
    return DecodeOnModule(handle, ctrl, priority, 0);
}

int APS5_VABI sceAudiodecDecodeWithPriorityEx(void* context, std::uint32_t handle, AudiodecCtrl* ctrl, std::int32_t priority) {
    return DecodeInternal(context, handle, ctrl, priority, 0);
}

int APS5_VABI sceAudiodecDeleteDecoderEx(void* context, std::uint32_t handle) {
    if (context == nullptr) return AudiodecErrorArgument;
    const auto codec = sceAjmInstanceCodecType(handle);
    const bool native = codec == 0 || codec == 1 || codec == 2 || codec == 24;
    const auto address = reinterpret_cast<std::uintptr_t>(context);
    const unsigned offset = native ? 4 : 0;
    if (address > UINTPTR_MAX - offset) return AudiodecErrorArgument;
    std::uint32_t owner = 0;
    if (!ReadGuest(owner, reinterpret_cast<const std::uint32_t*>(address + offset))) return AudiodecErrorArgument;
    const int status = RetryService([&] {
        return native ? sceAjmInstanceDestroy(owner, handle) : sceAjmiInstanceDestroy(owner, handle);
    });
    return status == AjmInvalidInstance ? AudiodecErrorInvalidHandle : status;
}

int APS5_VABI sceAudiodecGetAjmContext(const void* context, std::uint32_t* ajmiContext, std::uint32_t* ajmContext) {
    if (context == nullptr) return AudiodecErrorArgument;
    std::uint32_t ids[2];
    const int savedErrno = errno;
    const bool copied = GuestCopyIn_nid_no_patch(ids, context, sizeof(ids));
    errno = savedErrno;
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
    if (!GuestWritable_nid_no_patch(out, 32)) return AudiodecErrorArgument;
    auto& module = Module();
    if (module.status < 0) return module.status;
    const int savedErrno = errno;
    const bool copied = GuestCopyOut_nid_no_patch(out, module.data(), 32);
    errno = savedErrno;
    return copied ? 0 : AudiodecErrorArgument;
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
    const auto address = reinterpret_cast<std::uintptr_t>(context);
    if (codecType >= 11) {
        if (address > UINTPTR_MAX - 0x1Fu) return AudiodecErrorArgument;
        auto* const lock = reinterpret_cast<PthreadRwlock*>((address + 0xFu) & ~static_cast<std::uintptr_t>(7u));
        auto* const selfAddress = reinterpret_cast<std::uint32_t*>(reinterpret_cast<std::uintptr_t>(lock) + (codecType == 11 ? 8u : 0xCu));
        auto* const otherAddress = reinterpret_cast<std::uint32_t*>(reinterpret_cast<std::uintptr_t>(lock) + (codecType == 11 ? 0xCu : 8u));
        scePthreadRwlockWrlock(lock);
        std::uint32_t selfFlag = 0;
        std::uint32_t otherFlag = 0;
        if (!ReadGuest(selfFlag, selfAddress) || !ReadGuest(otherFlag, otherAddress)) {
            scePthreadRwlockUnlock(lock);
            return AudiodecErrorArgument;
        }
        if (selfFlag != 0) {
            scePthreadRwlockUnlock(lock);
            return AudiodecErrorAlreadyRegistered;
        }
        if (otherFlag != 0) {
            selfFlag = 1;
            const bool written = WriteGuest(selfAddress, selfFlag);
            scePthreadRwlockUnlock(lock);
            return written ? 0 : AudiodecErrorArgument;
        }
        std::uint32_t ajmContext = 0;
        if (!ReadGuest(ajmContext, reinterpret_cast<const std::uint32_t*>(address + 4u))) {
            scePthreadRwlockUnlock(lock);
            return AudiodecErrorArgument;
        }
        const int status = RetryService([&] {
            return sceAjmModuleRegister(ajmContext, 24, 0);
        });
        const int result = status == AjmInvalidParameter ? AudiodecErrorInvalidType : status;
        if (result == 0) {
            selfFlag = 1;
            const bool written = WriteGuest(selfAddress, selfFlag);
            scePthreadRwlockUnlock(lock);
            return written ? 0 : AudiodecErrorArgument;
        }
        scePthreadRwlockUnlock(lock);
        return result;
    }
    if (codecType == 1 || codecType == 2 || codecType == 3) {
        if (address > UINTPTR_MAX - 4u) return AudiodecErrorArgument;
        std::uint32_t ajmContext = 0;
        if (!ReadGuest(ajmContext, reinterpret_cast<const std::uint32_t*>(address + 4u))) {
            return AudiodecErrorArgument;
        }
        const std::uint32_t ajmCodec = codecType == 1 ? 1u : (codecType == 2 ? 0u : 2u);
        const int status = RetryService([&] {
            return sceAjmModuleRegister(ajmContext, ajmCodec, 0);
        });
        return status == AjmInvalidParameter ? AudiodecErrorInvalidType : status;
    }
    if (codecType == 4 || codecType == 6 || codecType == 10) {
        std::uint32_t ajmiContext = 0;
        if (!ReadGuest(ajmiContext, reinterpret_cast<const std::uint32_t*>(address))) {
            return AudiodecErrorArgument;
        }
        const std::uint32_t ajmiCodec = codecType == 4 ? 6u : (codecType == 6 ? 9u : 20u);
        const int status = RetryService([&] {
            return sceAjmiModuleRegister(ajmiContext, ajmiCodec, 0);
        });
        return status == AjmInvalidParameter ? AudiodecErrorInvalidType : status;
    }
    return AudiodecErrorInvalidType;
}

int APS5_VABI sceAudiodecTerminate(void* context) {
    if (context == nullptr) return 0;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAudiodecUnregisterCodec(const void* context, std::uint32_t codecType) {
    if (context == nullptr) return AudiodecErrorArgument;
    switch (codecType) {
    case 1: case 2: case 3: case 4: case 6: case 10: case 11: case 12: break;
    default: return AudiodecErrorInvalidType;
    }
    const auto address = reinterpret_cast<std::uintptr_t>(context);
    if (codecType >= 11) {
        if (address > UINTPTR_MAX - 0x1Fu) return AudiodecErrorArgument;
        auto* const lock = reinterpret_cast<PthreadRwlock*>((address + 0xFu) & ~static_cast<std::uintptr_t>(7u));
        auto* const selfAddress = reinterpret_cast<std::uint32_t*>(reinterpret_cast<std::uintptr_t>(lock) + (codecType == 11 ? 8u : 0xCu));
        const auto* const otherAddress = reinterpret_cast<const std::uint32_t*>(reinterpret_cast<std::uintptr_t>(lock) + (codecType == 11 ? 0xCu : 8u));
        scePthreadRwlockWrlock(lock);
        std::uint32_t selfFlag = 0;
        std::uint32_t otherFlag = 0;
        if (!ReadGuest(selfFlag, selfAddress) || !ReadGuest(otherFlag, otherAddress)) {
            scePthreadRwlockUnlock(lock);
            return AudiodecErrorArgument;
        }
        if (selfFlag == 0) {
            scePthreadRwlockUnlock(lock);
            return AudiodecErrorNotRegistered;
        }
        if (otherFlag != 0) {
            selfFlag = 0;
            const bool written = WriteGuest(selfAddress, selfFlag);
            scePthreadRwlockUnlock(lock);
            return written ? 0 : AudiodecErrorArgument;
        }
        std::uint32_t ajmContext = 0;
        if (!ReadGuest(ajmContext, reinterpret_cast<const std::uint32_t*>(address + 4u))) {
            scePthreadRwlockUnlock(lock);
            return AudiodecErrorArgument;
        }
        const int status = RetryService([&] {
            return sceAjmModuleUnregister(ajmContext, 24);
        });
        if (status == 0) {
            selfFlag = 0;
            const bool written = WriteGuest(selfAddress, selfFlag);
            scePthreadRwlockUnlock(lock);
            return written ? 0 : AudiodecErrorArgument;
        }
        scePthreadRwlockUnlock(lock);
        return status;
    }
    if (codecType == 1 || codecType == 2 || codecType == 3) {
        if (address > UINTPTR_MAX - 4u) return AudiodecErrorArgument;
        std::uint32_t ajmContext = 0;
        if (!ReadGuest(ajmContext, reinterpret_cast<const std::uint32_t*>(address + 4u))) {
            return AudiodecErrorArgument;
        }
        const std::uint32_t ajmCodec = codecType == 1 ? 1u : (codecType == 2 ? 0u : 2u);
        return RetryService([&] {
            return sceAjmModuleUnregister(ajmContext, ajmCodec);
        });
    }
    if (codecType == 4 || codecType == 6 || codecType == 10) {
        std::uint32_t ajmiContext = 0;
        if (!ReadGuest(ajmiContext, reinterpret_cast<const std::uint32_t*>(address))) {
            return AudiodecErrorArgument;
        }
        const std::uint32_t ajmiCodec = codecType == 4 ? 6u : (codecType == 6 ? 9u : 20u);
        return RetryService([&] {
            return sceAjmiModuleUnregister(ajmiContext, ajmiCodec);
        });
    }
    return AudiodecErrorInvalidType;
}

}
