#ifndef CORE_LIBS_PRX_LIBKERNEL_FILE_ARCHIVEDFILE_HPP
#define CORE_LIBS_PRX_LIBKERNEL_FILE_ARCHIVEDFILE_HPP

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>

#include "SceTypes.hpp"

namespace File {

std::optional<int> OpenArchivedFile(const std::filesystem::path& native, int flags);
void RefuseMergedDirectory(const std::filesystem::path& native);
bool IsArchivedFile(int fd);
void RefuseArchivedFile(int fd, const char* operation);
std::optional<std::int64_t> ReadArchivedFile(int fd, void* buf, std::size_t nbytes, const std::int64_t* offset);
std::optional<std::int64_t> ReadArchivedIovecs(int fd, const void* iov, int iovcnt, const std::int64_t* offset);
std::optional<std::int64_t> SeekArchivedFile(int fd, std::int64_t offset, int whence);
std::optional<std::int64_t> ArchivedFileRemaining(int fd);
bool FillArchivedStat(const std::filesystem::path& native, FileStat* sb);
bool FillArchivedStatFromDescriptor(int fd, FileStat* sb);
void ShareArchivedFile(int from, int to);
void ForgetArchivedFile(int fd);

}

#endif
