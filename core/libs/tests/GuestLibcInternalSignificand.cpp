#include "prx/libSceLibcInternal/include/MathInternals.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>

namespace {

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "libc internal significand: %s\n", message);
    std::abort();
}

}

int main() {
    Require(sceLibcInternalSignificand_nid_no_patch(12.0) == 1.5, "double normal value");
    Require(sceLibcInternalSignificand_nid_no_patch(-0.375) == -1.5, "double negative normal value");
    Require(sceLibcInternalSignificand_nid_no_patch(0x1p-1074) == 1.0, "double least subnormal");
    Require(sceLibcInternalSignificand_nid_no_patch(-0x1p-1074) == -1.0, "double negative least subnormal");
    Require(sceLibcInternalSignificand_nid_no_patch(0.0) == 0.0 && !std::signbit(sceLibcInternalSignificand_nid_no_patch(0.0)), "double positive zero");
    Require(sceLibcInternalSignificand_nid_no_patch(-0.0) == 0.0 && std::signbit(sceLibcInternalSignificand_nid_no_patch(-0.0)), "double negative zero");
    Require(sceLibcInternalSignificand_nid_no_patch(std::numeric_limits<double>::infinity()) == std::numeric_limits<double>::infinity(), "double positive infinity");
    Require(sceLibcInternalSignificand_nid_no_patch(-std::numeric_limits<double>::infinity()) == -std::numeric_limits<double>::infinity(), "double negative infinity");
    Require(std::isnan(sceLibcInternalSignificand_nid_no_patch(std::numeric_limits<double>::quiet_NaN())), "double NaN");

    Require(sceLibcInternalSignificandF_nid_no_patch(12.0f) == 1.5f, "float normal value");
    Require(sceLibcInternalSignificandF_nid_no_patch(-0.375f) == -1.5f, "float negative normal value");
    Require(sceLibcInternalSignificandF_nid_no_patch(0x1p-149f) == 1.0f, "float least subnormal");
    Require(sceLibcInternalSignificandF_nid_no_patch(-0x1p-149f) == -1.0f, "float negative least subnormal");
    Require(sceLibcInternalSignificandF_nid_no_patch(0.0f) == 0.0f && !std::signbit(sceLibcInternalSignificandF_nid_no_patch(0.0f)), "float positive zero");
    Require(sceLibcInternalSignificandF_nid_no_patch(-0.0f) == 0.0f && std::signbit(sceLibcInternalSignificandF_nid_no_patch(-0.0f)), "float negative zero");
    Require(sceLibcInternalSignificandF_nid_no_patch(std::numeric_limits<float>::infinity()) == std::numeric_limits<float>::infinity(), "float positive infinity");
    Require(sceLibcInternalSignificandF_nid_no_patch(-std::numeric_limits<float>::infinity()) == -std::numeric_limits<float>::infinity(), "float negative infinity");
    Require(std::isnan(sceLibcInternalSignificandF_nid_no_patch(std::numeric_limits<float>::quiet_NaN())), "float NaN");

    std::puts("libc internal significand checks passed");
    return 0;
}
