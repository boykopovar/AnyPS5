#include "prx/libkernel/File/include/File.hpp"

#include <stdexcept>
#include <string>

extern "C" {

int APS5_VABI sceKernelClose(int d) {
 (void)d;
 throw std::runtime_error(std::string(__func__) + " is not implemented");
}

int APS5_VABI sceKernelLseek(int d, int64_t offset, int whence) {
 (void)d;
 (void)offset;
 (void)whence;

 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceKernelOpen(const char* path, int flags, uint16_t mode) {
 (void)path;
 (void)flags;
 (void)mode;

 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int64_t APS5_VABI sceKernelRead(int d, void* buf, size_t nbytes) {
 (void)d;
 (void)buf;
 (void)nbytes;

 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceKernelStat(const char* path, FileStat* sb) {
 (void)path;
 (void)sb;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceKernelUnlink(const char* path) {
 (void)path;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int64_t APS5_VABI sceKernelWrite(int d, const void* buf, size_t nbytes) {
 (void)d;
 (void)buf;
 (void)nbytes;

 NotImplemented_nid_no_patch(__func__);
 return 0;
}

}
