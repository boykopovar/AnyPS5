#include "prx/libSceNgs2/include/Api.hpp"
#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

int APS5_VABI sceNgs2FftInit() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceNgs2FftProcess() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceNgs2FftQuerySize() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceNgs2ParseWaveformUser(std::uintptr_t handler, std::uintptr_t userData, void* outInfo) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceNgs2SystemEnumHandles(std::uintptr_t* outHandles, std::uint32_t maxHandles) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceNgs2SystemEnumRackHandles(std::uintptr_t systemHandle, std::uintptr_t* outHandles, std::uint32_t maxHandles) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

}
