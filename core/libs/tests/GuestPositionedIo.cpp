#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libkernel/File/include/FileFlags.hpp"
#include <array>
#include <atomic>
#include <barrier>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>
#ifdef _WIN32
#include <windows.h>
#include <fcntl.h>
#include <io.h>
#else
#include <unistd.h>
#endif

struct GuestIovec {
    void* base;
    std::size_t length;
};

extern "C" {
int APS5_VABI sceKernelOpen(const char*, int, std::uint16_t);
int APS5_VABI sceKernelClose(int);
std::int64_t APS5_VABI sceKernelRead(int, void*, std::size_t);
std::int64_t APS5_VABI sceKernelLseek(int, std::int64_t, int);
std::int64_t APS5_VABI sceKernelPread(int, void*, std::size_t, std::int64_t);
std::int64_t APS5_VABI sceKernelPwrite(int, const void*, std::size_t, std::int64_t);
std::int64_t APS5_VABI sceKernelPreadv(int, const GuestIovec*, int, std::int64_t);
std::int64_t APS5_VABI sceKernelPwritev(int, const GuestIovec*, int, std::int64_t);
}

static void Check(bool value, const char* message) {
    if (!value) {
        std::fprintf(stderr, "Positioned I/O: %s\n", message);
        std::abort();
    }
}

static int Duplicate(int file) {
#ifdef _WIN32
    return ::_dup(file);
#else
    return ::dup(file);
#endif
}

static void ConcurrentTransfers(int file) {
    const int duplicate = Duplicate(file);
    Check(duplicate >= 0, "duplicate file");
    constexpr int Iterations = 2048;
    constexpr int Workers = 4;
    std::barrier start(Workers + 1);
    std::atomic<bool> valid{true};
    std::vector<std::thread> workers;
    for (int worker = 0; worker < Workers; ++worker) {
        workers.emplace_back([&, worker] {
            std::array<char, 8> bytes{};
            if (worker == 1 || worker == 3) bytes.fill(static_cast<char>('A' + worker));
            const int descriptor = worker % 2 == 0 ? file : duplicate;
            const std::int64_t offset = 16384 + worker * 16;
            GuestIovec vectors[] = {{bytes.data(), 3}, {bytes.data() + 3, 5}};
            start.arrive_and_wait();
            for (int i = 0; i < Iterations; ++i) {
                std::int64_t result;
                if (worker == 0) result = sceKernelPread(descriptor, bytes.data(), bytes.size(), offset);
                else if (worker == 1) result = sceKernelPwrite(descriptor, bytes.data(), bytes.size(), offset);
                else if (worker == 2) result = sceKernelPreadv(descriptor, vectors, 2, offset);
                else result = sceKernelPwritev(descriptor, vectors, 2, offset);
                if (result != 8) valid = false;
                if ((worker == 0 || worker == 2) && bytes != std::array<char, 8>{}) valid = false;
            }
        });
    }
    start.arrive_and_wait();
    for (int i = 0; i < Iterations; ++i) {
        unsigned char byte = 0;
        if (sceKernelRead(file, &byte, 1) != 1 || byte != static_cast<unsigned char>(i % 251 + 1)) valid = false;
        std::this_thread::yield();
    }
    for (auto& worker : workers) worker.join();
    Check(valid, "concurrent transfers preserve sequential data and positioned results");
    Check(sceKernelLseek(file, 0, 1) == Iterations, "concurrent transfers preserve the sequential offset");
    Check(sceKernelLseek(duplicate, 0, 1) == Iterations, "duplicate retains the shared sequential offset");
    for (int worker : {1, 3}) {
        std::array<char, 8> bytes{};
        std::array<char, 8> expected{};
        expected.fill(static_cast<char>('A' + worker));
        Check(sceKernelPread(file, bytes.data(), bytes.size(), 16384 + worker * 16) == 8,
            "read positioned writes");
        Check(bytes == expected, "scalar and vector writes reach their requested offsets");
    }
    Check(sceKernelClose(duplicate) == 0, "close duplicate");
}

static void Boundaries(int file) {
    Check(sceKernelLseek(file, 7, 0) == 7, "set boundary-test position");
    std::array<char, 8> bytes{};
    bytes.fill('x');
    Check(sceKernelPread(file, bytes.data(), bytes.size(), 32766) == 2, "short read at EOF");
    Check(bytes[0] == 0 && bytes[1] == 0 && bytes[2] == 'x', "short read only writes returned bytes");
    bytes.fill('x');
    Check(sceKernelPread(file, bytes.data(), bytes.size(), 32768) == 0, "read at EOF");
    Check(bytes[0] == 'x', "EOF leaves output untouched");
    Check(sceKernelPread(file, bytes.data(), 0, 10) == 0, "empty read");
    Check(sceKernelPwrite(file, bytes.data(), 0, 10) == 0, "empty write");
    Check(sceKernelLseek(file, 0, 1) == 7, "short and empty transfers preserve offset");
}

