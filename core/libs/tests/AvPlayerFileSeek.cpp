#include "prx/libSceAvPlayer/include/FileReplacementStream.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <source_location>

namespace {

void Check(bool condition, const std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "seek requirement failed at %s:%u\n", location.file_name(), location.line());
        std::abort();
    }
}

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
        Check(stream.Open("in-memory seek fixture"));
        auto* context = stream.Context();
        const auto seek = [&](std::int64_t offset, int whence) { return context->seek(context->opaque, offset, whence); };
        const auto limit = static_cast<std::int64_t>(size);
        Check(seek(0, AVSEEK_SIZE) == limit);
        Check(seek(4, SEEK_SET) == 4);
        Check(seek(5, SEEK_CUR) == 9);
        Check(seek(-3, SEEK_CUR) == 6);
        Check(seek(-1, SEEK_END) == limit - 1);
        Check(seek(1, SEEK_END) == limit);
        Check(seek(std::numeric_limits<std::int64_t>::max(), SEEK_CUR) == limit);
        Check(seek(std::numeric_limits<std::int64_t>::min(), SEEK_CUR) == 0);
        Check(seek(std::numeric_limits<std::int64_t>::min(), SEEK_END) == 0);
        Check(seek(3, SEEK_SET | AVSEEK_FORCE) == 3);
        Check(seek(12, 12345) == -1);
        Check(seek(0, SEEK_CUR) == 3);
        Check(seek(std::numeric_limits<std::int64_t>::max(), SEEK_SET) == limit);
        Check(seek(-1, SEEK_CUR) == limit - 1);
    }
    Check(file.opened == 1 && file.closed == 1);
}

}

int main() {
    CheckSeeks(17);
    CheckSeeks(static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()));
    for (const auto size : {std::uint64_t{1} << 63, std::numeric_limits<std::uint64_t>::max()}) {
        File file{size};
        {
            AvPlayer::FileReplacementStream stream({&file, Open, Close, Read, Size});
            Check(!stream.Open("unrepresentable file size"));
            Check(stream.Context() == nullptr);
        }
        Check(file.opened == 1 && file.closed == 1);
    }
}
