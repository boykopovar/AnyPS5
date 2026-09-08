#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

int sceAgcSetPacketPredication(uint32_t* packet, uint32_t predication){
 (void)packet;
 (void)predication;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int sceAgcSetRangePredication(uint32_t* start, const volatile uint32_t* end, uint32_t predication){
 (void)start;
 (void)end;
 (void)predication;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}
}
