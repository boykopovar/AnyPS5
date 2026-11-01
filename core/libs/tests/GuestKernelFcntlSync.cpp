#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <source_location>
#include <string>

extern "C" std::uint64_t FileSynchronousWritesForTest_nid_no_patch();

namespace {

constexpr int GetFlags = 3;
constexpr int SetFlags = 4;
constexpr int Synchronous = 0x80;
constexpr int ReadWrite = 2;
constexpr int Create = 0x200;

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "check failed at line %u\n", static_cast<unsigned>(location.line()));
    std::exit(1);
}

std::uint64_t Synced() {
    return FileSynchronousWritesForTest_nid_no_patch();
}

void Toggle(const std::string& path) {
    const int file = open_nid_postfix(path.c_str(), ReadWrite | Create, 0600);
    Require(file >= 0);
    Require((fcntl_nid_postfix(file, GetFlags) & Synchronous) == 0);
    auto before = Synced();
    Require(write_nid_postfix(file, "a", 1) == 1 && Synced() == before);

    Require(fcntl_nid_postfix(file, SetFlags, Synchronous) == 0);
    Require((fcntl_nid_postfix(file, GetFlags) & Synchronous) != 0);
    before = Synced();
    Require(write_nid_postfix(file, "b", 1) == 1 && Synced() == before + 1);
    char c[] = "c", d[] = "d", y[] = "y", z[] = "z";
    const KernelIovec vectors[] = {{c, 1}, {d, 1}};
    Require(writev_nid_postfix(file, vectors, 2) == 2 && Synced() == before + 2);
    Require(pwrite_nid_disambig1_nid_postfix(file, "x", 1, 10) == 1 && Synced() == before + 3);
    const KernelIovec tail[] = {{y, 1}, {z, 1}};
    Require(pwritev_nid_postfix(file, tail, 2, 11) == 2 && Synced() == before + 4);

    const int duplicate = dup_nid_postfix(file);
    Require(duplicate >= 0 && (fcntl_nid_postfix(duplicate, GetFlags) & Synchronous) != 0);
    Require(write_nid_postfix(duplicate, "e", 1) == 1 && Synced() == before + 5);

    Require(fcntl_nid_postfix(duplicate, SetFlags, 0) == 0);
    Require((fcntl_nid_postfix(file, GetFlags) & Synchronous) == 0);
    before = Synced();
    Require(write_nid_postfix(file, "f", 1) == 1 && Synced() == before);

    char contents[13] = {};
    Require(pread_nid_postfix(file, contents, sizeof(contents), 0) == 13);
    Require(std::memcmp(contents, "abcdef", 6) == 0 && std::memcmp(contents + 10, "xyz", 3) == 0);
    Require(close_nid_postfix(duplicate) == 0 && close_nid_postfix(file) == 0);
}

void OpenedSynchronous(const std::string& path) {
    const int file = open_nid_postfix(path.c_str(), ReadWrite | Create | Synchronous, 0600);
    Require(file >= 0 && (fcntl_nid_postfix(file, GetFlags) & Synchronous) != 0);
    const auto before = Synced();
    Require(write_nid_postfix(file, "h", 1) == 1 && Synced() == before + 1);
    Require(close_nid_postfix(file) == 0);
}

void Pipes() {
    int ends[2] = {-1, -1};
    Require(pipe_nid_postfix(ends) == 0);
    Require(fcntl_nid_postfix(ends[1], SetFlags, Synchronous) == 0);
    Require((fcntl_nid_postfix(ends[1], GetFlags) & Synchronous) != 0);
    const auto before = Synced();
    Require(write_nid_postfix(ends[1], "i", 1) == 1 && Synced() == before);
    Require(close_nid_postfix(ends[0]) == 0 && close_nid_postfix(ends[1]) == 0);
}

}

int main() {
    const auto stamp = std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    const std::string first = "anyps5-fsync-" + stamp + ".bin";
    const std::string second = "anyps5-fsync-open-" + stamp + ".bin";
    Toggle(first);
    OpenedSynchronous(second);
    Pipes();
    std::filesystem::remove(first);
    std::filesystem::remove(second);
    std::puts("guest_kernel_fcntl_sync passed");
    return 0;
}
