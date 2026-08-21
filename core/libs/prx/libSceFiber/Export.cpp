#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"

extern "C" {

int32_t sceFiberFinalize(FiberObject* fiber) {
 (void)fiber;
 return 0;
}

int32_t sceFiberGetInfo(FiberObject* fiber, FiberInfo* fiber_info) {
 (void)fiber;
 (void)fiber_info;
 return 0;
}

int32_t sceFiberGetSelf(FiberObject** fiber) {
 (void)fiber;
 return 0;
}

int32_t sceFiberGetThreadFramePointerAddress(uint64_t* addr_frame_pointer) {
 (void)addr_frame_pointer;
 return 0;
}

int32_t sceFiberOptParamInitialize(FiberOptParam* opt_param) {
 (void)opt_param;
 return 0;
}

int32_t sceFiberRename(FiberObject* fiber, const char* name) {
 (void)fiber;
 (void)name;
 return 0;
}

int32_t sceFiberReturnToThread(uint64_t arg_on_return, uint64_t* arg_on_run) {
 (void)arg_on_return;
 (void)arg_on_run;
 return 0;
}

int32_t sceFiberRun_nid_postfix(FiberObject* fiber, uint64_t arg_on_run, uint64_t* arg_on_return) {
 (void)fiber;
 (void)arg_on_run;
 (void)arg_on_return;
 return 0;
}

int32_t sceFiberStartContextSizeCheck(uint32_t flags) {
 (void)flags;
 return 0;
}

int32_t sceFiberStopContextSizeCheck(void) {
 return 0;
}

int32_t sceFiberSwitch(FiberObject* fiber, uint64_t arg_on_run, uint64_t* arg_on_return) {
 (void)fiber;
 (void)arg_on_run;
 (void)arg_on_return;
 return 0;
}

}
