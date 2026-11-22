#include "prx/libSceContentSearch/include/Api.hpp"
#include "prx/libSceContentSearch/include/InventoryExports.hpp"
#include "prx/libSceUserService/UserService.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "ProcessParametersFixture.hpp"
#include "SceTypes.hpp"
#include <array>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#ifdef __APPLE__
#include <initializer_list>
#endif
#include <stdexcept>

namespace {

void ClobberErrno(std::uintptr_t, std::size_t) {
    errno = 99;
}

void Require(bool value) { if (!value) std::abort(); }

template <typename TAction>
bool Throws(TAction action) {
    try {
        action();
    } catch (const std::logic_error&) {
        return true;
    }
    return false;
}

constexpr int ErrorNotInitialized = static_cast<int>(0x809D1001u);
constexpr int ErrorInvalidArgument = static_cast<int>(0x809D1003u);
constexpr int ErrorAlreadyInitialized = static_cast<int>(0x809D1002u);
constexpr int ErrorLimitTooBig = static_cast<int>(0x809D100Au);
constexpr int ErrorLegacyServiceUnavailable = static_cast<int>(0x809D100Du);
constexpr int ErrorServiceUnavailable = static_cast<int>(0x809D0002u);
alignas(16) std::array<std::byte, 0x40> processParameters{};

void SetSdkVersion(std::uint32_t version) {
    std::memcpy(processParameters.data() + 0x10, &version, sizeof(version));
}

}

