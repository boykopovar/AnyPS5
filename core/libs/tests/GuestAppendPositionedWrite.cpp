#include "prx/libkernel/include/Api.hpp"
#include "prx/libkernel/File/include/File.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libkernel/File/include/FileFlags.hpp"
#include <array>
#include <atomic>
#include <barrier>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <thread>
#include <vector>

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "Append positioned write check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

int main() {
    constexpr int Records = 2000;
    constexpr int Seeks = 20000;
    constexpr std::size_t RecordBytes = 16;
    constexpr std::int64_t InitialBytes = 64;
    const auto root = std::filesystem::path("anyps5-append-positioned-write-test-" +
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    Require(std::filesystem::create_directory(root));
    const auto path = (root / "data").string();
    { std::ofstream stream(path, std::ios::binary); stream << std::string(InitialBytes, '.'); Require(stream.good()); }
    const int descriptor = sceKernelOpen(path.c_str(), SCE_KERNEL_O_RDWR | SCE_KERNEL_O_APPEND, 0);
    Require(descriptor >= 0);
    const int duplicate = dup_nid_postfix(descriptor);
    Require(duplicate >= 0);
    std::barrier start(3);
    std::atomic<int> moved{0};
    std::thread single([&] {
        std::array<char, RecordBytes> record;
        record.fill('p');
        start.arrive_and_wait();
        for (int i = 0; i < Records; ++i) Require(sceKernelPwrite(duplicate, record.data(), record.size(), 0) == RecordBytes);
    });
    std::thread vector([&] {
        std::array<char, RecordBytes / 2> head;
        std::array<char, RecordBytes / 2> tail;
        head.fill('v');
        tail.fill('v');
        const KernelIovec iov[2]{{head.data(), head.size()}, {tail.data(), tail.size()}};
        start.arrive_and_wait();
        for (int i = 0; i < Records; ++i) Require(sceKernelPwritev(descriptor, iov, 2, 0) == RecordBytes);
    });
    start.arrive_and_wait();
    for (int i = 0; i < Seeks; ++i) {
        const std::int64_t target = i % InitialBytes;
        Require(sceKernelLseek(descriptor, target, SEEK_SET) == target);
        if (sceKernelLseek(descriptor, 0, SEEK_CUR) != target) moved.fetch_add(1);
    }
    single.join();
    vector.join();
    Require(moved.load() == 0);
    constexpr std::int64_t FinalBytes = InitialBytes + 2 * Records * static_cast<std::int64_t>(RecordBytes);
    Require(std::filesystem::file_size(path) == static_cast<std::uintmax_t>(FinalBytes));
    std::vector<char> contents(static_cast<std::size_t>(FinalBytes));
    Require(sceKernelPread(descriptor, contents.data(), contents.size(), 0) == FinalBytes);
    int singleRecords = 0;
    int vectorRecords = 0;
    for (std::size_t offset = InitialBytes; offset < contents.size(); offset += RecordBytes) {
        const char first = contents[offset];
        for (std::size_t i = 1; i < RecordBytes; ++i) Require(contents[offset + i] == first);
        singleRecords += first == 'p' ? 1 : 0;
        vectorRecords += first == 'v' ? 1 : 0;
    }
    Require(singleRecords == Records && vectorRecords == Records);
    Require(sceKernelLseek(descriptor, 5, SEEK_SET) == 5);
    Require(sceKernelPwrite(duplicate, "!", 1, 0) == 1);
    Require(sceKernelLseek(duplicate, 0, SEEK_CUR) == 5 && sceKernelLseek(descriptor, 0, SEEK_CUR) == 5);
    Require(sceKernelClose(duplicate) == 0);
    Require(sceKernelClose(descriptor) == 0);
    std::filesystem::remove_all(root);
}
