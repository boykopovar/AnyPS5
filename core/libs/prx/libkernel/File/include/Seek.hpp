#ifndef CORE_LIBS_PRX_LIBKERNEL_FILE_SEEK_HPP
#define CORE_LIBS_PRX_LIBKERNEL_FILE_SEEK_HPP

#include <cstdint>

namespace File {

constexpr int GuestSeekData = 3;
constexpr int GuestSeekHole = 4;

struct SeekRegion {
    int error;
    int whence;
    std::int64_t offset;
};

int UnknownWhenceError(int fd);
SeekRegion ResolveSeekRegion(int fd, std::int64_t offset, int whence);

}

#endif
