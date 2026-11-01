#pragma once

#include <cstdint>

#ifndef ANYPS5_AIO_TESTING
#error AIO test controls require ANYPS5_AIO_TESTING
#endif

extern "C" {

void AioAdvanceIdentityForTest_nid_no_patch(std::int64_t next);
std::int32_t AioProcessingWaitersForTest_nid_no_patch();

}
