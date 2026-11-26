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

void APS5_VABI sceLibcInternalFundamentalTypeInfoBaseDestructor_nid_no_patch(void* self) {
    RunTypeInfoDestructor(self, false);
}
APS5_EXPORT("0jk9oqKd2Gw", sceLibcInternalFundamentalTypeInfoBaseDestructor_nid_no_patch);

void APS5_VABI sceLibcInternalFundamentalTypeInfoCompleteDestructor_nid_no_patch(void* self) {
    RunTypeInfoDestructor(self, false);
}
APS5_EXPORT("kzWL2iOsv0E", sceLibcInternalFundamentalTypeInfoCompleteDestructor_nid_no_patch);

void APS5_VABI sceLibcInternalFundamentalTypeInfoDeletingDestructor_nid_no_patch(void* self) {
    RunTypeInfoDestructor(self, true);
}
APS5_EXPORT("YsZuwZrJZlU", sceLibcInternalFundamentalTypeInfoDeletingDestructor_nid_no_patch);

void APS5_VABI sceLibcInternalArrayTypeInfoBaseDestructor_nid_no_patch(void* self) {
    RunTypeInfoDestructor(self, false);
}
APS5_EXPORT("ckFrsyD2830", sceLibcInternalArrayTypeInfoBaseDestructor_nid_no_patch);

void APS5_VABI sceLibcInternalArrayTypeInfoCompleteDestructor_nid_no_patch(void* self) {
    RunTypeInfoDestructor(self, false);
}
APS5_EXPORT("1ZMchlkvNNQ", sceLibcInternalArrayTypeInfoCompleteDestructor_nid_no_patch);

void APS5_VABI sceLibcInternalArrayTypeInfoDeletingDestructor_nid_no_patch(void* self) {
    RunTypeInfoDestructor(self, true);
}
APS5_EXPORT("R5nRbLQT3oI", sceLibcInternalArrayTypeInfoDeletingDestructor_nid_no_patch);

void APS5_VABI sceLibcInternalFunctionTypeInfoBaseDestructor_nid_no_patch(void* self) {
    RunTypeInfoDestructor(self, false);
}
APS5_EXPORT("NtqD4Q0vUUI", sceLibcInternalFunctionTypeInfoBaseDestructor_nid_no_patch);

void APS5_VABI sceLibcInternalFunctionTypeInfoCompleteDestructor_nid_no_patch(void* self) {
    RunTypeInfoDestructor(self, false);
}
APS5_EXPORT("dsQ5Xwhl9no", sceLibcInternalFunctionTypeInfoCompleteDestructor_nid_no_patch);

void APS5_VABI sceLibcInternalFunctionTypeInfoDeletingDestructor_nid_no_patch(void* self) {
    RunTypeInfoDestructor(self, true);
}
APS5_EXPORT("kVvGL9aF5gg", sceLibcInternalFunctionTypeInfoDeletingDestructor_nid_no_patch);

void APS5_VABI sceLibcInternalEnumTypeInfoBaseDestructor_nid_no_patch(void* self) {
    RunTypeInfoDestructor(self, false);
}
APS5_EXPORT("iQiT26+ZGnA", sceLibcInternalEnumTypeInfoBaseDestructor_nid_no_patch);

void APS5_VABI sceLibcInternalEnumTypeInfoCompleteDestructor_nid_no_patch(void* self) {
    RunTypeInfoDestructor(self, false);
}
APS5_EXPORT("upwSZWmYwqE", sceLibcInternalEnumTypeInfoCompleteDestructor_nid_no_patch);

void APS5_VABI sceLibcInternalEnumTypeInfoDeletingDestructor_nid_no_patch(void* self) {
    RunTypeInfoDestructor(self, true);
}
APS5_EXPORT("CjzjU2nFUWw", sceLibcInternalEnumTypeInfoDeletingDestructor_nid_no_patch);

void APS5_VABI sceLibcInternalClassTypeInfoBaseDestructor_nid_no_patch(void* self) {
    RunTypeInfoDestructor(self, false);
}
APS5_EXPORT("xXM1q-ayw2Y", sceLibcInternalClassTypeInfoBaseDestructor_nid_no_patch);

void APS5_VABI sceLibcInternalClassTypeInfoCompleteDestructor_nid_no_patch(void* self) {
    RunTypeInfoDestructor(self, false);
}
APS5_EXPORT("goLVqD-eiIY", sceLibcInternalClassTypeInfoCompleteDestructor_nid_no_patch);

void APS5_VABI sceLibcInternalClassTypeInfoDeletingDestructor_nid_no_patch(void* self) {
    RunTypeInfoDestructor(self, true);
}
APS5_EXPORT("XAk7W3NcpTY", sceLibcInternalClassTypeInfoDeletingDestructor_nid_no_patch);

