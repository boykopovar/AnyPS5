#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"

extern "C" {

int sceSysmoduleGetModuleInfoForUnwind(uint64_t addr, int flags, ModuleInfoForUnwind* info) {
 (void)addr;
 (void)flags;
 (void)info;
 return 0;
}

int sceSysmoduleIsLoaded(uint16_t id) {
 (void)id;
 return 0;
}

int sceSysmoduleLoadModule(uint16_t id) {
 (void)id;
 return 0;
}

int sceSysmoduleLoadModuleInternalWithArg(uint16_t id, int arg1, int arg2, int arg3, int* ret) {
 (void)id;
 (void)arg1;
 (void)arg2;
 (void)arg3;
 (void)ret;
 return 0;
}

int sceSysmoduleUnloadModule(uint16_t id) {
 (void)id;
 return 0;
}

}
