#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include <atomic>
#include <stdexcept>
#include <string>

namespace {
std::atomic<bool> g_initialized{false};

constexpr std::uint32_t MaxSearchLimit = 92;
constexpr int ErrorLimitTooBig = static_cast<int>(0x809D100Au);

void RequireInitialized(const char* funcName) {
    if (!g_initialized.load()) throw std::logic_error(std::string(funcName) + ": not initialized");
}
}

extern "C" {

int APS5_VABI sceContentSearchInit(const ContentSearchInitParam* init_param) {
    if (init_param == nullptr) APS5_INVALID_ARG_EX;
    bool expected = false;
    if (!g_initialized.compare_exchange_strong(expected, true)) throw std::logic_error(std::string(__func__) + ": already initialized");
    return 0;
}

int APS5_VABI sceContentSearchCloseMetadata(std::int32_t metadataId) {
    RequireInitialized(__func__);
    throw std::invalid_argument(std::string(__func__) + ": unknown metadata id " + std::to_string(metadataId));
}

int APS5_VABI sceContentSearchGetMetadataFieldInfo(std::int32_t metadataId, const char* field, std::int32_t* metadataType, std::int32_t* size) {
    if (field == nullptr || metadataType == nullptr || size == nullptr) APS5_INVALID_ARG_EX;
    RequireInitialized(__func__);
    throw std::invalid_argument(std::string(__func__) + ": unknown metadata id " + std::to_string(metadataId));
}

int APS5_VABI sceContentSearchGetMetadataValue(std::int32_t metadataId, const char* field, void* value) {
    if (field == nullptr || value == nullptr) APS5_INVALID_ARG_EX;
    RequireInitialized(__func__);
    throw std::invalid_argument(std::string(__func__) + ": unknown metadata id " + std::to_string(metadataId));
}

int APS5_VABI sceContentSearchOpenMetadata(const char* filePath, std::int32_t* metadataId) {
    if (filePath == nullptr || metadataId == nullptr) APS5_INVALID_ARG_EX;
    RequireInitialized(__func__);
    throw std::logic_error(std::string(__func__) + ": " + filePath + " is not in the content library, and the console's error for it is unknown");
}

int APS5_VABI sceContentSearchSearchContent(const void* columnSet, std::uint32_t columnSetLength, const void* orderByConditions, std::uint32_t orderByConditionsLength, std::uint32_t offset, std::uint32_t limit, std::int64_t* numOfContent, void* infos, std::int64_t* lastUpdateId) {
    if (numOfContent == nullptr) APS5_INVALID_ARG_EX;
    if (columnSet == nullptr && columnSetLength != 0) APS5_INVALID_ARG_EX;
    if (orderByConditions == nullptr && orderByConditionsLength != 0) APS5_INVALID_ARG_EX;
    if (infos == nullptr && limit != 0) APS5_INVALID_ARG_EX;
    RequireInitialized(__func__);
    if (limit > MaxSearchLimit) return ErrorLimitTooBig;
    *numOfContent = 0;
    if (lastUpdateId != nullptr) *lastUpdateId = 0;
    return 0;
}

int APS5_VABI sceContentSearchTerm(void) {
    bool expected = true;
    if (!g_initialized.compare_exchange_strong(expected, false)) throw std::logic_error(std::string(__func__) + ": not initialized");
    return 0;
}

}
