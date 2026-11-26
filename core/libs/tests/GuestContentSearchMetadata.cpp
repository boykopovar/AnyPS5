#include "prx/libSceContentSearch/include/Api.hpp"
#include "prx/libSceContentSearch/include/InventoryExports.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include <algorithm>
#include <array>
#include <cerrno>
#include <cstdint>
#include <cstdlib>
#include <stdexcept>

namespace {

void ClobberErrno(std::uintptr_t, std::size_t) {
    errno = 99;
}

void Require(bool value) {
    if (!value) std::abort();
}

template <typename TAction>
bool Throws(TAction action) {
    try {
        action();
    } catch (const std::runtime_error&) {
        return true;
    }
    return false;
}

constexpr int ErrorNotInitialized = static_cast<int>(0x809D1001u);
constexpr int ErrorInvalidArgument = static_cast<int>(0x809D1003u);
constexpr int ErrorAvEventState = static_cast<int>(0x809D1014u);
constexpr int ErrorServiceUnavailable = static_cast<int>(0x809D0002u);

}

int main() {
    GuestArena::GuestArenaSetHostReadHook_nid_no_patch(ClobberErrno);
    GuestArena::GuestArenaSetHostWriteHook_nid_no_patch(ClobberErrno);

    ContentSearchInitParam param{3 * 1024 * 1024};
    Require(sceContentSearchInit(&param) == 0);

    std::int32_t appIndex = -1;
    Require(sceContentSearchGetMyApplicationIndex(&appIndex) == ErrorServiceUnavailable);
    Require(appIndex == 0);

    Require(sceContentSearchEnableAvContentCreatedEvent() == ErrorAvEventState);
    Require(sceContentSearchAddAvContentCreatedEventCondition(1, 1) == 0);
    Require(sceContentSearchEnableAvContentCreatedEvent() == ErrorServiceUnavailable);

    std::int32_t metadataId = -1;
    Require(sceContentSearchOpenMetadataByContentId(1, &metadataId) == ErrorInvalidArgument);
    Require(metadataId == -1);

    std::int32_t metaType = -1;
    std::int32_t metaSize = -1;
    std::array<unsigned char, 64> valueBuf{};
    valueBuf.fill(0xA5);

    for (const std::int32_t id : {-1, 16}) {
        for (const char* field : {"title", "hdr_type", "timestamp", "timeline_tag", "constant_tag"}) {
            Require(sceContentSearchGetMetadataFieldInfo(id, field, &metaType, &metaSize) == ErrorInvalidArgument);
            Require(sceContentSearchGetMetadataValue(id, field, valueBuf.data()) == ErrorInvalidArgument);
        }
    }

    for (const std::int32_t id : {0, 15}) {
        for (const char* field : {"title", "hdr_type"}) {
            Require(sceContentSearchGetMetadataFieldInfo(id, field, &metaType, &metaSize) == ErrorInvalidArgument);
            Require(sceContentSearchGetMetadataValue(id, field, valueBuf.data()) == ErrorInvalidArgument);
        }
        for (const char* field : {"timestamp", "timeline_tag", "constant_tag"}) {
            Require(Throws([&] { sceContentSearchGetMetadataFieldInfo(id, field, &metaType, &metaSize); }));
            Require(Throws([&] { sceContentSearchGetMetadataValue(id, field, valueBuf.data()); }));
        }
    }
    Require(metaType == -1 && metaSize == -1);
    Require(std::all_of(valueBuf.begin(), valueBuf.end(), [](unsigned char byte) { return byte == 0xA5; }));

    Require(sceContentSearchTerm() == 0);
    Require(sceContentSearchGetMyApplicationIndex(&appIndex) == ErrorNotInitialized);
    Require(sceContentSearchEnableAvContentCreatedEvent() == ErrorNotInitialized);
    Require(sceContentSearchOpenMetadataByContentId(1, &metadataId) == ErrorNotInitialized);
    Require(sceContentSearchGetMetadataFieldInfo(0, "title", &metaType, &metaSize) == ErrorNotInitialized);
    Require(sceContentSearchGetMetadataValue(0, "title", valueBuf.data()) == ErrorNotInitialized);

    Require(sceContentSearchInit(&param) == 0);
    Require(sceContentSearchEnableAvContentCreatedEvent() == ErrorAvEventState);
    Require(sceContentSearchTerm() == 0);

    GuestArena::GuestArenaSetHostReadHook_nid_no_patch(nullptr);
    GuestArena::GuestArenaSetHostWriteHook_nid_no_patch(nullptr);
    return 0;
}
