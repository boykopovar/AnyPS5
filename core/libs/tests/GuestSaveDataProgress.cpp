#include "prx/libSceSaveData/include/Api.hpp"
#include "prx/libSceSaveData.native/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libc/include/General.hpp"
#include "SceTypes.hpp"
#include <cerrno>
#include <cstdio>
#include <cstdlib>

namespace {

constexpr int SaveDataErrorParameter = -2137063424;
constexpr int SaveDataErrorNotInitialized = -2137063423;

int failures = 0;

void Check(bool condition, const char* what) {
    if (condition) return;
    std::fprintf(stderr, "guest savedata progress check failed: %s\n", what);
    ++failures;
}

} // namespace

int main() {
    float progress = -1.0f;
    errno = 71;
    Check(sceSaveDataGetConvertProgress(nullptr) == SaveDataErrorParameter && errno == 71, "get convert progress null pointer returns parameter error");
    Check(sceSaveDataGetConvertProgress(&progress) == SaveDataErrorNotInitialized && errno == 71, "get convert progress uninitialized returns not initialized error");
    Check(sceSaveDataGetConvertProgress(reinterpret_cast<float*>(static_cast<std::uintptr_t>(0x10))) == SaveDataErrorNotInitialized && errno == 71, "get convert progress unmapped pointer uninitialized returns not initialized error");
    return failures == 0 ? 0 : 1;
}
