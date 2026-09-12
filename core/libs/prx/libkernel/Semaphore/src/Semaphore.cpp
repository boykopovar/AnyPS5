#include "prx/libkernel/Semaphore/include/Semaphore.hpp"

#include <stdexcept>

extern "C" {

int APS5_VABI sceKernelCreateSema(KernelSema* sem, const char* name, uint32_t attr, int init, int max, void* opt) {
 (void)sem;
 (void)name;
 (void)attr;
 (void)init;
 (void)max;
 (void)opt;
 throw std::runtime_error("sceKernelCreateSema is not implemented");
}

int APS5_VABI sceKernelPollSema(KernelSema sem, int need) {
 (void)sem;
 (void)need;
 throw std::runtime_error("sceKernelPollSema is not implemented");
}

int APS5_VABI sceKernelSignalSema(KernelSema sem, int count) {
 (void)sem;
 (void)count;
 throw std::runtime_error("sceKernelSignalSema is not implemented");
}

int APS5_VABI sceKernelWaitSema(KernelSema sem, int need, KernelUseconds* time) {
 (void)sem;
 (void)need;
 (void)time;
 throw std::runtime_error("sceKernelWaitSema is not implemented");
}

}
