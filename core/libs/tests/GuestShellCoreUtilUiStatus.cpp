#include "prx/libSceSystemService/include/SystemService.hpp"
#include "prx/libSceSystemService/SystemService.hpp"
#include "SceTypes.hpp"
#include <cstdint>
#include <cstdlib>

namespace {

void Require(bool value) { if (!value) std::abort(); }

}

int main() {
    std::uint64_t status = 1;
    Require(sceShellCoreUtilGetUIStatus(&status) == SYSTEM_SERVICE_OK && status == 0);
    Require(sceShellCoreUtilGetUIStatus(nullptr) == SYSTEM_SERVICE_ERROR_PARAMETER);
    Require(sceShellCoreUtilSetUIStatus(0xFF00FF00u, 0xFFFFFFFFu) == SYSTEM_SERVICE_OK);
    status = 0;
    Require(sceShellCoreUtilGetUIStatus(&status) == SYSTEM_SERVICE_OK && status == 0xFF00FF00u);
    Require(sceSystemServiceGetUIStatus(&status) == SYSTEM_SERVICE_OK && status == 0xFF00FF00u);
    Require(sceShellCoreUtilSetUIStatus(0x00001111u, 0x0000FFFFu) == SYSTEM_SERVICE_OK);
    Require(sceShellCoreUtilGetUIStatus(&status) == SYSTEM_SERVICE_OK && status == 0xFF001111u);
    Require(sceShellCoreUtilSetUIStatus(0, 0) == SYSTEM_SERVICE_OK);
    Require(sceShellCoreUtilGetUIStatus(&status) == SYSTEM_SERVICE_OK && status == 0xFF001111u);
    Require(sceShellCoreUtilSetUIStatus(0, 0xFFFFFFFFu) == SYSTEM_SERVICE_OK);
    Require(sceShellCoreUtilGetUIStatus(&status) == SYSTEM_SERVICE_OK && status == 0);
    auto* const inaccessible = reinterpret_cast<std::uint64_t*>(0x10);
    Require(sceShellCoreUtilGetUIStatus(inaccessible) == SYSTEM_SERVICE_ERROR_PARAMETER);
    Require(sceSystemServiceGetUIStatus(nullptr) == SYSTEM_SERVICE_ERROR_PARAMETER);
}
