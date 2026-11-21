#include "prx/libSceDipsw/include/Api.hpp"
#include <array>
#include <cstdint>
#include <cstddef>
#include <mutex>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

namespace {
std::array<std::uint8_t, 8192> dipswState{};
std::mutex dipswMutex;
}

extern "C" {

int APS5_VABI sceKernelCheckDipsw(std::uint16_t id) {
    std::lock_guard<std::mutex> lock(dipswMutex);
    return (dipswState[id / 8] >> (id % 8)) & 1;
}

int APS5_VABI sceKernelInitializeDipsw() {
    return 0;
}

int APS5_VABI sceKernelIsAssistMode() {
    return 0;
}

int APS5_VABI sceKernelIsDevelopmentMode() {
    return 0;
}

int APS5_VABI sceKernelIsDisableRazor() {
    return 1;
}

int APS5_VABI sceKernelGetDiableBinaryVersionCheckValue() {
    return 0;
}

int APS5_VABI sceKernelReadDipswData() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelSetDipsw(std::uint16_t id) {
    std::lock_guard<std::mutex> lock(dipswMutex);
    dipswState[id / 8] |= static_cast<std::uint8_t>(1u << (id % 8));
    return 0;
}

int APS5_VABI sceKernelUnsetDipsw(std::uint16_t id) {
    std::lock_guard<std::mutex> lock(dipswMutex);
    dipswState[id / 8] &= static_cast<std::uint8_t>(~(1u << (id % 8)));
    return 0;
}

int APS5_VABI sceKernelWriteDipswData() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

}
