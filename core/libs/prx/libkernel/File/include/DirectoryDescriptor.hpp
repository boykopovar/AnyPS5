#ifndef CORE_LIBS_PRX_LIBKERNEL_FILE_DIRECTORYDESCRIPTOR_HPP
#define CORE_LIBS_PRX_LIBKERNEL_FILE_DIRECTORYDESCRIPTOR_HPP

#include <filesystem>
#include <optional>

namespace File {

int OpenDirectoryDescriptor(const std::filesystem::path& path);
std::optional<int> DuplicateDirectoryDescriptor(int fd);
std::optional<std::filesystem::path> DirectoryDescriptorPath(int fd);
void ForgetDirectoryDescriptor(int fd);
int ReadDirectoryDescriptor(int fd, char* buf, int nbytes);

}

#endif
