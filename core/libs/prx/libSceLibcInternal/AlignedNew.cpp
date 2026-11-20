#include "prx/libSceLibcInternal/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include "SceTypes.hpp"
#include <cstddef>

extern "C" {

void* APS5_VABI _ZnamSt11align_val_tRKSt9nothrow_t(std::size_t size, std::size_t alignment, const void* tag) noexcept {
    return AlignedArrayNewNothrow_nid_no_patch(size, alignment, tag);
}

void APS5_VABI _ZdaPvSt11align_val_t(void* pointer, std::size_t alignment) {
    AlignedArrayDelete_nid_no_patch(pointer, alignment);
}

}
