#include "prx/libc/include/Api.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>

namespace {

void Require(bool condition, const char* label) {
    if (condition) return;
    std::fprintf(stderr, "sce v2 pmsw: %s\n", label);
    std::abort();
}

}

int main() {
    double value = 1.0;
    std::uint16_t* word = _ZNSt8__sce_v25_PmswEPd_nid_postfix(&value);
    Require(reinterpret_cast<unsigned char*>(word) == reinterpret_cast<unsigned char*>(&value) + 6, "double offset");
    Require(*word == 0x3ff0, "double word");
    *word = 0xfff0;
    Require(value == -std::numeric_limits<double>::infinity(), "double write");

    double posVal = 2.0;
    word = _ZNSt8__sce_v25_PmswEPd_nid_postfix(&posVal);
    Require(*word == 0x4000, "double 2.0 word");

    double negVal = -2.0;
    word = _ZNSt8__sce_v25_PmswEPd_nid_postfix(&negVal);
    Require(*word == 0xc000, "double -2.0 word");

    float single = 1.0f;
    word = _ZNSt8__sce_v26_FPmswEPf_nid_postfix(&single);
    Require(reinterpret_cast<unsigned char*>(word) == reinterpret_cast<unsigned char*>(&single) + 2, "float offset");
    Require(*word == 0x3f80, "float word");
    *word = 0xc000;
    Require(single == -2.0f, "float write");

    float singlePos = 2.0f;
    word = _ZNSt8__sce_v26_FPmswEPf_nid_postfix(&singlePos);
    Require(*word == 0x4000, "float 2.0f word");

    float singleNeg = -2.0f;
    word = _ZNSt8__sce_v26_FPmswEPf_nid_postfix(&singleNeg);
    Require(*word == 0xc000, "float -2.0f word");

    double extended = -2.0;
    word = _ZNSt8__sce_v26_LPmswEPe_nid_postfix(&extended);
    Require(reinterpret_cast<unsigned char*>(word) == reinterpret_cast<unsigned char*>(&extended) + 6, "long double offset");
    Require(*word == 0xc000, "long double word");

    double extPos = 4.0;
    word = _ZNSt8__sce_v26_LPmswEPe_nid_postfix(&extPos);
    Require(*word == 0x4010, "extended 4.0 word");

    return 0;
}
