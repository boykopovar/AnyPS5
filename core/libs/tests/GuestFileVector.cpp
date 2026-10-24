#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libkernel/File/include/FileFlags.hpp"
#include <array>
#include <barrier>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <string>
#include <thread>
#include <vector>
#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
static int MakePipe(int* ends) { return ::_pipe(ends, 64, _O_BINARY); }
static int ClosePipe(int end) { return ::_close(end); }
#else
#include <fcntl.h>
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
int APS5_VABI dup_nid_postfix(int);
std::int64_t APS5_VABI sceKernelRead(int, void*, std::size_t);
std::int64_t APS5_VABI sceKernelPread(int, void*, std::size_t, std::int64_t);
int APS5_VABI sceKernelLseek(int, std::int64_t, int);
std::int64_t APS5_VABI sceKernelReadv(int, const GuestIovec*, int);
std::int64_t APS5_VABI readv_nid_postfix(int, const GuestIovec*, int);
std::int64_t APS5_VABI writev_nid_postfix(int, const GuestIovec*, int);
std::int64_t APS5_VABI _readv_nid_postfix(int, const GuestIovec*, int);
std::int64_t APS5_VABI _writev_nid_postfix(int, const GuestIovec*, int);
int* APS5_VABI __error_nid_postfix();
std::int64_t APS5_VABI sceKernelWritev(int, const GuestIovec*, int);
std::int64_t APS5_VABI sceKernelPreadv(int, const GuestIovec*, int, std::int64_t);
std::int64_t APS5_VABI sceKernelPwritev(int, const GuestIovec*, int, std::int64_t);
std::int64_t APS5_VABI preadv_nid_postfix(int, const GuestIovec*, int, std::int64_t);
std::int64_t APS5_VABI pwritev_nid_postfix(int, const GuestIovec*, int, std::int64_t);
int* APS5_VABI __error_nid_postfix();
std::int64_t APS5_VABI sceKernelPread(int, void*, std::size_t, std::int64_t);
std::int64_t APS5_VABI sceKernelPwrite(int, const void*, std::size_t, std::int64_t);
std::int64_t APS5_VABI pread_nid_postfix(int, void*, std::size_t, std::int64_t);
std::int64_t APS5_VABI pwrite_nid_disambig1_nid_postfix(int, const void*, std::size_t, std::int64_t);
int APS5_VABI sceKernelFtruncate(int, std::int64_t);
int APS5_VABI kqueue_nid_postfix();
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
static constexpr std::int64_t ErrorEagain = static_cast<int>(0x80020023u);

