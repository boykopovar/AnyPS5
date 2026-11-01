#include "prx/libSceLibcInternal/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include "SceTypes.hpp"
#include <cstddef>

extern "C" {

void* APS5_VABI sceLibcInternalAlignedArrayNewNothrow_nid_no_patch(std::size_t size, std::size_t alignment, const void* tag) noexcept {
    return _ZnamSt11align_val_tRKSt9nothrow_t_nid_postfix(size, alignment, tag);
}
APS5_EXPORT("s2eGsgUF9vk", sceLibcInternalAlignedArrayNewNothrow_nid_no_patch);

void APS5_VABI sceLibcInternalAlignedArrayDelete_nid_no_patch(void* pointer, std::size_t alignment) {
    _ZdaPvSt11align_val_t_nid_postfix(pointer, alignment);
}
APS5_EXPORT("v09ZcAhZzSc", sceLibcInternalAlignedArrayDelete_nid_no_patch);

}
