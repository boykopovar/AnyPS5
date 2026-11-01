#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/FileStream.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <set>
#include <string>
#include <thread>
#include <vector>

static void Check(bool condition, int line) {
    if (!condition) {
        std::fprintf(stderr, "append stream check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

static constexpr const char* Path = "guest_append_streams.txt";
static constexpr int Writers = 4;
static constexpr int LinesPerWriter = 1500;
static constexpr std::size_t LineLength = 32;
static constexpr int LockExclusive = 2;
static constexpr int LockNonblocking = 4;
static constexpr int LockUnlock = 8;
static constexpr int GuestEwouldblock = 35;

static std::string Line(int writer, int index) {
    std::string line = "s" + std::to_string(writer) + " " + std::to_string(index) + " ";
    line.resize(LineLength - 1, '.');
    return line + "\n";
}

static std::string Contents() {
    std::ifstream input(Path, std::ios::binary);
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

static FileStream* OpenWriter(int writer) {
    if (writer == 0) return fopen_nid_postfix(Path, "a");
    if (writer == 1) return fopen_nid_postfix(Path, "a+");
    const std::string other = std::string(Path) + "." + std::to_string(writer);
    auto* stream = fopen_nid_postfix(other.c_str(), "w");
    Require(stream != nullptr);
    return freopen_nid_postfix(Path, writer == 2 ? "a" : "ab", stream);
}

static void WriteLines(int writer) {
    auto* stream = OpenWriter(writer);
    Require(stream != nullptr);
    for (int index = 0; index < LinesPerWriter; ++index) {
        const std::string line = Line(writer, index);
        Require(fwrite_nid_postfix(line.data(), 1, line.size(), stream) == line.size());
        Require(fflush_nid_postfix(stream) == 0);
    }
    Require(fclose_nid_postfix(stream) == 0);
}

static void CheckConcurrentStreams() {
    std::filesystem::remove(Path);
    std::vector<std::thread> writers;
    for (int writer = 0; writer < Writers; ++writer) writers.emplace_back(WriteLines, writer);
    for (auto& writer : writers) writer.join();
    const std::string contents = Contents();
    Require(contents.size() == static_cast<std::size_t>(Writers) * LinesPerWriter * LineLength);
    std::set<std::string> lines;
    for (std::size_t offset = 0; offset < contents.size(); offset += LineLength) lines.insert(contents.substr(offset, LineLength));
    for (int writer = 0; writer < Writers; ++writer)
        for (int index = 0; index < LinesPerWriter; ++index) Require(lines.count(Line(writer, index)) == 1);
}

static void CheckReadAndAppend() {
    const std::string before = Contents();
    auto* stream = fopen_nid_postfix(Path, "a+");
    Require(stream != nullptr && fseek_nid_postfix(stream, 0, SEEK_SET) == 0);
    char start[LineLength]{};
    Require(fread_nid_postfix(start, 1, LineLength, stream) == LineLength && before.compare(0, LineLength, start, LineLength) == 0);
    Require(fseek_nid_postfix(stream, 0, SEEK_SET) == 0 && fwrite_nid_postfix("x", 1, 1, stream) == 1 && fflush_nid_postfix(stream) == 0);
    Require(ftell_nid_postfix(stream) == static_cast<std::int64_t>(before.size() + 1));
    Require(fclose_nid_postfix(stream) == 0);
    Require(Contents() == before + "x");
}

static void CheckTruncate() {
    const std::string before = Contents();
    auto* stream = fopen_nid_postfix(Path, "a");
    Require(stream != nullptr);
    Require(ftruncate_nid_postfix(fileno_nid_postfix(stream), 5) == 0);
    Require(fwrite_nid_postfix("yz", 1, 2, stream) == 2 && fflush_nid_postfix(stream) == 0);
    Require(fclose_nid_postfix(stream) == 0);
    Require(Contents() == before.substr(0, 5) + "yz");
}

static void CheckLock() {
    auto* holder = fopen_nid_postfix(Path, "a");
    auto* other = fopen_nid_postfix(Path, "r");
    Require(holder != nullptr && other != nullptr);
    Require(flock_nid_postfix(fileno_nid_postfix(holder), LockExclusive | LockNonblocking) == 0);
    Require(flock_nid_postfix(fileno_nid_postfix(other), LockExclusive | LockNonblocking) == -1 && *__error_nid_postfix() == GuestEwouldblock);
    Require(flock_nid_postfix(fileno_nid_postfix(holder), LockUnlock) == 0);
    Require(flock_nid_postfix(fileno_nid_postfix(other), LockExclusive | LockNonblocking) == 0);
    Require(flock_nid_postfix(fileno_nid_postfix(other), LockUnlock) == 0);
    Require(flock_nid_postfix(fileno_nid_postfix(holder), LockExclusive | LockNonblocking) == 0);
    Require(fclose_nid_postfix(holder) == 0);
    Require(flock_nid_postfix(fileno_nid_postfix(other), LockExclusive | LockNonblocking) == 0);
    Require(fclose_nid_postfix(other) == 0);
}

int main() {
    CheckConcurrentStreams();
    CheckReadAndAppend();
    CheckTruncate();
    CheckLock();
    std::filesystem::remove(Path);
    for (int writer = 2; writer < Writers; ++writer) std::filesystem::remove(std::string(Path) + "." + std::to_string(writer));
    return 0;
}
