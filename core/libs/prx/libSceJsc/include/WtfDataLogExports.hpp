#pragma once

#include "SceTypes.hpp"
#include "prx/libc/include/FileStream.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

#include <cstddef>

namespace Jsc::WtfAbi {

struct PrintStream {
    const void* const* vtable;
};

struct FilePrintStream {
    const void* const* vtable;
    FileStream* file;
    int adoptionMode;
};

enum FilePrintStreamAdoptionMode : int {
    FilePrintStreamAdopt = 0,
    FilePrintStreamBorrow = 1,
};

enum PrintStreamSlot : std::size_t {
    PrintStreamCompleteDestructor = 0,
    PrintStreamDeletingDestructor = 1,
    PrintStreamVprintf = 2,
    PrintStreamFlush = 3,
    PrintStreamBegin = 4,
    PrintStreamEnd = 5,
};

struct PrintStreamPtr {
    PrintStream* pointer;
};

struct FilePrintStreamPtr {
    FilePrintStream* pointer;
};

static_assert(sizeof(FilePrintStream) == 24);
static_assert(offsetof(FilePrintStream, vtable) == 0);
static_assert(offsetof(FilePrintStream, file) == 8);
static_assert(offsetof(FilePrintStream, adoptionMode) == 16);
static_assert(sizeof(PrintStreamPtr) == 8);
static_assert(sizeof(FilePrintStreamPtr) == 8);

} // namespace Jsc::WtfAbi

extern "C" {

void APS5_VABI _ZN3WTF11setDataFileEOSt10unique_ptrINS_11PrintStreamESt14default_deleteIS1_EE(Jsc::WtfAbi::PrintStreamPtr* file);
Jsc::WtfAbi::FilePrintStreamPtr* APS5_VABI _ZN3WTF15FilePrintStream4openEPKcS2_(Jsc::WtfAbi::FilePrintStreamPtr* result, const char* filename, const char* mode);
Jsc::WtfAbi::PrintStream* WtfDataFile_nid_no_patch();
void WtfFilePrintStreamDestroy_nid_no_patch(Jsc::WtfAbi::FilePrintStream* stream);
void WtfFilePrintStreamDelete_nid_no_patch(Jsc::WtfAbi::FilePrintStream* stream);

}
