#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libkernel/File/include/FileFlags.hpp"
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
static int MakePipe(int* ends) { return ::_pipe(ends, 64, _O_BINARY); }
static int ClosePipe(int end) { return ::_close(end); }
#else
#include <unistd.h>
static int MakePipe(int* ends) { return ::pipe(ends); }
static int ClosePipe(int end) { return ::close(end); }
#endif

struct GuestIovec {
    void* base;
    std::size_t length;
};

extern "C" {
int APS5_VABI sceKernelOpen(const char*, int, std::uint16_t);
int APS5_VABI sceKernelClose(int);
std::int64_t APS5_VABI sceKernelRead(int, void*, std::size_t);
std::int64_t APS5_VABI sceKernelPread(int, void*, std::size_t, std::int64_t);
int APS5_VABI sceKernelLseek(int, std::int64_t, int);
std::int64_t APS5_VABI sceKernelReadv(int, const GuestIovec*, int);
std::int64_t APS5_VABI sceKernelWritev(int, const GuestIovec*, int);
std::int64_t APS5_VABI sceKernelPreadv(int, const GuestIovec*, int, std::int64_t);
std::int64_t APS5_VABI sceKernelPwritev(int, const GuestIovec*, int, std::int64_t);
}

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "File vector check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

static constexpr std::int64_t ErrorEbadf = static_cast<int>(0x80020009u);
static constexpr std::int64_t ErrorEfault = static_cast<int>(0x8002000Eu);
static constexpr std::int64_t ErrorEinval = static_cast<int>(0x80020016u);
static constexpr std::int64_t ErrorEspipe = static_cast<int>(0x8002001Du);

static std::string Contents(const std::filesystem::path& path) {
    std::ifstream stream(path, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
}

int main() {
    const auto root = std::filesystem::path("anyps5-file-vector-test-" +
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    Require(std::filesystem::create_directory(root));
    const auto path = root / "data.bin";
    { std::ofstream stream(path, std::ios::binary); stream << "0123456789"; }

    const int file = sceKernelOpen(path.string().c_str(), SCE_KERNEL_O_RDWR, 0);
    Require(file >= 0);

    char first[3] = {};
    char second[4] = {};
    GuestIovec reads[2] = {{first, sizeof(first)}, {second, sizeof(second)}};
    Require(sceKernelReadv(file, reads, 2) == 7);
    Require(std::memcmp(first, "012", 3) == 0 && std::memcmp(second, "3456", 4) == 0);
    Require(sceKernelLseek(file, 0, 1) == 7);
    Require(sceKernelReadv(file, reads, 2) == 3);
    Require(std::memcmp(first, "789", 3) == 0);
    Require(sceKernelReadv(file, reads, 2) == 0);

    Require(sceKernelPreadv(file, reads, 2, 1) == 7);
    Require(std::memcmp(first, "123", 3) == 0 && std::memcmp(second, "4567", 4) == 0);
    Require(sceKernelLseek(file, 0, 1) == 10);
    Require(sceKernelPreadv(file, reads, 2, 8) == 2);
    Require(std::memcmp(first, "89", 2) == 0);

    char ab[] = "AB";
    char cde[] = "CDE";
    GuestIovec writes[2] = {{ab, 2}, {cde, 3}};
    Require(sceKernelPwritev(file, writes, 2, 2) == 5);
    Require(sceKernelLseek(file, 0, 1) == 10);
    Require(Contents(path) == "01ABCDE789");
    Require(sceKernelLseek(file, 8, 0) == 8);
    Require(sceKernelWritev(file, writes, 2) == 5);
    Require(sceKernelLseek(file, 0, 1) == 13);
    Require(Contents(path) == "01ABCDE7ABCDE");

    GuestIovec gapped[3] = {{first, 2}, {nullptr, 0}, {second, 3}};
    Require(sceKernelPreadv(file, gapped, 3, 0) == 5);
    Require(std::memcmp(first, "01", 2) == 0 && std::memcmp(second, "ABC", 3) == 0);
    Require(sceKernelLseek(file, 2, 0) == 2);
    Require(sceKernelReadv(file, gapped, 3) == 5);
    Require(std::memcmp(first, "AB", 2) == 0 && std::memcmp(second, "CDE", 3) == 0);
    Require(sceKernelLseek(file, 0, 1) == 7);

    GuestIovec empty[1] = {{nullptr, 0}};
    Require(sceKernelReadv(file, empty, 1) == 0);
    Require(sceKernelReadv(file, reads, 0) == 0);
    Require(sceKernelWritev(file, nullptr, 0) == 0);

    Require(sceKernelReadv(file, reads, -1) == ErrorEinval);
    Require(sceKernelWritev(file, writes, -1) == ErrorEinval);
    Require(sceKernelPreadv(file, reads, -1, 0) == ErrorEinval);
    Require(sceKernelPwritev(file, writes, -1, 0) == ErrorEinval);
    Require(sceKernelReadv(file, reads, 1025) == ErrorEinval);
    Require(sceKernelWritev(file, writes, 1025) == ErrorEinval);
    Require(sceKernelReadv(file, nullptr, 1) == ErrorEfault);
    Require(sceKernelWritev(file, nullptr, 1) == ErrorEfault);
    Require(sceKernelPreadv(file, nullptr, 1, 0) == ErrorEfault);
    Require(sceKernelPwritev(file, nullptr, 1, 0) == ErrorEfault);
    Require(sceKernelPreadv(file, reads, 2, -1) == ErrorEinval);
    Require(sceKernelPwritev(file, writes, 2, -1) == ErrorEinval);
    Require(Contents(path) == "01ABCDE7ABCDE");

    Require(sceKernelClose(file) == 0);
    Require(sceKernelReadv(file, reads, 2) == ErrorEbadf);
    Require(sceKernelWritev(file, writes, 2) == ErrorEbadf);
    Require(sceKernelPreadv(file, reads, 2, 0) == ErrorEbadf);
    Require(sceKernelPwritev(file, writes, 2, 0) == ErrorEbadf);

    int ends[2] = {};
    Require(MakePipe(ends) == 0);
    char xyz[] = "xyz";
    GuestIovec message[1] = {{xyz, 3}};
    Require(sceKernelWritev(ends[1], message, 1) == 3);
    Require(sceKernelReadv(ends[0], reads, 2) == 3);
    Require(std::memcmp(first, "xyz", 3) == 0);
    Require(sceKernelPreadv(ends[0], reads, 2, 0) == ErrorEspipe);
    Require(sceKernelPwritev(ends[1], message, 1, 0) == ErrorEspipe);
    Require(ClosePipe(ends[0]) == 0 && ClosePipe(ends[1]) == 0);

    const int random = sceKernelOpen("/dev/urandom", 0, 0);
    Require(random >= 0);
    unsigned char noise[48]{};
    GuestIovec pieces[2] = {{noise, 16}, {noise + 16, 16}};
    Require(sceKernelReadv(random, pieces, 2) == 32);
    Require(sceKernelPreadv(random, pieces, 1, 1000) == 16);
    Require(sceKernelPread(random, noise + 32, 16, 1000) == 16);
    Require(std::memcmp(noise, noise + 16, 16) != 0 && std::memcmp(noise + 16, noise + 32, 16) != 0);
    Require(sceKernelClose(random) == 0);

    std::filesystem::remove_all(root);
    return 0;
}
