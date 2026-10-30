#include "prx/libSceEsvm/include/InventoryExports.hpp"
#include "prx/libSceEsvm/include/Api.hpp"
#include <cstddef>
#include <cstdint>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

APS5_DUMMY_FUN

}
extern "C" {

int APS5_VABI sceEsvmAppAudioSyncCreateContext() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceEsvmAppAudioSyncFlush() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceEsvmSyncVideo() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

}

extern "C" {

int APS5_VABI sceEsvmInitializePlayReady() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceEsvmInitializeWidevine() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceEsvmRequestMediaKeySystemAccessPlayReady() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceEsvmRequestMediaKeySystemAccessWidevine() {
    NotImplemented_nid_no_patch(__func__);
}

}
