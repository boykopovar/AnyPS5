#include "prx/libkernel/include/Api.hpp"
#include "prx/libkernel/File/include/File.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iterator>
#include <set>
#include <string>
#include <thread>
#include <utility>
#include <vector>
#ifdef _WIN32
#include <windows.h>
#endif

static void Check(bool condition, int line) {
    if (!condition) {
        std::fprintf(stderr, "append check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

static constexpr const char* Path = "guest_append_writes.txt";
static constexpr int CreateWriteAppend = 0x0209;
static constexpr int Truncate = 0x0400;
static constexpr int FromStart = 0;
static constexpr int FromCurrent = 1;
static constexpr int Writers = 5;
static constexpr int LinesPerWriter = 1500;
static constexpr std::size_t LineLength = 32;

static std::string Line(int writer, int index) {
    std::string line = "w" + std::to_string(writer) + " " + std::to_string(index) + " ";
    line.resize(LineLength - 1, '.');
    return line + "\n";
}

static void WriteLines(int fd, int writer) {
    for (int index = 0; index < LinesPerWriter; ++index) {
        std::string line = Line(writer, index);
        std::int64_t written = -1;
        if (writer == 0 || writer == 2) {
            written = sceKernelWrite(fd, line.data(), line.size());
        } else if (writer == 1) {
            written = write_nid_postfix(fd, line.data(), static_cast<std::int64_t>(line.size()));
        } else if (writer == 3) {
            const KernelIovec parts[2]{{line.data(), 7}, {line.data() + 7, line.size() - 7}};
            written = writev_nid_postfix(fd, parts, 2);
        } else {
            const KernelIovec parts[2]{{line.data(), 7}, {line.data() + 7, line.size() - 7}};
            written = pwritev_nid_postfix(fd, parts, 2, 0);
        }
        Require(written == static_cast<std::int64_t>(LineLength));
    }
}

static std::string Contents() {
    std::ifstream file(Path, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
}

int main() {
    const int first = sceKernelOpen(Path, CreateWriteAppend | Truncate, 0644);
    Require(first >= 0);
    const int second = sceKernelOpen(Path, CreateWriteAppend, 0644);
    const int duplicate = dup_nid_postfix(first);
    const int vectored = sceKernelOpen(Path, CreateWriteAppend, 0644);
    const int positionedVector = sceKernelOpen(Path, CreateWriteAppend, 0644);
    Require(second >= 0 && duplicate >= 0 && vectored >= 0 && positionedVector >= 0);

    const int descriptors[Writers]{first, second, duplicate, vectored, positionedVector};
    std::vector<std::thread> threads;
    for (int writer = 0; writer < Writers; ++writer) threads.emplace_back(WriteLines, descriptors[writer], writer);
    for (auto& thread : threads) thread.join();

    const std::string contents = Contents();
    Require(contents.size() == static_cast<std::size_t>(Writers) * LinesPerWriter * LineLength);
    std::set<std::pair<int, int>> seen;
    for (std::size_t offset = 0; offset < contents.size(); offset += LineLength) {
        std::string line = contents.substr(offset, LineLength);
        int writer = -1;
        int index = -1;
        Require(std::sscanf(line.c_str(), "w%d %d ", &writer, &index) == 2);
        Require(writer >= 0 && writer < Writers && index >= 0 && index < LinesPerWriter);
        Require(line == Line(writer, index));
        Require(seen.emplace(writer, index).second);
    }
    Require(seen.size() == static_cast<std::size_t>(Writers) * LinesPerWriter);

    const auto size = static_cast<std::int64_t>(contents.size());
    Require(sceKernelLseek(first, 0, FromStart) == 0);
    const std::string tail = Line(9, 0);
    Require(sceKernelWrite(first, tail.data(), tail.size()) == static_cast<std::int64_t>(LineLength));
    Require(sceKernelLseek(first, 0, FromCurrent) == size + static_cast<std::int64_t>(LineLength));
    Require(Contents().substr(static_cast<std::size_t>(size)) == tail);

    const std::string head = Contents().substr(0, LineLength);
    const std::string positioned = Line(9, 1);
    Require(sceKernelLseek(first, 0, FromStart) == 0);
    Require(sceKernelPwrite(first, positioned.data(), positioned.size(), 0) == static_cast<std::int64_t>(LineLength));
    Require(sceKernelLseek(first, 0, FromCurrent) == 0);
    std::string gathered = Line(9, 2);
    const KernelIovec pieces[2]{{gathered.data(), 7}, {gathered.data() + 7, gathered.size() - 7}};
    Require(pwritev_nid_postfix(first, pieces, 2, static_cast<std::int64_t>(LineLength)) == static_cast<std::int64_t>(LineLength));
    Require(sceKernelLseek(first, 0, FromCurrent) == 0);
    const std::string appended = Contents();
    Require(appended.size() == static_cast<std::size_t>(size) + 3 * LineLength);
    Require(appended.substr(0, LineLength) == head);
    Require(appended.substr(static_cast<std::size_t>(size) + LineLength) == positioned + gathered);

#ifdef _WIN32
    void* unreadable = VirtualAlloc(nullptr, 4096, MEM_COMMIT | MEM_RESERVE, PAGE_NOACCESS);
    Require(unreadable != nullptr);
    char validChar = 'x';
    const KernelIovec badPieces[2]{{&validChar, 1}, {unreadable, 10}};
    const KernelIovec badPiecesFirst[2]{{unreadable, 10}, {&validChar, 1}};
    const KernelIovec badPiecesZeroLength[2]{{unreadable, 0}, {&validChar, 1}};

    *__error_nid_postfix() = 0;
    Require(writev_nid_postfix(first, badPieces, 2) == -1 && *__error_nid_postfix() == 14);
    *__error_nid_postfix() = 0;
    Require(pwritev_nid_postfix(first, badPieces, 2, 0) == -1 && *__error_nid_postfix() == 14);
    Require(sceKernelWritev(first, badPieces, 2) == static_cast<int>(0x8002000eu));
    Require(sceKernelPwritev(first, badPieces, 2, 0) == static_cast<int>(0x8002000eu));

    *__error_nid_postfix() = 0;
    Require(writev_nid_postfix(first, badPiecesFirst, 2) == -1 && *__error_nid_postfix() == 14);
    *__error_nid_postfix() = 0;
    Require(pwritev_nid_postfix(first, badPiecesFirst, 2, 0) == -1 && *__error_nid_postfix() == 14);
    Require(sceKernelWritev(first, badPiecesFirst, 2) == static_cast<int>(0x8002000eu));
    Require(sceKernelPwritev(first, badPiecesFirst, 2, 0) == static_cast<int>(0x8002000eu));

    Require(Contents().size() == static_cast<std::size_t>(size) + 3 * LineLength);

    Require(writev_nid_postfix(first, badPiecesZeroLength, 2) == 1);
    Require(Contents().size() == static_cast<std::size_t>(size) + 3 * LineLength + 1);

    VirtualFree(unreadable, 0, MEM_RELEASE);
#endif

    for (const int fd : descriptors) Require(sceKernelClose(fd) == 0);
    std::remove(Path);
}
