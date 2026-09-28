#include "prx/libc/include/General.hpp"
#include "prx/libc/include/HeapDiagnostics.hpp"

extern "C" {

int Need_sceLibcInternal_nid_postfix = 1;

void APS5_VABI __cxa_finalize_nid_postfix(void* dsoHandle) {
    CxaFinalize_nid_no_patch(dsoHandle);
}

void APS5_VABI sceLibcHeapGetTraceInfo_nid_postfix(Info* info) {
    LibcHeapTraceInfo_nid_no_patch(info);
}

}
