#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

struct AddcontInfo {
    NpUnifiedEntitlementLabel label;
    uint32_t status;
};
static_assert(sizeof(AddcontInfo) == 24);

extern "C" {
int APS5_VABI sceAppContentAddcontMount(uint32_t, const NpUnifiedEntitlementLabel*, AppContentMountPoint*);
int APS5_VABI sceAppContentAddcontUnmount(const AppContentMountPoint*);
int APS5_VABI sceAppContentGetAddcontInfo(uint32_t, const NpUnifiedEntitlementLabel*, AddcontInfo*);
int APS5_VABI sceAppContentGetAddcontInfoList(uint32_t, AddcontInfo*, uint32_t, uint32_t*);
int APS5_VABI sceNpEntitlementAccessGetAddcontEntitlementInfo(uint32_t, const NpUnifiedEntitlementLabel*, NpEntitlementAccessAddcontEntitlementInfo*);
int APS5_VABI sceNpEntitlementAccessGetAddcontEntitlementInfoList(uint32_t, NpEntitlementAccessAddcontEntitlementInfo*, uint32_t, uint32_t*);
}

static constexpr int ErrorBusy = static_cast<int>(0x80D90003);
static constexpr int ErrorNotMounted = static_cast<int>(0x80D90004);
static constexpr int ErrorNotFound = static_cast<int>(0x80D90005);
static constexpr int ErrorMountFull = static_cast<int>(0x80D90006);
static constexpr int ErrorDrmNoEntitlement = static_cast<int>(0x80D90007);
static constexpr int ErrorEntitlementNotFound = static_cast<int>(0x80558007);
static constexpr uint32_t StatusNoExtraData = 0;
static constexpr uint32_t StatusInstalled = 4;
static constexpr uint32_t ExtraMounts = 63;
static constexpr uint32_t EntryCount = 3 + ExtraMounts;

static void Require(bool value, const char* message) {
    if (!value) {
        std::fprintf(stderr, "addcont: %s\n", message);
        std::abort();
    }
}

static void SetEnvironment(const char* name, const std::filesystem::path& value) {
#ifdef _WIN32
    Require(_putenv_s(name, value.string().c_str()) == 0, "set environment");
#else
    Require(::setenv(name, value.string().c_str(), 1) == 0, "set environment");
#endif
}

static void WriteFile(const std::filesystem::path& path, const std::string& text) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream file(path, std::ios::binary);
    file << text;
    Require(static_cast<bool>(file), "write fixture");
}

static std::string ReadFile(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
}

static bool Unavailable(const char* path) {
    try {
        ResolvePath_nid_no_patch(path);
    } catch (const std::filesystem::filesystem_error&) {
        return true;
    }
    return false;
}

static NpUnifiedEntitlementLabel Label(const std::string& text) {
    NpUnifiedEntitlementLabel label{};
    std::memcpy(label.data, text.data(), text.size());
    return label;
}

static std::string MountLabel(uint32_t index) {
    const auto digits = std::to_string(index);
    return "MOUNT" + std::string(11 - digits.size(), '0') + digits;
}

static void CheckInvalidDirectory(const std::filesystem::path& invalid) {
    uint32_t hitNum = 0;
    bool thrown = false;
    try {
        sceAppContentGetAddcontInfoList(0, nullptr, 0, &hitNum);
    } catch (const std::runtime_error&) {
        thrown = true;
    }
    Require(thrown, "a directory that is not an entitlement label throws");
    std::filesystem::remove_all(invalid);
}

static void CheckInfo() {
    uint32_t hitNum = 0;
    Require(sceAppContentGetAddcontInfoList(0, nullptr, 0, &hitNum) == 0 && hitNum == EntryCount, "count");
    std::vector<AddcontInfo> list(EntryCount + 2);
    std::memset(list.data(), 0x5a, list.size() * sizeof(AddcontInfo));
    Require(sceAppContentGetAddcontInfoList(0, list.data(), 2, &hitNum) == 0 && hitNum == 2, "short list count");
    Require(std::strcmp(list[0].label.data, "LICENSEONLY00001") == 0 && list[0].status == StatusNoExtraData, "license-only entry first");
    Require(std::strcmp(list[1].label.data, "ADDCONT000000001") == 0 && list[1].status == StatusInstalled, "ini entry with data");
    Require(list[2].status == 0x5a5a5a5a, "short list stops at its size");
    Require(sceAppContentGetAddcontInfoList(0, list.data(), static_cast<uint32_t>(list.size()), &hitNum) == 0 && hitNum == EntryCount, "full list count");
    Require(std::strcmp(list[2].label.data, "ADDCONT000000002") == 0 && list[2].status == StatusInstalled, "content ID directory");
    Require(std::strcmp(list[3].label.data, MountLabel(0).c_str()) == 0, "directories sorted by label");
    Require(list[EntryCount].status == 0x5a5a5a5a, "full list writes only the entries");
    Require(list[0].label.padding[0] == 0 && list[0].label.data[16] == 0, "label is terminated and padded with zeros");

    AddcontInfo info{};
    auto label = Label("ADDCONT000000002");
    Require(sceAppContentGetAddcontInfo(0, &label, &info) == 0, "info");
    Require(std::strcmp(info.label.data, "ADDCONT000000002") == 0 && info.status == StatusInstalled, "info content");
    label = Label("LICENSEONLY00001");
    Require(sceAppContentGetAddcontInfo(0, &label, &info) == 0 && info.status == StatusNoExtraData, "license-only info");
    label = Label("UNKNOWN000000001");
    Require(sceAppContentGetAddcontInfo(0, &label, &info) == ErrorDrmNoEntitlement, "unknown info");
}

