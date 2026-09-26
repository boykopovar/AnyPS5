#include "prx/libc/include/General.hpp"
#include <cstdint>

extern "C" {

// unknown signature
void APS5_VABI sceLibcInternalBacktraceForGame_nid_postfix(const char* heapName) {
    (void)heapName;
    NotImplemented_nid_no_patch(__func__);
}

// unknown signature
void APS5_VABI sceLibcInternalHeapErrorReportForGame_nid_postfix(void* heap, void* block, std::uint32_t error) {
    (void)heap;
    (void)block;
    (void)error;
    NotImplemented_nid_no_patch(__func__);
}

}
