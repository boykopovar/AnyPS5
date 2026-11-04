#include "prx/libkernel/include/Api.hpp"
#include "prx/libkernel/File/include/File.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libkernel/File/include/FileFlags.hpp"
#include <barrier>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <thread>
#include <vector>

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "Truncate race check failed at line %d\n", line);
        std::abort();
    }
}

#define Require(value) Check((value), __LINE__)

static bool CheckRace(const std::filesystem::path& file, bool duplicate) {
    constexpr std::size_t PayloadBytes = 8u << 20;
    constexpr std::int64_t Cursor = 137;
    constexpr unsigned Rounds = 4;
    constexpr unsigned Reads = 32;
    constexpr unsigned Truncates = 256;
    std::vector<unsigned char> payload(PayloadBytes);
    for (std::size_t i = 0; i < payload.size(); ++i) payload[i] = static_cast<unsigned char>(i % 251);
    {
        std::ofstream stream(file, std::ios::binary);
        stream.write(reinterpret_cast<const char*>(payload.data()), static_cast<std::streamsize>(payload.size()));
        Require(stream.good());
    }
    const int descriptor = sceKernelOpen(file.string().c_str(), SCE_KERNEL_O_RDWR, 0);
    Require(descriptor >= 0);
    const int resizer = duplicate ? dup_nid_postfix(descriptor) : descriptor;
    Require(resizer >= 0);
    const auto truncate = duplicate ? ftruncate_nid_postfix : sceKernelFtruncate;
    const auto pread = duplicate ? pread_nid_postfix : sceKernelPread;
    unsigned truncateFailures = 0;
    unsigned preadCountFailures = 0;
    unsigned preadDataFailures = 0;
    unsigned cursorFailures = 0;
    unsigned readFailures = 0;
    std::vector<unsigned char> output(PayloadBytes);
    for (unsigned round = 0; round < Rounds; ++round) {
        Require(sceKernelFtruncate(descriptor, PayloadBytes) == 0);
        Require(sceKernelLseek(descriptor, Cursor, 0) == Cursor);
        std::barrier start(2);
        std::thread resizing([&] {
            start.arrive_and_wait();
            for (unsigned iteration = 0; iteration < Truncates; ++iteration) {
                const auto length = static_cast<std::int64_t>(PayloadBytes + ((iteration & 1u) ? 4096 : 0));
                if (truncate(resizer, length) != 0) ++truncateFailures;
            }
        });
        start.arrive_and_wait();
        for (unsigned iteration = 0; iteration < Reads; ++iteration) {
            if (pread(descriptor, output.data(), output.size(), 0) != static_cast<std::int64_t>(output.size())) {
                ++preadCountFailures;
            } else if (output != payload) {
                ++preadDataFailures;
            }
        }
        resizing.join();
        const auto position = sceKernelLseek(descriptor, 0, 1);
        if (position != Cursor || sceKernelLseek(resizer, 0, 1) != Cursor) ++cursorFailures;
        unsigned char byte = 0xff;
        if (sceKernelRead(descriptor, &byte, 1) != 1 || byte != payload[Cursor]) ++readFailures;
        std::printf("%s round=%u cursor=%lld expected=%lld next_byte=%u expected_byte=%u\n",
            duplicate ? "posix-dup" : "kernel-direct", round, static_cast<long long>(position),
            static_cast<long long>(Cursor), byte, payload[Cursor]);
    }
    if (duplicate) Require(sceKernelClose(resizer) == 0);
    Require(sceKernelClose(descriptor) == 0);
    const auto size = std::filesystem::file_size(file);
    std::ifstream stream(file, std::ios::binary);
    std::vector<unsigned char> stored(PayloadBytes);
    stream.read(reinterpret_cast<char*>(stored.data()), static_cast<std::streamsize>(stored.size()));
    const auto count = stream.gcount();
    stream.close();
    std::size_t wrongBytes = 0;
    std::size_t firstWrong = PayloadBytes;
    for (std::size_t i = 0; i < stored.size(); ++i) {
        if (stored[i] != payload[i]) {
            if (wrongBytes == 0) firstWrong = i;
            ++wrongBytes;
        }
    }
    std::printf("%s preads=%u truncates=%u truncate_failures=%u pread_count_failures=%u pread_data_failures=%u "
        "cursor_failures=%u read_failures=%u stored_bytes=%lld wrong_bytes=%zu first_wrong=%lld "
        "file_size=%llu expected_size=%zu\n",
        duplicate ? "posix-dup" : "kernel-direct", Rounds * Reads, Rounds * Truncates,
        truncateFailures, preadCountFailures, preadDataFailures, cursorFailures, readFailures,
        static_cast<long long>(count), wrongBytes, wrongBytes == 0 ? -1ll : static_cast<long long>(firstWrong),
        static_cast<unsigned long long>(size), PayloadBytes + 4096);
    Require(std::filesystem::remove(file));
    return truncateFailures == 0 && preadCountFailures == 0 && preadDataFailures == 0 &&
        cursorFailures == 0 && readFailures == 0 && count == static_cast<std::streamsize>(PayloadBytes) &&
        wrongBytes == 0 && size == PayloadBytes + 4096;
}

int main() {
    const std::filesystem::path root("anyps5-truncate-race-" +
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    Require(std::filesystem::create_directory(root));
    const bool direct = CheckRace(root / "direct.bin", false);
    const bool duplicate = CheckRace(root / "duplicate.bin", true);
    Require(std::filesystem::remove(root));
    return direct && duplicate ? 0 : 1;
}
