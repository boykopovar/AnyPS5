#include "prx/libSceBemp2sys/libSceBemp2sys.hpp"
#include <cstddef>
#include <cstdlib>

static void Require(bool value) { if (!value) std::abort(); }

namespace {

void LogFunction(int, const char*) {}
void* Allocate(std::size_t) { return nullptr; }
void Deallocate(void*) {}
void* Reallocate(void*, std::size_t) { return nullptr; }

}

int main() {
    Require(bemp2sys_logprintf_getlevel_nid_postfix() == 0);
    bemp2sys_logprintf_setlevel_nid_postfix(3);
    Require(bemp2sys_logprintf_getlevel_nid_postfix() == 3);
    bemp2sys_logprintf_setlevel_nid_postfix(4);
    Require(bemp2sys_logprintf_getlevel_nid_postfix() == 3);
    bemp2sys_logprintf_setlevel_nid_postfix(-1);
    Require(bemp2sys_logprintf_getlevel_nid_postfix() == 3);
    bemp2sys_logprintf_setfunc_nid_postfix(reinterpret_cast<void*>(&LogFunction));
    bemp2sys_memmanager_set_nid_postfix(reinterpret_cast<void*>(&Allocate), reinterpret_cast<void*>(&Deallocate), reinterpret_cast<void*>(&Reallocate));
    return 0;
}