static void AccessModes(const std::filesystem::path& path) {
    for (int mode : {SCE_KERNEL_O_RDONLY, SCE_KERNEL_O_WRONLY}) {
        const int file = sceKernelOpen(path.string().c_str(), mode, 0);
        Check(file >= 0, "open restricted descriptor");
        char byte = 'z';
        GuestIovec vector{&byte, 1};
        const bool readOnly = mode == SCE_KERNEL_O_RDONLY;
        bool rejected = false;
        try {
            const auto result = readOnly ? sceKernelPwrite(file, &byte, 1, 0) : sceKernelPread(file, &byte, 1, 0);
            rejected = result < 0;
        } catch (const std::runtime_error&) {
            rejected = true;
        }
        Check(rejected, "scalar transfer cannot gain access by reopening");
        const auto result = readOnly ? sceKernelPwritev(file, &vector, 1, 0) : sceKernelPreadv(file, &vector, 1, 0);
        Check(result == static_cast<int>(0x80020009u), "vector transfer rejects wrong access with EBADF");
        Check(sceKernelLseek(file, 0, 1) == 0, "rejected transfer preserves offset");
        if (readOnly) {
            Check(sceKernelPread(file, &byte, 1, 0) == 1 && byte == 1, "rejected writes preserve file contents");
        } else {
            Check(sceKernelPwrite(file, &byte, 1, 32767) == 1, "write-only positioned write succeeds");
        }
        Check(sceKernelClose(file) == 0, "close restricted descriptor");
    }
}

static void NativeContents(const std::filesystem::path& path) {
    std::array<char, 32768> expected{};
    for (int i = 0; i < 4096; ++i) expected[i] = static_cast<char>(i % 251 + 1);
    for (int worker : {1, 3}) {
        for (int i = 0; i < 8; ++i) expected[16384 + worker * 16 + i] = static_cast<char>('A' + worker);
    }
    expected.back() = 'z';
    std::array<char, 32768> actual{};
    std::ifstream stream(path, std::ios::binary);
    stream.read(actual.data(), actual.size());
    Check(stream.gcount() == static_cast<std::streamsize>(actual.size()), "native file length");
    Check(actual == expected, "native reader verifies every file byte");
    Check(stream.peek() == std::char_traits<char>::eof(), "no unintended extension");
}

#ifdef _WIN32
static void SharingFailure(const std::filesystem::path& root) {
    const auto path = root / "exclusive.bin";
    const HANDLE handle = ::CreateFileW(path.c_str(), GENERIC_READ | GENERIC_WRITE, 0,
        nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
    Check(handle != INVALID_HANDLE_VALUE, "create exclusive file");
    DWORD written = 0;
    Check(::WriteFile(handle, "abcd", 4, &written, nullptr) && written == 4, "initialize exclusive file");
    const int file = ::_open_osfhandle(reinterpret_cast<std::intptr_t>(handle), _O_RDWR | _O_BINARY);
    Check(file >= 0, "attach exclusive descriptor");
    char byte = 'x';
    GuestIovec vector{&byte, 1};
    Check(sceKernelPreadv(file, &vector, 1, 0) == static_cast<int>(0x80020005u), "sharing failure reports EIO on read");
    Check(byte == 'x', "failed reopen leaves read output untouched");
    Check(sceKernelPwritev(file, &vector, 1, 0) == static_cast<int>(0x80020005u), "sharing failure reports EIO on write");
    Check(sceKernelLseek(file, 0, 1) == 4, "failed reopen preserves offset");
    Check(sceKernelClose(file) == 0, "close exclusive file");
    std::ifstream stream(path, std::ios::binary);
    std::array<char, 4> contents{};
    stream.read(contents.data(), contents.size());
    Check(contents == std::array<char, 4>{'a', 'b', 'c', 'd'}, "failed reopen preserves file contents");
}
#endif

int main() {
    const auto root = std::filesystem::path("anyps5-positioned-io-" +
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    Check(std::filesystem::create_directory(root), "create directory");
    const auto path = root / "data.bin";
    {
        std::array<char, 32768> data{};
        for (int i = 0; i < 4096; ++i) data[i] = static_cast<char>(i % 251 + 1);
        std::ofstream stream(path, std::ios::binary);
        stream.write(data.data(), data.size());
        Check(stream.good(), "create fixture");
    }
    const int file = sceKernelOpen(path.string().c_str(), SCE_KERNEL_O_RDWR, 0);
    Check(file >= 0, "open file");
    ConcurrentTransfers(file);
    Boundaries(file);
    Check(sceKernelClose(file) == 0, "close file");
    AccessModes(path);
    NativeContents(path);
#ifdef _WIN32
    SharingFailure(root);
#endif
    std::filesystem::remove_all(root);
}
