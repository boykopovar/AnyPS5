#include "prx/libSceContentSearch/include/Api.hpp"
#include "SceTypes.hpp"
#include <cstdint>
#include <cstdlib>
#ifdef __APPLE__
#include <initializer_list>
#endif
#include <stdexcept>

namespace {

void Require(bool value) { if (!value) std::abort(); }

template <typename TAction>
bool ThrowsRuntimeError(TAction action) {
    try {
        action();
    } catch (const std::runtime_error&) {
        return true;
    }
    return false;
}

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

}

int main() {
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

    Require(sceContentSearchGetContentLastUpdateId(&updateId) == ErrorNotInitialized);
    Require(sceContentSearchGetNumOfContent(&columnSet, 1, &count, &updateId) == ErrorNotInitialized);
    Require(sceContentSearchGetTotalContentSize(&columnSet, 1, &size, &updateId) == ErrorNotInitialized);
    Require(sceContentSearchSearchContent(&columnSet, 1, &orderBy, 1, 0, 2, &count, infos, &updateId) == ErrorNotInitialized);
    Require(sceContentSearchOpenMetadata("/data/photo.jpg", &metadataId) == ErrorNotInitialized);
    Require(sceContentSearchOpenMetadataByContentId(1, &metadataId) == ErrorNotInitialized);
    Require(sceContentSearchCloseMetadata(1) == ErrorNotInitialized);
    Require(count == -1 && size == -1 && updateId == -1 && metadataId == -1);

    Require(sceContentSearchTerm() == ErrorNotInitialized);
    Require(sceContentSearchInit(nullptr) == ErrorInvalidArgument);
    for (std::size_t memorySize : {std::size_t{0}, std::size_t{1}, std::size_t{16383}, param.memory_size - 1}) {
        ContentSearchInitParam invalidParam{memorySize};
        Require(sceContentSearchInit(&invalidParam) == ErrorInvalidArgument);
        Require(sceContentSearchTerm() == ErrorNotInitialized);
    }
    Require(sceContentSearchInit(&param) == 0);
    Require(sceContentSearchInit(nullptr) == ErrorAlreadyInitialized);
    Require(sceContentSearchInit(&param) == ErrorAlreadyInitialized);

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
    Require(sceContentSearchSearchContent(&columnSet, 1, &orderBy, 1, 0, 93, &count, infos, &updateId) == ErrorLimitTooBig);
    Require(count == -1 && updateId == -1);

    Require(sceContentSearchGetContentLastUpdateId(nullptr) == ErrorInvalidArgument);
    Require(sceContentSearchGetNumOfContent(&columnSet, 1, nullptr, &updateId) == ErrorInvalidArgument);
    Require(sceContentSearchGetNumOfContent(&columnSet, 1, &count, nullptr) == ErrorInvalidArgument);
    Require(sceContentSearchGetTotalContentSize(nullptr, 1, &size, &updateId) == ErrorInvalidArgument);
    Require(sceContentSearchSearchContent(&columnSet, 1, &orderBy, 1, 0, 2, &count, nullptr, &updateId) == ErrorInvalidArgument);
    ContentSearchContentColumn unknownColumn{8, 0, &photo, 2};
    ContentSearchContentColumnSet unknownColumnSet{&unknownColumn, 1, 2};
    Require(Throws([&] { sceContentSearchGetNumOfContent(&unknownColumnSet, 1, &count, &updateId); }));
    ContentSearchContentColumn missingValue{3, 0, nullptr, 2};
    ContentSearchContentColumnSet missingValueSet{&missingValue, 1, 2};
    Require(Throws([&] { sceContentSearchGetTotalContentSize(&missingValueSet, 1, &size, &updateId); }));
    ContentSearchContentColumn unknownOperator{3, 6, &photo, 2};
    ContentSearchContentColumnSet unknownOperatorSet{&unknownOperator, 1, 2};
    Require(Throws([&] { sceContentSearchSearchContent(&unknownOperatorSet, 1, &orderBy, 1, 0, 2, &count, infos, &updateId); }));
    ContentSearchContentOrderByCondition unknownSort{3, 0};
    Require(Throws([&] { sceContentSearchSearchContent(&columnSet, 1, &unknownSort, 1, 0, 2, &count, infos, &updateId); }));
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
    Require(sceContentSearchGetContentLastUpdateId(&updateId) == ErrorNotInitialized);
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

    Require(ThrowsRuntimeError([] { sceContentSearchGetApplicationLastUpdateId(); }));
    Require(ThrowsRuntimeError([] { sceContentSearchGetMyApplicationIndex(); }));
    Require(ThrowsRuntimeError([] { sceContentSearchGetNumOfApplication(); }));
    Require(ThrowsRuntimeError([] { sceContentSearchSearchApplication(); }));
}
