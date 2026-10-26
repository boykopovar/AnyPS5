#ifndef CORE_LIBS_PRX_LIBC_INCLUDE_APPARCHIVE_HPP
#define CORE_LIBS_PRX_LIBC_INCLUDE_APPARCHIVE_HPP

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

struct AppArchiveNode {
    std::uint32_t Handle;
    bool Directory;
    std::uint64_t Size;
};

struct AppArchiveBuffer {
    void* Base;
    std::size_t Length;
};

struct AppArchiveDirectoryEntry {
    std::string Name;
    bool Directory;
};

extern "C" {

bool AppArchiveLookup_nid_no_patch(const std::filesystem::path& hostPath, AppArchiveNode* node);
std::uint64_t AppArchiveRead_nid_no_patch(std::uint32_t handle, std::uint64_t offset, void* buffer, std::uint64_t length);
std::vector<AppArchiveDirectoryEntry> AppArchiveList_nid_no_patch(std::uint32_t handle);
void AppArchiveReady_nid_no_patch();
void AppArchiveRefuseOnly_nid_no_patch(const std::filesystem::path& hostPath, const char* guestPath, const char* operation);
void AppArchiveRefuseMergedListing_nid_no_patch(const std::filesystem::path& hostPath, const char* guestPath);
void AppArchiveRefuseDescriptor_nid_no_patch(int descriptor, const char* operation);
std::filesystem::path AppArchivePath_nid_no_patch();
bool AppArchiveOnly_nid_no_patch(const std::filesystem::path& hostPath);
bool AppArchiveHasDirectory_nid_no_patch(const std::filesystem::path& hostPath);

void AppArchiveAttach_nid_no_patch(int descriptor, const AppArchiveNode& node);
void AppArchiveShare_nid_no_patch(int from, int to);
void AppArchiveDetach_nid_no_patch(int descriptor);
bool AppArchiveDescriptor_nid_no_patch(int descriptor, AppArchiveNode* node, std::uint64_t* position);
bool AppArchiveDescriptorRead_nid_no_patch(int descriptor, const AppArchiveBuffer* buffers, int count, const std::int64_t* offset, std::uint64_t* read);
bool AppArchiveDescriptorSeek_nid_no_patch(int descriptor, std::int64_t offset, int whence, std::int64_t* position);

}

#endif
