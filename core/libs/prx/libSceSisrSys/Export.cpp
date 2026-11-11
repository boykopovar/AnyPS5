#include "prx/libSceSisrSys/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include <cerrno>

extern "C" {

static SisrSysSetupParams g_setup{};

int APS5_VABI sceSisrSysProcessUmdEvent(const SisrSysUmdEvent* event) {
    if (event == nullptr || event->magic != 0) NotImplemented_nid_no_patch(__func__);
    switch (event->type) {
        case 1:
        case 2:
        case 4:
        case 6:
        case 7:
            return static_cast<int>(event->type);
        default:
            NotImplemented_nid_no_patch(__func__);
    }
}

int APS5_VABI sceSisrSysSetup(const SisrSysSetupParams* params) {
    if (params != nullptr) g_setup = *params;
    return 0;
}

}
