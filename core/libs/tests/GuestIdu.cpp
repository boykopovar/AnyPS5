#include "prx/libSceIdu/libSceIdu.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <stdexcept>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace {

constexpr int invalidArgument = static_cast<int>(0xFFFFFF99u);
constexpr int notInitialized = static_cast<int>(0xFFFFFF9Au);

void Require(bool value) {
    if (!value) std::abort();
}

std::array<unsigned char, 0x40> Block(std::uint32_t firstWord) {
    std::array<unsigned char, 0x40> block{};
    block.fill(0x5A);
    std::memcpy(block.data(), &firstWord, sizeof(firstWord));
    return block;
}

char* UnreadablePage() {
#ifdef _WIN32
    SYSTEM_INFO info{};
    GetSystemInfo(&info);
    auto* page = static_cast<char*>(VirtualAlloc(nullptr, info.dwPageSize, MEM_RESERVE | MEM_COMMIT, PAGE_NOACCESS));
    Require(page != nullptr);
    return page;
#else
    const long pageSize = sysconf(_SC_PAGESIZE);
    Require(pageSize > 0);
    void* page = mmap(nullptr, static_cast<std::size_t>(pageSize), PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    Require(page != MAP_FAILED);
    return static_cast<char*>(page);
#endif
}

template <typename Call>
void RequireFault(Call call) {
    bool threw = false;
    try {
        static_cast<void>(call());
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);
}

} // namespace

int main() {
    char name[] = "name";
    char empty[] = "";
    auto any = Block(0x5A5A5A5Au);
    auto initWrong = Block(9);
    auto fsStats = Block(0x18);
    auto fsStatsWrong = Block(0x17);
    auto startParam = Block(0xC);
    auto startParamWrong = Block(0xD);
    const auto anyBefore = any;
    const auto fsStatsBefore = fsStats;
    const auto startParamBefore = startParam;

    for (int call = 0; call < 2; ++call) {
        Require(sceIduGetEnv() == notInitialized);
        Require(sceIduIsStaffMode() == notInitialized);
        Require(sceIduReboot() == notInitialized);
        Require(libSceIduUnknown_3XKI6v75VEo_nid_no_patch() == notInitialized);
        Require(libSceIduUnknown_J9y431a1uM0_nid_no_patch() == notInitialized);
        Require(libSceIduUnknown_tmeq5nZRqIs_nid_no_patch() == notInitialized);

        Require(sceIduInit(nullptr) == invalidArgument);
        Require(sceIduInit(initWrong.data()) == invalidArgument);
        Require(sceIduInit(any.data()) == invalidArgument);

        Require(sceIduGetHardwareId(nullptr) == invalidArgument);
        Require(sceIduGetHardwareId(any.data()) == notInitialized);
        Require(libSceIduUnknown_uoHZCJW8Cdk_nid_no_patch(nullptr) == invalidArgument);
        Require(libSceIduUnknown_uoHZCJW8Cdk_nid_no_patch(any.data()) == notInitialized);

        Require(sceIduInstallPkg(nullptr, any.data()) == invalidArgument);
        Require(sceIduInstallPkg(any.data(), nullptr) == invalidArgument);
        Require(sceIduInstallPkg(nullptr, nullptr) == invalidArgument);
        Require(sceIduInstallPkg(any.data(), any.data()) == notInitialized);

        Require(sceIduGetTitleList(nullptr, 1) == invalidArgument);
        Require(sceIduGetTitleList(nullptr, -1) == invalidArgument);
        Require(sceIduGetTitleList(any.data(), 0) == invalidArgument);
        Require(sceIduGetTitleList(nullptr, 0) == notInitialized);
        Require(sceIduGetTitleList(any.data(), 1) == notInitialized);
        Require(sceIduGetTitleList(any.data(), -1) == notInitialized);

        Require(sceIduGetDiscTitleInfo(nullptr, 1) == invalidArgument);
        Require(sceIduGetDiscTitleInfo(nullptr, UINT32_MAX) == invalidArgument);
        Require(sceIduGetDiscTitleInfo(any.data(), 0) == invalidArgument);
        Require(sceIduGetDiscTitleInfo(nullptr, 0) == notInitialized);
        Require(sceIduGetDiscTitleInfo(any.data(), UINT32_MAX) == notInitialized);

        Require(sceIduGetFsStats(nullptr) == invalidArgument);
        Require(sceIduGetFsStats(fsStatsWrong.data()) == invalidArgument);
        Require(sceIduGetFsStats(fsStats.data()) == notInitialized);

        Require(sceIduNotificationHide(nullptr) == invalidArgument);
        Require(sceIduNotificationHide(empty) == invalidArgument);
        Require(sceIduNotificationHide(name) == notInitialized);
        Require(sceIduIsRifInstalled(nullptr) == invalidArgument);
        Require(sceIduIsRifInstalled(empty) == invalidArgument);
        Require(sceIduIsRifInstalled(name) == notInitialized);

        Require(sceIduInstallRif(nullptr, any.data(), 1) == invalidArgument);
        Require(sceIduInstallRif(empty, any.data(), 1) == invalidArgument);
        Require(sceIduInstallRif(name, nullptr, 1) == invalidArgument);
        Require(sceIduInstallRif(name, any.data(), 0) == invalidArgument);
        Require(sceIduInstallRif(name, any.data(), 1) == notInitialized);

        Require(sceIduStartTitle(nullptr, 0, nullptr, startParam.data()) == invalidArgument);
        Require(sceIduStartTitle(any.data(), 1, nullptr, startParam.data()) == invalidArgument);
        Require(sceIduStartTitle(any.data(), 0, any.data(), startParam.data()) == invalidArgument);
        Require(sceIduStartTitle(any.data(), -1, any.data(), startParam.data()) == invalidArgument);
        Require(sceIduStartTitle(any.data(), 0, nullptr, nullptr) == invalidArgument);
        Require(sceIduStartTitle(any.data(), 0, nullptr, startParamWrong.data()) == invalidArgument);
        Require(sceIduStartTitle(any.data(), 0, nullptr, startParam.data()) == notInitialized);
        Require(sceIduStartTitle(any.data(), -1, nullptr, startParam.data()) == notInitialized);
        Require(sceIduStartTitle(any.data(), 2, any.data(), startParam.data()) == notInitialized);
    }

    char* unreadable = UnreadablePage();
    RequireFault([&] { return sceIduInit(unreadable); });
    RequireFault([&] { return sceIduGetFsStats(unreadable); });
    RequireFault([&] { return sceIduNotificationHide(unreadable); });
    RequireFault([&] { return sceIduIsRifInstalled(unreadable); });
    RequireFault([&] { return sceIduInstallRif(unreadable, any.data(), 1); });
    RequireFault([&] { return sceIduStartTitle(any.data(), 0, nullptr, unreadable); });
    Require(sceIduInstallRif(name, unreadable, 1) == notInitialized);
    Require(sceIduStartTitle(unreadable, 0, nullptr, startParam.data()) == notInitialized);
    Require(sceIduStartTitle(any.data(), 1, unreadable, startParam.data()) == notInitialized);
    Require(sceIduGetHardwareId(unreadable) == notInitialized);

    Require(any == anyBefore);
    Require(fsStats == fsStatsBefore);
    Require(startParam == startParamBefore);
    return 0;
}
