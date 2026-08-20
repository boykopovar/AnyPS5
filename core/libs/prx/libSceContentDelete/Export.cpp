#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"

extern "C" {

int sceContentDeleteInitialize(const ContentDeleteInitParam* init_param) {
 (void)init_param;
 return 0;
}

}
