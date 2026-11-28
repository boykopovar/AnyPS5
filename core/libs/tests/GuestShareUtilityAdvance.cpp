#include "prx/libSceShareUtility/libSceShareUtility.hpp"

#include <cerrno>
#include <cstdint>
#include <cstdlib>
#include <initializer_list>
#include <stdexcept>
#include <string>

namespace {

constexpr int AdvanceResult = static_cast<int>(0x81170001u);

using Query = int (APS5_VABI *)();

template <typename TCall>
void RequireNotImplemented(TCall call, const std::string& name) {
    try {
        call();
    } catch (const std::runtime_error& error) {
        if (std::string(error.what()) != name + " not implemented") std::abort();
        return;
    }
    std::abort();
}

void CheckAnswersBeforeInitialization() {
    constexpr int invalidArgument = static_cast<int>(0x81170003u);
    constexpr int invalidSize = static_cast<int>(0x81170004u);
    constexpr int notInitialized = static_cast<int>(0x81170102u);
    unsigned char block[16]{};
    for (const std::uint64_t mode : {0ull, 2ull, 0x100000001ull}) {
        if (sceShareUtilityInitialize(mode, 0x20000) != invalidArgument) std::abort();
        if (sceShareUtilityInitializeEx2(mode, 0x20000, 0x200, block) != invalidArgument) std::abort();
    }
    for (const std::uint64_t mode : {0ull, 1ull, 0x100000002ull}) {
        if (sceShareUtilityAdvanceInitialize(mode, 0x20000, 0x200, block) != invalidArgument) std::abort();
    }
    for (const std::uint64_t size : {0ull, 0x1ffffull}) {
        if (sceShareUtilityInitialize(1, size) != invalidArgument) std::abort();
        if (sceShareUtilityInitializeEx2(1, size, 0x200, block) != invalidArgument) std::abort();
        if (sceShareUtilityAdvanceInitialize(2, size, 0x200, block) != invalidArgument) std::abort();
    }
    if (sceShareUtilityAdvanceSetUploadContentData(nullptr, 1) != invalidArgument) std::abort();
    if (sceShareUtilityAdvanceSetUploadContentData(block, 0) != invalidArgument) std::abort();
    if (sceShareUtilityAdvanceSetUploadContentData(nullptr, 0x2000) != invalidArgument) std::abort();
    for (const std::uint64_t size : {0x2000ull, 0xffffffffffffffffull}) {
        if (sceShareUtilityAdvanceSetUploadContentData(block, size) != invalidSize) std::abort();
    }
    if (sceShareUtilityAdvanceSetUploadContentData(block, 1) != notInitialized) std::abort();
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

void CheckLifecycle() {
    constexpr int invalidArgument = static_cast<int>(0x81170003u);
    constexpr int invalidSize = static_cast<int>(0x81170004u);
    constexpr int alreadyInitialized = static_cast<int>(0x81170101u);
    constexpr int notInitialized = static_cast<int>(0x81170102u);
    unsigned char block[16]{1, 2, 3, 4};

    if (sceShareUtilityInitializeEx2(1, 0x20000, 0xff, block) != invalidArgument) std::abort();
    if (sceShareUtilityInitializeEx2(1, 0x20000, 0x300, block) != invalidArgument) std::abort();
    if (sceShareUtilityInitializeEx2(1, 0x20000, 0x200, nullptr) != invalidArgument) std::abort();
    if (sceShareUtilityTerminate() != notInitialized) std::abort();

    if (sceShareUtilityInitialize(1, 0x20000) != 0) std::abort();
    if (sceShareUtilityInitialize(1, 0x20000) != alreadyInitialized) std::abort();
    if (sceShareUtilityInitializeEx2(1, 0x20000, 0x200, block) != alreadyInitialized) std::abort();
    if (sceShareUtilityOpenShareMenu(-1, 1) != invalidArgument) std::abort();
    if (sceShareUtilityOpenShareMenu(0, 0) != invalidArgument) std::abort();
    if (sceShareUtilityOpenShareMenuForShareContent(nullptr, 1) != invalidArgument) std::abort();
    if (sceShareUtilityOpenShareMenuForShareContent(block, 0x1000) != invalidSize) std::abort();
    RequireNotImplemented([] { return sceShareUtilityOpenShareMenuBroadcast(); }, "sceShareUtilityOpenShareMenuBroadcast");
    RequireNotImplemented([] { return sceShareUtilityOpenShareMenuDefault(); }, "sceShareUtilityOpenShareMenuDefault");
    RequireNotImplemented([] { return sceShareUtilityOpenShareMenu(0, 1); }, "sceShareUtilityOpenShareMenu");
    RequireNotImplemented([&] { return sceShareUtilityOpenShareMenuForShareContent(block, sizeof(block)); }, "sceShareUtilityOpenShareMenuForShareContent");
    if (sceShareUtilityTerminate() != 0) std::abort();
    if (sceShareUtilityTerminate() != notInitialized) std::abort();
    if (sceShareUtilityOpenShareMenuDefault() != notInitialized) std::abort();

    if (sceShareUtilityInitializeEx2(1, 0x20000, 0x100, block) != 0) std::abort();
    if (sceShareUtilityTerminate() != 0) std::abort();

    if (sceShareUtilityAdvanceInitialize(2, 0x20000, 0xff, block) != invalidArgument) std::abort();
    if (sceShareUtilityAdvanceInitialize(2, 0x20000, 0x200, nullptr) != invalidArgument) std::abort();
    if (sceShareUtilityAdvanceTerminate() != notInitialized) std::abort();

    if (sceShareUtilityAdvanceInitialize(2, 0x20000, 0x200, block) != 0) std::abort();
    if (sceShareUtilityAdvanceInitialize(2, 0x20000, 0x200, block) != alreadyInitialized) std::abort();
    if (sceShareUtilityAdvanceSetPlayingContent(42) != 0) std::abort();
    if (sceShareUtilityAdvanceResetPlayingContent() != 0) std::abort();
    errno = 71;
    if (sceShareUtilityAdvanceSetUploadContentData(reinterpret_cast<const void*>(0x10), 4) != invalidArgument) std::abort();
    if (errno != 71) std::abort();
    if (sceShareUtilityAdvanceSetUploadContentData(block, sizeof(block)) != 0) std::abort();
    if (sceShareUtilityAdvanceWakeUpShareMenu(-1, 2) != invalidArgument) std::abort();
    if (sceShareUtilityAdvanceWakeUpShareMenu(42, 0) != invalidArgument) std::abort();
    if (sceShareUtilityAdvanceWakeUpShareMenuForShareContent(nullptr, 1) != invalidArgument) std::abort();
    if (sceShareUtilityAdvanceWakeUpShareMenuForShareContent(block, 0x1000) != invalidSize) std::abort();
    RequireNotImplemented([] { return sceShareUtilityAdvanceWakeUpShareMenuBroadcast(); }, "sceShareUtilityAdvanceWakeUpShareMenuBroadcast");
    RequireNotImplemented([] { return sceShareUtilityAdvanceWakeUpShareMenuDefault(); }, "sceShareUtilityAdvanceWakeUpShareMenuDefault");
    RequireNotImplemented([] { return sceShareUtilityAdvanceWakeUpShareMenu(42, 2); }, "sceShareUtilityAdvanceWakeUpShareMenu");
    RequireNotImplemented([&] { return sceShareUtilityAdvanceWakeUpShareMenuForShareContent(block, sizeof(block)); }, "sceShareUtilityAdvanceWakeUpShareMenuForShareContent");
    if (sceShareUtilityAdvanceTerminate() != 0) std::abort();
    if (sceShareUtilityAdvanceTerminate() != notInitialized) std::abort();
    if (sceShareUtilityAdvanceWakeUpShareMenuDefault() != notInitialized) std::abort();
    if (sceShareUtilityAdvanceSetPlayingContent(42) != notInitialized) std::abort();
    if (sceShareUtilityAdvanceSetUploadContentData(block, 1) != notInitialized) std::abort();
}

}

int main() {
    CheckAnswersBeforeInitialization();
    CheckLifecycle();
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
