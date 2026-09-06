#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

int sceSysmoduleGetModuleInfoForUnwind(uint64_t addr, int flags, ModuleInfoForUnwind* info) {
 (void)addr;
 (void)flags;
 (void)info;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int sceSysmoduleIsLoaded(uint16_t id) {
 (void)id;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int sceSysmoduleLoadModule(uint16_t id) {
 (void)id;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int sceSysmoduleLoadModuleInternalWithArg(uint16_t id, int arg1, int arg2, int arg3, int* ret) {
 (void)id;
 (void)arg1;
 (void)arg2;
 (void)arg3;
 (void)ret;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int sceSysmoduleUnloadModule(uint16_t id) {
 (void)id;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

}
