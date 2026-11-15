#pragma once

#include "SceTypes.hpp"

extern "C" {

unsigned long long APS5_VABI sceLibcInternalMultiPrecisionGet_nid_no_patch(unsigned long long* value);
void APS5_VABI sceLibcInternalMultiPrecisionAdd_nid_no_patch(unsigned long long* value, unsigned long long addend);
void APS5_VABI sceLibcInternalMultiPrecisionMultiply_nid_no_patch(unsigned long long* product, unsigned long long left, unsigned long long right);
void APS5_VABI sceLibcInternalMultiPrecisionRemainder_nid_no_patch(unsigned long long* value, unsigned long long divisor);

}
