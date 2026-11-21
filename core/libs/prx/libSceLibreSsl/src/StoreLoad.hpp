#pragma once

#include "prx/libSceLibreSsl/include/Api.hpp"
#include <cstdint>

namespace LibreSsl {

int ByFileCtrl_nid_no_patch(LibreSslX509Lookup* lookup, int command, const char* argument, std::int64_t type, char** result);
int StoreLoadLocations_nid_no_patch(LibreSslX509Store* store, const char* file, const char* path);

}
