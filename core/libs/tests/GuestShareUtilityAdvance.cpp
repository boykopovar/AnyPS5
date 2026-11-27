#include "prx/libSceShareUtility/libSceShareUtility.hpp"

#include <cstdint>
#include <cstdlib>
#include <initializer_list>

namespace {

constexpr int AdvanceResult = static_cast<int>(0x81170001u);

using Query = int (APS5_VABI *)();

void CheckAnswersBeforeInitialization() {
    constexpr int invalidArgument = static_cast<int>(0x81170003u);
    constexpr int invalidSize = static_cast<int>(0x81170004u);
    constexpr int notInitialized = static_cast<int>(0x81170102u);
    unsigned char block[16]{};
    for (const std::uint64_t mode : {0ull, 2ull, 0x100000001ull}) {
        if (sceShareUtilityInitialize(mode, 0x20000) != invalidArgument) std::abort();
        if (sceShareUtilityInitializeEx2(mode, 0x20000) != invalidArgument) std::abort();
    }
    for (const std::uint64_t mode : {0ull, 1ull, 0x100000002ull}) {
        if (sceShareUtilityAdvanceInitialize(mode, 0x20000) != invalidArgument) std::abort();
    }
    for (const std::uint64_t size : {0ull, 0x1ffffull}) {
        if (sceShareUtilityInitialize(1, size) != invalidArgument) std::abort();
        if (sceShareUtilityInitializeEx2(1, size) != invalidArgument) std::abort();
        if (sceShareUtilityAdvanceInitialize(2, size) != invalidArgument) std::abort();
    }
    if (sceShareUtilityAdvanceSetUploadContentData(nullptr, 1) != invalidArgument) std::abort();
    if (sceShareUtilityAdvanceSetUploadContentData(block, 0) != invalidArgument) std::abort();
    if (sceShareUtilityAdvanceSetUploadContentData(nullptr, 0x2000) != invalidArgument) std::abort();
    for (const std::uint64_t size : {0x2000ull, 0xffffffffffffffffull}) {
        if (sceShareUtilityAdvanceSetUploadContentData(block, size) != invalidSize) std::abort();
    }
    for (const Query query : std::initializer_list<Query>{
             sceShareUtilityTerminate,
             sceShareUtilityAdvanceTerminate,
             sceShareUtilityAdvanceResetPlayingContent,
             sceShareUtilityAdvanceWakeUpShareMenuBroadcast,
             sceShareUtilityAdvanceWakeUpShareMenuDefault,
             sceShareUtilityOpenShareMenuBroadcast,
             sceShareUtilityOpenShareMenuDefault,
         }) {
        if (query() != notInitialized) std::abort();
    }
    for (const std::int64_t content : {0ll, 1ll, 0x7fffffffffffffffll}) {
        if (sceShareUtilityAdvanceSetPlayingContent(content) != notInitialized) std::abort();
    }
    if (sceShareUtilityAdvanceSetPlayingContent(-1) != invalidArgument) std::abort();
    using Menu = int (APS5_VABI *)(std::int64_t, std::uint32_t);
    using Share = int (APS5_VABI *)(const void*, std::uint64_t);
    for (const Menu menu : std::initializer_list<Menu>{sceShareUtilityAdvanceWakeUpShareMenu, sceShareUtilityOpenShareMenu}) {
        for (const std::uint32_t type : {1u, 2u, 3u, 4u}) {
            if (menu(0, type) != notInitialized) std::abort();
            if (menu(-1, type) != invalidArgument) std::abort();
        }
        for (const std::uint32_t type : {0u, 5u, 0xffffffffu}) {
            if (menu(0, type) != invalidArgument) std::abort();
        }
    }
    for (const Share share : std::initializer_list<Share>{sceShareUtilityAdvanceWakeUpShareMenuForShareContent,
                                                          sceShareUtilityOpenShareMenuForShareContent}) {
        if (share(nullptr, 1) != invalidArgument) std::abort();
        if (share(block, 0) != invalidArgument) std::abort();
        if (share(nullptr, 0x1000) != invalidArgument) std::abort();
        if (share(block, 0x1000) != invalidSize) std::abort();
        if (share(block, 1) != notInitialized) std::abort();
        if (share(block, 0xfff) != notInitialized) std::abort();
    }
}

} // namespace

int main() {
    CheckAnswersBeforeInitialization();
    for (const Query query : std::initializer_list<Query>{
             sceShareUtilityAdvanceFromFileWithContentIdList,
             sceShareUtilityAdvanceFromFileWithTitleIdList,
             sceShareUtilityAdvanceGetServiceInfo,
             sceShareUtilityAdvanceQueryServiceInfo,
             sceShareUtilityAdvanceQueryServiceInfoA,
             sceShareUtilityAdvanceValidateContents,
         }) {
        if (query() != AdvanceResult) std::abort();
    }
}
