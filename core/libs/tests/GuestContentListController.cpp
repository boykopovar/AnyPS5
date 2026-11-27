#include "GuardedBuffer.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceContentListController/include/ContentListController.hpp"
#include "prx/libSceUsbStorage/UsbStorage.hpp"

#include <algorithm>
#include <array>
#include <cerrno>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <random>
#include <string>
#include <vector>

namespace {

constexpr int kErrNoCallback = static_cast<int>(0x81a30001u);
constexpr int kErrInvalidArg = static_cast<int>(0x81a30003u);
constexpr int kErrDeviceNotFound = static_cast<int>(0x81a30004u);
constexpr int kErrUnsupportedFormat = static_cast<int>(0x81a30007u);

void Require(bool value) {
    if (!value) std::abort();
}

std::filesystem::path g_usbRoot;
int g_mountCount = 0;
int g_unmountCount = 0;
int g_unmapResult = 0;
std::int32_t g_getdentsReadStep = 0;

struct RegisteredHook {
    std::int32_t eventType = 0;
    UsbStorageCallback callback = nullptr;
    void* userData = nullptr;
};

std::vector<RegisteredHook> g_hooks;
std::vector<std::pair<SceCLCEvent, void*>> g_receivedEvents;
int g_userTag = 0x77;

void APS5_VABI OnClcEvent(const SceCLCEvent* event, void* userData) {
    g_receivedEvents.push_back({*event, userData});
}

std::int32_t APS5_VABI MockGetDeviceList(std::uint32_t* devices, std::int32_t* count) {
    devices[0] = 7;
    *count = 1;
    return 0;
}

std::int32_t APS5_VABI MockTwoDevices(std::uint32_t* devices, std::int32_t* count) {
    devices[0] = 7;
    devices[1] = 8;
    *count = 2;
    return 0;
}

std::filesystem::path CreateUniqueTempDir() {
    const auto base = std::filesystem::temp_directory_path();
    const auto tick = static_cast<unsigned long long>(std::chrono::steady_clock::now().time_since_epoch().count());
    const unsigned long long rnd = static_cast<unsigned long long>(std::random_device{}());
    for (unsigned attempt = 0; attempt < 64; ++attempt) {
        const auto candidate = base / ("anyps5_clc_test_" + std::to_string(tick) + "_" + std::to_string(rnd) + "_" + std::to_string(attempt));
        std::error_code ec;
        if (std::filesystem::create_directory(candidate, ec) && !ec) {
            return candidate;
        }
    }
    std::abort();
}

struct TempDirGuard {
    std::filesystem::path path;
    ~TempDirGuard() {
        if (!path.empty()) {
            std::error_code ec;
            std::filesystem::remove_all(path, ec);
        }
    }
};

std::int32_t APS5_VABI MockGetDeviceInfo(std::uint32_t deviceId, UsbStorageDeviceInfo* info) {
    if (deviceId != 7) return static_cast<int>(0x80f40004u);
    std::memset(info, 0, sizeof(*info));
    std::strcpy(info->vendor, "MockVendor");
    info->vendorLength = 10;
    std::strcpy(info->product, "MockUSB");
    info->productLength = 7;
    std::strcpy(info->serialNumber, "SN123456");
    info->serialNumberLength = 8;
    return 0;
}

std::int32_t APS5_VABI MockGetdentsOpen(std::uint32_t deviceId, std::int32_t* fd, std::uint64_t* bufferSize) {
    if (deviceId != 7) return static_cast<int>(0x80f40004u);
    *fd = 55;
    *bufferSize = 512;
    g_getdentsReadStep = 0;
    return 0;
}

std::int32_t APS5_VABI MockGetdentsRead(std::int32_t fd, void* buffer, std::uint64_t bufferSize, std::uint64_t* bytesRead) {
    if (fd != 55 || bufferSize < 64) return static_cast<int>(0x80f40002u);
    if (g_getdentsReadStep > 0) {
        *bytesRead = 0;
        return 0;
    }
    ++g_getdentsReadStep;
    auto* out = static_cast<std::uint8_t*>(buffer);
    std::memset(out, 0, 64);
    const char* folderName = "music";
    const std::uint32_t fileno1 = 1;
    const std::uint16_t reclen1 = 16;
    out[6] = 4;
    out[7] = 5;
    std::memcpy(out, &fileno1, 4);
    std::memcpy(out + 4, &reclen1, 2);
    std::memcpy(out + 8, folderName, 5);

    const char* fileName = "root.mp3";
    const std::uint32_t fileno2 = 2;
    const std::uint16_t reclen2 = 24;
    out[16 + 6] = 8;
    out[16 + 7] = 8;
    std::memcpy(out + 16, &fileno2, 4);
    std::memcpy(out + 16 + 4, &reclen2, 2);
    std::memcpy(out + 16 + 8, fileName, 8);

    *bytesRead = 40;
    return 0;
}

std::int32_t APS5_VABI MockGetdentsClose(std::int32_t fd) {
    return fd == 55 ? 0 : static_cast<int>(0x80f40002u);
}

std::int32_t APS5_VABI MockRequestMap(std::uint32_t deviceId, const char* directory, std::int32_t flags, std::uint64_t volumeSize, char* mountPoint, std::uint64_t* mountId) {
    if (deviceId != 7 || flags != 3 || volumeSize != 0) return static_cast<int>(0x80f40002u);
    std::filesystem::path hostDir = g_usbRoot;
    std::string dirStr(directory);
    if (!dirStr.empty() && dirStr[0] == '/') dirStr.erase(0, 1);
    if (!dirStr.empty()) hostDir /= dirStr;
    AddPathAlias_nid_no_patch("/clc_mnt7", hostDir.string().c_str());
    std::strcpy(mountPoint, "/clc_mnt7");
    *mountId = 100;
    ++g_mountCount;
    return 0;
}

std::int32_t APS5_VABI MockRequestUnmap(std::uint32_t deviceId, const char* mountPoint) {
    if (deviceId != 7 || std::strcmp(mountPoint, "/clc_mnt7") != 0) return static_cast<int>(0x80f40002u);
    RemovePathAlias_nid_no_patch("/clc_mnt7");
    ++g_unmountCount;
    return g_unmapResult;
}

std::int32_t APS5_VABI MockRegisterCallback(std::int32_t eventType, UsbStorageCallback callback, void* userData) {
    g_hooks.push_back({eventType, callback, userData});
    return 0;
}

void AppendBe16(std::vector<std::uint8_t>& out, std::uint16_t v) {
    out.push_back(static_cast<std::uint8_t>((v >> 8) & 0xff));
    out.push_back(static_cast<std::uint8_t>(v & 0xff));
}

void AppendBe24(std::vector<std::uint8_t>& out, std::uint32_t v) {
    out.push_back(static_cast<std::uint8_t>((v >> 16) & 0xff));
    out.push_back(static_cast<std::uint8_t>((v >> 8) & 0xff));
    out.push_back(static_cast<std::uint8_t>(v & 0xff));
}

void AppendBe32(std::vector<std::uint8_t>& out, std::uint32_t v) {
    out.push_back(static_cast<std::uint8_t>((v >> 24) & 0xff));
    out.push_back(static_cast<std::uint8_t>((v >> 16) & 0xff));
    out.push_back(static_cast<std::uint8_t>((v >> 8) & 0xff));
    out.push_back(static_cast<std::uint8_t>(v & 0xff));
}

void AppendLe32(std::vector<std::uint8_t>& out, std::uint32_t v) {
    out.push_back(static_cast<std::uint8_t>(v & 0xff));
    out.push_back(static_cast<std::uint8_t>((v >> 8) & 0xff));
    out.push_back(static_cast<std::uint8_t>((v >> 16) & 0xff));
    out.push_back(static_cast<std::uint8_t>((v >> 24) & 0xff));
}

void AppendSynchsafe32(std::vector<std::uint8_t>& out, std::uint32_t v) {
    out.push_back(static_cast<std::uint8_t>((v >> 21) & 0x7f));
    out.push_back(static_cast<std::uint8_t>((v >> 14) & 0x7f));
    out.push_back(static_cast<std::uint8_t>((v >> 7) & 0x7f));
    out.push_back(static_cast<std::uint8_t>(v & 0x7f));
}

void WriteBytes(const std::filesystem::path& path, const std::vector<std::uint8_t>& bytes) {
    std::ofstream f(path, std::ios::binary | std::ios::trunc);
    Require(f.is_open());
    f.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    Require(f.good());
}

void AppendId3v2TextFrame(std::vector<std::uint8_t>& tag, const char* id, const std::string& text) {
    tag.insert(tag.end(), id, id + 4);
    AppendBe32(tag, static_cast<std::uint32_t>(1 + text.size()));
    AppendBe16(tag, 0);
    tag.push_back(3);
    tag.insert(tag.end(), text.begin(), text.end());
}

std::pair<std::uint64_t, std::uint64_t> CreateTestMp3(const std::filesystem::path& path) {
    std::vector<std::uint8_t> frames;
    AppendId3v2TextFrame(frames, "TIT2", "MP3 Title");
    AppendId3v2TextFrame(frames, "TPE1", "MP3 Artist");

    std::vector<std::uint8_t> apicPayload;
    apicPayload.push_back(0);
    const char* mime = "image/jpeg";
    apicPayload.insert(apicPayload.end(), mime, mime + std::strlen(mime) + 1);
    apicPayload.push_back(3);
    apicPayload.push_back(0);
    const std::size_t coverRelOffset = frames.size() + 10 + apicPayload.size();
    const std::vector<std::uint8_t> coverBytes = {0xff, 0xd8, 0xff, 0xe0, 0x11, 0x22};
    apicPayload.insert(apicPayload.end(), coverBytes.begin(), coverBytes.end());

    frames.insert(frames.end(), {'A', 'P', 'I', 'C'});
    AppendBe32(frames, static_cast<std::uint32_t>(apicPayload.size()));
    AppendBe16(frames, 0);
    frames.insert(frames.end(), apicPayload.begin(), apicPayload.end());

    std::vector<std::uint8_t> file;
    file.insert(file.end(), {'I', 'D', '3', 3, 0, 0});
    AppendSynchsafe32(file, static_cast<std::uint32_t>(frames.size()));
    const std::uint64_t coverOffset = 10 + coverRelOffset;
    file.insert(file.end(), frames.begin(), frames.end());

    const std::vector<std::uint8_t> mpegSync = {0xff, 0xfb, 0x90, 0x00};
    file.insert(file.end(), mpegSync.begin(), mpegSync.end());
    file.resize(file.size() + 4092, 0);

    std::vector<std::uint8_t> id3v1(128, 0);
    id3v1[0] = 'T';
    id3v1[1] = 'A';
    id3v1[2] = 'G';
    std::memcpy(id3v1.data() + 3, "IgnoredTitle", 12);
    std::memcpy(id3v1.data() + 63, "MP3 Album", 9);
    file.insert(file.end(), id3v1.begin(), id3v1.end());

    WriteBytes(path, file);
    return {coverOffset, coverBytes.size()};
}

std::vector<std::uint8_t> MakeBox(const char* type, const std::vector<std::uint8_t>& body) {
    std::vector<std::uint8_t> box;
    AppendBe32(box, static_cast<std::uint32_t>(8 + body.size()));
    box.insert(box.end(), type, type + 4);
    box.insert(box.end(), body.begin(), body.end());
    return box;
}

std::vector<std::uint8_t> MakeIlstDataItem(std::uint8_t b0, const char* rest3, const std::vector<std::uint8_t>& payload) {
    std::vector<std::uint8_t> dataBody(8, 0);
    dataBody[3] = 1;
    dataBody.insert(dataBody.end(), payload.begin(), payload.end());
    std::vector<std::uint8_t> dataBox = MakeBox("data", dataBody);
    std::vector<std::uint8_t> item;
    AppendBe32(item, static_cast<std::uint32_t>(8 + dataBox.size()));
    item.push_back(b0);
    item.insert(item.end(), rest3, rest3 + 3);
    item.insert(item.end(), dataBox.begin(), dataBox.end());
    return item;
}

std::pair<std::uint64_t, std::uint64_t> CreateTestM4a(const std::filesystem::path& path) {
    std::vector<std::uint8_t> mvhdBody(24, 0);
    AppendBe32(mvhdBody, 1000);
    mvhdBody.resize(16);
    mvhdBody.assign(20, 0);
    mvhdBody[12] = 0x00;
    mvhdBody[13] = 0x00;
    mvhdBody[14] = 0x03;
    mvhdBody[15] = 0xe8;
    mvhdBody[16] = 0x00;
    mvhdBody[17] = 0x00;
    mvhdBody[18] = 0x13;
    mvhdBody[19] = 0x88;
    std::vector<std::uint8_t> mvhd = MakeBox("mvhd", mvhdBody);

    std::vector<std::uint8_t> ilstBody;
    auto addText = [&](std::uint8_t b0, const char* r3, const std::string& s) {
        std::vector<std::uint8_t> v(s.begin(), s.end());
        auto b = MakeIlstDataItem(b0, r3, v);
        ilstBody.insert(ilstBody.end(), b.begin(), b.end());
    };
    addText(0xa9, "nam", "M4A Title");
    addText(0xa9, "alb", "M4A Album");
    addText(0xa9, "ART", "M4A Artist");

    const std::vector<std::uint8_t> coverBytes = {0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a};
    auto covrBox = MakeIlstDataItem('c', "ovr", coverBytes);
    const std::size_t covrOffsetInIlst = ilstBody.size() + 8 + 16;
    ilstBody.insert(ilstBody.end(), covrBox.begin(), covrBox.end());

    std::vector<std::uint8_t> ilst = MakeBox("ilst", ilstBody);
    std::vector<std::uint8_t> metaBody(4, 0);
    metaBody.insert(metaBody.end(), ilst.begin(), ilst.end());
    std::vector<std::uint8_t> meta = MakeBox("meta", metaBody);
    std::vector<std::uint8_t> udta = MakeBox("udta", meta);

    std::vector<std::uint8_t> moovBody;
    moovBody.insert(moovBody.end(), mvhd.begin(), mvhd.end());
    moovBody.insert(moovBody.end(), udta.begin(), udta.end());
    std::vector<std::uint8_t> moov = MakeBox("moov", moovBody);

    std::vector<std::uint8_t> ftypBody = {'M', '4', 'A', ' ', 0, 0, 0, 0};
    std::vector<std::uint8_t> ftyp = MakeBox("ftyp", ftypBody);

    const std::uint64_t coverOffset = ftyp.size() + 8 + mvhd.size() + 8 + 8 + 4 + 8 + covrOffsetInIlst;
    std::vector<std::uint8_t> file;
    file.insert(file.end(), ftyp.begin(), ftyp.end());
    file.insert(file.end(), moov.begin(), moov.end());

    WriteBytes(path, file);
    return {coverOffset, coverBytes.size()};
}

std::pair<std::uint64_t, std::uint64_t> CreateTestFlac(const std::filesystem::path& path) {
    std::vector<std::uint8_t> file = {'f', 'L', 'a', 'C'};

    std::vector<std::uint8_t> streamInfo(34, 0);
    const std::uint32_t sampleRate = 44100;
    const std::uint64_t totalSamples = 88200;
    streamInfo[10] = static_cast<std::uint8_t>((sampleRate >> 12) & 0xff);
    streamInfo[11] = static_cast<std::uint8_t>((sampleRate >> 4) & 0xff);
    streamInfo[12] = static_cast<std::uint8_t>((sampleRate & 0x0f) << 4);
    streamInfo[13] = static_cast<std::uint8_t>((totalSamples >> 32) & 0x0f);
    streamInfo[14] = static_cast<std::uint8_t>((totalSamples >> 24) & 0xff);
    streamInfo[15] = static_cast<std::uint8_t>((totalSamples >> 16) & 0xff);
    streamInfo[16] = static_cast<std::uint8_t>((totalSamples >> 8) & 0xff);
    streamInfo[17] = static_cast<std::uint8_t>(totalSamples & 0xff);
    file.push_back(0x00);
    AppendBe24(file, static_cast<std::uint32_t>(streamInfo.size()));
    file.insert(file.end(), streamInfo.begin(), streamInfo.end());

    std::vector<std::uint8_t> vc;
    AppendLe32(vc, 4);
    vc.insert(vc.end(), {'t', 'e', 's', 't'});
    AppendLe32(vc, 3);
    auto addVc = [&](const std::string& kv) {
        AppendLe32(vc, static_cast<std::uint32_t>(kv.size()));
        vc.insert(vc.end(), kv.begin(), kv.end());
    };
    addVc("TITLE=FLAC Title");
    addVc("ALBUM=FLAC Album");
    addVc("ARTIST=FLAC Artist");
    file.push_back(0x04);
    AppendBe24(file, static_cast<std::uint32_t>(vc.size()));
    file.insert(file.end(), vc.begin(), vc.end());

    const std::vector<std::uint8_t> coverBytes = {1, 2, 3, 4, 5};
    std::vector<std::uint8_t> pic;
    AppendBe32(pic, 3);
    AppendBe32(pic, 9);
    const char* mime = "image/png";
    pic.insert(pic.end(), mime, mime + 9);
    AppendBe32(pic, 0);
    AppendBe32(pic, 1);
    AppendBe32(pic, 1);
    AppendBe32(pic, 24);
    AppendBe32(pic, 0);
    AppendBe32(pic, static_cast<std::uint32_t>(coverBytes.size()));
    const std::uint64_t picDataOffsetInBlock = pic.size();
    pic.insert(pic.end(), coverBytes.begin(), coverBytes.end());

    file.push_back(0x86);
    AppendBe24(file, static_cast<std::uint32_t>(pic.size()));
    const std::uint64_t coverOffset = file.size() + picDataOffsetInBlock;
    file.insert(file.end(), pic.begin(), pic.end());

    WriteBytes(path, file);
    return {coverOffset, coverBytes.size()};
}

}

