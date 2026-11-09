#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libc/include/general/ExportMacros.hpp"
#ifdef _WIN32
#define MODULE_EXPORT __declspec(dllexport)
#else
#define MODULE_EXPORT __attribute__((visibility("default")))
#endif
extern "C" MODULE_EXPORT int APS5_VABI GuestModuleAdd_nid_postfix(int a, int b) {
    return a + b;
}
#ifndef _WIN32
extern "C" MODULE_EXPORT int APS5_VABI GuestModuleMul_nid_no_patch(int a, int b) {
    return a * b;
}
APS5_EXPORT("GuestModuleMul#guest", GuestModuleMul_nid_no_patch);
extern "C" MODULE_EXPORT int APS5_VABI GuestModuleSub_nid_no_patch(int a, int b) {
    return a - b;
}
APS5_EXPORT("BOyBJaKwOa8#guest", GuestModuleSub_nid_no_patch);
#endif
