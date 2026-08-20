#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"

extern "C" {

int sceContentSearchInit(const ContentSearchInitParam* init_param) {
 (void)init_param;
 return 0;
}

}
