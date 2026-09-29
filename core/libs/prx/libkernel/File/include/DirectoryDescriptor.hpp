#ifndef CORE_LIBS_PRX_LIBKERNEL_FILE_DIRECTORYDESCRIPTOR_HPP
#define CORE_LIBS_PRX_LIBKERNEL_FILE_DIRECTORYDESCRIPTOR_HPP

#include <filesystem>
#include <optional>

// Windows cannot open a directory as a file descriptor, so a guest open() of a directory gets a
// descriptor for NUL that is tracked here with its path and a getdents cursor.
namespace File {

int OpenDirectoryDescriptor(const std::filesystem::path& path);
std::optional<std::filesystem::path> DirectoryDescriptorPath(int fd);
void ForgetDirectoryDescriptor(int fd);
// Fills FreeBSD dirent records { u32 fileno; u16 reclen; u8 type; u8 namlen; char name[]; } (4-byte aligned).
// Returns the bytes written, or -1 when fd is not a directory descriptor.
int ReadDirectoryDescriptor(int fd, char* buf, int nbytes);

}

#endif
