#ifndef CORE_LIBS_PRX_LIBKERNEL_FILE_DIRECTORYDESCRIPTOR_HPP
#define CORE_LIBS_PRX_LIBKERNEL_FILE_DIRECTORYDESCRIPTOR_HPP

#include <cstdint>
#include <filesystem>
#include <optional>
#include "prx/libc/include/general/VabiMacros.hpp"

struct FileStat;

extern "C" const char* GetDirectoryDescriptorPath_nid_no_patch(int fd, int* error);
extern "C" std::uint64_t GetDirectoryDescriptorPosition_nid_no_patch(int fd);
extern "C" int IsDirectoryDescriptor_nid_no_patch(int fd);
extern "C" int ReadDirectoryDescriptor_nid_no_patch(int fd, char* buf, int nbytes);
extern "C" std::int64_t SeekDirectoryDescriptor_nid_no_patch(int fd, std::int64_t offset, int whence);
extern "C" int APS5_VABI ReadDirectoryDescriptorCallback_nid_no_patch(int fd, char* buf, int nbytes);
extern "C" std::int64_t APS5_VABI SeekDirectoryDescriptorCallback_nid_no_patch(int fd, std::int64_t offset, int whence);
extern "C" int OpenInDirectory_nid_no_patch(int directory, const char* path, int flags, std::uint16_t mode);
extern "C" int StatInDirectory_nid_no_patch(int directory, const char* path, FileStat* sb, bool follow);
extern "C" int ChmodNative_nid_no_patch(const std::filesystem::path& native, int mode);

namespace File {

int OpenDirectoryDescriptor(const std::filesystem::path& path);
bool IsDirectoryDescriptor(int fd);
bool IsVirtualDirectoryDescriptor(int fd);
bool IsNativeDirectoryDescriptor(int fd);
std::optional<std::filesystem::path> DirectoryDescriptorPath(int fd);
void ForgetDirectoryDescriptor(int fd);
void DuplicateDirectoryDescriptor(int from, int to);
std::optional<std::int64_t> SeekDirectoryDescriptor(int fd, std::int64_t offset, int whence);
int ReadDirectoryDescriptor(int fd, char* buf, int nbytes, std::int64_t* base = nullptr);

}

#endif
