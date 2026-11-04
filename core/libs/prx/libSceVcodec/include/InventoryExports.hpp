#pragma once

#include "SceTypes.hpp"

extern "C" {

int APS5_VABI sceVcodecCloseHwBudget();
int APS5_VABI sceVcodecOpenHwBudgetByAppid();
int APS5_VABI sceVcodecSetHwBudgetSize();
int APS5_VABI sceVcodecOpenHwBudget();
int APS5_VABI sceVcodecSetHwBudgetType();

}
