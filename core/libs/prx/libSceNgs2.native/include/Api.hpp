#pragma once

#include "SceTypes.hpp"

struct Ngs2JobSchedulerOption;

extern "C" {

int APS5_VABI sceNgs2JobSchedulerResetOption(Ngs2JobSchedulerOption*);
int APS5_VABI sceNgs2ModuleArrayEnumItems(void);
int APS5_VABI sceNgs2ModuleEnumConfigs(void);
int APS5_VABI sceNgs2ModuleQueueEnumItems(void);
int APS5_VABI sceNgs2RackQueryInfo(void);
int APS5_VABI sceNgs2RackRunCommands(void);
int APS5_VABI sceNgs2StreamCreate(void);
int APS5_VABI sceNgs2StreamCreateWithAllocator(void);
int APS5_VABI sceNgs2StreamDestroy(void);
int APS5_VABI sceNgs2StreamQueryBufferSize(void);
int APS5_VABI sceNgs2StreamQueryInfo(void);
int APS5_VABI sceNgs2StreamResetOption(void);
int APS5_VABI sceNgs2StreamRunCommands(void);
int APS5_VABI sceNgs2SystemQueryInfo(void);
int APS5_VABI sceNgs2SystemRunCommands(void);

}
