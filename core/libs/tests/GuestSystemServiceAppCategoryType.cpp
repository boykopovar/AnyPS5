#include "prx/libSceSystemService/include/SystemService.hpp"
#include "prx/libSceSystemService/SystemService.hpp"
#include "SceTypes.hpp"
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <limits>
#include <string_view>

namespace {

constexpr std::uint32_t CategoryType = 7;
constexpr std::uint32_t Sentinel = 0xA5A5A5A5u;

void Require(bool value) { if (!value) std::abort(); }

template<typename TCall>
bool Throws(TCall call) {
    try {
        call();
    } catch (const std::exception&) {
        return true;
    }
    return false;
}

int ReadSystemService(std::int32_t appId, std::uint32_t* categoryType) {
    return sceSystemServiceGetAppCategoryType(appId, categoryType);
}

int ReadLnc(std::int32_t appId, std::uint32_t* categoryType) {
    return sceLncUtilGetAppCategoryType(appId, categoryType);
}

bool BothThrow(std::int32_t appId) {
    std::uint32_t systemServiceValue = Sentinel;
    std::uint32_t lncValue = Sentinel;
    return Throws([&] { static_cast<void>(ReadSystemService(appId, &systemServiceValue)); }) &&
           Throws([&] { static_cast<void>(ReadLnc(appId, &lncValue)); }) &&
           systemServiceValue == Sentinel && lncValue == Sentinel;
}

void CheckNullOutputs() {
    Require(ReadSystemService(SYSTEM_SERVICE_RUNNING_APP_ID, nullptr) == SYSTEM_SERVICE_ERROR_PARAMETER);
    Require(ReadLnc(SYSTEM_SERVICE_RUNNING_APP_ID, nullptr) == LNC_UTIL_ERROR_INVALID_ARGUMENT);
}

void CheckUnwritableOutputs() {
    auto* const inaccessible = reinterpret_cast<std::uint32_t*>(0x10);
    Require(ReadSystemService(SYSTEM_SERVICE_RUNNING_APP_ID, inaccessible) == SYSTEM_SERVICE_ERROR_PARAMETER);
    Require(ReadLnc(SYSTEM_SERVICE_RUNNING_APP_ID, inaccessible) == LNC_UTIL_ERROR_INVALID_ARGUMENT);
}

void CheckOtherApplications() {
    const std::int32_t others[] = {SYSTEM_SERVICE_RUNNING_APP_ID + 1, SYSTEM_SERVICE_RUNNING_APP_ID - 1, 0, -1,
                                   std::numeric_limits<std::int32_t>::min(), std::numeric_limits<std::int32_t>::max()};
    for (const std::int32_t appId : others) Require(BothThrow(appId));
    std::uint32_t value = Sentinel;
    Require(ReadSystemService(SYSTEM_SERVICE_RUNNING_APP_ID, &value) == 0 && value == CategoryType);
}

void CheckKnownCategoryType() {
    std::uint32_t systemServiceValue = Sentinel;
    std::uint32_t lncValue = Sentinel;
    Require(ReadSystemService(SYSTEM_SERVICE_RUNNING_APP_ID, &systemServiceValue) == 0 && systemServiceValue == CategoryType);
    Require(ReadLnc(SYSTEM_SERVICE_RUNNING_APP_ID, &lncValue) == 0 && lncValue == CategoryType);
    CheckNullOutputs();
    CheckUnwritableOutputs();
    CheckOtherApplications();
}

void CheckMissingCategoryType() {
    std::uint32_t systemServiceValue = Sentinel;
    std::uint32_t lncValue = Sentinel;
    Require(Throws([&] { static_cast<void>(ReadSystemService(SYSTEM_SERVICE_RUNNING_APP_ID, &systemServiceValue)); }) &&
            Throws([&] { static_cast<void>(ReadLnc(SYSTEM_SERVICE_RUNNING_APP_ID, &lncValue)); }) &&
            systemServiceValue == Sentinel && lncValue == Sentinel);
    CheckNullOutputs();
}

}

int main(int argc, char** argv) {
    Require(argc == 2);
    const std::string_view mode = argv[1];
    if (mode == "known") CheckKnownCategoryType();
    else if (mode == "absent") CheckMissingCategoryType();
    else if (mode == "missing") CheckMissingCategoryType();
    else Require(false);
}
