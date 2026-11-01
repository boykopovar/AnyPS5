#include "prx/libSceIcu/Identified.hpp"
#include <array>
#include <cstdint>

extern "C" std::int32_t APS5_VABI res_getPublicType_nid_postfix(std::uint32_t resource) {
    static constexpr std::array<std::int32_t, 16> publicTypes{0, 1, 2, 3, 2, 2, 0, 7, 8, 8, -1, -1, -1, -1, 14, -1};
    return publicTypes[resource >> 28];
}