static std::string Contents(const std::filesystem::path& path) {
    std::ifstream stream(path, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
}

static void ConcurrentRecords(const std::filesystem::path& path) {
    constexpr std::size_t Workers = 4;
    constexpr std::size_t Records = 256;
    constexpr std::size_t Parts = 32;
    constexpr std::size_t RecordBytes = Parts * sizeof(std::uint16_t);
    { std::ofstream stream(path, std::ios::binary); Require(static_cast<bool>(stream)); }
    std::array<int, Workers> descriptors{};
    descriptors[0] = sceKernelOpen(path.string().c_str(), SCE_KERNEL_O_RDWR, 0);
    Require(descriptors[0] >= 0);
    for (std::size_t worker = 1; worker < Workers; ++worker) {
        descriptors[worker] = dup_nid_postfix(descriptors[0]);
        Require(descriptors[worker] >= 0);
    }
    std::barrier start(static_cast<std::ptrdiff_t>(Workers));
    std::array<std::thread, Workers> writers;
    for (std::size_t worker = 0; worker < Workers; ++worker) {
        writers[worker] = std::thread([&, worker] {
            std::uint16_t label = 0;
            std::array<GuestIovec, Parts> pieces;
            pieces.fill({&label, sizeof(label)});
            start.arrive_and_wait();
            for (std::size_t record = 0; record < Records; ++record) {
                label = static_cast<std::uint16_t>(worker * Records + record);
                const auto written = worker % 2 == 0 ? sceKernelWritev(descriptors[worker], pieces.data(), static_cast<int>(pieces.size()))
                                                     : writev_nid_postfix(descriptors[worker], pieces.data(), static_cast<int>(pieces.size()));
                Require(written == RecordBytes);
            }
        });
    }
    for (auto& writer : writers) writer.join();
    const auto bytes = Contents(path);
    Require(bytes.size() == Workers * Records * RecordBytes);
    std::array<unsigned, Workers * Records> seen{};
    for (std::size_t offset = 0; offset < bytes.size(); offset += RecordBytes) {
        std::array<std::uint16_t, Parts> record;
        std::memcpy(record.data(), bytes.data() + offset, RecordBytes);
        Require(record[0] < seen.size());
        for (const auto label : record) {
            if (label != record[0]) std::fprintf(stderr, "writev interleaved a record at byte %zu: %u versus %u\n", offset, static_cast<unsigned>(record[0]), static_cast<unsigned>(label));
            Require(label == record[0]);
        }
        ++seen[record[0]];
    }
    for (const auto count : seen) Require(count == 1);
    Require(sceKernelLseek(descriptors[0], 0, SEEK_SET) == 0);
    std::array<std::vector<std::uint16_t>, Workers> labels;
    std::array<std::thread, Workers> readers;
    for (std::size_t worker = 0; worker < Workers; ++worker) {
        readers[worker] = std::thread([&, worker] {
            std::array<std::uint16_t, Parts> record{};
            std::array<GuestIovec, Parts> pieces;
            for (std::size_t part = 0; part < Parts; ++part) pieces[part] = {&record[part], sizeof(record[part])};
            start.arrive_and_wait();
            for (;;) {
                const auto read = worker % 2 == 0 ? sceKernelReadv(descriptors[worker], pieces.data(), static_cast<int>(pieces.size()))
                                                 : readv_nid_postfix(descriptors[worker], pieces.data(), static_cast<int>(pieces.size()));
                if (read == 0) break;
                Require(read == RecordBytes);
                for (const auto label : record) Require(label == record[0]);
                labels[worker].push_back(record[0]);
            }
        });
    }
    for (auto& reader : readers) reader.join();
    seen.fill(0);
    for (const auto& records : labels) for (const auto label : records) {
        Require(label < seen.size());
        ++seen[label];
    }
    for (const auto count : seen) Require(count == 1);
    Require(sceKernelLseek(descriptors[0], 0, SEEK_CUR) == static_cast<std::int64_t>(bytes.size()));
#ifdef _WIN32
    std::array<std::thread, Workers> positioned;
    for (std::size_t worker = 0; worker < Workers; ++worker) {
        positioned[worker] = std::thread([&, worker] {
            std::array<std::uint16_t, Parts> record{};
            std::array<GuestIovec, Parts> pieces;
            for (std::size_t part = 0; part < Parts; ++part) pieces[part] = {&record[part], sizeof(record[part])};
            start.arrive_and_wait();
            for (std::size_t iteration = 0; iteration < Records; ++iteration) {
                std::int64_t transferred;
                if (worker % 2 == 0) {
                    record.fill(static_cast<std::uint16_t>(0x8000 + worker * Records + iteration));
                    transferred = worker == 0 ? sceKernelPwritev(descriptors[worker], pieces.data(), static_cast<int>(pieces.size()), 0)
                                              : pwritev_nid_postfix(descriptors[worker], pieces.data(), static_cast<int>(pieces.size()), 0);
                } else {
                    transferred = worker == 1 ? sceKernelPreadv(descriptors[worker], pieces.data(), static_cast<int>(pieces.size()), 0)
                                              : preadv_nid_postfix(descriptors[worker], pieces.data(), static_cast<int>(pieces.size()), 0);
                    for (const auto label : record) Require(label == record[0]);
                }
                Require(transferred == RecordBytes);
            }
        });
    }
    for (auto& transfer : positioned) transfer.join();
    Require(sceKernelLseek(descriptors[0], 0, SEEK_CUR) == static_cast<std::int64_t>(bytes.size()));
#endif
    for (const auto descriptor : descriptors) Require(sceKernelClose(descriptor) == 0);
}

int main() {
    const auto root = std::filesystem::path("anyps5-file-vector-test-" +
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    Require(std::filesystem::create_directory(root));
    ConcurrentRecords(root / "concurrent.bin");
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
    Require(sceKernelLseek(file, 0, 0) == 0);
    Require(readv_nid_postfix(file, reads, 2) == 7);
    Require(std::memcmp(first, "012", 3) == 0 && std::memcmp(second, "3456", 4) == 0);
    Require(sceKernelLseek(file, 0, 1) == 7);
    Require(readv_nid_postfix(file, reads, 2) == 3);
    Require(readv_nid_postfix(file, reads, 2) == 0);
    Require(sceKernelLseek(file, 3, 0) == 3 && _readv_nid_postfix(file, reads, 2) == 7);
    Require(std::memcmp(first, "345", 3) == 0 && std::memcmp(second, "6789", 4) == 0);
    Require(readv_nid_postfix(file, reads, -1) == -1 && *__error_nid_postfix() == 22);
    Require(writev_nid_postfix(file, nullptr, 1) == -1 && *__error_nid_postfix() == 14);

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
    GuestIovec swapped[2] = {{cde, 3}, {ab, 2}};
    Require(sceKernelLseek(file, 8, 0) == 8 && writev_nid_postfix(file, swapped, 2) == 5);
    Require(sceKernelLseek(file, 0, 1) == 13 && Contents(path) == "01ABCDE7CDEAB");
    Require(sceKernelLseek(file, 8, 0) == 8 && writev_nid_postfix(file, writes, 2) == 5);
    Require(Contents(path) == "01ABCDE7ABCDE");
    Require(sceKernelLseek(file, 8, 0) == 8 && _writev_nid_postfix(file, swapped, 2) == 5);
    Require(sceKernelLseek(file, 0, 1) == 13 && Contents(path) == "01ABCDE7CDEAB");
    Require(sceKernelLseek(file, 8, 0) == 8 && writev_nid_postfix(file, writes, 2) == 5);

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

    Require(sceKernelLseek(file, 4, 0) == 4);
    Require(preadv_nid_postfix(file, reads, 2, 1) == 7);
    Require(std::memcmp(first, "1AB", 3) == 0 && std::memcmp(second, "CDE7", 4) == 0);
    Require(sceKernelLseek(file, 0, 1) == 4);
    Require(preadv_nid_postfix(file, reads, 2, 13) == 0);
    Require(preadv_nid_postfix(file, reads, 2, -1) == -1 && *__error_nid_postfix() == 22);
    Require(preadv_nid_postfix(file, reads, -1, 0) == -1 && *__error_nid_postfix() == 22);
    Require(preadv_nid_postfix(file, nullptr, 1, 0) == -1 && *__error_nid_postfix() == 14);
    Require(pwritev_nid_postfix(file, writes, 2, 8) == 5);
    Require(sceKernelLseek(file, 0, 1) == 4);
    Require(Contents(path) == "01ABCDE7ABCDE");
    char x[] = "X";
    char y[] = "Y";
    GuestIovec marks[2] = {{x, 1}, {y, 1}};
    Require(pwritev_nid_postfix(file, marks, 2, 0) == 2);
    Require(Contents(path) == "XYABCDE7ABCDE");
    char zero[] = "0";
    char one[] = "1";
    GuestIovec restore[2] = {{zero, 1}, {one, 1}};
    Require(pwritev_nid_postfix(file, restore, 2, 0) == 2);
    Require(sceKernelLseek(file, 0, 1) == 4);
    Require(pwritev_nid_postfix(file, writes, 2, -1) == -1 && *__error_nid_postfix() == 22);
    Require(pwritev_nid_postfix(file, nullptr, 1, 0) == -1 && *__error_nid_postfix() == 14);

    char byte = 0;
    Require(sceKernelPread(file, &byte, 1, 2) == 1 && byte == 'A');
    Require(sceKernelPread(file, nullptr, 0, 0) == 0);
    Require(sceKernelPwrite(file, nullptr, 0, 0) == 0);
    Require(sceKernelPread(file, nullptr, 1, 0) == ErrorEfault);
    Require(sceKernelPwrite(file, nullptr, 1, 0) == ErrorEfault);
    Require(sceKernelPread(file, &byte, 1, -1) == ErrorEinval);
    Require(sceKernelPwrite(file, &byte, 1, -1) == ErrorEinval);
    Require(pread_nid_postfix(file, &byte, 1, 3) == 1 && byte == 'B');
    Require(pread_nid_postfix(file, nullptr, 0, 0) == 0);
    Require(pread_nid_postfix(file, nullptr, 1, 0) == -1 && *__error_nid_postfix() == 14);
    Require(pwrite_nid_disambig1_nid_postfix(file, nullptr, 1, 0) == -1 && *__error_nid_postfix() == 14);
    Require(pread_nid_postfix(file, &byte, 1, -1) == -1 && *__error_nid_postfix() == 22);
    Require(pwrite_nid_disambig1_nid_postfix(file, &byte, 1, -1) == -1 && *__error_nid_postfix() == 22);
    Require(pread_nid_postfix(9999, &byte, 1, 0) == -1 && *__error_nid_postfix() == 9);
    Require(pwrite_nid_disambig1_nid_postfix(9999, &byte, 1, 0) == -1 && *__error_nid_postfix() == 9);
    Require(sceKernelFtruncate(file, -1) == ErrorEinval);
    const int readOnly = sceKernelOpen(path.string().c_str(), SCE_KERNEL_O_RDONLY, 0);
    Require(readOnly >= 0);
    Require(sceKernelPwrite(readOnly, "X", 1, 0) == ErrorEbadf);
    Require(sceKernelPwrite(readOnly, "X", 1, -1) == ErrorEbadf);
    Require(pwrite_nid_disambig1_nid_postfix(readOnly, nullptr, 1, 0) == -1 && *__error_nid_postfix() == 9);
    Require(sceKernelPwritev(readOnly, writes, 2, -1) == ErrorEbadf);
    Require(pwritev_nid_postfix(readOnly, writes, 2, -1) == -1 && *__error_nid_postfix() == 9);
    Require(sceKernelPreadv(readOnly, reads, 2, -1) == ErrorEinval);
    Require(sceKernelFtruncate(readOnly, 0) == ErrorEinval);
    Require(sceKernelClose(readOnly) == 0);
    const int writeOnly = sceKernelOpen(path.string().c_str(), SCE_KERNEL_O_WRONLY, 0);
    Require(writeOnly >= 0);
    Require(sceKernelPread(writeOnly, &byte, 1, 0) == ErrorEbadf);
    Require(pread_nid_postfix(writeOnly, &byte, 1, 0) == -1 && *__error_nid_postfix() == 9);
    Require(sceKernelPread(writeOnly, &byte, 1, -1) == ErrorEbadf);
    Require(pread_nid_postfix(writeOnly, nullptr, 1, 0) == -1 && *__error_nid_postfix() == 9);
    Require(sceKernelPreadv(writeOnly, reads, 2, -1) == ErrorEbadf);
    Require(preadv_nid_postfix(writeOnly, reads, 2, -1) == -1 && *__error_nid_postfix() == 9);
    Require(sceKernelPwritev(writeOnly, writes, 2, -1) == ErrorEinval);
    Require(sceKernelClose(writeOnly) == 0);
    Require(Contents(path) == "01ABCDE7ABCDE");
    Require(sceKernelFtruncate(file, 5) == 0);
    Require(Contents(path) == "01ABC");

    Require(sceKernelClose(file) == 0);
    Require(sceKernelReadv(file, reads, 2) == ErrorEbadf);
    Require(sceKernelWritev(file, writes, 2) == ErrorEbadf);
    Require(sceKernelPreadv(file, reads, 2, 0) == ErrorEbadf);
    Require(sceKernelPwritev(file, writes, 2, 0) == ErrorEbadf);
    Require(sceKernelPread(file, &byte, 1, 0) == ErrorEbadf);
    Require(sceKernelPwrite(file, &byte, 1, 0) == ErrorEbadf);
    Require(sceKernelFtruncate(file, 0) == ErrorEbadf);
    Require(sceKernelPreadv(file, reads, 2, -1) == ErrorEbadf);
    Require(sceKernelPwritev(file, writes, 2, -1) == ErrorEbadf);
    Require(preadv_nid_postfix(file, reads, 2, -1) == -1 && *__error_nid_postfix() == 9);
    Require(pwritev_nid_postfix(file, writes, 2, -1) == -1 && *__error_nid_postfix() == 9);
    GuestIovec oversized[2] = {{first, static_cast<std::size_t>(std::numeric_limits<std::int64_t>::max())}, {second, 1}};
    Require(sceKernelReadv(file, oversized, 2) == ErrorEinval);
    Require(sceKernelWritev(file, oversized, 2) == ErrorEinval);
    Require(sceKernelPreadv(file, oversized, 2, 0) == ErrorEinval);
    Require(sceKernelPwritev(file, oversized, 2, 0) == ErrorEinval);
    Require(readv_nid_postfix(file, oversized, 2) == -1 && *__error_nid_postfix() == 22);
    Require(pwritev_nid_postfix(file, oversized, 2, 0) == -1 && *__error_nid_postfix() == 22);

    int ends[2] = {};
    Require(MakePipe(ends) == 0);
    char xyz[] = "xyz";
    GuestIovec message[1] = {{xyz, 3}};
    Require(sceKernelWritev(ends[1], message, 1) == 3);
    Require(sceKernelReadv(ends[0], reads, 2) == 3);
    Require(std::memcmp(first, "xyz", 3) == 0);
    char stuvw[] = "stuvw";
    GuestIovec longer[1] = {{stuvw, 5}};
    Require(sceKernelWritev(ends[1], longer, 1) == 5);
    Require(sceKernelReadv(ends[0], reads, 2) == 5);
    Require(std::memcmp(first, "stu", 3) == 0 && std::memcmp(second, "vw", 2) == 0);
    Require(sceKernelWritev(ends[1], longer, 1) == 5 && sceKernelWritev(ends[1], longer, 1) == 5);
    Require(readv_nid_postfix(ends[0], reads, 2) == 7);
    Require(std::memcmp(first, "stu", 3) == 0 && std::memcmp(second, "vwst", 4) == 0);
    Require(sceKernelReadv(ends[0], reads, 2) == 3 && std::memcmp(first, "uvw", 3) == 0);
    Require(sceKernelPreadv(ends[0], reads, 2, 0) == ErrorEspipe);
    Require(sceKernelPwritev(ends[1], message, 1, 0) == ErrorEspipe);
    Require(sceKernelPreadv(ends[0], reads, 2, -1) == ErrorEspipe);
    Require(sceKernelPwritev(ends[1], message, 1, -1) == ErrorEspipe);
    Require(preadv_nid_postfix(ends[0], reads, 2, -1) == -1 && *__error_nid_postfix() == 29);
    Require(pwritev_nid_postfix(ends[1], message, 1, -1) == -1 && *__error_nid_postfix() == 29);
#ifndef _WIN32
    Require(::fcntl(ends[0], F_SETFL, ::fcntl(ends[0], F_GETFL) | O_NONBLOCK) == 0);
    Require(sceKernelReadv(ends[0], reads, 2) == ErrorEagain);
    Require(readv_nid_postfix(ends[0], reads, 2) == -1 && *__error_nid_postfix() == 35);
#endif
    Require(ClosePipe(ends[0]) == 0 && ClosePipe(ends[1]) == 0);

    const int queue = kqueue_nid_postfix();
    Require(queue >= 0);
    Require(sceKernelPreadv(queue, reads, 2, -1) == ErrorEspipe);
    Require(sceKernelPwritev(queue, writes, 2, -1) == ErrorEspipe);
    Require(preadv_nid_postfix(queue, reads, 2, -1) == -1 && *__error_nid_postfix() == 29);
    Require(pwritev_nid_postfix(queue, writes, 2, -1) == -1 && *__error_nid_postfix() == 29);
    Require(sceKernelPreadv(queue, reads, 2, 0) == ErrorEspipe);
    Require(sceKernelPwritev(queue, writes, 2, 0) == ErrorEspipe);
    Require(sceKernelPread(queue, &byte, 1, 0) == ErrorEspipe);
    Require(pwrite_nid_disambig1_nid_postfix(queue, &byte, 1, -1) == -1 && *__error_nid_postfix() == 29);
    Require(sceKernelClose(queue) == 0);

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
