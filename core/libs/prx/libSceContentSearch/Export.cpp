#include "prx/libSceContentSearch/include/InventoryExports.hpp"
#include "prx/libSceContentSearch/include/Api.hpp"
#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include <atomic>
#include <stdexcept>
#include <string>

namespace {
std::atomic<bool> g_initialized{false};

constexpr int ErrorNotInitialized = static_cast<int>(0x809D1001u);
constexpr int ErrorAlreadyInitialized = static_cast<int>(0x809D1002u);
constexpr int ErrorInvalidArgument = static_cast<int>(0x809D1003u);
constexpr int ErrorLimitTooBig = static_cast<int>(0x809D100Au);
constexpr std::uint32_t MaxSearchLimit = 92;
constexpr std::int64_t LibraryUpdateId = 0;

bool IsContentColumn(std::int32_t column) {
    switch (column) {
    case 1: case 2: case 3: case 4: case 5: case 6: case 7: case 9: case 10: case 11: case 12: case 14: return true;
    default: return false;
    }
}

bool IsSortColumn(std::int32_t column) {
    switch (column) {
    case 1: case 4: case 5: case 6: case 9: case 10: return true;
    default: return false;
    }
}

bool IsConnector(std::int32_t connector) { return connector >= 0 && connector <= 2; }

void ValidateColumnSets(const char* funcName, const ContentSearchContentColumnSet* columnSet, std::uint32_t columnSetLength) {
    if (columnSet == nullptr && columnSetLength != 0) throw std::invalid_argument(std::string(funcName) + ": null column set");
    for (std::uint32_t i = 0; i < columnSetLength; ++i) {
        const auto& set = columnSet[i];
        if (set.columns == nullptr || set.columns_length <= 0) throw std::invalid_argument(std::string(funcName) + ": empty column set");
        if (!IsConnector(set.logical_connector)) throw std::invalid_argument(std::string(funcName) + ": unknown logical connector " + std::to_string(set.logical_connector));
        for (std::int32_t j = 0; j < set.columns_length; ++j) {
            const auto& column = set.columns[j];
            if (!IsContentColumn(column.column_type)) throw std::invalid_argument(std::string(funcName) + ": unknown column " + std::to_string(column.column_type));
            if (column.column_operator < 0 || column.column_operator > 5) throw std::invalid_argument(std::string(funcName) + ": unknown column operator " + std::to_string(column.column_operator));
            if (column.value == nullptr) throw std::invalid_argument(std::string(funcName) + ": null column value");
            if (!IsConnector(column.logical_connector)) throw std::invalid_argument(std::string(funcName) + ": unknown logical connector " + std::to_string(column.logical_connector));
        }
    }
}

void ValidateOrderBy(const char* funcName, const ContentSearchContentOrderByCondition* orderByConditions, std::uint32_t orderByConditionsLength) {
    if (orderByConditions == nullptr && orderByConditionsLength != 0) throw std::invalid_argument(std::string(funcName) + ": null order-by conditions");
    for (std::uint32_t i = 0; i < orderByConditionsLength; ++i) {
        const auto& condition = orderByConditions[i];
        if (!IsSortColumn(condition.column_type)) throw std::invalid_argument(std::string(funcName) + ": unknown sort column " + std::to_string(condition.column_type));
        if (condition.sort_type != 0 && condition.sort_type != 1) throw std::invalid_argument(std::string(funcName) + ": unknown sort type " + std::to_string(condition.sort_type));
    }
}

}