void APS5_VABI sceLibcInternalSiClassTypeInfoBaseDestructor_nid_no_patch(void* self) {
    RunTypeInfoDestructor(self, false);
}
APS5_EXPORT("bWHwovVFfqc", sceLibcInternalSiClassTypeInfoBaseDestructor_nid_no_patch);

void APS5_VABI sceLibcInternalSiClassTypeInfoCompleteDestructor_nid_no_patch(void* self) {
    RunTypeInfoDestructor(self, false);
}
APS5_EXPORT("6aEkwkEOGRg", sceLibcInternalSiClassTypeInfoCompleteDestructor_nid_no_patch);

void APS5_VABI sceLibcInternalSiClassTypeInfoDeletingDestructor_nid_no_patch(void* self) {
    RunTypeInfoDestructor(self, true);
}
APS5_EXPORT("Fg4w+h9wAMA", sceLibcInternalSiClassTypeInfoDeletingDestructor_nid_no_patch);

void APS5_VABI sceLibcInternalVmiClassTypeInfoBaseDestructor_nid_no_patch(void* self) {
    RunTypeInfoDestructor(self, false);
}
APS5_EXPORT("yYIymfQGl2E", sceLibcInternalVmiClassTypeInfoBaseDestructor_nid_no_patch);

void APS5_VABI sceLibcInternalVmiClassTypeInfoCompleteDestructor_nid_no_patch(void* self) {
    RunTypeInfoDestructor(self, false);
}
APS5_EXPORT("h-a7+0UuK7Q", sceLibcInternalVmiClassTypeInfoCompleteDestructor_nid_no_patch);

void APS5_VABI sceLibcInternalVmiClassTypeInfoDeletingDestructor_nid_no_patch(void* self) {
    RunTypeInfoDestructor(self, true);
}
APS5_EXPORT("W5k0jlyBpgM", sceLibcInternalVmiClassTypeInfoDeletingDestructor_nid_no_patch);

void APS5_VABI sceLibcInternalPbaseTypeInfoBaseDestructor_nid_no_patch(void* self) {
    RunTypeInfoDestructor(self, false);
}
APS5_EXPORT("krshE4JAE6M", sceLibcInternalPbaseTypeInfoBaseDestructor_nid_no_patch);

void APS5_VABI sceLibcInternalPbaseTypeInfoCompleteDestructor_nid_no_patch(void* self) {
    RunTypeInfoDestructor(self, false);
}
APS5_EXPORT("vIJPARS8imE", sceLibcInternalPbaseTypeInfoCompleteDestructor_nid_no_patch);

void APS5_VABI sceLibcInternalPbaseTypeInfoDeletingDestructor_nid_no_patch(void* self) {
    RunTypeInfoDestructor(self, true);
}
APS5_EXPORT("GLxD5v2uMHw", sceLibcInternalPbaseTypeInfoDeletingDestructor_nid_no_patch);

void APS5_VABI sceLibcInternalPointerTypeInfoBaseDestructor_nid_no_patch(void* self) {
    RunTypeInfoDestructor(self, false);
}
APS5_EXPORT("vCLVhOcdQMY", sceLibcInternalPointerTypeInfoBaseDestructor_nid_no_patch);

void APS5_VABI sceLibcInternalPointerTypeInfoCompleteDestructor_nid_no_patch(void* self) {
    RunTypeInfoDestructor(self, false);
}
APS5_EXPORT("bhfgrK+MZdk", sceLibcInternalPointerTypeInfoCompleteDestructor_nid_no_patch);

void APS5_VABI sceLibcInternalPointerTypeInfoDeletingDestructor_nid_no_patch(void* self) {
    RunTypeInfoDestructor(self, true);
}
APS5_EXPORT("64180GwMVro", sceLibcInternalPointerTypeInfoDeletingDestructor_nid_no_patch);

void APS5_VABI sceLibcInternalPointerToMemberTypeInfoBaseDestructor_nid_no_patch(void* self) {
    RunTypeInfoDestructor(self, false);
}
APS5_EXPORT("Re3Lpk8mwEo", sceLibcInternalPointerToMemberTypeInfoBaseDestructor_nid_no_patch);

void APS5_VABI sceLibcInternalPointerToMemberTypeInfoCompleteDestructor_nid_no_patch(void* self) {
    RunTypeInfoDestructor(self, false);
}
APS5_EXPORT("KaZ3xf5c9Vc", sceLibcInternalPointerToMemberTypeInfoCompleteDestructor_nid_no_patch);

void APS5_VABI sceLibcInternalPointerToMemberTypeInfoDeletingDestructor_nid_no_patch(void* self) {
    RunTypeInfoDestructor(self, true);
}
APS5_EXPORT("NdeDffcZ-30", sceLibcInternalPointerToMemberTypeInfoDeletingDestructor_nid_no_patch);

}
