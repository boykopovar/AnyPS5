#include "prx/libSceJson2/include/Api.hpp"
#include "SceTypes.hpp"

[[noreturn]] void APS5_VABI JsonMemAllocatorPureVirtual_nid_no_patch();

extern "C" {

[[noreturn]] void APS5_VABI _ZN3sce4Json12MemAllocatorD0Ev(void*) { JsonMemAllocatorPureVirtual_nid_no_patch(); }

}
