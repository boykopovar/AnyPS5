#pragma once

#include "SceTypes.hpp"

extern "C" {

void APS5_VABI __tsan_acquire(void*);
void APS5_VABI __tsan_release(void*);

}
