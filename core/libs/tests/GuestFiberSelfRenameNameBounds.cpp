#include "prx/libSceFiber/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>

namespace {

constexpr std::int32_t SCE_OK = 0;
constexpr std::int32_t FiberErrorNull = static_cast<std::int32_t>(0x80590001);
constexpr std::int32_t FiberErrorInvalid = static_cast<std::int32_t>(0x80590004);
constexpr std::int32_t FiberErrorPermission = static_cast<std::int32_t>(0x80590005);
constexpr std::int32_t FiberErrorState = static_cast<std::int32_t>(0x80590006);

void Require(bool condition) {
    if (!condition) {
        std::abort();
    }
}

alignas(16) std::uint8_t g_stackA[16384];
alignas(16) std::uint8_t g_stackB[16384];

alignas(8) std::uint8_t g_fiberStorageA[256];
alignas(8) std::uint8_t g_fiberStorageB[256];

FiberObject* const g_fiberA = reinterpret_cast<FiberObject*>(g_fiberStorageA);
FiberObject* const g_fiberB = reinterpret_cast<FiberObject*>(g_fiberStorageB);

void APS5_VABI FiberSelfTestEntry(std::uint64_t, std::uint64_t) {
    FiberObject* self = nullptr;
    Require(sceFiberGetSelf(nullptr) == FiberErrorNull);
    Require(sceFiberGetSelf(&self) == SCE_OK);
    Require(self == g_fiberA);

    Require(sceFiberRename(self, "renamed_from_inside") == SCE_OK);
    FiberInfo info{};
    info.size = sizeof(info);
    Require(sceFiberGetInfo(self, &info) == SCE_OK);
    Require(std::strcmp(info.name, "renamed_from_inside") == 0);

    Require(sceFiberRename(g_fiberB, "renamed_other_from_inside") == SCE_OK);
    FiberInfo infoB{};
    infoB.size = sizeof(infoB);
    Require(sceFiberGetInfo(g_fiberB, &infoB) == SCE_OK);
    Require(std::strcmp(infoB.name, "renamed_other_from_inside") == 0);

    std::uint64_t switchedOut = 0;
    Require(sceFiberSwitch(g_fiberB, 1234, &switchedOut) == SCE_OK);
    Require(switchedOut == 5678);

    self = nullptr;
    Require(sceFiberGetSelf(&self) == SCE_OK);
    Require(self == g_fiberA);

    Require(sceFiberReturnToThread(9999, nullptr) == SCE_OK);
}

void APS5_VABI FiberSwitchSelfEntry(std::uint64_t, std::uint64_t argOnRun) {
    Require(argOnRun == 1234);

    FiberObject* self = nullptr;
    Require(sceFiberGetSelf(&self) == SCE_OK);
    Require(self == g_fiberB);

    FiberInfo info{};
    info.size = sizeof(info);
    Require(sceFiberGetInfo(self, &info) == SCE_OK);
    Require(std::strcmp(info.name, "renamed_other_from_inside") == 0);

    Require(sceFiberSwitch(g_fiberA, 5678, nullptr) == SCE_OK);
    Require(false);
}

void AuditFiberGetSelfLifecycle() {
    FiberObject* self = nullptr;
    Require(sceFiberGetSelf(nullptr) == FiberErrorNull);
    Require(sceFiberGetSelf(&self) == FiberErrorPermission);
    Require(self == nullptr);

    Require(_sceFiberInitializeImpl_nid_postfix(g_fiberA, "fiber_self_a", FiberSelfTestEntry, 0, g_stackA, sizeof(g_stackA), nullptr, 0) == SCE_OK);
    Require(_sceFiberInitializeImpl_nid_postfix(g_fiberB, "fiber_self_b", FiberSwitchSelfEntry, 0, g_stackB, sizeof(g_stackB), nullptr, 0) == SCE_OK);

    Require(sceFiberGetSelf(&self) == FiberErrorPermission);

    std::uint64_t retVal = 0;
    Require(sceFiberRun_nid_postfix(g_fiberA, 0, &retVal) == SCE_OK);
    Require(retVal == 9999);

    Require(sceFiberGetSelf(&self) == FiberErrorPermission);

    Require(sceFiberFinalize(g_fiberA) == SCE_OK);
    Require(sceFiberFinalize(g_fiberB) == SCE_OK);
    Require(sceFiberGetSelf(&self) == FiberErrorPermission);
}

void AuditFiberRenameAndNameBounds() {
    Require(sceFiberRename(nullptr, "test_name") == FiberErrorNull);
    Require(sceFiberRename(g_fiberA, nullptr) == FiberErrorNull);
    Require(sceFiberRename(nullptr, nullptr) == FiberErrorNull);

    alignas(8) std::uint8_t uninitStorage[256] = {};
    auto* uninitFiber = reinterpret_cast<FiberObject*>(uninitStorage);
    Require(sceFiberRename(uninitFiber, "valid_name") == FiberErrorInvalid);

    FiberInfo info{};
    info.size = sizeof(info);
    Require(sceFiberGetInfo(nullptr, &info) == FiberErrorNull);
    Require(sceFiberGetInfo(uninitFiber, nullptr) == FiberErrorNull);
    Require(sceFiberGetInfo(nullptr, nullptr) == FiberErrorNull);
    Require(sceFiberGetInfo(uninitFiber, &info) == FiberErrorInvalid);

    info.size = sizeof(info) - 8;
    Require(_sceFiberInitializeImpl_nid_postfix(g_fiberA, "orig_a", FiberSelfTestEntry, 0, g_stackA, sizeof(g_stackA), nullptr, 0) == SCE_OK);
    Require(sceFiberGetInfo(g_fiberA, &info) == FiberErrorInvalid);
    info.size = sizeof(info) + 8;
    Require(sceFiberGetInfo(g_fiberA, &info) == FiberErrorInvalid);
    info.size = 0;
    Require(sceFiberGetInfo(g_fiberA, &info) == FiberErrorInvalid);
    info.size = sizeof(info);
    Require(sceFiberGetInfo(g_fiberA, &info) == SCE_OK);
    Require(std::strcmp(info.name, "orig_a") == 0);

    errno = 0;
    Require(sceFiberGetInfo(g_fiberA, reinterpret_cast<FiberInfo*>(std::uintptr_t{0x10})) == FiberErrorInvalid);
    Require(errno == 0);

    Require(sceFiberRename(g_fiberA, "") == SCE_OK);
    Require(sceFiberGetInfo(g_fiberA, &info) == SCE_OK);
    Require(info.name[0] == '\0');
    Require(std::strlen(info.name) == 0);

    const char* exact31Name = "1234567890123456789012345678901";
    Require(std::strlen(exact31Name) == 31);
    Require(sceFiberRename(g_fiberA, exact31Name) == SCE_OK);
    Require(sceFiberGetInfo(g_fiberA, &info) == SCE_OK);
    Require(std::strlen(info.name) == 31);
    Require(std::strcmp(info.name, exact31Name) == 0);

    const char* overlongName = "1234567890123456789012345678901_EXTRA_OVERFLOW_CHARACTERS_HERE";
    Require(sceFiberRename(g_fiberA, overlongName) == SCE_OK);
    Require(sceFiberGetInfo(g_fiberA, &info) == SCE_OK);
    Require(std::strlen(info.name) == 31);
    Require(std::strncmp(info.name, overlongName, 31) == 0);
    Require(std::strcmp(info.name, exact31Name) == 0);
    Require(info.name[31] == '\0');

    Require(_sceFiberInitializeImpl_nid_postfix(g_fiberB, "orig_b", FiberSwitchSelfEntry, 0, g_stackB, sizeof(g_stackB), nullptr, 0) == SCE_OK);

    const char* collisionA = "SharedPrefixAcrossMultipleFibers_SuffixAAA";
    const char* collisionB = "SharedPrefixAcrossMultipleFibers_SuffixBBB";
    Require(sceFiberRename(g_fiberA, collisionA) == SCE_OK);
    Require(sceFiberRename(g_fiberB, collisionB) == SCE_OK);

    FiberInfo infoA{};
    infoA.size = sizeof(infoA);
    FiberInfo infoB{};
    infoB.size = sizeof(infoB);
    Require(sceFiberGetInfo(g_fiberA, &infoA) == SCE_OK);
    Require(sceFiberGetInfo(g_fiberB, &infoB) == SCE_OK);

    Require(std::strlen(infoA.name) == 31);
    Require(std::strlen(infoB.name) == 31);
    Require(std::strcmp(infoA.name, infoB.name) == 0);
    Require(std::strncmp(infoA.name, "SharedPrefixAcrossMultipleFiber", 31) == 0);

    Require(sceFiberFinalize(g_fiberA) == SCE_OK);
    Require(sceFiberFinalize(g_fiberB) == SCE_OK);

    Require(sceFiberRename(g_fiberA, "after_finalize") == FiberErrorInvalid);
    Require(sceFiberGetInfo(g_fiberA, &info) == FiberErrorInvalid);
    Require(sceFiberRename(g_fiberB, "after_finalize") == FiberErrorInvalid);
    Require(sceFiberGetInfo(g_fiberB, &info) == FiberErrorInvalid);
}

} // namespace

int main() {
    AuditFiberGetSelfLifecycle();
    AuditFiberRenameAndNameBounds();
    return 0;
}
