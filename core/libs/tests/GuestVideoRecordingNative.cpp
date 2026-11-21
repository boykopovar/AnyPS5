#include "prx/libSceVideoRecording.native/libSceVideoRecording.native.hpp"

#include <array>
#include <cstdint>
#include <cstdlib>

namespace {

void Require(bool value) {
    if (!value) std::abort();
}

constexpr int invalidArgument = static_cast<int>(0x80a80003u);

}

int main() {
    std::array<std::uint8_t, 64> buffer{};
    const void* handle = buffer.data();

    for (const auto call : std::array<decltype(&sceVideoRecordingGetFormatByAppId), 3>{
             sceVideoRecordingGetFormatByAppId, sceVideoRecordingGetRecordedDurationByAppId, sceVideoRecordingGetStatusByAppId}) {
        Require(call(-1) == invalidArgument);
    }
    Require(sceVideoRecordingFinalizeByAppId(-1, 0) == invalidArgument);
    for (const std::uint32_t argument2 : std::array<std::uint32_t, 3>{3, 4, 0xFFFFFFFFu}) {
        Require(sceVideoRecordingFinalizeByAppId(1, argument2) == invalidArgument);
        Require(sceVideoRecordingFinalizeByHandle(handle, argument2) == invalidArgument);
    }
    Require(sceVideoRecordingFinalizeByHandle(nullptr, 0) == invalidArgument);

    char extension[8] = {};
    Require(sceVideoRecordingGetFormatExtensionByAppId(-1, extension, sizeof(extension)) == invalidArgument);
    Require(sceVideoRecordingGetFormatExtensionByAppId(1, nullptr, sizeof(extension)) == invalidArgument);
    for (const std::size_t size : std::array<std::size_t, 3>{0, 1, 4}) {
        Require(sceVideoRecordingGetFormatExtensionByAppId(1, extension, size) == invalidArgument);
    }

    Require(sceVideoRecordingGetCountScanInAction(nullptr) == invalidArgument);
    Require(sceVideoRecordingGetFinalizeActiveInfo(nullptr) == invalidArgument);
    Require(sceVideoRecordingIsProfileChangeSupported(nullptr) == invalidArgument);

    for (const auto call : std::array<decltype(&sceVideoRecordingGetStatusByHandle), 5>{
             sceVideoRecordingGetRecordedDurationByHandle, sceVideoRecordingGetStatusByHandle, sceVideoRecordingSetInfoByHandle,
             sceVideoRecordingStartByHandle, sceVideoRecordingStopByHandle}) {
        Require(call(nullptr) == invalidArgument);
    }
    Require(sceVideoRecordingSetResourceStateByHandle(nullptr, 0) == invalidArgument);
    for (const std::uint32_t state : std::array<std::uint32_t, 3>{2, 3, 0xFFFFFFFFu}) {
        Require(sceVideoRecordingSetResourceStateByHandle(handle, state) == invalidArgument);
    }

    for (const std::uint32_t limit : std::array<std::uint32_t, 3>{2, 3, 0xFFFFFFFFu}) {
        Require(sceVideoRecordingSetProfileLimit(limit) == invalidArgument);
    }
    for (const std::uint32_t mode : std::array<std::uint32_t, 3>{3, 4, 0xFFFFFFFFu}) {
        Require(sceVideoRecordingSetPrivacyImage(buffer.data(), buffer.data(), mode) == invalidArgument);
    }
    return 0;
}
