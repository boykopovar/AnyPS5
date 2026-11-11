#pragma once

#include "SceTypes.hpp"

extern "C" {

struct SisrSysSetupParams {
    void* field0;
    void* field1;
    void* field2;
    void* field3;
};

struct SisrSysUmdEvent {
    uint32_t magic;
    uint32_t type;
};

int APS5_VABI sceSisrSysProcessUmdEvent(const SisrSysUmdEvent* event);
int APS5_VABI sceSisrSysSetup(const SisrSysSetupParams* params);

}
