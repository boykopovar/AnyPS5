#include "prx/libSceNKWebKit/include/WtfDataFileExports.hpp"

extern "C" {

Jsc::WtfAbi::PrintStream& APS5_VABI _ZN3WTF8dataFileEv() {
    return *WtfDataFile_nid_no_patch();
}

void APS5_VABI _ZN3WTF15FilePrintStreamD0Ev(Jsc::WtfAbi::FilePrintStream* self) {
    WtfFilePrintStreamDelete_nid_no_patch(self);
}

void APS5_VABI _ZN3WTF15FilePrintStreamD1Ev(Jsc::WtfAbi::FilePrintStream* self) {
    WtfFilePrintStreamDestroy_nid_no_patch(self);
}

void APS5_VABI _ZN3WTF15FilePrintStreamD2Ev(Jsc::WtfAbi::FilePrintStream* self) {
    WtfFilePrintStreamDestroy_nid_no_patch(self);
}

}
