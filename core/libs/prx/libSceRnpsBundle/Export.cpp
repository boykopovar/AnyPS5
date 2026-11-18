#include "prx/libSceRnpsBundle/include/InventoryExports.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

int APS5_VABI sceRnpsBundleInit() {
    return 0;
}

int APS5_VABI sceRnpsBundleInitEx() {
    return 0;
}

int APS5_VABI sceRnpsBundleTerm() {
    return 0;
}

int APS5_VABI sceRnpsBundleVerifyAndDecrypt() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceRnpsBundleVerifyChain() {
    NotImplemented_nid_no_patch(__func__);
}

}
