#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"

extern "C" {

int sceCommonDialogInitialize(void) {
 return 0;
}

bool sceCommonDialogIsUsed(void) {
 return false;
}

}
