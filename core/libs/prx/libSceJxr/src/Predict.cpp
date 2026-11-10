#include "prx/libSceJxr/libSceJxr.hpp"
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>

extern "C" {
const char* JxrSetUniformQuantizer_nid_no_patch(void* codec, std::size_t band);
const char* JxrUseDcQuantizer_nid_no_patch(void* codec, std::size_t tile);
const char* JxrUseLpQuantizer_nid_no_patch(void* codec, std::size_t count, std::size_t tile);
const char* JxrGetDcAcPredMode_nid_no_patch(void* codec, std::size_t column, std::int32_t* mode);
const char* JxrUpdatePredInfo_nid_no_patch(void* codec, void* macroblock, std::size_t column, std::int32_t format);
}

namespace {

void Check(const char* error, const char* function) {
    if (error) throw std::invalid_argument(std::string(function) + ": " + error);
}

} // namespace

extern "C" {

void APS5_VABI setUniformQuantizer_nid_postfix(void* codec, std::size_t band) {
    Check(JxrSetUniformQuantizer_nid_no_patch(codec, band), __func__);
}

void APS5_VABI useDCQuantizer_nid_postfix(void* codec, std::size_t tile) {
    Check(JxrUseDcQuantizer_nid_no_patch(codec, tile), __func__);
}

void APS5_VABI useLPQuantizer_nid_postfix(void* codec, std::size_t count, std::size_t tile) {
    Check(JxrUseLpQuantizer_nid_no_patch(codec, count, tile), __func__);
}

std::int32_t APS5_VABI getDCACPredMode_nid_postfix(void* codec, std::size_t column) {
    std::int32_t mode = 0;
    Check(JxrGetDcAcPredMode_nid_no_patch(codec, column, &mode), __func__);
    return mode;
}

void APS5_VABI updatePredInfo_nid_postfix(void* codec, void* macroblock, std::size_t column, std::int32_t format) {
    Check(JxrUpdatePredInfo_nid_no_patch(codec, macroblock, column, format), __func__);
}

} // extern "C"
