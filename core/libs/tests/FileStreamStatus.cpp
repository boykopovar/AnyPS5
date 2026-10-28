#include "prx/libc/include/FileStream.hpp"
#include <atomic>
#include <barrier>
#include <cerrno>
#include <cstdio>
#include <thread>

int main() {
    auto* native = std::tmpfile();
    if (!native) return 1;
    FileStream stream(native);
    stream.MarkDirectory();
    constexpr std::int16_t unrelated = 0x1104;
    stream.GuestState().flags = unrelated;
    unsigned failures = 0;
    for (const bool endOfFile : {false, true}) {
        for (const bool error : {false, true}) {
            stream.PublishStatus(endOfFile, error);
            const auto flags = stream.GuestState().flags;
            failures += flags != (unrelated | (endOfFile ? 0x20 : 0) | (error ? 0x40 : 0));
            errno = 123;
            stream.KeepDirectoryReadError(false);
            failures += errno != 123;
            stream.KeepDirectoryReadError(true);
            failures += errno != (error ? 21 : 123);
        }
    }
    std::barrier start(2);
    std::atomic<unsigned> invalid = 0;
    std::thread publish([&] {
        start.arrive_and_wait();
        for (unsigned index = 0; index < 20000; ++index)
            stream.PublishStatus((index & 1) != 0, (index & 2) != 0);
    });
    std::thread read([&] {
        start.arrive_and_wait();
        for (unsigned index = 0; index < 20000; ++index) {
            errno = 123;
            stream.KeepDirectoryReadError(true);
            if (errno != 123 && errno != 21) invalid.fetch_add(1, std::memory_order_relaxed);
            const auto flags = __atomic_load_n(&stream.GuestState().flags, __ATOMIC_RELAXED);
            if ((flags & ~0x60) != unrelated) invalid.fetch_add(1, std::memory_order_relaxed);
        }
    });
    publish.join();
    read.join();
    failures += invalid.load(std::memory_order_relaxed);
    failures += std::fclose(native) != 0;
    std::printf("file stream status: 4 serial cases, 20000 publications, 20000 reads, %u failures\n", failures);
    return failures == 0 ? 0 : 1;
}
