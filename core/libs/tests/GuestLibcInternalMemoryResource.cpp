#include "prx/libSceLibcInternal/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include <cstdio>
#include <cstdlib>

namespace {

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "libSceLibcInternal memory resource: %s\n", message);
    std::abort();
}

}

int main() {
    void* newDelete = sceLibcInternalPmrNewDeleteResource_nid_no_patch();
    Require(newDelete != nullptr, "new_delete_resource returned null");
    Require(newDelete == _Aligned_new_delete_resource_nid_postfix(), "new_delete_resource mismatch with libc");
    Require(newDelete == sceLibcInternalPmrNewDeleteResource_nid_no_patch(), "new_delete_resource is stable");

    void* defaultRes = sceLibcInternalPmrGetDefaultResource_nid_no_patch();
    Require(defaultRes != nullptr, "get_default_resource returned null");
    Require(defaultRes == _Aligned_get_default_resource_nid_postfix(), "get_default_resource mismatch with libc");
    Require(defaultRes == sceLibcInternalPmrGetDefaultResource_nid_no_patch(), "get_default_resource is stable");

    void* nullRes = sceLibcInternalPmrNullMemoryResource_nid_no_patch();
    Require(nullRes != nullptr, "null_memory_resource returned null");
    Require(nullRes == null_memory_resource_nid_postfix(), "null_memory_resource mismatch with libc");
    Require(nullRes == sceLibcInternalPmrNullMemoryResource_nid_no_patch(), "null_memory_resource is stable");

    Require(defaultRes == newDelete, "initial default resource matches new_delete_resource");
    Require(newDelete != nullRes, "new_delete and null resources must be distinct");

    void* previousDefault = sceLibcInternalPmrSetDefaultResource_nid_no_patch(nullRes);
    Require(previousDefault == defaultRes, "set_default_resource returned previous default");
    Require(sceLibcInternalPmrGetDefaultResource_nid_no_patch() == nullRes, "get_default_resource reflects update");

    void* previousFromNull = sceLibcInternalPmrSetDefaultResource_nid_no_patch(nullptr);
    Require(previousFromNull == nullRes, "set_default_resource with null returns previous default");
    Require(sceLibcInternalPmrGetDefaultResource_nid_no_patch() == newDelete, "set_default_resource with null resets default to new_delete");

    void* restoredDefault = sceLibcInternalPmrSetDefaultResource_nid_no_patch(defaultRes);
    Require(restoredDefault == newDelete, "restoring default returned new_delete");
    Require(sceLibcInternalPmrGetDefaultResource_nid_no_patch() == defaultRes, "get_default_resource restored");

    return 0;
}
