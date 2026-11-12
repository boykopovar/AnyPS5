#pragma once

#include "SceTypes.hpp"
#include <cstddef>
#include <cstdint>

extern "C" {

int APS5_VABI _ZN3sce16CommonDialogUtil6Server13setFinishDataEPvm(void* self, void* data, std::size_t size);
int APS5_VABI _ZN3sce16CommonDialogUtil6Server6setRunEPvm(void* self, void* data, std::size_t size);
int APS5_VABI _ZN3sce16CommonDialogUtil6Server9setFinishEv(void* self);
int APS5_VABI _ZN3sce16CommonDialogUtil6Server9setResultEi(void* self, int result);
int APS5_VABI _ZN3sce16CommonDialogUtil6ServerC2EPKcmmis(void);
int APS5_VABI _ZN3sce16CommonDialogUtil6ServerD0Ev(void);
int APS5_VABI _ZN3sce16CommonDialogUtil6ServerD1Ev(void);
int APS5_VABI _ZN3sce16CommonDialogUtil6ServerD2Ev(void);
int APS5_VABI _ZNK3sce16CommonDialogUtil6Server10getVersionERj(const void* self, std::uint32_t* version);
int APS5_VABI _ZNK3sce16CommonDialogUtil6Server10isCloseReqEv(const void* self);
int APS5_VABI _ZNK3sce16CommonDialogUtil6Server14getServerStateEv(const void* self);

}
