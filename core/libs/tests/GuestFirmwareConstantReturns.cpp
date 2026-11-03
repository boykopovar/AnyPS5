#include "SceTypes.hpp"
#include "prx/libSceAvSetting/libSceAvSetting.hpp"
#include "prx/libSceComposite/libSceComposite.hpp"
#include "prx/libSceFsInternalForVsh/libSceFsInternalForVsh.hpp"
#include "prx/libSceHttpCache/libSceHttpCache.hpp"
#include "prx/libScePlayReady/libScePlayReady.hpp"
#include "prx/libSceSdma/libSceSdma.hpp"
#include "prx/libSceSsl/libSceSsl.hpp"
#include "prx/libSceVisionManager/libSceVisionManager.hpp"
#include "prx/libmdbg_syscore/libmdbg_syscore.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>

namespace {

void Require(bool value) { if (!value) std::abort(); }

void CheckAvSetting() {
    constexpr int AvSettingError = static_cast<int>(0x802A00FF);
    Require(sceAvSettingSetDisplayBlackLevel() == AvSettingError);
    Require(sceAvSettingTerm() == AvSettingError);
}

void CheckAvSettingCallbackFuncsInit(void (APS5_VABI* initialize)(void*), std::size_t size) {
    initialize(nullptr);
    constexpr std::size_t Offset = 8;
    for (const std::uint8_t fill : {std::uint8_t{0xaa}, std::uint8_t{0x00}, std::uint8_t{0xff}}) {
        std::array<std::uint8_t, 0x40> buffer{};
        buffer.fill(fill);
        initialize(buffer.data() + Offset);
        for (std::size_t i = 0; i < buffer.size(); ++i) {
            const bool inside = i >= Offset && i < Offset + size;
            const std::uint8_t expected = !inside ? fill : i == Offset ? static_cast<std::uint8_t>(size) : 0;
            Require(buffer[i] == expected);
        }
    }
}

void CheckAvSettingCallbackFuncs() {
    CheckAvSettingCallbackFuncsInit(sceAvSettingCallbackFuncsInit, 0x20);
    CheckAvSettingCallbackFuncsInit(sceAvSettingCallbackFuncsInit_, 0x20);
    CheckAvSettingCallbackFuncsInit(sceAvSettingCallbackFuncsInitForLnc, 0x10);
    CheckAvSettingCallbackFuncsInit(sceAvSettingCallbackFuncsInitForLnc_, 0x10);
}

void CheckComposite() {
    constexpr int Unsupported = static_cast<int>(0x80d40006);
    Require(sceCompositorCreateIndirectRenderTarget() == Unsupported);
    Require(sceCompositorDeleteIndirectRenderTarget() == Unsupported);
    Require(sceCompositorSetIndirectCanvasCommand() == 0);
    Require(sceCompositorSetIndirectCompositionCommand() == 0);
    Require(sceCompositorSetIndirectRenderTargetConfigCommand() == 0);
}

void CheckFsInternal() {
    constexpr int KernelNotSupported = static_cast<int>(0x8002002d);
    Require(sceFsErriCtl() == KernelNotSupported);
    Require(sceFsErriGetState() == KernelNotSupported);
}

void CheckHttpCacheAndPlayReady() {
    Require(sceHttpCacheDebugSetUrl() == 0);
    Require(scePlayReadyMemGetStats() == static_cast<int>(0x80004001));
}

void CheckSdma() {
    constexpr int SdmaError = static_cast<int>(0x809900c9);
    Require(sceSdmaInitialize() == 0);
    Require(sceSdmaFinalize() == 0);
    Require(sceSdmaQueryWorkingBufferSize() == 0x8000);
    Require(sceSdmaFlushAllWithEvent() == SdmaError);
    Require(sceSdmaWaitForEvent() == SdmaError);
}

void CheckSsl() {
    Require(CRYPTO_uninitAsymmetricKey() == 0);
    Require(VLONG_freeVlongQueue() == 0);
}

void CheckVisionManager() {
    constexpr int VisionError = static_cast<int>(0x80e70004);
    Require(sceVisionManagerOpen() == VisionError);
    Require(sceVisionManagerClose() == VisionError);
    Require(sceVisionManagerRequestFindUser() == VisionError);
    Require(sceVisionManagerRequestFindUserInRegion() == VisionError);
    Require(sceVisionManagerRequestFindUserWithoutIdentify() == VisionError);
    Require(sceVisionManagerRequestRegisterUserInRegion() == VisionError);
    Require(sceVisionManagerGetResultOfFindUser() == VisionError);
    Require(sceVisionManagerGetResultOfFindUserInRegion() == VisionError);
    Require(sceVisionManagerGetResultOfFindUserWithoutIdentify() == VisionError);
    Require(sceVisionManagerGetResultOfRegisterUserInRegion() == VisionError);
    Require(sceVisionManagerLoadSettings() == 0);

    constexpr int VisionInvalid = static_cast<int>(0x80e70000);
    constexpr std::uint64_t LibraryVersion = 0x1020000214072421;
    constexpr std::uint64_t RegisterUserDataVersion = 0x1017000014043000;
    std::uint64_t version[3] = {~0ull, ~0ull, ~0ull};
    Require(sceVisionManagerGetLibraryVersion(nullptr) == VisionInvalid);
    Require(sceVisionManagerGetLibraryVersion(&version[1]) == 0);
    Require(version[0] == ~0ull && version[1] == LibraryVersion && version[2] == ~0ull);

    const std::uint64_t versions[] = {0, 1, RegisterUserDataVersion, LibraryVersion, LibraryVersion - 1, RegisterUserDataVersion + 1, ~0ull};
    const std::uint32_t slots[] = {0, 0, 16, 30, 0, 0, 0};
    for (std::size_t i = 0; i < sizeof(versions) / sizeof(versions[0]); ++i) {
        std::uint32_t count = 0xaaaaaaaa;
        const int result = sceVisionManagerGetNumOfRegisterUserDataSlot(versions[i], &count);
        Require(slots[i] == 0 ? result == VisionInvalid && count == 0xaaaaaaaa : result == 0 && count == slots[i]);
    }

    const std::uint32_t fills[] = {0xaaaaaaaau, 0u, ~0u};
    for (const std::uint32_t fill : fills) {
        std::uint32_t sizes[3] = {fill, fill, fill};
        Require(sceVisionManagerGetWorkingMemorySize(&sizes[0], &sizes[1], &sizes[2]) == 0);
        Require(sizes[0] == 0 && sizes[1] == 0x1600000 && sizes[2] == 0x4200000);
    }

    int context = 0;
    const std::int32_t parameters[] = {0, 1, 3, 4, 5, 6, -1, 0x7fffffff, static_cast<std::int32_t>(0x80000000)};
    for (const std::int32_t parameter : parameters) {
        const bool findPad = parameter != 0 && parameter < 5;
        const bool padTracking = parameter != 0 && parameter < 4;
        Require(sceVisionManagerRequestFindPad(&context, &parameter) == (findPad ? VisionError : VisionInvalid));
        Require(sceVisionManagerRequestFindPad(nullptr, &parameter) == VisionInvalid);
        Require(sceVisionManagerRequestPadTracking(nullptr, &parameter) == (padTracking ? VisionError : VisionInvalid));
        Require(sceVisionManagerRequestPadTracking(&context, &parameter) == (padTracking ? VisionError : VisionInvalid));
        const std::int32_t regions[] = {-1, 0, 1, 2, 4, 5, 100};
        for (const std::int32_t region : regions) {
            const bool accepted = findPad && region >= 1 && region <= 4;
            Require(sceVisionManagerRequestFindPadCorrespondRegion(&context, &parameter, region, &context) == (accepted ? VisionError : VisionInvalid));
        }
        Require(sceVisionManagerRequestFindPadCorrespondRegion(nullptr, &parameter, 1, &context) == VisionInvalid);
        Require(sceVisionManagerRequestFindPadCorrespondRegion(&context, &parameter, 1, nullptr) == VisionInvalid);
    }
    Require(sceVisionManagerRequestFindPad(&context, nullptr) == VisionInvalid);
    Require(sceVisionManagerRequestPadTracking(&context, nullptr) == VisionInvalid);
    Require(sceVisionManagerRequestFindPadCorrespondRegion(&context, nullptr, 1, &context) == VisionInvalid);

    for (int mask = 0; mask < 8; ++mask) {
        void* const first = (mask & 1) ? &context : nullptr;
        void* const second = (mask & 2) ? &context : nullptr;
        void* const third = (mask & 4) ? &context : nullptr;
        const int expected = mask == 7 ? VisionError : VisionInvalid;
        Require(sceVisionManagerGetResultOfFindPad(first, second, third) == expected);
        Require(sceVisionManagerGetResultOfFindPadCorrespondRegion(first, second, third) == expected);
    }

    const std::uint32_t inputs[][2] = {{0, 0}, {9, 0}, {0, 9}, {10, 0}, {0, 10}, {29, 3}, {3, 29}, {30, 0}, {0, 30}, {~0u, 0}, {0x80000000, 1}, {100, 200}};
    const std::uint32_t levels[] = {0, 0, 0, 5, 5, 5, 5, 10, 10, 10, 10, 10};
    for (std::size_t i = 0; i < sizeof(levels) / sizeof(levels[0]); ++i) {
        std::uint32_t output[10];
        for (auto& value : output) value = 0xaaaaaaaa;
        Require(libSceVisionManagerUnknown__minus_R5dI6KjYpY_nid_no_patch(inputs[i], &output[1]) == 0);
        Require(output[0] == 0xaaaaaaaa && output[1] == levels[i] && output[9] == 0xaaaaaaaa);
        for (std::size_t k = 2; k < 9; ++k) Require(output[k] == 0);
    }
    std::uint32_t output[8] = {0xaaaaaaaa};
    Require(libSceVisionManagerUnknown__minus_R5dI6KjYpY_nid_no_patch(nullptr, output) == VisionInvalid);
    Require(libSceVisionManagerUnknown__minus_R5dI6KjYpY_nid_no_patch(inputs[0], nullptr) == VisionInvalid);
    Require(output[0] == 0xaaaaaaaa);
}

void CheckDebugSyscore() {
    Require(sceDebugGetProcessEventCntlFlag() == 1);
    Require(sceDebugSetProcessEventCntlFlag() == 1);
}

}

int main() {
    CheckAvSetting();
    CheckAvSettingCallbackFuncs();
    CheckComposite();
    CheckFsInternal();
    CheckHttpCacheAndPlayReady();
    CheckSdma();
    CheckSsl();
    CheckVisionManager();
    CheckDebugSyscore();
    return 0;
}
