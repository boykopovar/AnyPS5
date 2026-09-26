#include "prx/libc/include/HeapDiagnostics.hpp"

extern "C" {

void APS5_VABI sceLibcHeapGetTraceInfo_nid_postfix(Info* info) {
    LibcHeapTraceInfo_nid_no_patch(info);
}

}
