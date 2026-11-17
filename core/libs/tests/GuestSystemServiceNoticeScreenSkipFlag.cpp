#include "prx/libSceSystemService/include/SystemService.hpp"
#include "prx/libSceSystemService/SystemService.hpp"
#include "SceTypes.hpp"
#include "Testing.hpp"
#include <cstdint>
#include <cstdlib>

int main() {
    bool value = true;
    Require(sceSystemServiceGetNoticeScreenSkipFlag(&value) == SYSTEM_SERVICE_OK && !value);
    Require(sceSystemServiceGetNoticeScreenSkipFlag(nullptr) == SYSTEM_SERVICE_ERROR_PARAMETER);
    Require(sceSystemServiceDisableNoticeScreenSkipFlagAutoSet() == SYSTEM_SERVICE_OK);
    Require(sceSystemServiceSetNoticeScreenSkipFlag() == SYSTEM_SERVICE_OK);
    value = false;
    Require(sceSystemServiceGetNoticeScreenSkipFlag(&value) == SYSTEM_SERVICE_OK && value);
    Require(sceSystemServiceSetNoticeScreenSkipFlag() == SYSTEM_SERVICE_OK);
    Require(sceSystemServiceGetNoticeScreenSkipFlag(&value) == SYSTEM_SERVICE_OK && value);
    auto* const inaccessible = reinterpret_cast<bool*>(0x10);
    Require(sceSystemServiceGetNoticeScreenSkipFlag(inaccessible) == SYSTEM_SERVICE_ERROR_PARAMETER);
    Require(sceSystemServiceGetNoticeScreenSkipFlag(&value) == SYSTEM_SERVICE_OK && value);
}