int main() {
    std::uint32_t devices[8]{};
    std::int32_t deviceCount = -1;
    std::uint64_t counts = 0;
    SceCLCDeviceInfo devInfo{};
    SceCLCFileInfo fileInfo{};

    Require(sceCLCGetDeviceList(nullptr, &deviceCount) == kErrInvalidArg);
    Require(sceCLCGetDeviceList(devices, nullptr) == kErrInvalidArg);
    Require(sceCLCGetDeviceList(devices, &deviceCount) == kErrNoCallback);
    Require(deviceCount == 0);

    errno = 0x1357;
    Require(sceCLCSetUsbStorageCallback(nullptr) == kErrInvalidArg);
    Require(sceCLCSetUsbStorageCallback(reinterpret_cast<const SceCLCUsbStorageCallback*>(0x10)) == kErrInvalidArg);
    Require(errno == 0x1357);

    Require(sceCLCGetUsbStorageCallback(nullptr) == kErrInvalidArg);
    Require(sceCLCGetUsbStorageCallback(reinterpret_cast<SceCLCUsbStorageCallback*>(0x10)) == kErrInvalidArg);
    Require(errno == 0x1357);

    auto* const badDevices = reinterpret_cast<std::uint32_t*>(0x10);
    auto* const badCount = reinterpret_cast<std::int32_t*>(0x10);
    auto* const badDevInfo = reinterpret_cast<SceCLCDeviceInfo*>(0x10);
    auto* const badPath = reinterpret_cast<const char*>(0x10);
    auto* const badCounts = reinterpret_cast<std::uint64_t*>(0x10);
    auto* const badListInfo = reinterpret_cast<SceCLCListInfo*>(0x10);
    auto* const badTrackInfo = reinterpret_cast<SceCLCTrackListInfo*>(0x10);
    auto* const badListOut = reinterpret_cast<SceCLCListOutput*>(0x10);
    auto* const badListItems = reinterpret_cast<SceCLCListEntry*>(0x10);
    auto* const badFileInfo = reinterpret_cast<SceCLCFileInfo*>(0x10);

    Require(sceCLCGetDeviceList(badDevices, &deviceCount) == kErrInvalidArg);
    Require(sceCLCGetDeviceList(devices, badCount) == kErrInvalidArg);
    Require(sceCLCGetDeviceInfo(7, badDevInfo) == kErrInvalidArg);
    Require(sceCLCGetFolderInfo(7, badPath, &counts) == kErrInvalidArg);
    Require(sceCLCGetFolderInfo(7, "/", badCounts) == kErrInvalidArg);
    Require(sceCLCGetFileListInfo(badListInfo) == kErrInvalidArg);
    Require(sceCLCGetTrackListInfo(badTrackInfo) == kErrInvalidArg);
    Require(sceCLCGetFileList(0, 0, 0, 0, 1, badListOut) == kErrInvalidArg);
    Require(sceCLCGetTrackList(0, 1, badListOut) == kErrInvalidArg);
    SceCLCListOutput badItemsOut{};
    badItemsOut.items = badListItems;
    Require(sceCLCGetFileList(0, 0, 0, 0, 1, &badItemsOut) == kErrInvalidArg);
    Require(sceCLCGetTrackList(0, 1, &badItemsOut) == kErrInvalidArg);
    Require(sceCLCGetFileInfo(7, badPath, &fileInfo) == kErrInvalidArg);
    Require(sceCLCGetFileInfo(7, "/music/a.mp3", badFileInfo) == kErrInvalidArg);
    Require(sceCLCGetTrackInfo(0, badFileInfo) == kErrInvalidArg);
    Require(libSceContentListControllerUnknown_SoBk_minus_kuNLsE_nid_no_patch(badPath) == kErrInvalidArg);
    Require(errno == 0x1357);

    SceCLCUsbStorageCallback partialCb{};
    partialCb.getDeviceList = &MockGetDeviceList;
    partialCb.requestMap = &MockRequestMap;
    Require(sceCLCSetUsbStorageCallback(&partialCb) == 0);
    Require(sceCLCGetDeviceList(devices, &deviceCount) == 0);
    Require(deviceCount == 1 && devices[0] == 7);

    Require(sceCLCGetDeviceInfo(7, nullptr) == kErrInvalidArg);
    Require(sceCLCGetDeviceInfo(99, &devInfo) == kErrDeviceNotFound);
    Require(sceCLCGetDeviceInfo(7, &devInfo) == kErrNoCallback);
    Require(sceCLCGetFolderInfo(7, nullptr, &counts) == kErrInvalidArg);
    Require(sceCLCGetFolderInfo(7, "/", nullptr) == kErrInvalidArg);
    Require(sceCLCGetFolderInfo(99, "/", &counts) == kErrDeviceNotFound);
    Require(sceCLCGetFolderInfo(7, "/", &counts) == kErrNoCallback);
    Require(sceCLCGetFolderInfo(7, "/music/", &counts) == kErrNoCallback);
    Require(sceCLCGetFileInfo(7, "/music/a.mp3", &fileInfo) == kErrNoCallback);
    Require(g_mountCount == 0 && g_unmountCount == 0);

    SceCLCUsbStorageCallback fullCb{};
    fullCb.getDeviceList = &MockGetDeviceList;
    fullCb.getDeviceInfo = &MockGetDeviceInfo;
    fullCb.getdentsOpen = &MockGetdentsOpen;
    fullCb.getdentsRead = &MockGetdentsRead;
    fullCb.getdentsClose = &MockGetdentsClose;
    fullCb.requestMap = &MockRequestMap;
    fullCb.requestUnmap = &MockRequestUnmap;
    fullCb.registerCallback = &MockRegisterCallback;

    Require(sceCLCRegisterEventCallback(&OnClcEvent, &g_userTag) == 0);
    Require(sceCLCSetUsbStorageCallback(&fullCb) == 0);
    Require(g_hooks.size() == 2);
    Require(g_hooks[0].eventType == 1 && g_hooks[1].eventType == 2);

    SceCLCUsbStorageCallback roundTrip{};
    std::memset(&roundTrip, 0xA5, sizeof(roundTrip));
    Require(sceCLCGetUsbStorageCallback(&roundTrip) == 0);
    Require(std::memcmp(&roundTrip, &fullCb, sizeof(fullCb)) == 0);

    const UsbStorageEvent mountEv{1, 7, 0};
    const UsbStorageEvent unmountEv{2, 7, 0};
    const UsbStorageEvent ignoredEv0{0, 7, 0};
    const UsbStorageEvent ignoredEv3{3, 7, 0};
    g_hooks[0].callback(&ignoredEv0, g_hooks[0].userData);
    g_hooks[0].callback(&ignoredEv3, g_hooks[0].userData);
    Require(g_receivedEvents.empty());
    g_hooks[0].callback(&mountEv, g_hooks[0].userData);
    g_hooks[1].callback(&unmountEv, g_hooks[1].userData);
    Require(g_receivedEvents.size() == 2);
    Require(g_receivedEvents[0].first.event == 1 && g_receivedEvents[0].first.device == 7 && g_receivedEvents[0].second == &g_userTag);
    Require(g_receivedEvents[1].first.event == 2 && g_receivedEvents[1].first.device == 7 && g_receivedEvents[1].second == &g_userTag);
    Require(sceCLCRegisterEventCallback(nullptr, nullptr) == 0);
    g_hooks[0].callback(&mountEv, g_hooks[0].userData);
    Require(g_receivedEvents.size() == 2);

    std::memset(devices, 0xA5, sizeof(devices));
    deviceCount = -1;
    Require(sceCLCGetDeviceList(devices, &deviceCount) == 0);
    Require(deviceCount == 1 && devices[0] == 7);

    std::memset(&devInfo, 0xA5, sizeof(devInfo));
    Require(sceCLCGetDeviceInfo(7, &devInfo) == 0);
    Require(std::strcmp(devInfo.vendor, "MockVendor") == 0);
    Require(std::strcmp(devInfo.product, "MockUSB") == 0);
    Require(std::strcmp(devInfo.serial, "SN123456") == 0);
    Require(devInfo.vendor[255] == '\0' && devInfo.product[255] == '\0' && devInfo.serial[255] == '\0');
    Require(sceCLCGetDeviceInfo(99, &devInfo) == kErrDeviceNotFound);

    counts = 0xA5A5A5A5A5A5A5A5ULL;
    Require(sceCLCGetFolderInfo(7, "/", &counts) == 0);
    Require(static_cast<std::int32_t>(counts & 0xffffffffu) == 1);
    Require(static_cast<std::int32_t>(counts >> 32) == 1);

    g_usbRoot = CreateUniqueTempDir();
    TempDirGuard usbRootGuard{g_usbRoot};
    std::error_code ec;
    const std::filesystem::path musicDir = g_usbRoot / "music";
    std::filesystem::create_directories(musicDir / "z_album_dir", ec);
    Require(!ec);

    WriteBytes(musicDir / ".hidden.mp3", {1, 2, 3});
    WriteBytes(musicDir / "dummy.txt", {'h', 'i'});
    const auto mp3Cover = CreateTestMp3(musicDir / "a_track.mp3");
    const auto m4aCover = CreateTestM4a(musicDir / "b_track.m4a");
    const auto flacCover = CreateTestFlac(musicDir / "c_track.flac");

    counts = 0xA5A5A5A5A5A5A5A5ULL;
    Require(sceCLCGetFolderInfo(7, "/music", &counts) == 0);
    Require(static_cast<std::int32_t>(counts & 0xffffffffu) == 1);
    Require(static_cast<std::int32_t>(counts >> 32) == 3);
    Require(g_mountCount > 0 && g_mountCount == g_unmountCount);

    SceCLCListInfo listInfo{};
    std::memset(&listInfo, 0xA5, sizeof(listInfo));
    Require(sceCLCGetFileListInfo(nullptr) == kErrInvalidArg);
    Require(sceCLCGetFileListInfo(&listInfo) == 0);
    Require(listInfo.deviceId == 7);
    Require(std::strcmp(listInfo.dir, "/music/") == 0);
    Require(listInfo.folderCount == 1 && listInfo.fileCount == 3);

    std::array<SceCLCListEntry, 8> entryBuf{};
    SceCLCListOutput listOut{};
    std::memset(entryBuf.data(), 0xA5, sizeof(entryBuf));
    listOut.items = entryBuf.data();
    Require(sceCLCGetFileList(0, 0, 0, 0, 10, &listOut) == 0);
    Require(listOut.totalCount == 4 && listOut.returnedCount == 4 && listOut.startIndex == 0);
    Require(entryBuf[0].isFile == 0 && std::strcmp(entryBuf[0].name, "/music/z_album_dir") == 0);
    Require(entryBuf[1].isFile == 1 && std::strcmp(entryBuf[1].name, "/music/a_track.mp3") == 0);
    Require(entryBuf[2].isFile == 1 && std::strcmp(entryBuf[2].name, "/music/b_track.m4a") == 0);
    Require(entryBuf[3].isFile == 1 && std::strcmp(entryBuf[3].name, "/music/c_track.flac") == 0);

    Require(sceCLCGetFileList(0, 0, 1, 1, 2, &listOut) == 0);
    Require(listOut.totalCount == 3 && listOut.returnedCount == 2 && listOut.startIndex == 1);
    Require(std::strcmp(entryBuf[0].name, "/music/b_track.m4a") == 0);
    Require(std::strcmp(entryBuf[1].name, "/music/c_track.flac") == 0);

    Require(sceCLCGetFileList(0, 0, 2, 0, 4, &listOut) == 0);
    Require(listOut.totalCount == 1 && listOut.returnedCount == 1);
    Require(entryBuf[0].isFile == 0 && std::strcmp(entryBuf[0].name, "/music/z_album_dir") == 0);

    Require(sceCLCGetFileList(0, 0, 0, 0, 10, nullptr) == kErrInvalidArg);
    Require(sceCLCGetFileList(0, 0, 0, 0xfa0, 1, &listOut) == kErrInvalidArg);
    Require(sceCLCGetFileList(0, 0, 0, 0, 0, &listOut) == kErrInvalidArg);
    Require(sceCLCGetFileList(0, 0, 0, 0, 0xfa1, &listOut) == kErrInvalidArg);
    Require(sceCLCGetFileList(0, 0, 1, 4, 1, &listOut) == kErrInvalidArg);
    listOut.items = nullptr;
    Require(sceCLCGetFileList(0, 0, 0, 0, 1, &listOut) == kErrInvalidArg);
    listOut.items = entryBuf.data();

    Require(libSceContentListControllerUnknown_SoBk_minus_kuNLsE_nid_no_patch(nullptr) == kErrInvalidArg);
    std::string tooLong(256, 'a');
    Require(libSceContentListControllerUnknown_SoBk_minus_kuNLsE_nid_no_patch(tooLong.c_str()) == kErrInvalidArg);
    Require(libSceContentListControllerUnknown_SoBk_minus_kuNLsE_nid_no_patch("(^\\..*|.*\\.flac$)") == 0);
    Require(sceCLCGetFolderInfo(7, "/music", &counts) == 0);
    Require(static_cast<std::int32_t>(counts & 0xffffffffu) == 1);
    Require(static_cast<std::int32_t>(counts >> 32) == 2);

    Require(libSceContentListControllerUnknown_SoBk_minus_kuNLsE_nid_no_patch("") == 0);
    Require(sceCLCGetFolderInfo(7, "/music", &counts) == 0);
    Require(static_cast<std::int32_t>(counts & 0xffffffffu) == 1);
    Require(static_cast<std::int32_t>(counts >> 32) == 0);

    Require(libSceContentListControllerUnknown_SoBk_minus_kuNLsE_nid_no_patch("^\\..*") == 0);
    Require(sceCLCGetFolderInfo(7, "/music", &counts) == 0);
    Require(static_cast<std::int32_t>(counts & 0xffffffffu) == 1);
    Require(static_cast<std::int32_t>(counts >> 32) == 3);

    Require(sceCLCSetTrackList() == 0);
    SceCLCTrackListInfo trackInfo{};
    std::memset(&trackInfo, 0xA5, sizeof(trackInfo));
    Require(sceCLCGetTrackListInfo(nullptr) == kErrInvalidArg);
    Require(sceCLCGetTrackListInfo(&trackInfo) == 0);
    Require(trackInfo.deviceId == 7);
    Require(std::strcmp(trackInfo.dir, "/music/") == 0);
    Require(trackInfo.fileCount == 3);

    Require(sceCLCSetPlaybackResult(1, 1) == 0);
    Require(sceCLCSetPlaybackResult(1, 2) == kErrInvalidArg);
    Require(sceCLCSetPlaybackResult(-1, 0) == kErrInvalidArg);
    Require(sceCLCSetPlaybackResult(3, 0) == kErrInvalidArg);

    Require(sceCLCGetTrackList(0, 10, &listOut) == 0);
    Require(listOut.totalCount == 3 && listOut.returnedCount == 3);
    Require(entryBuf[0].isFile == 1 && entryBuf[0].state == 0 && std::strcmp(entryBuf[0].name, "/music/a_track.mp3") == 0);
    Require(entryBuf[1].isFile == 1 && entryBuf[1].state == 1 && std::strcmp(entryBuf[1].name, "/music/b_track.m4a") == 0);
    Require(entryBuf[2].isFile == 1 && entryBuf[2].state == 0 && std::strcmp(entryBuf[2].name, "/music/c_track.flac") == 0);

    Require(sceCLCGetFileList(0, 0, 1, 0, 10, &listOut) == 0);
    Require(entryBuf[1].state == 1);

    {
        GuardedBuffer guarded;
        auto* singleEntry = guarded.At<SceCLCListEntry>(1);
        SceCLCListOutput guardedOut{};
        guardedOut.totalCount = -7;
        guardedOut.returnedCount = -8;
        guardedOut.startIndex = -9;
        guardedOut.items = singleEntry;
        errno = 71;
        Require(sceCLCGetFileList(0, 0, 0, 0, 2, &guardedOut) == kErrInvalidArg && errno == 71);
        Require(guardedOut.totalCount == -7 && guardedOut.returnedCount == -8 && guardedOut.startIndex == -9);
        const auto* entryBytes = reinterpret_cast<const std::uint8_t*>(singleEntry);
        for (std::size_t i = 0; i < sizeof(SceCLCListEntry); ++i) {
            Require(entryBytes[i] == 0xA5);
        }
        errno = 71;
        Require(sceCLCGetTrackList(0, 2, &guardedOut) == kErrInvalidArg && errno == 71);
        Require(guardedOut.totalCount == -7 && guardedOut.returnedCount == -8 && guardedOut.startIndex == -9);
        for (std::size_t i = 0; i < sizeof(SceCLCListEntry); ++i) {
            Require(entryBytes[i] == 0xA5);
        }
    }

    Require(sceCLCGetFileInfo(7, "/music/dummy.txt", &fileInfo) == kErrUnsupportedFormat);
    Require(sceCLCGetFileInfo(7, "badpath.mp3", &fileInfo) == kErrInvalidArg);
    Require(sceCLCGetFileInfo(7, nullptr, &fileInfo) == kErrInvalidArg);
    Require(sceCLCGetFileInfo(7, "/music/a_track.mp3", nullptr) == kErrInvalidArg);

    std::memset(&fileInfo, 0xA5, sizeof(fileInfo));
    Require(sceCLCGetFileInfo(7, "/music/a_track.mp3", &fileInfo) == 0);
    Require(fileInfo.index == 0 && fileInfo.deviceId == 7);
    Require(std::strcmp(fileInfo.path, "/music/a_track.mp3") == 0);
    Require(std::strcmp(fileInfo.title, "MP3 Title") == 0);
    Require(std::strcmp(fileInfo.album, "MP3 Album") == 0);
    Require(std::strcmp(fileInfo.artist, "MP3 Artist") == 0);
    Require(std::strcmp(fileInfo.coverArtPath, "/music/a_track.mp3") == 0);
    Require(fileInfo.coverArtOffset == mp3Cover.first && fileInfo.coverArtSize == mp3Cover.second);
    Require(fileInfo.duration == 1);

    std::memset(&fileInfo, 0xA5, sizeof(fileInfo));
    Require(sceCLCGetFileInfo(7, "/music/b_track.m4a", &fileInfo) == 0);
    Require(std::strcmp(fileInfo.title, "M4A Title") == 0);
    Require(std::strcmp(fileInfo.album, "M4A Album") == 0);
    Require(std::strcmp(fileInfo.artist, "M4A Artist") == 0);
    Require(std::strcmp(fileInfo.coverArtPath, "/music/b_track.m4a") == 0);
    Require(fileInfo.coverArtOffset == m4aCover.first && fileInfo.coverArtSize == m4aCover.second);
    Require(fileInfo.duration == 5);

    std::memset(&fileInfo, 0xA5, sizeof(fileInfo));
    Require(sceCLCGetFileInfo(7, "/music/c_track.flac", &fileInfo) == 0);
    Require(std::strcmp(fileInfo.title, "FLAC Title") == 0);
    Require(std::strcmp(fileInfo.album, "FLAC Album") == 0);
    Require(std::strcmp(fileInfo.artist, "FLAC Artist") == 0);
    Require(std::strcmp(fileInfo.coverArtPath, "/music/c_track.flac") == 0);
    Require(fileInfo.coverArtOffset == flacCover.first && fileInfo.coverArtSize == flacCover.second);
    Require(fileInfo.duration == 2);

    std::memset(&fileInfo, 0xA5, sizeof(fileInfo));
    Require(sceCLCGetFileInfo(7, "/music/missing.mp3", &fileInfo) == 0);
    Require(fileInfo.index == 0 && fileInfo.deviceId == 7);
    Require(std::strcmp(fileInfo.path, "/music/missing.mp3") == 0);
    Require(fileInfo.title[0] == '\0' && fileInfo.album[0] == '\0' && fileInfo.artist[0] == '\0');
    Require(fileInfo.coverArtPath[0] == '\0' && fileInfo.coverArtOffset == 0 && fileInfo.coverArtSize == 0 && fileInfo.duration == 0);
    Require(g_mountCount == g_unmountCount);

    g_unmapResult = static_cast<int>(0x80f40099u);
    Require(sceCLCGetFolderInfo(7, "/music", &counts) == g_unmapResult);
    Require(sceCLCGetFileInfo(7, "/music/a_track.mp3", &fileInfo) == g_unmapResult);
    g_unmapResult = 0;
    Require(g_mountCount == g_unmountCount);

    Require(sceCLCGetShuffle() == 0);
    Require(sceCLCSetShuffle(1, 2) == 0);
    Require(sceCLCGetShuffle() == 1);

    std::memset(&fileInfo, 0xA5, sizeof(fileInfo));
    Require(sceCLCGetTrackInfo(0, &fileInfo) == 0);
    Require(fileInfo.index == 2);
    Require(std::strcmp(fileInfo.title, "FLAC Title") == 0);
    Require(sceCLCGetTrackInfo(3, &fileInfo) == kErrInvalidArg);
    Require(sceCLCGetTrackInfo(-1, &fileInfo) == kErrInvalidArg);

    Require(sceCLCSetShuffle(0, 0) == 0);
    Require(sceCLCGetShuffle() == 0);
    Require(sceCLCGetTrackInfo(1, &fileInfo) == 0);
    Require(fileInfo.index == 1);
    Require(std::strcmp(fileInfo.title, "M4A Title") == 0);

    Require(sceCLCClearTrackList() == 0);
    std::memset(&trackInfo, 0xA5, sizeof(trackInfo));
    Require(sceCLCGetTrackListInfo(&trackInfo) == 0);
    Require(trackInfo.deviceId == 0 && trackInfo.dir[0] == '\0' && trackInfo.fileCount == 0);

    Require(sceCLCClearFileList() == 0);
    std::memset(&listInfo, 0xA5, sizeof(listInfo));
    Require(sceCLCGetFileListInfo(&listInfo) == 0);
    Require(listInfo.deviceId == 0 && listInfo.dir[0] == '\0' && listInfo.folderCount == 0 && listInfo.fileCount == 0);

    {
        SceCLCUsbStorageCallback twoCb = fullCb;
        twoCb.getDeviceList = &MockTwoDevices;
        Require(sceCLCSetUsbStorageCallback(&twoCb) == 0);

        GuardedBuffer guardedDev;
        auto* oneDev = guardedDev.At<std::uint32_t>(1);
        std::int32_t preservedCount = 99;
        errno = 71;
        Require(sceCLCGetDeviceList(oneDev, &preservedCount) == kErrInvalidArg && errno == 71);
        Require(preservedCount == 99);
        Require(*oneDev == 0xA5A5A5A5u);
        Require(sceCLCGetDeviceInfo(7, &devInfo) == 0);
        Require(sceCLCGetDeviceInfo(8, &devInfo) == kErrDeviceNotFound);

        Require(sceCLCSetUsbStorageCallback(&fullCb) == 0);
        Require(sceCLCGetDeviceList(devices, &deviceCount) == 0);
        Require(deviceCount == 1 && devices[0] == 7);
    }

    {
        const auto rootMp3Cover = CreateTestMp3(g_usbRoot / "root.mp3");
        std::memset(&fileInfo, 0xA5, sizeof(fileInfo));
        Require(sceCLCGetFileInfo(7, "/root.mp3", &fileInfo) == 0);
        Require(std::strcmp(fileInfo.path, "/root.mp3") == 0);
        Require(std::strcmp(fileInfo.title, "MP3 Title") == 0);
        Require(std::strcmp(fileInfo.coverArtPath, "/root.mp3") == 0);
        Require(fileInfo.coverArtOffset == rootMp3Cover.first && fileInfo.coverArtSize == rootMp3Cover.second);
    }

    {
        const int mountsBefore = g_mountCount;
        Require(sceCLCGetFileInfo(7, "/music/../secret.mp3", &fileInfo) == kErrInvalidArg);
        Require(sceCLCGetFileInfo(7, "/../secret.mp3", &fileInfo) == kErrInvalidArg);
        Require(sceCLCGetFileInfo(7, "/music\\a_track.mp3", &fileInfo) == kErrInvalidArg);
        Require(sceCLCGetFileInfo(7, "C:/a_track.mp3", &fileInfo) == kErrInvalidArg);
        Require(sceCLCGetFileInfo(7, "/C:/a_track.mp3", &fileInfo) == kErrInvalidArg);
        Require(sceCLCGetFolderInfo(7, "/music/../", &counts) == kErrInvalidArg);
        Require(sceCLCGetFolderInfo(7, "/..", &counts) == kErrInvalidArg);
        Require(sceCLCGetFolderInfo(7, "/C:/music", &counts) == kErrInvalidArg);
        Require(g_mountCount == mountsBefore && g_unmountCount == g_mountCount);
    }

    {
        std::vector<std::uint8_t> payload = {1, 0xff, 0xfe, 0x3d, 0xd8, 0x00, 0xde, 0x20, 0x00, 'H', 0x00, 'i', 0x00};
        std::vector<std::uint8_t> frame;
        frame.insert(frame.end(), {'T', 'I', 'T', '2'});
        AppendSynchsafe32(frame, static_cast<std::uint32_t>(payload.size()));
        AppendBe16(frame, 0);
        frame.insert(frame.end(), payload.begin(), payload.end());
        std::vector<std::uint8_t> file = {'I', 'D', '3', 4, 0, 0};
        AppendSynchsafe32(file, static_cast<std::uint32_t>(frame.size()));
        file.insert(file.end(), frame.begin(), frame.end());
        WriteBytes(musicDir / "utf16_surrogate.mp3", file);

        std::memset(&fileInfo, 0xA5, sizeof(fileInfo));
        Require(sceCLCGetFileInfo(7, "/music/utf16_surrogate.mp3", &fileInfo) == 0);
        Require(std::strcmp(fileInfo.title, "\xf0\x9f\x98\x80 Hi") == 0);
    }

    {
        std::vector<std::uint8_t> inner;
        AppendBe32(inner, 1);
        inner.insert(inner.end(), {'u', 'd', 't', 'a'});
        AppendBe32(inner, 0xffffffffu);
        AppendBe32(inner, 0xffffffffu);
        std::vector<std::uint8_t> box = std::move(inner);
        for (int d = 0; d < 40; ++d) {
            box = MakeBox("moov", box);
        }
        WriteBytes(musicDir / "malformed_deep.m4a", box);
        std::memset(&fileInfo, 0xA5, sizeof(fileInfo));
        Require(sceCLCGetFileInfo(7, "/music/malformed_deep.m4a", &fileInfo) == 0);

        std::vector<std::uint8_t> badPic1(32, 0);
        badPic1[4] = 0xff;
        badPic1[5] = 0xff;
        badPic1[6] = 0xff;
        badPic1[7] = 0xff;
        std::vector<std::uint8_t> badPic2(32, 0);
        badPic2[8] = 0xff;
        badPic2[9] = 0xff;
        badPic2[10] = 0xff;
        badPic2[11] = 0xec;

        std::vector<std::uint8_t> flac = {'f', 'L', 'a', 'C'};
        flac.push_back(0x06);
        AppendBe24(flac, static_cast<std::uint32_t>(badPic1.size()));
        flac.insert(flac.end(), badPic1.begin(), badPic1.end());
        flac.push_back(0x86);
        AppendBe24(flac, static_cast<std::uint32_t>(badPic2.size()));
        flac.insert(flac.end(), badPic2.begin(), badPic2.end());
        WriteBytes(musicDir / "malformed_pic.flac", flac);

        std::memset(&fileInfo, 0xA5, sizeof(fileInfo));
        Require(sceCLCGetFileInfo(7, "/music/malformed_pic.flac", &fileInfo) == 0);
        Require(fileInfo.coverArtSize == 0 && fileInfo.coverArtPath[0] == '\0');
    }

    {
        const std::filesystem::path rescanDir = g_usbRoot / "rescan_dir";
        std::filesystem::create_directories(rescanDir, ec);
        Require(!ec);
        WriteBytes(rescanDir / "a.mp3", {0xff, 0xfb, 0x90, 0x00});
        WriteBytes(rescanDir / "b.mp3", {0xff, 0xfb, 0x90, 0x00});
        WriteBytes(rescanDir / "c.mp3", {0xff, 0xfb, 0x90, 0x00});

        Require(sceCLCGetFolderInfo(7, "/rescan_dir", &counts) == 0);
        Require(static_cast<std::int32_t>(counts >> 32) == 3);
        Require(sceCLCSetTrackList() == 0);
        Require(sceCLCSetPlaybackResult(1, 1) == 0);

        WriteBytes(rescanDir / "0_early.mp3", {0xff, 0xfb, 0x90, 0x00});
        Require(sceCLCGetFolderInfo(7, "/rescan_dir", &counts) == 0);
        Require(static_cast<std::int32_t>(counts >> 32) == 4);
        Require(sceCLCGetFileList(0, 0, 1, 0, 10, &listOut) == 0);
        Require(listOut.returnedCount == 4);
        Require(std::strcmp(entryBuf[0].name, "/rescan_dir/0_early.mp3") == 0 && entryBuf[0].state == 0);
        Require(std::strcmp(entryBuf[1].name, "/rescan_dir/a.mp3") == 0 && entryBuf[1].state == 0);
        Require(std::strcmp(entryBuf[2].name, "/rescan_dir/b.mp3") == 0 && entryBuf[2].state == 1);
        Require(std::strcmp(entryBuf[3].name, "/rescan_dir/c.mp3") == 0 && entryBuf[3].state == 0);
    }

    {
        const std::filesystem::path emptyDir = g_usbRoot / "empty_dir";
        std::filesystem::create_directories(emptyDir, ec);
        Require(!ec);
        Require(sceCLCGetTrackListInfo(&trackInfo) == 0 && trackInfo.fileCount == 3);
        Require(sceCLCGetFolderInfo(7, "/empty_dir", &counts) == 0);
        Require(static_cast<std::int32_t>(counts & 0xffffffffu) == 0 && static_cast<std::int32_t>(counts >> 32) == 0);
        Require(sceCLCSetTrackList() == 0);
        std::memset(&trackInfo, 0xA5, sizeof(trackInfo));
        Require(sceCLCGetTrackListInfo(&trackInfo) == 0);
        Require(trackInfo.deviceId == 0 && trackInfo.dir[0] == '\0' && trackInfo.fileCount == 0);
        Require(sceCLCGetTrackList(0, 10, &listOut) == 0);
        Require(listOut.totalCount == 0 && listOut.returnedCount == 0);
        Require(sceCLCGetTrackInfo(0, &fileInfo) == kErrInvalidArg);
    }

    return 0;
}
