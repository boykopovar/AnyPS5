#pragma once

#include "prx/libSceLibreSsl/include/Api.hpp"
#include "prx/libSceLibreSsl/include/Bio.hpp"

namespace LibreSsl {

LibreSslX509* ReadPemRecord(LibreSsl::Bio* bio, LibreSslX509** certificate,
                            LibreSslPemPasswordCallback callback, void* userData,
                            const char* function);

}
