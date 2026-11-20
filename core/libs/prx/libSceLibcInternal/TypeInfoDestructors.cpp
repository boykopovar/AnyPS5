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

void APS5_VABI _ZN10__cxxabiv123__fundamental_type_infoD1Ev(void* self) {
    RunTypeInfoDestructor(self, false);
}

void APS5_VABI _ZN10__cxxabiv123__fundamental_type_infoD0Ev(void* self) {
    RunTypeInfoDestructor(self, true);
}

void APS5_VABI _ZN10__cxxabiv117__array_type_infoD2Ev(void* self) {
    RunTypeInfoDestructor(self, false);
}

void APS5_VABI _ZN10__cxxabiv117__array_type_infoD1Ev(void* self) {
    RunTypeInfoDestructor(self, false);
}

void APS5_VABI _ZN10__cxxabiv117__array_type_infoD0Ev(void* self) {
    RunTypeInfoDestructor(self, true);
}

void APS5_VABI _ZN10__cxxabiv120__function_type_infoD2Ev(void* self) {
    RunTypeInfoDestructor(self, false);
}

void APS5_VABI _ZN10__cxxabiv120__function_type_infoD1Ev(void* self) {
    RunTypeInfoDestructor(self, false);
}

void APS5_VABI _ZN10__cxxabiv120__function_type_infoD0Ev(void* self) {
    RunTypeInfoDestructor(self, true);
}

void APS5_VABI _ZN10__cxxabiv116__enum_type_infoD2Ev(void* self) {
    RunTypeInfoDestructor(self, false);
}

void APS5_VABI _ZN10__cxxabiv116__enum_type_infoD1Ev(void* self) {
    RunTypeInfoDestructor(self, false);
}

void APS5_VABI _ZN10__cxxabiv116__enum_type_infoD0Ev(void* self) {
    RunTypeInfoDestructor(self, true);
}

void APS5_VABI _ZN10__cxxabiv117__class_type_infoD2Ev(void* self) {
    RunTypeInfoDestructor(self, false);
}

void APS5_VABI _ZN10__cxxabiv117__class_type_infoD1Ev(void* self) {
    RunTypeInfoDestructor(self, false);
}

void APS5_VABI _ZN10__cxxabiv117__class_type_infoD0Ev(void* self) {
    RunTypeInfoDestructor(self, true);
}

void APS5_VABI _ZN10__cxxabiv120__si_class_type_infoD2Ev(void* self) {
    RunTypeInfoDestructor(self, false);
}

void APS5_VABI _ZN10__cxxabiv120__si_class_type_infoD1Ev(void* self) {
    RunTypeInfoDestructor(self, false);
}

void APS5_VABI _ZN10__cxxabiv120__si_class_type_infoD0Ev(void* self) {
    RunTypeInfoDestructor(self, true);
}

void APS5_VABI _ZN10__cxxabiv121__vmi_class_type_infoD2Ev(void* self) {
    RunTypeInfoDestructor(self, false);
}

void APS5_VABI _ZN10__cxxabiv121__vmi_class_type_infoD1Ev(void* self) {
    RunTypeInfoDestructor(self, false);
}

void APS5_VABI _ZN10__cxxabiv121__vmi_class_type_infoD0Ev(void* self) {
    RunTypeInfoDestructor(self, true);
}

void APS5_VABI _ZN10__cxxabiv117__pbase_type_infoD2Ev(void* self) {
    RunTypeInfoDestructor(self, false);
}

void APS5_VABI _ZN10__cxxabiv117__pbase_type_infoD1Ev(void* self) {
    RunTypeInfoDestructor(self, false);
}

void APS5_VABI _ZN10__cxxabiv117__pbase_type_infoD0Ev(void* self) {
    RunTypeInfoDestructor(self, true);
}

void APS5_VABI _ZN10__cxxabiv119__pointer_type_infoD2Ev(void* self) {
    RunTypeInfoDestructor(self, false);
}

void APS5_VABI _ZN10__cxxabiv119__pointer_type_infoD1Ev(void* self) {
    RunTypeInfoDestructor(self, false);
}

void APS5_VABI _ZN10__cxxabiv119__pointer_type_infoD0Ev(void* self) {
    RunTypeInfoDestructor(self, true);
}

void APS5_VABI _ZN10__cxxabiv129__pointer_to_member_type_infoD2Ev(void* self) {
    RunTypeInfoDestructor(self, false);
}

void APS5_VABI _ZN10__cxxabiv129__pointer_to_member_type_infoD1Ev(void* self) {
    RunTypeInfoDestructor(self, false);
}

void APS5_VABI _ZN10__cxxabiv129__pointer_to_member_type_infoD0Ev(void* self) {
    RunTypeInfoDestructor(self, true);
}

}
