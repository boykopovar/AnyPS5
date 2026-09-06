#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

int32_t sceFiberFinalize(FiberObject* fiber) {
 (void)fiber;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int32_t sceFiberGetInfo(FiberObject* fiber, FiberInfo* fiber_info) {
 (void)fiber;
 (void)fiber_info;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int32_t sceFiberGetSelf(FiberObject** fiber) {
 (void)fiber;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int32_t sceFiberGetThreadFramePointerAddress(uint64_t* addr_frame_pointer) {
 (void)addr_frame_pointer;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int32_t sceFiberOptParamInitialize(FiberOptParam* opt_param) {
 (void)opt_param;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int32_t sceFiberRename(FiberObject* fiber, const char* name) {
 (void)fiber;
 (void)name;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int32_t sceFiberReturnToThread(uint64_t arg_on_return, uint64_t* arg_on_run) {
 (void)arg_on_return;
 (void)arg_on_run;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int32_t sceFiberRun_nid_postfix(FiberObject* fiber, uint64_t arg_on_run, uint64_t* arg_on_return) {
 (void)fiber;
 (void)arg_on_run;
 (void)arg_on_return;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int32_t sceFiberStartContextSizeCheck(uint32_t flags) {
 (void)flags;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int32_t sceFiberStopContextSizeCheck(void) {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int32_t sceFiberSwitch(FiberObject* fiber, uint64_t arg_on_run, uint64_t* arg_on_return) {
 (void)fiber;
 (void)arg_on_run;
 (void)arg_on_return;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

}
