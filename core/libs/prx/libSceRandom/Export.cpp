#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"

extern "C" {

int sceRandomGetRandomNumber(void* buf, size_t size) {
 (void)buf;
 (void)size;
 return 0;
}

}
