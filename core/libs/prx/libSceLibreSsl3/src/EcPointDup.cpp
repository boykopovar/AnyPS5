#include "prx/libSceLibreSsl3/include/Api.hpp"
#include "prx/libSceLibreSsl/include/Ec.hpp"

extern "C" EcPoint* APS5_VABI EC_POINT_dup_nid_postfix(const EcPoint* point, const EcGroup* group) {
    return EcPointDuplicate_nid_no_patch(point, group);
}