static void CheckEntitlements() {
    uint32_t hitNum = 0;
    std::vector<NpEntitlementAccessAddcontEntitlementInfo> list(EntryCount);
    Require(sceNpEntitlementAccessGetAddcontEntitlementInfoList(0, list.data(), EntryCount, &hitNum) == 0 && hitNum == EntryCount, "entitlement list");
    Require(std::strcmp(list[0].entitlement_label.data, "LICENSEONLY00001") == 0 && list[0].package_type == 3, "license-only package type");
    Require(list[1].package_type == 2 && list[2].package_type == 2 && list[1].download_status == 4, "add-on with data package type");
    NpEntitlementAccessAddcontEntitlementInfo info{};
    auto label = Label("ADDCONT000000002");
    Require(sceNpEntitlementAccessGetAddcontEntitlementInfo(0, &label, &info) == 0 && info.package_type == 2, "entitlement info");
    label = Label("UNKNOWN000000001");
    Require(sceNpEntitlementAccessGetAddcontEntitlementInfo(0, &label, &info) == ErrorEntitlementNotFound, "unknown entitlement");
}

static void CheckMounts(const std::filesystem::path& addcont) {
    AppContentMountPoint first{};
    auto label = Label("ADDCONT000000001");
    Require(sceAppContentAddcontMount(0, &label, &first) == 0, "mount");
    Require(std::strcmp(first.data, "/addcont0") == 0, "first mount point");
    Require(ResolvePath_nid_no_patch("/addcont0/data.txt") == (addcont / "ADDCONT000000001" / "data.txt").make_preferred(), "mount point resolves to the directory");
    Require(ReadFile(ResolvePath_nid_no_patch("/addcont0/data.txt")) == "first", "mounted file");
    AppContentMountPoint again{};
    Require(sceAppContentAddcontMount(0, &label, &again) == ErrorBusy, "second mount of the same add-on");

    AppContentMountPoint second{};
    label = Label("ADDCONT000000002");
    Require(sceAppContentAddcontMount(0, &label, &second) == 0 && std::strcmp(second.data, "/addcont1") == 0, "second mount point");
    Require(ReadFile(ResolvePath_nid_no_patch("/addcont1/data.txt")) == "second", "content ID directory file");

    AppContentMountPoint untouched{};
    std::memset(&untouched, 0x5a, sizeof(untouched));
    auto unchanged = untouched;
    label = Label("LICENSEONLY00001");
    Require(sceAppContentAddcontMount(0, &label, &unchanged) == ErrorNotFound, "license-only add-on has no data to mount");
    label = Label("UNKNOWN000000001");
    Require(sceAppContentAddcontMount(0, &label, &unchanged) == ErrorNotFound, "unknown add-on");
    Require(std::memcmp(&unchanged, &untouched, sizeof(unchanged)) == 0, "failed mount leaves the mount point");

    Require(sceAppContentAddcontUnmount(&first) == 0, "unmount");
    Require(Unavailable("/addcont0/data.txt"), "unmounted point is unavailable");
    Require(Unavailable("/addcont9"), "free slot is unavailable");
    Require(sceAppContentAddcontUnmount(&first) == ErrorNotMounted, "second unmount");
    AppContentMountPoint other{};
    std::memcpy(other.data, "/addcont9", 10);
    Require(sceAppContentAddcontUnmount(&other) == ErrorNotMounted, "unmount of a free slot");

    label = Label("ADDCONT000000001");
    Require(sceAppContentAddcontMount(0, &label, &first) == 0 && std::strcmp(first.data, "/addcont0") == 0, "remount takes the lowest free slot");

    std::vector<AppContentMountPoint> mounts(ExtraMounts);
    for (uint32_t index = 0; index + 1 < ExtraMounts; ++index) {
        label = Label(MountLabel(index));
        Require(sceAppContentAddcontMount(0, &label, &mounts[index]) == 0, "mount up to the limit");
    }
    Require(std::strcmp(mounts[ExtraMounts - 2].data, "/addcont63") == 0, "last mount point");
    label = Label(MountLabel(ExtraMounts - 1));
    Require(sceAppContentAddcontMount(0, &label, &mounts[ExtraMounts - 1]) == ErrorMountFull, "65th mount");
    Require(sceAppContentAddcontUnmount(&mounts[0]) == 0, "unmount below the limit");
    Require(sceAppContentAddcontMount(0, &label, &mounts[ExtraMounts - 1]) == 0, "mount after a slot is freed");
    Require(std::strcmp(mounts[ExtraMounts - 1].data, mounts[0].data) == 0, "freed slot is reused");
}

int main() {
    const auto root = std::filesystem::temp_directory_path() / ("anyps5_guest_addcont-" + std::to_string(std::random_device{}()));
    const auto addcont = root / "addcont";
    WriteFile(addcont / "ADDCONT000000001" / "data.txt", "first");
    WriteFile(addcont / "UP0000-PPSA00000_00-ADDCONT000000002" / "data.txt", "second");
    WriteFile(addcont / "readme.txt", "not an add-on");
    for (uint32_t index = 0; index < ExtraMounts; ++index) std::filesystem::create_directories(addcont / MountLabel(index));
    std::filesystem::create_directories(addcont / "not-a-label");
    WriteFile(root / "entitlements.ini", "# owned add-ons\nLICENSEONLY00001\n  ADDCONT000000001 ; also has data\n");
    SetEnvironment("ANYPS5_ADDCONT", addcont);
    SetEnvironment("ANYPS5_ENTITLEMENTS", root / "entitlements.ini");

    CheckInvalidDirectory(addcont / "not-a-label");
    CheckInfo();
    CheckEntitlements();
    CheckMounts(addcont);

    std::error_code error;
    std::filesystem::remove_all(root, error);
}
