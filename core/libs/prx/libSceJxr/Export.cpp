#include "prx/libc/include/Api.hpp"
#include "prx/libSceJxr/include/Api.hpp"
#include <cstddef>
#include <cstdint>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

namespace {
constexpr std::int64_t WmpErrSuccess = 0;
}

extern "C" {

APS5_DUMMY_FUN
std::int64_t APS5_VABI PKFree_nid_postfix(void** memory) {
    if (memory != nullptr) {
        free_nid_postfix(*memory);

        *memory = nullptr;
    }
    return WmpErrSuccess;
}

int APS5_VABI PixelFormatLookup_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI readIS_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI writeIS_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

}
