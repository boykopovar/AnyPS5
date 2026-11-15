#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"

#include <stdexcept>

namespace {

int CurveName(const EcGroup* group) {
    if (!group) throw std::invalid_argument("EC_GROUP_cmp requires two groups");
    return *reinterpret_cast<const int*>(group);
}

} // namespace

extern "C" {

int APS5_VABI EC_GROUP_cmp_nid_postfix(const EcGroup* a, const EcGroup* b, void*) {
    return CurveName(a) != CurveName(b) ? 1 : 0;
}

}
