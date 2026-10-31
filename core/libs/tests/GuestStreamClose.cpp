#include "prx/libkernel/include/Api.hpp"
#include "prx/libkernel/File/include/File.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/FileStream.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <string>
#include <thread>
#include <vector>

static void Check(bool condition, int line) {
    if (!condition) {
        std::fprintf(stderr, "stream close check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

static constexpr const char* Appended = "guest_stream_close_appended.txt";
static constexpr const char* Reused = "guest_stream_close_reused.txt";
static constexpr const char* Racing = "guest_stream_close_racing.bin";
static constexpr int ReadWrite = 0x0002;
static constexpr int CreateReadWrite = 0x0202;
static constexpr std::int64_t BadDescriptor = static_cast<std::int32_t>(0x80020009u);
static constexpr int Append = 0x0008;
static constexpr int CreateWriteAppend = 0x0209;
static constexpr int Truncate = 0x0400;
static constexpr int CloseOnExec = 0x00100000;
static constexpr int Directory = 0x00020000;
static constexpr int GetDescriptorFlags = 1;
static constexpr int GetStatusFlags = 3;
static constexpr int FromStart = 0;

static void Store(const char* path, const std::string& contents) {
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    file << contents;
}

static std::string Contents(const char* path) {
    std::ifstream file(path, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
}

static FileStream* Reopen(int closed, const char* mode) {
    auto* stream = fopen_nid_postfix(Reused, mode);
    Require(stream != nullptr);
    Require(fileno_nid_postfix(stream) == closed);
    return stream;
}

static void CloseWaitsForPositionedWrites() {
    const std::vector<char> data(std::size_t{8} << 20, 'p');
    for (int round = 0; round < 16; ++round) {
        const int descriptor = sceKernelOpen(Racing, CreateReadWrite | Truncate, 0644);
        Require(descriptor >= 0);
        auto* stream = fdopen_nid_postfix(descriptor, "r+");
        Require(stream != nullptr);
        std::atomic<bool> started = false;
        std::int64_t written = 0;
        std::thread writer([&] {
            started = true;
            written = sceKernelPwrite(descriptor, data.data(), data.size(), 0);
        });
        while (!started) std::this_thread::yield();
        std::this_thread::sleep_for(std::chrono::milliseconds(round % 4));
        if (round % 2 != 0) stream = freopen_nid_postfix(Reused, "r", stream);
        Require(stream != nullptr && fclose_nid_postfix(stream) == 0);
        writer.join();
        Require(written == static_cast<std::int64_t>(data.size()) || written == BadDescriptor);
    }
    std::remove(Racing);
}

static void CloseWaitsForALockedStream() {
    const int descriptor = sceKernelOpen(Racing, CreateReadWrite | Truncate, 0644);
    Require(descriptor >= 0);
    auto* stream = fdopen_nid_postfix(descriptor, "r+");
    Require(stream != nullptr);
    std::atomic<bool> locked = false;
    std::int64_t position = -1;
    std::thread holder([&] {
        flockfile_nid_postfix(stream);
        locked = true;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        position = sceKernelLseek(descriptor, 0, FromStart);
        funlockfile_nid_postfix(stream);
    });
    while (!locked) std::this_thread::yield();
    Require(fclose_nid_postfix(stream) == 0);
    holder.join();
    Require(position == 0);
    std::remove(Racing);
}

static void ReopenWhileStreamsAreCreated() {
    std::atomic<bool> done = false;
    std::thread opener([&] {
        while (!done) {
            auto* other = fopen_nid_postfix(Appended, "r");
            Require(other != nullptr && fclose_nid_postfix(other) == 0);
        }
    });
    auto* stream = fopen_nid_postfix(Reused, "r");
    Require(stream != nullptr);
    for (int round = 0; round < 500; ++round) {
        stream = freopen_nid_postfix(Reused, "r", stream);
        Require(stream != nullptr);
    }
    done = true;
    opener.join();
    Require(fclose_nid_postfix(stream) == 0);
}

int main() {
    Store(Reused, "0123456789");
    const int appending = sceKernelOpen(Appended, CreateWriteAppend | Truncate | CloseOnExec, 0644);
    Require(appending >= 0);
    Require((fcntl_nid_postfix(appending, GetStatusFlags) & Append) != 0);
    Require(fcntl_nid_postfix(appending, GetDescriptorFlags) == 1);
    auto* stream = fdopen_nid_postfix(appending, "a");
    Require(stream != nullptr);
    Require(fclose_nid_postfix(stream) == 0);

    stream = Reopen(appending, "r+");
    Require(fcntl_nid_postfix(appending, GetStatusFlags) == ReadWrite);
    Require(fcntl_nid_postfix(appending, GetDescriptorFlags) == 0);
    Require(sceKernelPwrite(appending, "AB", 2, 2) == 2);
    Require(sceKernelLseek(appending, 4, FromStart) == 4);
    Require(sceKernelWrite(appending, "CD", 2) == 2);
    Require(fclose_nid_postfix(stream) == 0);
    Require(Contents(Reused) == "01ABCD6789");

    const int random = sceKernelOpen("/dev/urandom", 0, 0);
    Require(random >= 0);
    stream = fdopen_nid_postfix(random, "r");
    Require(stream != nullptr);
    Require(fclose_nid_postfix(stream) == 0);

    stream = Reopen(random, "r");
    char read[16]{};
    Require(sceKernelRead(random, read, sizeof(read)) == 10);
    Require(std::string(read, 10) == "01ABCD6789");
    Require(fclose_nid_postfix(stream) == 0);

    const int directory = sceKernelOpen(".", Directory, 0);
    Require(directory >= 0);
    stream = fdopen_nid_postfix(directory, "r");
    Require(stream != nullptr);
    Require(fclose_nid_postfix(stream) == 0);

    stream = Reopen(directory, "r");
    Require(sceKernelRead(directory, read, sizeof(read)) == 10);
    Require(std::string(read, 10) == "01ABCD6789");
    Require(fclose_nid_postfix(stream) == 0);

    const int replaced = sceKernelOpen(Appended, CreateWriteAppend | CloseOnExec, 0644);
    Require(replaced >= 0);
    stream = fdopen_nid_postfix(replaced, "a");
    Require(stream != nullptr);
    stream = freopen_nid_postfix(Reused, "r+", stream);
    Require(stream != nullptr);
    Require(fileno_nid_postfix(stream) == replaced);
    Require(fcntl_nid_postfix(replaced, GetStatusFlags) == ReadWrite);
    Require(fcntl_nid_postfix(replaced, GetDescriptorFlags) == 0);
    Require(sceKernelPwrite(replaced, "EF", 2, 8) == 2);
    Require(fclose_nid_postfix(stream) == 0);
    Require(Contents(Reused) == "01ABCD67EF");

    CloseWaitsForPositionedWrites();
    Require(Contents(Reused) == "01ABCD67EF");
    CloseWaitsForALockedStream();
    ReopenWhileStreamsAreCreated();

    std::remove(Appended);
    std::remove(Reused);
}
