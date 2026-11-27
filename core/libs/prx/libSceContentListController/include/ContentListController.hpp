#pragma once

#include <cstddef>
#include <cstdint>
#include "SceTypes.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceUsbStorage/UsbStorage.hpp"

struct SceCLCEvent {
    std::uint32_t event;
    std::uint32_t device;
};

using SceCLCEventCallback = void (APS5_VABI *)(const SceCLCEvent* event, void* userData);

struct SceCLCUsbStorageCallback {
    std::int32_t (APS5_VABI *getDeviceList)(std::uint32_t* devices, std::int32_t* count);
    std::int32_t (APS5_VABI *getDeviceInfo)(std::uint32_t deviceId, UsbStorageDeviceInfo* info);
    std::int32_t (APS5_VABI *getdentsOpen)(std::uint32_t deviceId, std::int32_t* fd, std::uint64_t* bufferSize);
    std::int32_t (APS5_VABI *getdentsRead)(std::int32_t fd, void* buffer, std::uint64_t bufferSize, std::uint64_t* bytesRead);
    std::int32_t (APS5_VABI *getdentsClose)(std::int32_t fd);
    std::int32_t (APS5_VABI *requestMap)(std::uint32_t deviceId, const char* directory, std::int32_t flags, std::uint64_t volumeSize, char* mountPoint, std::uint64_t* mountId);
    std::int32_t (APS5_VABI *requestUnmap)(std::uint32_t deviceId, const char* mountPoint);
    void* reserved;
    std::int32_t (APS5_VABI *registerCallback)(std::int32_t eventType, UsbStorageCallback callback, void* userData);
    void* reserved2;
};
static_assert(sizeof(SceCLCUsbStorageCallback) == 0x50);

struct SceCLCDeviceInfo {
    char vendor[256];
    char product[256];
    char serial[256];
};
static_assert(sizeof(SceCLCDeviceInfo) == 0x300);

struct SceCLCListInfo {
    std::int32_t deviceId;
    char dir[0x200];
    std::int32_t folderCount;
    std::int32_t fileCount;
};
static_assert(sizeof(SceCLCListInfo) == 0x20c);

struct SceCLCTrackListInfo {
    std::int32_t deviceId;
    char dir[0x200];
    std::int32_t reserved;
    std::int32_t fileCount;
};
static_assert(offsetof(SceCLCTrackListInfo, fileCount) == 0x208);
static_assert(sizeof(SceCLCTrackListInfo) == 0x20c);

struct SceCLCListEntry {
    std::int32_t deviceId;
    char name[0x200];
    std::int32_t isFile;
    std::int32_t state;
};
static_assert(sizeof(SceCLCListEntry) == 0x20c);

struct SceCLCListOutput {
    std::int32_t totalCount;
    std::int32_t returnedCount;
    std::int32_t startIndex;
    std::int32_t pad;
    SceCLCListEntry* items;
};
static_assert(sizeof(SceCLCListOutput) == 0x18);

struct SceCLCFileInfo {
    std::int32_t index;
    std::uint32_t deviceId;
    char path[0x200];
    char title[256];
    char album[256];
    char artist[256];
    char coverArtPath[0x200];
    std::uint64_t coverArtOffset;
    std::uint64_t coverArtSize;
    std::uint64_t duration;
};
static_assert(sizeof(SceCLCFileInfo) == 0x720);

extern "C" {

int APS5_VABI sceCLCClearFileList();
int APS5_VABI sceCLCClearTrackList();
int APS5_VABI sceCLCGetDeviceInfo(std::uint32_t deviceId, SceCLCDeviceInfo* outInfo);
int APS5_VABI sceCLCGetDeviceList(std::uint32_t* devices, std::int32_t* count);
int APS5_VABI sceCLCGetFileInfo(std::uint32_t deviceId, const char* path, SceCLCFileInfo* outInfo);
int APS5_VABI sceCLCGetFileList(std::uint32_t unused0, std::uint32_t unused1, std::int32_t listMode, std::uint32_t startIndex, std::int32_t maxCount, SceCLCListOutput* outList);
int APS5_VABI sceCLCGetFileListInfo(SceCLCListInfo* outInfo);
int APS5_VABI sceCLCGetFolderInfo(std::uint32_t deviceId, const char* path, std::uint64_t* outCounts);
std::uint8_t APS5_VABI sceCLCGetShuffle();
int APS5_VABI sceCLCGetTrackInfo(std::int32_t trackIndex, SceCLCFileInfo* outInfo);
int APS5_VABI sceCLCGetTrackList(std::uint32_t startIndex, std::int32_t maxCount, SceCLCListOutput* outList);
int APS5_VABI sceCLCGetTrackListInfo(SceCLCTrackListInfo* outInfo);
int APS5_VABI sceCLCGetUsbStorageCallback(SceCLCUsbStorageCallback* callback);
int APS5_VABI sceCLCRegisterEventCallback(SceCLCEventCallback callback, void* userData);
int APS5_VABI sceCLCSetPlaybackResult(std::int32_t trackIndex, std::uint32_t result);
int APS5_VABI sceCLCSetShuffle(std::int32_t enable, std::int32_t currentTrackIndex);
int APS5_VABI sceCLCSetTrackList();
int APS5_VABI sceCLCSetUsbStorageCallback(const SceCLCUsbStorageCallback* callback);
int APS5_VABI libSceContentListControllerUnknown_SoBk_minus_kuNLsE_nid_no_patch(const char* pattern);

}
