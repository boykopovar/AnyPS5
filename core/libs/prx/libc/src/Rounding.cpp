#include "prx/libc/include/general/VabiMacros.hpp"

#include <cmath>
#include <cstdint>
#include <limits>

static_assert(sizeof(long double) == 16 && std::numeric_limits<long double>::digits == 64);
static_assert(sizeof(long long) == sizeof(std::int64_t));

extern "C" {

double APS5_VABI rint_nid_postfix(double value) { return std::rint(value); }
float APS5_VABI rintf_nid_postfix(float value) { return std::rint(value); }
long double APS5_VABI rintl_nid_postfix(long double value) { return std::rint(value); }

std::int64_t APS5_VABI lrint_nid_postfix(double value) { return std::llrint(value); }
std::int64_t APS5_VABI lrintf_nid_postfix(float value) { return std::llrint(value); }
std::int64_t APS5_VABI lrintl_nid_postfix(long double value) { return std::llrint(value); }

std::int64_t APS5_VABI llrint_nid_postfix(double value) { return std::llrint(value); }
std::int64_t APS5_VABI llrintf_nid_postfix(float value) { return std::llrint(value); }
std::int64_t APS5_VABI llrintl_nid_postfix(long double value) { return std::llrint(value); }

}
