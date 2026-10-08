#include "SceTypes.hpp"
#include <cstdint>
#include <cstdlib>
#include <stdexcept>

extern "C" int APS5_VABI sceContentSearchInit(const ContentSearchInitParam* init_param);
extern "C" int APS5_VABI sceContentSearchTerm(void);
extern "C" int APS5_VABI sceContentSearchOpenMetadata(const char* filePath, std::int32_t* metadataId);
extern "C" int APS5_VABI sceContentSearchCloseMetadata(std::int32_t metadataId);
extern "C" int APS5_VABI sceContentSearchSearchContent(const void* columnSet, std::uint32_t columnSetLength, const void* orderByConditions, std::uint32_t orderByConditionsLength, std::uint32_t offset, std::uint32_t limit, std::int64_t* numOfContent, void* infos, std::int64_t* lastUpdateId);

namespace {

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

constexpr int ErrorLimitTooBig = static_cast<int>(0x809D100Au);

}

int main() {
    ContentSearchInitParam param{0x100000};
    std::int64_t count = -1;
    std::int64_t updateId = -1;
    unsigned char infos[0x960] = {};
    std::int32_t metadataId = -1;
    Require(Throws([&] { sceContentSearchSearchContent(nullptr, 0, nullptr, 0, 0, 1, &count, infos, &updateId); }));
    Require(sceContentSearchInit(&param) == 0);
    Require(sceContentSearchSearchContent(nullptr, 0, nullptr, 0, 0, 1, &count, infos, &updateId) == 0);
    Require(count == 0 && updateId == 0);
    count = -1;
    Require(sceContentSearchSearchContent(nullptr, 0, nullptr, 0, 5, 92, &count, infos, nullptr) == 0);
    Require(count == 0);
    count = -1;
    Require(sceContentSearchSearchContent(nullptr, 0, nullptr, 0, 0, 93, &count, infos, &updateId) == ErrorLimitTooBig);
    Require(count == -1);
    Require(Throws([&] { sceContentSearchSearchContent(nullptr, 0, nullptr, 0, 0, 1, nullptr, infos, &updateId); }));
    Require(Throws([&] { sceContentSearchSearchContent(nullptr, 1, nullptr, 0, 0, 1, &count, infos, &updateId); }));
    Require(Throws([&] { sceContentSearchSearchContent(nullptr, 0, nullptr, 0, 0, 1, &count, nullptr, &updateId); }));
    Require(Throws([&] { sceContentSearchOpenMetadata("/data/photo.jpg", &metadataId); }));
    Require(metadataId == -1);
    Require(Throws([] { sceContentSearchCloseMetadata(1); }));
    Require(sceContentSearchTerm() == 0);
    Require(Throws([&] { sceContentSearchSearchContent(nullptr, 0, nullptr, 0, 0, 1, &count, infos, &updateId); }));
}
