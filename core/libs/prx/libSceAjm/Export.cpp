#include "prx/libSceAjm/include/Api.hpp"
#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

APS5_DUMMY_FUN

int APS5_VABI sceAjmGetFailedInstance() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceAjmInstanceGetSize() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceAjmTelemetryGetLog() {
    NotImplemented_nid_no_patch(__func__);
}

}
