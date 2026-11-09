#ifndef CORE_LIBS_PRX_LIBSCENPTROPHY2_INCLUDE_TROPHYNOTICE_HPP
#define CORE_LIBS_PRX_LIBSCENPTROPHY2_INCLUDE_TROPHYNOTICE_HPP

#include "prx/libSceNpTrophy2/include/TrophyPackage.hpp"

#include <cstdint>
#include <string>

namespace NpTrophy2 {

std::string UnlockNoticeText(const TrophyPackage* package, std::int32_t trophyId);

}

#endif
