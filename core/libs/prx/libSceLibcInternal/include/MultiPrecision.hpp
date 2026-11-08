#pragma once

#include "SceTypes.hpp"

extern "C" {

unsigned long long APS5_VABI _ZSt7_MP_GetPy(unsigned long long* value);
void APS5_VABI _ZSt7_MP_AddPyy(unsigned long long* value, unsigned long long addend);
void APS5_VABI _ZSt7_MP_MulPyyy(unsigned long long* product, unsigned long long left, unsigned long long right);
void APS5_VABI _ZSt7_MP_RemPyy(unsigned long long* value, unsigned long long divisor);

}
