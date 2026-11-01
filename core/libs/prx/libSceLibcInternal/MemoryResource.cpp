#include "prx/libSceLibcInternal/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include "SceTypes.hpp"

extern "C" {

void* APS5_VABI sceLibcInternalPmrNewDeleteResource_nid_no_patch() noexcept {
    return _Aligned_new_delete_resource_nid_postfix();
}
APS5_EXPORT("9G32u5RRYxE", sceLibcInternalPmrNewDeleteResource_nid_no_patch);

void* APS5_VABI sceLibcInternalPmrGetDefaultResource_nid_no_patch() noexcept {
    return _Aligned_get_default_resource_nid_postfix();
}
APS5_EXPORT("d2u38zs4Pe8", sceLibcInternalPmrGetDefaultResource_nid_no_patch);

void* APS5_VABI sceLibcInternalPmrNullMemoryResource_nid_no_patch() noexcept {
    return null_memory_resource_nid_postfix();
}
APS5_EXPORT("eWMGI7B7Lyc", sceLibcInternalPmrNullMemoryResource_nid_no_patch);

void* APS5_VABI sceLibcInternalPmrSetDefaultResource_nid_no_patch(void* resource) noexcept {
    return _Aligned_set_default_resource_nid_postfix(resource);
}
APS5_EXPORT("TKYsv0jdvRw", sceLibcInternalPmrSetDefaultResource_nid_no_patch);

}
