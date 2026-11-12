#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include "prx/libSceLibreSsl/include/Ec.hpp"

#include <stdexcept>
#include <string>

extern "C" {

int APS5_VABI EC_GROUP_cmp_nid_postfix(const EcGroup* first, const EcGroup* second, BignumContext* context) {
    (void)context;
    if (!first || !second) throw std::invalid_argument(std::string(__func__) + ": null group");
    return first->curve.nid == second->curve.nid ? 0 : 1;
}

}
