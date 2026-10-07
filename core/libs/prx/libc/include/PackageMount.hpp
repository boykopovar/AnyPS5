#ifndef CORE_LIBS_PRX_LIBC_INCLUDE_PACKAGEMOUNT_HPP
#define CORE_LIBS_PRX_LIBC_INCLUDE_PACKAGEMOUNT_HPP

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "SceTypes.hpp"

namespace PackageMount {

constexpr int FirstDescriptor = 0x08000000;
constexpr int LastDescriptor = 0x0fffffff;

struct EntryInfo {
    bool Directory = false;
    std::uint64_t Size = 0;
};

struct DirectoryEntry {
    std::string Name;
    bool Directory = false;
    std::uint32_t Inode = 0;
};

inline bool IsDescriptor(const int descriptor) {
    return descriptor >= FirstDescriptor && descriptor <= LastDescriptor;
}

}

extern "C" bool PackageLookup_nid_no_patch(const char* guestPath, PackageMount::EntryInfo* info);
extern "C" std::uint64_t PackageReadPath_nid_no_patch(const char* guestPath, std::uint64_t offset, void* destination, std::uint64_t size);
extern "C" bool PackageReadAll_nid_no_patch(const char* guestPath, std::vector<std::uint8_t>* bytes);
extern "C" bool PackageOpen_nid_no_patch(const char* guestPath, int flags, int* result);
extern "C" int PackageClose_nid_no_patch(int descriptor);
extern "C" std::int64_t PackageRead_nid_no_patch(int descriptor, void* buffer, std::size_t size);
extern "C" std::int64_t PackagePread_nid_no_patch(int descriptor, void* buffer, std::size_t size, std::int64_t offset);
extern "C" std::int64_t PackageSeek_nid_no_patch(int descriptor, std::int64_t offset, int whence);
extern "C" int PackageFstat_nid_no_patch(int descriptor, FileStat* status);
extern "C" bool PackageStat_nid_no_patch(const char* guestPath, FileStat* status);
extern "C" int PackageGetdents_nid_no_patch(int descriptor, char* buffer, int size, std::int64_t* base);
extern "C" bool PackageListDirectory_nid_no_patch(const char* guestPath, std::vector<PackageMount::DirectoryEntry>* entries);

#endif
