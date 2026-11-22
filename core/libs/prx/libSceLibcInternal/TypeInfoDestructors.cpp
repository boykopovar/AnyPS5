#include "prx/libSceLibcInternal/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include "SceTypes.hpp"

namespace {

void RunTypeInfoDestructor(void* self, bool deleting) {
    if (deleting) _ZdlPv_nid_postfix(self);
}

}

extern "C" {

void APS5_VABI _ZN10__cxxabiv123__fundamental_type_infoD2Ev(void* self) {
    RunTypeInfoDestructor(self, false);
}
APS5_EXPORT("0jk9oqKd2Gw", _ZN10__cxxabiv123__fundamental_type_infoD2Ev);

void APS5_VABI _ZN10__cxxabiv123__fundamental_type_infoD1Ev(void* self) {
    RunTypeInfoDestructor(self, false);
}
APS5_EXPORT("kzWL2iOsv0E", _ZN10__cxxabiv123__fundamental_type_infoD1Ev);

void APS5_VABI _ZN10__cxxabiv123__fundamental_type_infoD0Ev(void* self) {
    RunTypeInfoDestructor(self, true);
}
APS5_EXPORT("YsZuwZrJZlU", _ZN10__cxxabiv123__fundamental_type_infoD0Ev);

void APS5_VABI _ZN10__cxxabiv117__array_type_infoD2Ev(void* self) {
    RunTypeInfoDestructor(self, false);
}
APS5_EXPORT("ckFrsyD2830", _ZN10__cxxabiv117__array_type_infoD2Ev);

void APS5_VABI _ZN10__cxxabiv117__array_type_infoD1Ev(void* self) {
    RunTypeInfoDestructor(self, false);
}
APS5_EXPORT("1ZMchlkvNNQ", _ZN10__cxxabiv117__array_type_infoD1Ev);

void APS5_VABI _ZN10__cxxabiv117__array_type_infoD0Ev(void* self) {
    RunTypeInfoDestructor(self, true);
}
APS5_EXPORT("R5nRbLQT3oI", _ZN10__cxxabiv117__array_type_infoD0Ev);

void APS5_VABI _ZN10__cxxabiv120__function_type_infoD2Ev(void* self) {
    RunTypeInfoDestructor(self, false);
}
APS5_EXPORT("NtqD4Q0vUUI", _ZN10__cxxabiv120__function_type_infoD2Ev);

void APS5_VABI _ZN10__cxxabiv120__function_type_infoD1Ev(void* self) {
    RunTypeInfoDestructor(self, false);
}
APS5_EXPORT("dsQ5Xwhl9no", _ZN10__cxxabiv120__function_type_infoD1Ev);

void APS5_VABI _ZN10__cxxabiv120__function_type_infoD0Ev(void* self) {
    RunTypeInfoDestructor(self, true);
}
APS5_EXPORT("kVvGL9aF5gg", _ZN10__cxxabiv120__function_type_infoD0Ev);

void APS5_VABI _ZN10__cxxabiv116__enum_type_infoD2Ev(void* self) {
    RunTypeInfoDestructor(self, false);
}
APS5_EXPORT("iQiT26+ZGnA", _ZN10__cxxabiv116__enum_type_infoD2Ev);

void APS5_VABI _ZN10__cxxabiv116__enum_type_infoD1Ev(void* self) {
    RunTypeInfoDestructor(self, false);
}
APS5_EXPORT("upwSZWmYwqE", _ZN10__cxxabiv116__enum_type_infoD1Ev);

void APS5_VABI _ZN10__cxxabiv116__enum_type_infoD0Ev(void* self) {
    RunTypeInfoDestructor(self, true);
}
APS5_EXPORT("CjzjU2nFUWw", _ZN10__cxxabiv116__enum_type_infoD0Ev);

void APS5_VABI _ZN10__cxxabiv117__class_type_infoD2Ev(void* self) {
    RunTypeInfoDestructor(self, false);
}
APS5_EXPORT("xXM1q-ayw2Y", _ZN10__cxxabiv117__class_type_infoD2Ev);

void APS5_VABI _ZN10__cxxabiv117__class_type_infoD1Ev(void* self) {
    RunTypeInfoDestructor(self, false);
}
APS5_EXPORT("goLVqD-eiIY", _ZN10__cxxabiv117__class_type_infoD1Ev);

void APS5_VABI _ZN10__cxxabiv117__class_type_infoD0Ev(void* self) {
    RunTypeInfoDestructor(self, true);
}
APS5_EXPORT("XAk7W3NcpTY", _ZN10__cxxabiv117__class_type_infoD0Ev);

void APS5_VABI _ZN10__cxxabiv120__si_class_type_infoD2Ev(void* self) {
    RunTypeInfoDestructor(self, false);
}
APS5_EXPORT("bWHwovVFfqc", _ZN10__cxxabiv120__si_class_type_infoD2Ev);

void APS5_VABI _ZN10__cxxabiv120__si_class_type_infoD1Ev(void* self) {
    RunTypeInfoDestructor(self, false);
}
APS5_EXPORT("6aEkwkEOGRg", _ZN10__cxxabiv120__si_class_type_infoD1Ev);

void APS5_VABI _ZN10__cxxabiv120__si_class_type_infoD0Ev(void* self) {
    RunTypeInfoDestructor(self, true);
}
APS5_EXPORT("Fg4w+h9wAMA", _ZN10__cxxabiv120__si_class_type_infoD0Ev);

void APS5_VABI _ZN10__cxxabiv121__vmi_class_type_infoD2Ev(void* self) {
    RunTypeInfoDestructor(self, false);
}
APS5_EXPORT("yYIymfQGl2E", _ZN10__cxxabiv121__vmi_class_type_infoD2Ev);

void APS5_VABI _ZN10__cxxabiv121__vmi_class_type_infoD1Ev(void* self) {
    RunTypeInfoDestructor(self, false);
}
APS5_EXPORT("h-a7+0UuK7Q", _ZN10__cxxabiv121__vmi_class_type_infoD1Ev);

void APS5_VABI _ZN10__cxxabiv121__vmi_class_type_infoD0Ev(void* self) {
    RunTypeInfoDestructor(self, true);
}
APS5_EXPORT("W5k0jlyBpgM", _ZN10__cxxabiv121__vmi_class_type_infoD0Ev);

void APS5_VABI _ZN10__cxxabiv117__pbase_type_infoD2Ev(void* self) {
    RunTypeInfoDestructor(self, false);
}
APS5_EXPORT("krshE4JAE6M", _ZN10__cxxabiv117__pbase_type_infoD2Ev);

void APS5_VABI _ZN10__cxxabiv117__pbase_type_infoD1Ev(void* self) {
    RunTypeInfoDestructor(self, false);
}
APS5_EXPORT("vIJPARS8imE", _ZN10__cxxabiv117__pbase_type_infoD1Ev);

void APS5_VABI _ZN10__cxxabiv117__pbase_type_infoD0Ev(void* self) {
    RunTypeInfoDestructor(self, true);
}
APS5_EXPORT("GLxD5v2uMHw", _ZN10__cxxabiv117__pbase_type_infoD0Ev);

void APS5_VABI _ZN10__cxxabiv119__pointer_type_infoD2Ev(void* self) {
    RunTypeInfoDestructor(self, false);
}
APS5_EXPORT("vCLVhOcdQMY", _ZN10__cxxabiv119__pointer_type_infoD2Ev);

void APS5_VABI _ZN10__cxxabiv119__pointer_type_infoD1Ev(void* self) {
    RunTypeInfoDestructor(self, false);
}
APS5_EXPORT("bhfgrK+MZdk", _ZN10__cxxabiv119__pointer_type_infoD1Ev);

void APS5_VABI _ZN10__cxxabiv119__pointer_type_infoD0Ev(void* self) {
    RunTypeInfoDestructor(self, true);
}
APS5_EXPORT("64180GwMVro", _ZN10__cxxabiv119__pointer_type_infoD0Ev);

void APS5_VABI _ZN10__cxxabiv129__pointer_to_member_type_infoD2Ev(void* self) {
    RunTypeInfoDestructor(self, false);
}
APS5_EXPORT("Re3Lpk8mwEo", _ZN10__cxxabiv129__pointer_to_member_type_infoD2Ev);

void APS5_VABI _ZN10__cxxabiv129__pointer_to_member_type_infoD1Ev(void* self) {
    RunTypeInfoDestructor(self, false);
}
APS5_EXPORT("KaZ3xf5c9Vc", _ZN10__cxxabiv129__pointer_to_member_type_infoD1Ev);

void APS5_VABI _ZN10__cxxabiv129__pointer_to_member_type_infoD0Ev(void* self) {
    RunTypeInfoDestructor(self, true);
}
APS5_EXPORT("NdeDffcZ-30", _ZN10__cxxabiv129__pointer_to_member_type_infoD0Ev);

}
