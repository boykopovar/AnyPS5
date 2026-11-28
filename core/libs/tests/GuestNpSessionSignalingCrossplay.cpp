#include "prx/libSceNpSessionSignalingCrossplay/libSceNpSessionSignalingCrossplay.hpp"
#include "prx/libc/include/GuestArena.hpp"

#include <array>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {

void ClobberErrno(std::uintptr_t, std::size_t) {
    errno = 99;
}

constexpr int ErrorNotInitialized = static_cast<int>(0x80553D01u);
constexpr int ErrorAlreadyInitialized = static_cast<int>(0x80553D02u);
constexpr int ErrorInvalidArgument = static_cast<int>(0x80553D03u);

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "%s\n", message);
    std::abort();
}

} // namespace

int main() {
    GuestArena::GuestArenaSetHostReadHook_nid_no_patch(ClobberErrno);
    GuestArena::GuestArenaSetHostWriteHook_nid_no_patch(ClobberErrno);

    std::array<std::uint8_t, 64> buffer{};
    void* p = buffer.data();
    for (const int expectedErrno : {0, 7, 42}) {
        errno = expectedErrno;
        Require(sceNpSessionSignalingCrossplayInitialize(nullptr) == ErrorInvalidArgument,
                "Initialize must reject a null parameter");
        Require(sceNpSessionSignalingCrossplayGetMemoryInfo(nullptr) == ErrorInvalidArgument,
                "GetMemoryInfo must reject a null info");
        Require(sceNpSessionSignalingCrossplayUnknown02(1, nullptr, 2, p, p) == ErrorInvalidArgument,
                "wypjyjGWaLM must reject a null second argument");
        Require(sceNpSessionSignalingCrossplayUnknown02(1, p, 2, nullptr, p) == ErrorInvalidArgument,
                "wypjyjGWaLM must reject a null fourth argument");
        Require(sceNpSessionSignalingCrossplayUnknown02(1, p, 2, p, nullptr) == ErrorInvalidArgument,
                "wypjyjGWaLM must reject a null fifth argument");
        Require(sceNpSessionSignalingCrossplayUnknown02(0, nullptr, 0, nullptr, nullptr) == ErrorInvalidArgument,
                "wypjyjGWaLM must reject all null pointers");
        Require(errno == expectedErrno, "The null checks must not change errno");
    }
    Require(buffer == std::array<std::uint8_t, 64>{}, "Rejected calls must not write");

    Require(sceNpSessionSignalingCrossplayTerminate() == ErrorNotInitialized,
            "Terminate must return not initialized before Initialize");

    NpSessionSignalingCrossplayMemoryInfo info{};
    std::array<unsigned char, sizeof(info)> filledA5{};
    filledA5.fill(0xA5);
    std::memcpy(&info, filledA5.data(), sizeof(info));
    Require(sceNpSessionSignalingCrossplayGetMemoryInfo(&info) == ErrorNotInitialized,
            "GetMemoryInfo must return not initialized before Initialize");
    Require(std::memcmp(&info, filledA5.data(), sizeof(info)) == 0,
            "Uninitialized GetMemoryInfo must not modify info");

    errno = 71;
    Require(sceNpSessionSignalingCrossplayGetMemoryInfo(reinterpret_cast<NpSessionSignalingCrossplayMemoryInfo*>(0x10)) == ErrorNotInitialized,
            "GetMemoryInfo must return not initialized for unmapped pointer before Initialize");
    Require(errno == 71, "Pre-init GetMemoryInfo must preserve errno");

    errno = 71;
    Require(sceNpSessionSignalingCrossplayInitialize(reinterpret_cast<const NpSessionSignalingCrossplayInitParam*>(0x10)) == ErrorInvalidArgument,
            "Initialize must reject an unreadable parameter pointer");
    Require(errno == 71, "Initialize must preserve errno when guest copy fails");

    NpSessionSignalingCrossplayInitParam zeroHeapParam{};
    Require(sceNpSessionSignalingCrossplayInitialize(&zeroHeapParam) == ErrorInvalidArgument,
            "Initialize must reject a zero heapSize");

    NpSessionSignalingCrossplayInitParam validParam{};
    validParam.heapSize = 0x10000;
    errno = 71;
    Require(sceNpSessionSignalingCrossplayInitialize(&validParam) == 0,
            "Initialize must succeed with a valid heapSize");
    Require(errno == 71, "Initialize must preserve errno on success");

    Require(sceNpSessionSignalingCrossplayInitialize(&validParam) == ErrorAlreadyInitialized,
            "Repeated Initialize must return already initialized");
    Require(sceNpSessionSignalingCrossplayInitialize(nullptr) == ErrorInvalidArgument,
            "Initialize with nullptr must still return invalid argument");

    Require(sceNpSessionSignalingCrossplayGetMemoryInfo(nullptr) == ErrorInvalidArgument,
            "GetMemoryInfo must reject nullptr after Initialize");
    errno = 71;
    Require(sceNpSessionSignalingCrossplayGetMemoryInfo(reinterpret_cast<NpSessionSignalingCrossplayMemoryInfo*>(0x10)) == ErrorInvalidArgument,
            "GetMemoryInfo must reject an unwritable output pointer");
    Require(errno == 71, "GetMemoryInfo must preserve errno when guest write fails");

    std::memcpy(&info, filledA5.data(), sizeof(info));
    errno = 71;
    Require(sceNpSessionSignalingCrossplayGetMemoryInfo(&info) == 0,
            "GetMemoryInfo must succeed after Initialize");
    Require(errno == 71, "GetMemoryInfo must preserve errno on success");
    Require(info.poolSize >= validParam.heapSize && info.poolSize > 0,
            "GetMemoryInfo must report a non-zero poolSize");
    Require(info.maxInuseSize >= info.currentInuseSize,
            "GetMemoryInfo must report maxInuseSize >= currentInuseSize");

    Require(sceNpSessionSignalingCrossplayTerminate() == 0,
            "Terminate must succeed after Initialize");
    Require(sceNpSessionSignalingCrossplayTerminate() == ErrorNotInitialized,
            "Second Terminate must return not initialized");

    std::memcpy(&info, filledA5.data(), sizeof(info));
    Require(sceNpSessionSignalingCrossplayGetMemoryInfo(&info) == ErrorNotInitialized,
            "GetMemoryInfo must return not initialized after Terminate");
    Require(std::memcmp(&info, filledA5.data(), sizeof(info)) == 0,
            "Post-terminate GetMemoryInfo must not modify info");
}
