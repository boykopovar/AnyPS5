#include "SceTypes.hpp"
#include "prx/libSceSaveData/include/Api.hpp"
#include <cstdlib>

namespace {

void Require(bool value) { if (!value) std::abort(); }

}

int main() {
    for (int call = 0; call < 3; ++call) {
        Require(sceSaveDataGetSavePoint() == 2);
        Require(sceSaveDataGetClientThreadPriority() == 0);
        Require(sceSaveDataDebugGetThreadId() == 0);
    }
    return 0;
}