int main() {
    const std::uint64_t processSize = processParameters.size();
    const std::uint32_t processMagic = 0x4942524f;
    std::memcpy(processParameters.data(), &processSize, sizeof(processSize));
    std::memcpy(processParameters.data() + 8, &processMagic, sizeof(processMagic));
    SetSdkVersion(0x064fffff);
    Require(ProcessParametersFixture::Install(processParameters.data(), processParameters.size()));
    GuestArena::GuestArenaSetHostReadHook_nid_no_patch(ClobberErrno);
    GuestArena::GuestArenaSetHostWriteHook_nid_no_patch(ClobberErrno);
    ContentSearchInitParam param{3 * 1024 * 1024};
    ContentSearchColumnValue photo{8, 1};
    ContentSearchContentColumn column{3, 0, &photo, 2};
    ContentSearchContentColumnSet columnSet{&column, 1, 2};
    ContentSearchContentOrderByCondition orderBy{4, 0};
    unsigned char infos[0x960 * 2] = {};
    std::int64_t count = -1;
    std::int64_t size = -1;
    std::int64_t updateId = -1;
    std::int32_t metadataId = -1;
    std::int32_t applicationIndex = -1;
    unsigned char applicationInfos[16] = {};

    Require(sceContentSearchSearchApplication(nullptr, 0, nullptr, 0, nullptr, 0, 0, 1, &count, applicationInfos, &updateId) == ErrorNotInitialized);
    Require(sceContentSearchGetApplicationLastUpdateId(&updateId) == ErrorNotInitialized);
    Require(sceContentSearchGetMyApplicationIndex(&applicationIndex) == ErrorNotInitialized);
    Require(sceContentSearchGetNumOfApplication(&columnSet, 1, &orderBy, 1, &count, &updateId) == ErrorNotInitialized);
    Require(sceContentSearchGetContentLastUpdateId(&updateId) == ErrorNotInitialized);
    Require(sceContentSearchGetNumOfContent(&columnSet, 1, &count, &updateId) == ErrorNotInitialized);
    Require(sceContentSearchGetTotalContentSize(&columnSet, 1, &size, &updateId) == ErrorNotInitialized);
    Require(sceContentSearchSearchContent(&columnSet, 1, &orderBy, 1, 0, 2, &count, infos, &updateId) == ErrorNotInitialized);
    Require(sceContentSearchGetMetadataFieldInfo(0, nullptr, nullptr, nullptr) == ErrorNotInitialized);
    Require(sceContentSearchGetMetadataValue(0, nullptr, nullptr) == ErrorNotInitialized);
    Require(sceContentSearchOpenMetadata("/data/photo.jpg", &metadataId) == ErrorNotInitialized);
    Require(sceContentSearchOpenMetadataByContentId(1, &metadataId) == ErrorNotInitialized);
    Require(sceContentSearchCloseMetadata(1) == ErrorNotInitialized);
    Require(count == -1 && size == -1 && updateId == -1 && metadataId == -1);
    std::uint64_t avContentId = 0xA5A5A5A5A5A5A5A5u;
    unsigned char avEvent[12] = {0x1D, 0x00, 0x00, 0x10, 0x88, 0x77, 0x66, 0x55, 0x44, 0x33, 0x22, 0x11};
    Require(sceContentSearchAddAvContentCreatedEventCondition(1, 1) == ErrorNotInitialized);
    Require(sceContentSearchEnableAvContentCreatedEvent() == ErrorNotInitialized);
    Require(sceContentSearchDisableAvContentCreatedEvent() == ErrorNotInitialized);
    Require(sceContentSearchGetContentIdByAvContentCreatedEvent(nullptr, nullptr) == ErrorNotInitialized);
    Require(sceContentSearchGetContentIdByAvContentCreatedEvent(avEvent, &avContentId) == ErrorNotInitialized);
    Require(avContentId == 0xA5A5A5A5A5A5A5A5u);

    Require(sceContentSearchTerm() == ErrorNotInitialized);
    Require(sceContentSearchInit(nullptr) == ErrorInvalidArgument);
    errno = 71;
    Require(sceContentSearchInit(reinterpret_cast<const ContentSearchInitParam*>(0x10)) == ErrorInvalidArgument);
    Require(errno == 71);
    for (std::size_t memorySize : {std::size_t{0}, std::size_t{1}, std::size_t{16383}, param.memory_size - 1}) {
        ContentSearchInitParam invalidParam{memorySize};
        Require(sceContentSearchInit(&invalidParam) == ErrorInvalidArgument);
        Require(sceContentSearchTerm() == ErrorNotInitialized);
    }
    Require(sceContentSearchInit(&param) == 0);
    Require(sceContentSearchInit(nullptr) == ErrorAlreadyInitialized);
    Require(sceContentSearchInit(&param) == ErrorAlreadyInitialized);
    Require(sceUserServiceInitialize(nullptr) == USER_SERVICE_OK);

    constexpr int ErrorAvEventState = static_cast<int>(0x809D1014u);
    Require(sceContentSearchDisableAvContentCreatedEvent() == ErrorAvEventState);
    Require(sceContentSearchEnableAvContentCreatedEvent() == ErrorAvEventState);
    Require(sceContentSearchAddAvContentCreatedEventCondition(0, 1) == ErrorInvalidArgument);
    Require(sceContentSearchAddAvContentCreatedEventCondition(1, 2) == ErrorInvalidArgument);
    Require(sceContentSearchEnableAvContentCreatedEvent() == ErrorAvEventState);
    Require(sceContentSearchAddAvContentCreatedEventCondition(1, 1) == 0);
    Require(sceContentSearchAddAvContentCreatedEventCondition(1, 1) == 0);
    Require(sceContentSearchDisableAvContentCreatedEvent() == ErrorAvEventState);
    Require(sceContentSearchGetContentIdByAvContentCreatedEvent(nullptr, &avContentId) == ErrorInvalidArgument);
    Require(sceContentSearchGetContentIdByAvContentCreatedEvent(avEvent, nullptr) == ErrorInvalidArgument);
    avEvent[0] = 0x1E;
    Require(sceContentSearchGetContentIdByAvContentCreatedEvent(avEvent, &avContentId) == ErrorInvalidArgument);
    Require(avContentId == 0xA5A5A5A5A5A5A5A5u);
    avEvent[0] = 0x1D;
    Require(sceContentSearchGetContentIdByAvContentCreatedEvent(avEvent, &avContentId) == 0);
    Require(avContentId == 0x1122334455667788u);
    count = -1;
    updateId = -1;
    Require(sceContentSearchSearchApplication(&columnSet, 0, nullptr, 0, nullptr, 0, 0, 1, &count, applicationInfos, &updateId) == ErrorInvalidArgument);
    Require(sceContentSearchSearchApplication(nullptr, 0, &orderBy, 0, nullptr, 0, 0, 1, &count, applicationInfos, &updateId) == ErrorInvalidArgument);
    Require(sceContentSearchSearchApplication(nullptr, 0, nullptr, 0, &columnSet, 0, 0, 1, &count, applicationInfos, &updateId) == ErrorInvalidArgument);
    Require(sceContentSearchSearchApplication(nullptr, 0, nullptr, 0, nullptr, 0, 0, 0, &count, applicationInfos, &updateId) == ErrorInvalidArgument);
    Require(sceContentSearchSearchApplication(nullptr, 0, nullptr, 0, nullptr, 0, 0, 93, nullptr, nullptr, nullptr) == ErrorLimitTooBig);
    Require(sceContentSearchSearchApplication(nullptr, 0, nullptr, 0, nullptr, 0, 0, 1, nullptr, applicationInfos, &updateId) == ErrorInvalidArgument);
    Require(sceContentSearchSearchApplication(nullptr, 0, nullptr, 0, nullptr, 0, 0, 1, &count, nullptr, &updateId) == ErrorInvalidArgument);
    Require(sceContentSearchSearchApplication(nullptr, 0, nullptr, 0, nullptr, 0, 0, 1, &count, applicationInfos, nullptr) == ErrorInvalidArgument);
    Require(count == -1 && updateId == -1);
    errno = 71;
    Require(sceContentSearchSearchApplication(nullptr, 0, nullptr, 0, nullptr, 0, 7, 92, &count, applicationInfos, &updateId) == ErrorLegacyServiceUnavailable);
    Require(count == 0 && updateId == 0 && errno == 71);
    SetSdkVersion(0x06500000);
    count = -1;
    updateId = -1;
    Require(sceContentSearchSearchApplication(&columnSet, 1, &orderBy, 1, &columnSet, 1, 7, 92, &count, applicationInfos, &updateId) == ErrorServiceUnavailable);
    Require(count == 0 && updateId == 0);
    count = -1;
    updateId = -1;
    errno = 71;
    Require(sceContentSearchSearchApplication(nullptr, 0, nullptr, 0, nullptr, 0, 0, 1, reinterpret_cast<std::int64_t*>(0x10), applicationInfos, &updateId) == ErrorInvalidArgument);
    Require(count == -1 && updateId == -1 && errno == 71);
    Require(sceContentSearchSearchApplication(nullptr, 0, nullptr, 0, nullptr, 0, 0, 1, &count, applicationInfos, reinterpret_cast<std::int64_t*>(0x10)) == ErrorInvalidArgument);
    Require(count == 0 && errno == 71);

    const auto* invalidSets = reinterpret_cast<const ContentSearchContentColumnSet*>(0x10);
    const auto* invalidOrders = reinterpret_cast<const ContentSearchContentOrderByCondition*>(0x10);
    ContentSearchContentColumnSet invalidNestedSet{reinterpret_cast<const ContentSearchContentColumn*>(0x10), 1, 2};
    ContentSearchContentColumnSet setsWithInvalidSecond[] = {columnSet, invalidNestedSet};
    count = -1;
    updateId = -1;
    errno = 71;
    Require(sceContentSearchGetNumOfContent(invalidSets, 1, &count, &updateId) == ErrorInvalidArgument);
    Require(sceContentSearchGetTotalContentSize(&invalidNestedSet, 1, &size, &updateId) == ErrorInvalidArgument);
    Require(sceContentSearchGetNumOfApplication(&columnSet, 1, invalidOrders, 1, &count, &updateId) == ErrorInvalidArgument);
    Require(sceContentSearchSearchContent(setsWithInvalidSecond, 2, &orderBy, 1, 0, 1, &count, infos, &updateId) == ErrorInvalidArgument);
    Require(sceContentSearchSearchContent(&columnSet, 1, invalidOrders, 1, 0, 1, &count, infos, &updateId) == ErrorInvalidArgument);
    Require(count == -1 && updateId == -1 && size == -1 && errno == 71);

    Require(sceContentSearchGetContentLastUpdateId(&updateId) == 0);
    Require(updateId == 0);
    updateId = -1;
    Require(sceContentSearchGetNumOfContent(&columnSet, 1, &count, &updateId) == 0);
    Require(count == 0 && updateId == 0);
    count = -1;
    updateId = -1;
    Require(sceContentSearchGetNumOfContent(nullptr, 0, &count, &updateId) == 0);
    Require(count == 0 && updateId == 0);
    updateId = -1;
    Require(sceContentSearchGetTotalContentSize(&columnSet, 1, &size, &updateId) == 0);
    Require(size == 0 && updateId == 0);
    count = -1;
    updateId = -1;
    Require(sceContentSearchSearchContent(&columnSet, 1, &orderBy, 1, 0, 92, &count, infos, &updateId) == 0);
    Require(count == 0 && updateId == 0);
    count = -1;
    updateId = -1;
    errno = 71;
    Require(sceContentSearchSearchContent(nullptr, 0, nullptr, 0, 0, 1,
        reinterpret_cast<std::int64_t*>(0x10), infos, &updateId) == ErrorInvalidArgument);
    Require(updateId == -1 && errno == 71);
    Require(sceContentSearchSearchContent(nullptr, 0, nullptr, 0, 0, 1,
        &count, infos, reinterpret_cast<std::int64_t*>(0x10)) == ErrorInvalidArgument);
    Require(count == 0 && errno == 71);
    count = -1;
    updateId = -1;
    Require(sceContentSearchSearchContent(&columnSet, 1, &orderBy, 1, 0, 93, &count, infos, &updateId) == ErrorLimitTooBig);
    Require(count == -1 && updateId == -1);
    updateId = -1;
    Require(sceContentSearchGetApplicationLastUpdateId(&updateId) == 0 && updateId == 0);
    errno = 71;
    Require(sceContentSearchGetApplicationLastUpdateId(reinterpret_cast<std::int64_t*>(0x10)) == ErrorInvalidArgument);
    Require(errno == 71);
    count = -1;
    updateId = -1;
    Require(sceContentSearchGetNumOfApplication(&columnSet, 1, &orderBy, 1, &count, &updateId) == 0);
    Require(count == 0 && updateId == 0);
    count = -1;
    updateId = -1;
    Require(sceContentSearchGetNumOfApplication(nullptr, 0, nullptr, 0, &count, &updateId) == 0);
    Require(count == 0 && updateId == 0);
    count = -1;
    updateId = -1;
    errno = 71;
    Require(sceContentSearchGetNumOfApplication(nullptr, 0, nullptr, 0,
        reinterpret_cast<std::int64_t*>(0x10), &updateId) == ErrorInvalidArgument);
    Require(updateId == -1 && errno == 71);
    Require(sceContentSearchGetNumOfApplication(nullptr, 0, nullptr, 0,
        &count, reinterpret_cast<std::int64_t*>(0x10)) == ErrorInvalidArgument);
    Require(count == 0 && errno == 71);
    count = -1;
    updateId = -1;

    Require(sceContentSearchGetContentLastUpdateId(nullptr) == ErrorInvalidArgument);
    errno = 71;
    Require(sceContentSearchGetContentLastUpdateId(reinterpret_cast<std::int64_t*>(0x10)) == ErrorInvalidArgument);
    Require(errno == 71);
    errno = 71;
    Require(sceContentSearchGetContentLastUpdateId(&updateId) == 0);
    Require(errno == 71);
    Require(sceContentSearchGetApplicationLastUpdateId(nullptr) == ErrorInvalidArgument);
    Require(sceContentSearchGetMyApplicationIndex(nullptr) == ErrorInvalidArgument);
    errno = 71;
    Require(sceContentSearchGetMyApplicationIndex(reinterpret_cast<std::int32_t*>(0x10)) == ErrorInvalidArgument);
    Require(errno == 71);
    Require(sceContentSearchGetNumOfApplication(&columnSet, 1, &orderBy, 1, nullptr, &updateId) == ErrorInvalidArgument);
    Require(sceContentSearchGetNumOfApplication(&columnSet, 1, &orderBy, 1, &count, nullptr) == ErrorInvalidArgument);
    Require(sceContentSearchGetNumOfApplication(&columnSet, 1, &orderBy, 0, &count, &updateId) == ErrorInvalidArgument);
    Require(sceContentSearchGetNumOfApplication(&columnSet, 0, &orderBy, 1, &count, &updateId) == ErrorInvalidArgument);
    Require(sceContentSearchGetNumOfContent(&columnSet, 1, nullptr, &updateId) == ErrorInvalidArgument);
    errno = 71;
    Require(sceContentSearchGetNumOfContent(&columnSet, 1, reinterpret_cast<std::int64_t*>(0x10), &updateId) == ErrorInvalidArgument);
    Require(errno == 71);
    count = -1;
    Require(sceContentSearchGetNumOfContent(&columnSet, 1, &count, nullptr) == ErrorInvalidArgument);
    Require(count == -1);
    errno = 71;
    Require(sceContentSearchGetNumOfContent(&columnSet, 1, &count, reinterpret_cast<std::int64_t*>(0x10)) == ErrorInvalidArgument);
    Require(count == 0 && errno == 71);
    errno = 71;
    Require(sceContentSearchGetNumOfContent(&columnSet, 1, &count, &updateId) == 0);
    Require(errno == 71);
    Require(sceContentSearchGetTotalContentSize(nullptr, 1, &size, &updateId) == ErrorInvalidArgument);
    Require(sceContentSearchGetTotalContentSize(nullptr, 0, nullptr, &updateId) == ErrorInvalidArgument);
    errno = 71;
    Require(sceContentSearchGetTotalContentSize(nullptr, 0, reinterpret_cast<std::int64_t*>(0x10), &updateId) == ErrorInvalidArgument);
    Require(errno == 71);
    size = -1;
    Require(sceContentSearchGetTotalContentSize(nullptr, 0, &size, nullptr) == ErrorInvalidArgument);
    Require(size == -1);
    errno = 71;
    Require(sceContentSearchGetTotalContentSize(nullptr, 0, &size, reinterpret_cast<std::int64_t*>(0x10)) == ErrorInvalidArgument);
    Require(size == 0 && errno == 71);
    errno = 71;
    Require(sceContentSearchGetTotalContentSize(nullptr, 0, &size, &updateId) == 0);
    Require(errno == 71);
    count = -1;
    size = -1;
    updateId = -1;
    std::int32_t metadataType = 0, fieldSize = 0;
    Require(sceContentSearchGetMetadataFieldInfo(1, nullptr, nullptr, nullptr) == ErrorInvalidArgument);
    Require(sceContentSearchGetMetadataFieldInfo(1, "field", nullptr, &fieldSize) == ErrorInvalidArgument);
    Require(sceContentSearchGetMetadataFieldInfo(1, "field", &metadataType, nullptr) == ErrorInvalidArgument);
    Require(sceContentSearchGetMetadataValue(1, nullptr, nullptr) == ErrorInvalidArgument);
    Require(sceContentSearchGetMetadataValue(1, "field", nullptr) == ErrorInvalidArgument);
    Require(sceContentSearchOpenMetadata(nullptr, nullptr) == ErrorInvalidArgument);
    Require(sceContentSearchOpenMetadata("/data/photo.jpg", nullptr) == ErrorInvalidArgument);
    Require(sceContentSearchOpenMetadata(nullptr, &metadataId) == ErrorInvalidArgument);
    Require(sceContentSearchOpenMetadataByContentId(1, nullptr) == ErrorInvalidArgument);
    Require(sceContentSearchSearchContent(&columnSet, 1, &orderBy, 1, 0, 2, &count, nullptr, &updateId) == ErrorInvalidArgument);
    ContentSearchContentColumn unknownColumn{8, 0, &photo, 2};
    ContentSearchContentColumnSet unknownColumnSet{&unknownColumn, 1, 2};
    Require(Throws([&] { sceContentSearchGetNumOfContent(&unknownColumnSet, 1, &count, &updateId); }));
    Require(Throws([&] { sceContentSearchGetNumOfApplication(&unknownColumnSet, 1, &orderBy, 1, &count, &updateId); }));
    ContentSearchContentColumn missingValue{3, 0, nullptr, 2};
    ContentSearchContentColumnSet missingValueSet{&missingValue, 1, 2};
    Require(Throws([&] { sceContentSearchGetTotalContentSize(&missingValueSet, 1, &size, &updateId); }));
    ContentSearchContentColumn unknownOperator{3, 6, &photo, 2};
    ContentSearchContentColumnSet unknownOperatorSet{&unknownOperator, 1, 2};
    Require(Throws([&] { sceContentSearchSearchContent(&unknownOperatorSet, 1, &orderBy, 1, 0, 2, &count, infos, &updateId); }));
    ContentSearchContentOrderByCondition unknownSort{3, 0};
    Require(Throws([&] { sceContentSearchSearchContent(&columnSet, 1, &unknownSort, 1, 0, 2, &count, infos, &updateId); }));
    Require(Throws([&] { sceContentSearchGetNumOfApplication(&columnSet, 1, &unknownSort, 1, &count, &updateId); }));
    Require(count == -1 && updateId == -1);
    Require(sceContentSearchSearchContent(nullptr, 0, nullptr, 0, 0, 0, nullptr, nullptr, nullptr) == ErrorInvalidArgument);
    Require(sceContentSearchSearchContent(nullptr, 0, nullptr, 0, 0, 93, nullptr, nullptr, nullptr) == ErrorLimitTooBig);
    Require(sceContentSearchSearchContent(&columnSet, 0, nullptr, 0, 0, 93, nullptr, nullptr, nullptr) == ErrorInvalidArgument);
    Require(sceContentSearchSearchContent(nullptr, 0, &orderBy, 0, 0, 92, &count, infos, &updateId) == ErrorInvalidArgument);
    Require(sceContentSearchGetNumOfContent(&columnSet, 0, &count, &updateId) == ErrorInvalidArgument);
    Require(sceContentSearchGetTotalContentSize(&columnSet, 0, &size, &updateId) == ErrorInvalidArgument);
    Require(count == -1 && updateId == -1);

    Require(sceContentSearchCloseMetadata(1) == ErrorInvalidArgument);

    Require(sceContentSearchTerm() == 0);
    Require(sceContentSearchTerm() == ErrorNotInitialized);
    Require(sceContentSearchOpenMetadata(nullptr, nullptr) == ErrorNotInitialized);
    Require(sceContentSearchOpenMetadataByContentId(1, nullptr) == ErrorNotInitialized);
    Require(sceContentSearchGetTotalContentSize(&columnSet, 1, &size, &updateId) == ErrorNotInitialized);
    Require(sceContentSearchGetNumOfContent(&columnSet, 1, &count, &updateId) == ErrorNotInitialized);
    Require(sceContentSearchSearchContent(&columnSet, 1, &orderBy, 1, 0, 2, &count, infos, &updateId) == ErrorNotInitialized);
    Require(sceContentSearchCloseMetadata(1) == ErrorNotInitialized);
    Require(sceContentSearchGetContentLastUpdateId(&updateId) == ErrorNotInitialized);
    Require(sceContentSearchGetApplicationLastUpdateId(&updateId) == ErrorNotInitialized);
    Require(sceContentSearchGetMyApplicationIndex(&applicationIndex) == ErrorNotInitialized);
    Require(sceContentSearchGetNumOfApplication(&columnSet, 1, &orderBy, 1, &count, &updateId) == ErrorNotInitialized);
    Require(sceContentSearchSearchApplication(nullptr, 0, nullptr, 0, nullptr, 0, 0, 1, &count, applicationInfos, &updateId) == ErrorNotInitialized);
    Require(updateId == -1);
    constexpr int binderResult = static_cast<int>(0x809D000F);
    Require(sceContentBinderInitialize() == binderResult);
    Require(sceContentBinderStart() == binderResult);
    Require(sceContentBinderReset() == binderResult);
    Require(sceContentBinderIsUpdated() == binderResult);
    Require(sceContentBinderGetNumOfDifference() == binderResult);
    Require(sceContentBinderSearchDifference() == binderResult);
    Require(sceContentBinderEnd() == binderResult);
    Require(sceContentBinderTerminate() == binderResult);

    GuestArena::GuestArenaSetHostReadHook_nid_no_patch(nullptr);
    GuestArena::GuestArenaSetHostWriteHook_nid_no_patch(nullptr);
}
