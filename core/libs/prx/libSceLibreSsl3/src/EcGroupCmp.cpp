#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include "prx/libSceLibreSsl/include/Ec.hpp"
#include "prx/libc/include/General.hpp"

#include <stdexcept>

extern "C" int APS5_VABI EC_GROUP_cmp_nid_postfix(const EcGroup* first, const EcGroup* second, BignumContext*) {
    if (!first || !second) throw std::invalid_argument("EC_GROUP_cmp requires two groups");
    if (first == second) return 0;
    const auto& a = first->group;
    const auto& b = second->group;
    if (a.id != b.id || a.pbits != b.pbits || a.nbits != b.nbits) return 1;
    if (mbedtls_mpi_cmp_mpi(&a.P, &b.P) != 0) return 1;
    if (mbedtls_mpi_cmp_mpi(&a.A, &b.A) != 0) return 1;
    if (mbedtls_mpi_cmp_mpi(&a.B, &b.B) != 0) return 1;
    if (mbedtls_mpi_cmp_mpi(&a.N, &b.N) != 0) return 1;
    return mbedtls_ecp_point_cmp(&a.G, &b.G) == 0 ? 0 : 1;
}
