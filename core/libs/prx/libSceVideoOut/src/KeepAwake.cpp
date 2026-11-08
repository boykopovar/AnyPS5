#include "prx/libSceVideoOut/include/KeepAwake.hpp"
#include <cstring>

bool KeepAwakeEnabledFromValue(const char* value) {
    return value == nullptr || std::strcmp(value, "0") != 0;
}
