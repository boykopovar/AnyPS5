#include "prx/libSceLibreSsl3/include/Api.hpp"
#include "prx/libSceLibreSsl/include/Ec.hpp"
#include <mbedtls/ecp.h>
#include <stdexcept>

extern "C" EcPoint* APS5_VABI EC_POINT_dup_nid_postfix(const EcPoint* point, const EcGroup* group) {
    if (!point) return nullptr;
    if (!group) throw std::invalid_argument("EC_POINT_dup requires a group");
    if (point->curve.nid != group->curve.nid) return nullptr;
    auto* copy = new EcPoint(group->curve);
    if (mbedtls_ecp_copy(&copy->point, &point->point) != 0) {
        delete copy;
        return nullptr;
    }
    return copy;
}
