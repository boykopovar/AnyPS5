#include "prx/libkernel/File/include/File.hpp"

#include <stdexcept>

extern "C" {

int APS5_VABI sceKernelClose(int d) {
 (void)d;
 throw std::runtime_error("sceKernelClose is not implemented");
}

int APS5_VABI sceKernelLseek(int d, int64_t offset, int whence) {
 (void)d;
 (void)offset;
 (void)whence;
 throw std::runtime_error("sceKernelLseek is not implemented");
}

int APS5_VABI sceKernelOpen(const char* path, int flags, uint16_t mode) {
 (void)path;
 (void)flags;
 (void)mode;
 throw std::runtime_error("sceKernelOpen is not implemented");
}

int64_t APS5_VABI sceKernelRead(int d, void* buf, size_t nbytes) {
 (void)d;
 (void)buf;
 (void)nbytes;
 throw std::runtime_error("sceKernelRead is not implemented");
}

int APS5_VABI sceKernelStat(const char* path, FileStat* sb) {
 (void)path;
 (void)sb;
 throw std::runtime_error("sceKernelStat is not implemented");
}

int APS5_VABI sceKernelUnlink(const char* path) {
 (void)path;
 throw std::runtime_error("sceKernelUnlink is not implemented");
}

int64_t APS5_VABI sceKernelWrite(int d, const void* buf, size_t nbytes) {
 (void)d;
 (void)buf;
 (void)nbytes;
 throw std::runtime_error("sceKernelWrite is not implemented");
}

}