extern "C" {

int APS5_VABI sceContentSearchAddAvContentCreatedEventCondition() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceContentSearchDisableAvContentCreatedEvent() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceContentSearchEnableAvContentCreatedEvent() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceContentSearchGetContentIdByAvContentCreatedEvent() {
    NotImplemented_nid_no_patch(__func__);
}


int APS5_VABI sceContentSearchInit(const ContentSearchInitParam* init_param) {
    if (g_initialized.load()) return ErrorAlreadyInitialized;
    if (init_param == nullptr || init_param->memory_size == 0 || init_param->memory_size % 16384 != 0) return ErrorInvalidArgument;
    bool expected = false;
    if (!g_initialized.compare_exchange_strong(expected, true)) return ErrorAlreadyInitialized;
    return 0;
}

int APS5_VABI sceContentSearchCloseMetadata(std::int32_t metadataId) {
    if (!g_initialized.load()) return ErrorNotInitialized;
    return ErrorInvalidArgument;
}

int APS5_VABI sceContentSearchGetMetadataFieldInfo(std::int32_t metadataId, const char* field, std::int32_t* metadataType, std::int32_t* size) {
    if (!g_initialized.load()) return ErrorNotInitialized;
    if (field == nullptr || metadataType == nullptr || size == nullptr) return ErrorInvalidArgument;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceContentSearchGetMetadataValue(std::int32_t metadataId, const char* field, void* value) {
    if (!g_initialized.load()) return ErrorNotInitialized;
    if (field == nullptr || value == nullptr) return ErrorInvalidArgument;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceContentSearchOpenMetadata(const char* filePath, std::int32_t* metadataId) {
    if (!g_initialized.load()) return ErrorNotInitialized;
    if (filePath == nullptr || metadataId == nullptr) return ErrorInvalidArgument;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceContentSearchSearchContent(const ContentSearchContentColumnSet* columnSet, std::uint32_t columnSetLength, const ContentSearchContentOrderByCondition* orderByConditions, std::uint32_t orderByConditionsLength, std::uint32_t offset, std::uint32_t limit, std::int64_t* numOfContent, void* infos, std::int64_t* lastUpdateId) {
    (void)offset;
    if (!g_initialized.load()) return ErrorNotInitialized;
    if ((columnSet == nullptr) != (columnSetLength == 0) || (orderByConditions == nullptr) != (orderByConditionsLength == 0) || limit == 0) return ErrorInvalidArgument;
    if (limit > MaxSearchLimit) return ErrorLimitTooBig;
    if (numOfContent == nullptr || lastUpdateId == nullptr || infos == nullptr) return ErrorInvalidArgument;
    ValidateColumnSets(__func__, columnSet, columnSetLength);
    ValidateOrderBy(__func__, orderByConditions, orderByConditionsLength);
    *numOfContent = 0;
    *lastUpdateId = LibraryUpdateId;
    return 0;
}

int APS5_VABI sceContentSearchGetContentLastUpdateId(std::int64_t* lastUpdateId) {
    if (!g_initialized.load()) return ErrorNotInitialized;
    if (lastUpdateId == nullptr) return ErrorInvalidArgument;
    *lastUpdateId = LibraryUpdateId;
    return 0;
}

int APS5_VABI sceContentSearchOpenMetadataByContentId(std::int64_t contentId, std::int32_t* metadataId) {
    if (!g_initialized.load()) return ErrorNotInitialized;
    if (metadataId == nullptr) return ErrorInvalidArgument;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceContentSearchGetTotalContentSize(const ContentSearchContentColumnSet* columnSet, std::uint32_t columnSetLength, std::int64_t* size, std::int64_t* lastUpdateId) {
    if (!g_initialized.load()) return ErrorNotInitialized;
    if ((columnSet == nullptr) != (columnSetLength == 0) || size == nullptr || lastUpdateId == nullptr) return ErrorInvalidArgument;
    ValidateColumnSets(__func__, columnSet, columnSetLength);
    *size = 0;
    *lastUpdateId = LibraryUpdateId;
    return 0;
}

int APS5_VABI sceContentSearchGetNumOfContent(const ContentSearchContentColumnSet* columnSet, std::uint32_t columnSetLength, std::int64_t* numOfContent, std::int64_t* lastUpdateId) {
    if (!g_initialized.load()) return ErrorNotInitialized;
    if ((columnSet == nullptr) != (columnSetLength == 0) || numOfContent == nullptr || lastUpdateId == nullptr) return ErrorInvalidArgument;
    ValidateColumnSets(__func__, columnSet, columnSetLength);
    *numOfContent = 0;
    *lastUpdateId = LibraryUpdateId;
    return 0;
}

int APS5_VABI sceContentSearchTerm(void) {
    bool expected = true;
    if (!g_initialized.compare_exchange_strong(expected, false)) return ErrorNotInitialized;
    return 0;
}

int APS5_VABI sceContentBinderEnd() {
    return static_cast<int>(0x809D000F);
}
int APS5_VABI sceContentBinderGetNumOfDifference() {
    return static_cast<int>(0x809D000F);
}
int APS5_VABI sceContentBinderInitialize() {
    return static_cast<int>(0x809D000F);
}
int APS5_VABI sceContentBinderIsUpdated() {
    return static_cast<int>(0x809D000F);
}
int APS5_VABI sceContentBinderReset() {
    return static_cast<int>(0x809D000F);
}
int APS5_VABI sceContentBinderSearchDifference() {
    return static_cast<int>(0x809D000F);
}
int APS5_VABI sceContentBinderStart() {
    return static_cast<int>(0x809D000F);
}
int APS5_VABI sceContentBinderTerminate() {
    return static_cast<int>(0x809D000F);
}
int APS5_VABI sceContentSearchGetApplicationLastUpdateId() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceContentSearchGetMyApplicationIndex() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceContentSearchGetNumOfApplication() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceContentSearchSearchApplication() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

}
