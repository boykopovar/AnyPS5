#include "prx/libSceKeyboard/include/PadEmulate.hpp"
#include "prx/libc/include/General.hpp"

namespace {

constexpr int PAD_EMULATE_ERROR_NOT_OPEN = static_cast<int>(0x80BC0002u);
constexpr int PAD_EMULATE_ERROR_NULL = static_cast<int>(0x80BC0022u);

}

extern "C" {

int APS5_VABI sceKeyboardPadEmulateClose(void) {
    return PAD_EMULATE_ERROR_NOT_OPEN;
}

int APS5_VABI sceKeyboardPadEmulateOpen(void* param) {
    if (param == nullptr) return PAD_EMULATE_ERROR_NULL;
    NotImplemented_nid_no_patch(__func__);
}

}
