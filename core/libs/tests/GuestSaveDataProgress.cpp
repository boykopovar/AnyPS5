#include "prx/libSceSaveData/include/Api.hpp"
#include "prx/libSceSaveData.native/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libc/include/General.hpp"
#include "SceTypes.hpp"
#include "Testing.hpp"
#include <cstdio>
#include <cstdlib>

namespace {

constexpr int SaveDataErrorParameter = -2137063424;
constexpr int SaveDataErrorNotInitialized = -2137063423;

} // namespace

int main() {
    float progress = -1.0f;
    Expect(sceSaveDataGetConvertProgress(nullptr) == SaveDataErrorParameter, "get convert progress null pointer returns parameter error");
    Expect(sceSaveDataGetConvertProgress(&progress) == SaveDataErrorNotInitialized, "get convert progress uninitialized returns not initialized error");
    return Failures() == 0 ? 0 : 1;
}
