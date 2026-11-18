#include "prx/libSceLibcInternal/include/FloatWords.hpp"
#include <bit>
#include <limits>
#include <stdexcept>

static_assert(std::endian::native == std::endian::little);
static_assert(sizeof(float) == 4 && std::numeric_limits<float>::is_iec559);
static_assert(sizeof(double) == 8 && std::numeric_limits<double>::is_iec559);
static_assert(sizeof(long double) == 16 && std::numeric_limits<long double>::digits == 64);

extern "C" {

std::uint16_t* APS5_VABI _Plsw_nid_postfix(double* value) {
    if (!value) throw std::runtime_error("_Plsw: null value");
    return reinterpret_cast<std::uint16_t*>(value);
}

std::uint16_t* APS5_VABI _Pmsw_nid_postfix(double* value) {
    if (!value) throw std::runtime_error("_Pmsw: null value");
    return reinterpret_cast<std::uint16_t*>(reinterpret_cast<unsigned char*>(value) + 6);
}

std::uint16_t* APS5_VABI _FPlsw_nid_postfix(float* value) {
    if (!value) throw std::runtime_error("_FPlsw: null value");
    return reinterpret_cast<std::uint16_t*>(value);
}

std::uint16_t* APS5_VABI _FPmsw_nid_postfix(float* value) {
    if (!value) throw std::runtime_error("_FPmsw: null value");
    return reinterpret_cast<std::uint16_t*>(reinterpret_cast<unsigned char*>(value) + 2);
}

std::uint16_t* APS5_VABI _LPlsw_nid_postfix(long double* value) {
    if (!value) throw std::runtime_error("_LPlsw: null value");
    return reinterpret_cast<std::uint16_t*>(value);
}

std::uint16_t* APS5_VABI _LPmsw_nid_postfix(long double* value) {
    if (!value) throw std::runtime_error("_LPmsw: null value");
    return reinterpret_cast<std::uint16_t*>(reinterpret_cast<unsigned char*>(value) + 8);
}

}
