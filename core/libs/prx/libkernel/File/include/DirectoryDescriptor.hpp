#ifndef CORE_LIBS_PRX_LIBKERNEL_FILE_DIRECTORYDESCRIPTOR_HPP
#define CORE_LIBS_PRX_LIBKERNEL_FILE_DIRECTORYDESCRIPTOR_HPP

#include <cstdint>
#include <filesystem>
#include <optional>

namespace File {

int OpenDirectoryDescriptor(const std::filesystem::path& path);
std::optional<std::filesystem::path> DirectoryDescriptorPath(int fd);
void ForgetDirectoryDescriptor(int fd);
std::optional<std::int64_t> SeekDirectoryDescriptor(int fd, std::int64_t offset, int whence);
int ReadDirectoryDescriptor(int fd, char* buf, int nbytes);

}

#endif
