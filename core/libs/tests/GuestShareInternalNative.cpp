#include "prx/libSceShareInternal.native/libSceShareInternal.native.hpp"

#include <array>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>

namespace {

constexpr int ErrorInvalidArgument = static_cast<int>(0x81960002u);

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "%s\n", message);
    std::abort();
}

void CheckIdOnly() {
    for (const auto call : std::array<decltype(&sceShareFeaturePermitCompatible), 3>{
             sceShareFeaturePermitCompatible, sceShareFeatureProhibitCompatible,
             libSceShareInternal_nativeUnknown__plus_LB32s0kCYg_nid_no_patch}) {
        Require(call(-1) == ErrorInvalidArgument, "An id of -1 must be rejected");
    }
}

void CheckPointerAndId() {
    std::array<std::uint8_t, 64> buffer{};
    void* p = buffer.data();

    Require(sceShareGetContentParam(-1, p, 1, p) == ErrorInvalidArgument, "GetContentParam must reject id -1");
    for (const std::size_t size : std::array<std::size_t, 4>{0, 0x1801, 0x2000, SIZE_MAX})
        Require(sceShareGetContentParam(1, p, size, p) == ErrorInvalidArgument, "GetContentParam must reject the size");
    Require(sceShareGetContentParam(1, nullptr, 1, p) == ErrorInvalidArgument, "GetContentParam must reject a null 2nd");
    Require(sceShareGetContentParam(1, p, 0x1800, nullptr) == ErrorInvalidArgument,
            "GetContentParam must reject a null 4th");

    Require(sceShareGetStoredAppStatus(-1, 0, p) == ErrorInvalidArgument, "GetStoredAppStatus must reject id -1");
    Require(sceShareGetStoredAppStatus(1, 0, nullptr) == ErrorInvalidArgument, "GetStoredAppStatus must reject null");
    Require(sceShareSetCurrentStatus(-1, 0, p) == ErrorInvalidArgument, "SetCurrentStatus must reject id -1");
    Require(sceShareSetCurrentStatus(1, 0, nullptr) == ErrorInvalidArgument, "SetCurrentStatus must reject null");

    Require(sceShareRegisterEventCallback(1, 0, nullptr) == ErrorInvalidArgument,
            "RegisterEventCallback must reject a null callback");
    Require(sceShareSetUploadEventInfo(1, nullptr, p) == ErrorInvalidArgument, "SetUploadEventInfo must reject null 2nd");
    Require(sceShareSetUploadEventInfo(1, p, nullptr) == ErrorInvalidArgument, "SetUploadEventInfo must reject null 3rd");
    Require(sceShareUnregisterEventCallback(nullptr) == ErrorInvalidArgument,
            "UnregisterEventCallback must reject a null callback");

    Require(libSceShareInternal_nativeUnknown_0IbPXX_minus_WvQQ_nid_no_patch(1, nullptr) == ErrorInvalidArgument,
            "0IbPXX-WvQQ must reject a null 2nd");
    Require(libSceShareInternal_nativeUnknown_IvkDssl52mA_nid_no_patch(1, nullptr) == ErrorInvalidArgument,
            "IvkDssl52mA must reject a null 2nd");

    Require(libSceShareInternal_nativeUnknown_8E7VGc9qRvY_nid_no_patch(-1, p, p, p) == ErrorInvalidArgument,
            "8E7VGc9qRvY must reject id -1");
    Require(libSceShareInternal_nativeUnknown_8E7VGc9qRvY_nid_no_patch(1, nullptr, p, p) == ErrorInvalidArgument,
            "8E7VGc9qRvY must reject a null 2nd");
    Require(libSceShareInternal_nativeUnknown_8E7VGc9qRvY_nid_no_patch(1, p, nullptr, p) == ErrorInvalidArgument,
            "8E7VGc9qRvY must reject a null 3rd");
    Require(libSceShareInternal_nativeUnknown_8E7VGc9qRvY_nid_no_patch(1, p, p, nullptr) == ErrorInvalidArgument,
            "8E7VGc9qRvY must reject a null 4th");
}

void CheckNamed() {
    std::array<std::uint8_t, 64> buffer{};
    void* p = buffer.data();
    const char name[] = "x";
    const char empty[] = "";
    const auto call = libSceShareInternal_nativeUnknown_zyzD8P5ZKPA_nid_no_patch;

    Require(call(nullptr, p, 1, p) == ErrorInvalidArgument, "zyzD8P5ZKPA must reject a null name");
    Require(call(name, nullptr, 1, p) == ErrorInvalidArgument, "zyzD8P5ZKPA must reject a null 2nd");
    Require(call(name, p, 1, nullptr) == ErrorInvalidArgument, "zyzD8P5ZKPA must reject a null 4th");
    for (const std::size_t size : std::array<std::size_t, 4>{0, 0x11, 0x100, SIZE_MAX})
        Require(call(name, p, size, p) == ErrorInvalidArgument, "zyzD8P5ZKPA must reject the size");
    Require(call(empty, p, 1, p) == ErrorInvalidArgument, "zyzD8P5ZKPA must reject an empty name");
    Require(call(empty, p, 0x10, p) == ErrorInvalidArgument, "zyzD8P5ZKPA must reject an empty name at size 0x10");
    Require(call(reinterpret_cast<const char*>(0x10), p, 0, p) == ErrorInvalidArgument,
            "zyzD8P5ZKPA must check the size before it reads the name");
    Require(buffer == std::array<std::uint8_t, 64>{}, "Rejected calls must not write");
}

} // namespace

int main() {
    for (const int expectedErrno : {0, 7, 42}) {
        errno = expectedErrno;
        CheckIdOnly();
        CheckPointerAndId();
        CheckNamed();
        Require(errno == expectedErrno, "The early answers must not change errno");
    }
}
