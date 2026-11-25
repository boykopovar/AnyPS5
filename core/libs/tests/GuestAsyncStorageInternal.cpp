#include "prx/libSceAsyncStorageInternal/libSceAsyncStorageInternal.hpp"
#include <cstdint>
#include <cstdlib>

namespace {

constexpr int nullError = static_cast<int>(0x81860001);
constexpr int busyError = static_cast<int>(0x8186000d);

void Require(bool value) {
    if (!value) {
        std::abort();
    }
}

void HandlerA() {}
void HandlerB() {}

}

int main() {
    Require(libSceAsyncStorageInternalUnknown_65H7_minus_ohmD9c_nid_no_patch(nullptr, 0u) == nullError);
    std::uint32_t count = 0x12345678u;
    Require(libSceAsyncStorageInternalUnknown_65H7_minus_ohmD9c_nid_no_patch(&count, 1u) == 0);
    Require(count == 0u);

    Require(libSceAsyncStorageInternalUnknown_hP_plus_HenjaOi8_nid_no_patch(nullptr, 0u) == nullError);
    std::uint64_t valA = 0xdeadbeefcafebabeULL;
    Require(libSceAsyncStorageInternalUnknown_hP_plus_HenjaOi8_nid_no_patch(&valA, 1u) == 0);
    Require(valA == 0ULL);

    Require(libSceAsyncStorageInternalUnknown_iYDbqYNVajM_nid_no_patch(nullptr) == nullError);
    std::uint64_t valB = 0xfeedface12345678ULL;
    Require(libSceAsyncStorageInternalUnknown_iYDbqYNVajM_nid_no_patch(&valB) == 0);
    Require(valB == 0ULL);

    Require(libSceAsyncStorageInternalUnknown_EKjWxMVMPzY_nid_no_patch(nullptr) == nullError);
    Require(libSceAsyncStorageInternalUnknown_jR1uFCw8qwc_nid_no_patch(nullptr) == nullError);

    Require(libSceAsyncStorageInternalUnknown_EKjWxMVMPzY_nid_no_patch(reinterpret_cast<void*>(&HandlerA)) == 0);
    Require(libSceAsyncStorageInternalUnknown_EKjWxMVMPzY_nid_no_patch(reinterpret_cast<void*>(&HandlerB)) == busyError);

    Require(libSceAsyncStorageInternalUnknown_jR1uFCw8qwc_nid_no_patch(reinterpret_cast<void*>(&HandlerA)) == 0);
    Require(libSceAsyncStorageInternalUnknown_jR1uFCw8qwc_nid_no_patch(reinterpret_cast<void*>(&HandlerB)) == busyError);

    Require(libSceAsyncStorageInternalUnknown_TN9xRCVggRw_nid_no_patch() == 0);
    Require(libSceAsyncStorageInternalUnknown_EKjWxMVMPzY_nid_no_patch(reinterpret_cast<void*>(&HandlerB)) == 0);
    Require(libSceAsyncStorageInternalUnknown_jR1uFCw8qwc_nid_no_patch(reinterpret_cast<void*>(&HandlerB)) == 0);
    Require(libSceAsyncStorageInternalUnknown_TN9xRCVggRw_nid_no_patch() == 0);

    Require(libSceAsyncStorageInternalUnknown_r299OxwCYsY_nid_no_patch() == 0ULL);
    return 0;
}
