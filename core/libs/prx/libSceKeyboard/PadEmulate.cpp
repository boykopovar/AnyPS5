#include "prx/libSceKeyboard/include/PadEmulate.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libSceKeyboard/include/KeyboardState.hpp"

extern "C" {

int APS5_VABI sceKeyboardPadEmulateClose(void) {
    if (!Keyboard::IsInitialized()) return KEYBOARD_ERROR_NOT_INITIALIZED;
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceKeyboardPadEmulateOpen(void) {
    if (!Keyboard::IsInitialized()) return KEYBOARD_ERROR_NOT_INITIALIZED;
    NotImplemented_nid_no_patch(__func__);
}

}
