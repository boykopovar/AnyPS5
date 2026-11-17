#include "prx/libSceAvPlayer/include/FileReplacementStream.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "Testing.hpp"
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <source_location>

namespace {

struct File {
    std::uint64_t size;
    unsigned opened = 0;
    unsigned closed = 0;
};

std::int32_t APS5_VABI Open(void* object, const char*) {
    ++static_cast<File*>(object)->opened;
    return 0;
}

std::int32_t APS5_VABI Close(void* object) {
    ++static_cast<File*>(object)->closed;
    return 0;
}

std::int32_t APS5_VABI Read(void*, std::uint8_t*, std::uint64_t, std::uint32_t) {
    return 0;
}

std::uint64_t APS5_VABI Size(void* object) {
    return static_cast<File*>(object)->size;
}

void CheckSeeks(std::uint64_t size) {
    File file{size};
    {
        AvPlayer::FileReplacementStream stream({&file, Open, Close, Read, Size});
        Require(stream.Open("in-memory seek fixture"));
        auto* context = stream.Context();
        const auto seek = [&](std::int64_t offset, int whence) { return context->seek(context->opaque, offset, whence); };
        const auto limit = static_cast<std::int64_t>(size);
        Require(seek(0, AVSEEK_SIZE) == limit);
        Require(seek(4, SEEK_SET) == 4);
        Require(seek(5, SEEK_CUR) == 9);
        Require(seek(-3, SEEK_CUR) == 6);
        Require(seek(-1, SEEK_END) == limit - 1);
        Require(seek(1, SEEK_END) == limit);
        Require(seek(std::numeric_limits<std::int64_t>::max(), SEEK_CUR) == limit);
        Require(seek(std::numeric_limits<std::int64_t>::min(), SEEK_CUR) == 0);
        Require(seek(std::numeric_limits<std::int64_t>::min(), SEEK_END) == 0);
        Require(seek(3, SEEK_SET | AVSEEK_FORCE) == 3);
        Require(seek(12, 12345) == -1);
        Require(seek(0, SEEK_CUR) == 3);
        Require(seek(std::numeric_limits<std::int64_t>::max(), SEEK_SET) == limit);
        Require(seek(-1, SEEK_CUR) == limit - 1);
    }
    Require(file.opened == 1 && file.closed == 1);
}

}

int main() {
    CheckSeeks(17);
    CheckSeeks(static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()));
    for (const auto size : {std::uint64_t{1} << 63, std::numeric_limits<std::uint64_t>::max()}) {
        File file{size};
        {
            AvPlayer::FileReplacementStream stream({&file, Open, Close, Read, Size});
            Require(!stream.Open("unrepresentable file size"));
            Require(stream.Context() == nullptr);
        }
        Require(file.opened == 1 && file.closed == 1);
    }
}
