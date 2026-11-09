#include "prx/libScePs2EmuMenuDialog/include/Api.hpp"
#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"

namespace {

constexpr int PS2_EMU_MENU_DIALOG_ERROR_UNAVAILABLE = static_cast<int>(0x80B8000Fu);
constexpr int COMMON_DIALOG_STATUS_NONE = 0;

}

extern "C" {

int APS5_VABI scePs2EmuMenuDialogClose() {
    return PS2_EMU_MENU_DIALOG_ERROR_UNAVAILABLE;
}

int APS5_VABI scePs2EmuMenuDialogGetResult() {
    return PS2_EMU_MENU_DIALOG_ERROR_UNAVAILABLE;
}

int APS5_VABI scePs2EmuMenuDialogGetStatus() {
    return COMMON_DIALOG_STATUS_NONE;
}

int APS5_VABI scePs2EmuMenuDialogInitialize() {
    return PS2_EMU_MENU_DIALOG_ERROR_UNAVAILABLE;
}

int APS5_VABI scePs2EmuMenuDialogOpen() {
    return PS2_EMU_MENU_DIALOG_ERROR_UNAVAILABLE;
}

int APS5_VABI scePs2EmuMenuDialogTerminate() {
    return PS2_EMU_MENU_DIALOG_ERROR_UNAVAILABLE;
}

int APS5_VABI scePs2EmuMenuDialogUpdateStatus() {
    return COMMON_DIALOG_STATUS_NONE;
}

}
