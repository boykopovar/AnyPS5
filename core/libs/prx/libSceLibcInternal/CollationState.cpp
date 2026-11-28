#include "prx/libSceLibcInternal/include/RuntimeInternals.hpp"

namespace {

LibcCollationState narrowState {};
LibcCollationState wideState {};

}

extern "C" {

LibcCollationState* APS5_VABI _Getpcostate_nid_postfix() {
    return &narrowState;
}

LibcCollationState* APS5_VABI _Getpwcostate_nid_postfix() {
    return &wideState;
}

}
