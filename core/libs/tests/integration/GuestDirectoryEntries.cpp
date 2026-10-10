#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libkernel/File/include/FileFlags.hpp"

#include <Testing/Test.hpp>

#include <array>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <string>
#include <system_error>

extern "C" {
int APS5_VABI sceKernelOpen(const char*, int, std::uint16_t);
int APS5_VABI sceKernelClose(int);
std::int64_t APS5_VABI sceKernelRead(int, void*, std::size_t);
int APS5_VABI sceKernelLseek(int, std::int64_t, int);
int APS5_VABI sceKernelGetdents(int, char*, int);
int APS5_VABI sceKernelGetdirentries(int, char*, int, std::int64_t*);
int APS5_VABI getdents_nid_postfix(int, char*, int);
int APS5_VABI getdirentries_nid_postfix(int, char*, int, std::int64_t*);
int* APS5_VABI __error_nid_postfix();
std::int64_t APS5_VABI sceKernelPread(int, void*, std::size_t, std::int64_t);
std::int64_t APS5_VABI sceKernelPwrite(int, const void*, std::size_t, std::int64_t);
}

namespace {

using Testing::Case;
using Testing::Require;
using Testing::RequireEqual;

constexpr int errorEinval = static_cast<int>(0x80020016u);
constexpr int errorEfault = static_cast<int>(0x8002000Eu);
constexpr int errorEbadf = static_cast<int>(0x80020009u);
constexpr int posixEinval = 22;
constexpr int posixEfault = 14;
constexpr int posixEbadf = 9;
constexpr int seekSet = 0;
constexpr int seekCurrent = 1;
constexpr std::size_t entryCount = 6;
const std::string longName = "a-much-longer-entry-name-0123456789";

struct Entry {
    std::uint8_t type;
    std::uint16_t recordLength;
};

using Entries = std::map<std::string, Entry>;

class DirectoryFixture {
public:
    DirectoryFixture()
        : root("anyps5-directory-entries-test-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())) {
        Require(std::filesystem::create_directory(root), "create the test directory " + root.string());
        { std::ofstream stream(root / "data.bin", std::ios::binary); stream << "0123456789"; }
        { std::ofstream stream(root / "b"); }
        { std::ofstream stream(root / longName); }
        Require(std::filesystem::create_directory(root / "sub"), "create the sub directory");
    }

    ~DirectoryFixture() {
        std::error_code ignored;
        std::filesystem::remove_all(root, ignored);
    }

    DirectoryFixture(const DirectoryFixture&) = delete;
    DirectoryFixture& operator=(const DirectoryFixture&) = delete;

    const std::filesystem::path root;
};

class Descriptor {
public:
    Descriptor(const std::filesystem::path& path, int flags) : value(sceKernelOpen(path.string().c_str(), flags, 0)) {
        Require(value >= 0, "open " + path.string() + " returned " + std::to_string(value));
    }

    ~Descriptor() {
        if (open) sceKernelClose(value);
    }

    Descriptor(const Descriptor&) = delete;
    Descriptor& operator=(const Descriptor&) = delete;

    int Get() const noexcept { return value; }

    int Close() {
        open = false;
        return sceKernelClose(value);
    }

private:
    int value;
    bool open = true;
};

class OpenDirectory {
public:
    OpenDirectory() : descriptor(fixture.root, SCE_KERNEL_O_RDONLY | SCE_KERNEL_O_DIRECTORY) {}

    int Get() const noexcept { return descriptor.Get(); }
    int Close() { return descriptor.Close(); }

private:
    DirectoryFixture fixture;
    Descriptor descriptor;
};

class OpenDataFile {
public:
    OpenDataFile() : descriptor(fixture.root / "data.bin", SCE_KERNEL_O_RDWR) {}

    int Get() const noexcept { return descriptor.Get(); }
    int Close() { return descriptor.Close(); }

private:
    DirectoryFixture fixture;
    Descriptor descriptor;
};

int Collect(const char* buffer, int bytes, Entries& entries) {
    int records = 0;
    for (int offset = 0; offset < bytes; ++records) {
        std::uint16_t recordLength = 0;
        std::memcpy(&recordLength, buffer + offset + 4, sizeof(recordLength));
        const auto nameLength = static_cast<std::uint8_t>(buffer[offset + 7]);
        const std::string name(buffer + offset + 8);
        const std::string context = "entry \"" + name + "\" at offset " + std::to_string(offset);
        RequireEqual(name.size(), std::size_t{nameLength}, context + " name length");
        RequireEqual(int{recordLength}, (8 + nameLength + 1 + 3) & ~3, context + " record length");
        Require(offset + recordLength <= bytes, context + " fits in the " + std::to_string(bytes) + " returned bytes");
        Require(entries.emplace(name, Entry{static_cast<std::uint8_t>(buffer[offset + 6]), recordLength}).second,
                context + " is not repeated");
        offset += recordLength;
    }
    return records;
}

struct SmallBufferScan {
    Entries entries;
    int calls = 0;
};

SmallBufferScan ScanWithSmallBuffer(int directory) {
    SmallBufferScan scan;
    std::array<char, 48> small{};
    std::int64_t previousBase = -1;
    for (;;) {
        const std::string context = "call " + std::to_string(scan.calls);
        std::int64_t base = -1;
        const int read = sceKernelGetdirentries(directory, small.data(), static_cast<int>(small.size()), &base);
        Require(read >= 0 && read <= static_cast<int>(small.size()), context + " read " + std::to_string(read) + " bytes");
        Require(base >= 0, context + " base " + std::to_string(base) + " non-negative");
        Require(base != previousBase, context + " base " + std::to_string(base) + " differs from the previous call");
        if (scan.calls == 0) RequireEqual(base, std::int64_t{0}, context + " base");
        previousBase = base;
        if (read == 0) break;
        Require(Collect(small.data(), read, scan.entries) > 0, context + " returned at least one entry");
        ++scan.calls;
    }
    return scan;
}

int ReadAllWithSceGetdents(int directory, std::array<char, 4096>& buffer) {
    RequireEqual(sceKernelLseek(directory, 0, seekSet), 0, "rewind before sceKernelGetdents");
    const int read = sceKernelGetdents(directory, buffer.data(), static_cast<int>(buffer.size()));
    Require(read > 0, "sceKernelGetdents read " + std::to_string(read) + " bytes");
    return read;
}

void RequirePosixFailure(int result, int error, const std::string& context) {
    RequireEqual(result, -1, context + " result");
    RequireEqual(*__error_nid_postfix(), error, context + " errno");
}

const Case smallBufferScan{"Getdirentries_SmallBuffer_AdvancesBaseAndReturnsEveryEntryOnce", [] {
    OpenDirectory directory;
    const SmallBufferScan scan = ScanWithSmallBuffer(directory.Get());
    Require(scan.calls >= 3, "at least 3 non-empty calls, got " + std::to_string(scan.calls));
    RequireEqual(scan.entries.size(), entryCount, "entry count");
    for (const std::string name : {".", "..", "data.bin", "b", "sub", longName.c_str()}) {
        Require(scan.entries.contains(name), "entry \"" + name + "\" listed");
    }
}};

const Case recordLengths{"Getdirentries_Entries_UseFourByteAlignedRecordLengths", [] {
    OpenDirectory directory;
    const SmallBufferScan scan = ScanWithSmallBuffer(directory.Get());
    Require(scan.entries.contains(longName), "long entry listed");
    RequireEqual(int{scan.entries.at(longName).recordLength}, 44, "long entry record length");
    Require(scan.entries.contains("b"), "entry b listed");
    RequireEqual(int{scan.entries.at("b").recordLength}, 12, "entry b record length");
}};

const Case entryTypes{"Getdirentries_Entries_ReportDirectoryAndRegularFileTypesOrUnknown", [] {
    OpenDirectory directory;
    const SmallBufferScan scan = ScanWithSmallBuffer(directory.Get());
    Require(scan.entries.contains("sub") && scan.entries.contains("data.bin"), "sub and data.bin listed");
    const int subType = scan.entries.at("sub").type;
    const int fileType = scan.entries.at("data.bin").type;
    Require(subType == 4 || subType == 0, "sub type is directory or unknown, got " + std::to_string(subType));
    Require(fileType == 8 || fileType == 0, "data.bin type is regular or unknown, got " + std::to_string(fileType));
}};

const Case sceTooSmall{"SceKernelGetdents_BufferSmallerThanEntry_FailsWithEinval", [] {
    OpenDirectory directory;
    std::array<char, 8> tooSmall{};
    RequireEqual(sceKernelGetdents(directory.Get(), tooSmall.data(), static_cast<int>(tooSmall.size())), errorEinval, "8 bytes");
}};

const Case sceNullBuffer{"SceKernelGetdents_NullBuffer_FailsWithEfault", [] {
    OpenDirectory directory;
    RequireEqual(sceKernelGetdents(directory.Get(), nullptr, 64), errorEfault, "null buffer");
}};

const Case sceZeroLength{"SceKernelGetdents_ZeroLength_FailsWithEinval", [] {
    OpenDirectory directory;
    std::array<char, 48> small{};
    RequireEqual(sceKernelGetdents(directory.Get(), small.data(), 0), errorEinval, "zero length");
}};

const Case sceLargeBuffer{"SceKernelGetdents_LargeBuffer_ReturnsAllEntriesThenZero", [] {
    OpenDirectory directory;
    std::array<char, 4096> large{};
    Entries all;
    const int readAll = ReadAllWithSceGetdents(directory.Get(), large);
    RequireEqual(Collect(large.data(), readAll, all), static_cast<int>(entryCount), "entries in one call");
    RequireEqual(sceKernelGetdents(directory.Get(), large.data(), static_cast<int>(large.size())), 0, "read at end");
}};

const Case posixInvalidBuffers{"PosixGetdentsAndGetdirentries_InvalidBuffer_FailWithErrno", [] {
    OpenDirectory directory;
    std::array<char, 8> tooSmall{};
    std::array<char, 48> small{};
    RequirePosixFailure(getdents_nid_postfix(directory.Get(), tooSmall.data(), static_cast<int>(tooSmall.size())), posixEinval,
                        "getdents with 8 bytes");
    *__error_nid_postfix() = 0;
    RequirePosixFailure(getdents_nid_postfix(directory.Get(), nullptr, 64), posixEfault, "getdents with null buffer");
    *__error_nid_postfix() = 0;
    RequirePosixFailure(getdirentries_nid_postfix(directory.Get(), small.data(), 0, nullptr), posixEinval,
                        "getdirentries with zero length");
}};

const Case posixSmallBuffer{"PosixGetdirentries_SmallBuffer_ReportsBasePerCallAndKeepsErrno", [] {
    OpenDirectory directory;
    std::array<char, 48> small{};
    std::int64_t base = -1;
    *__error_nid_postfix() = 1234;
    const int firstPart = getdirentries_nid_postfix(directory.Get(), small.data(), static_cast<int>(small.size()), &base);
    Require(firstPart > 0, "first part read " + std::to_string(firstPart) + " bytes");
    RequireEqual(base, std::int64_t{0}, "first part base");
    RequireEqual(*__error_nid_postfix(), 1234, "errno after first part");
    const int secondPart = getdirentries_nid_postfix(directory.Get(), small.data(), static_cast<int>(small.size()), &base);
    Require(secondPart > 0, "second part read " + std::to_string(secondPart) + " bytes");
    Require(base > 0, "second part base " + std::to_string(base) + " positive");
    RequireEqual(*__error_nid_postfix(), 1234, "errno after second part");
}};

const Case posixGetdirentriesAll{"PosixGetdirentries_LargeBufferAfterRewind_MatchesSceGetdentsThenEnds", [] {
    OpenDirectory directory;
    std::array<char, 4096> large{};
    const int readAll = ReadAllWithSceGetdents(directory.Get(), large);
    std::array<char, 48> small{};
    std::int64_t base = -1;
    Require(getdirentries_nid_postfix(directory.Get(), small.data(), static_cast<int>(small.size()), &base) >= 0,
            "getdirentries before rewind");
    RequireEqual(sceKernelLseek(directory.Get(), 0, seekSet), 0, "rewind");
    Entries posix;
    const int posixRead = getdirentries_nid_postfix(directory.Get(), large.data(), static_cast<int>(large.size()), &base);
    RequireEqual(posixRead, readAll, "bytes read");
    RequireEqual(base, std::int64_t{0}, "base");
    RequireEqual(Collect(large.data(), posixRead, posix), static_cast<int>(entryCount), "entries");
    RequireEqual(getdents_nid_postfix(directory.Get(), large.data(), static_cast<int>(large.size())), 0, "getdents at end");
}};

const Case posixGetdentsAll{"PosixGetdents_LargeBufferAfterRewind_MatchesSceGetdents", [] {
    OpenDirectory directory;
    std::array<char, 4096> large{};
    const int readAll = ReadAllWithSceGetdents(directory.Get(), large);
    RequireEqual(sceKernelLseek(directory.Get(), 0, seekSet), 0, "rewind");
    Entries dents;
    const int dentsRead = getdents_nid_postfix(directory.Get(), large.data(), static_cast<int>(large.size()));
    RequireEqual(dentsRead, readAll, "bytes read");
    RequireEqual(Collect(large.data(), dentsRead, dents), static_cast<int>(entryCount), "entries");
}};

const Case posixClosed{"PosixGetdentsAndGetdirentries_ClosedDescriptor_FailWithEbadf", [] {
    OpenDirectory directory;
    std::array<char, 4096> large{};
    RequireEqual(directory.Close(), 0, "close");
    RequirePosixFailure(getdents_nid_postfix(directory.Get(), large.data(), static_cast<int>(large.size())), posixEbadf,
                        "getdents");
    *__error_nid_postfix() = 0;
    RequirePosixFailure(getdirentries_nid_postfix(directory.Get(), large.data(), static_cast<int>(large.size()), nullptr),
                        posixEbadf, "getdirentries");
}};

const Case preadKeepsOffset{"SceKernelPread_AtOffset_ReadsThereWithoutMovingFileOffset", [] {
    OpenDataFile file;
    RequireEqual(sceKernelLseek(file.Get(), 2, seekSet), 2, "seek to 2");
    char bytes[16] = {};
    RequireEqual(sceKernelPread(file.Get(), bytes, 4, 5), std::int64_t{4}, "pread 4 bytes at 5");
    RequireEqual(std::string(bytes, 4), std::string("5678"), "pread payload");
    RequireEqual(sceKernelLseek(file.Get(), 0, seekCurrent), 2, "offset after pread");
}};

const Case pwriteKeepsOffset{"SceKernelPwrite_AtOffset_WritesThereWithoutMovingFileOffset", [] {
    OpenDataFile file;
    RequireEqual(sceKernelLseek(file.Get(), 2, seekSet), 2, "seek to 2");
    RequireEqual(sceKernelPwrite(file.Get(), "XY", 2, 8), std::int64_t{2}, "pwrite 2 bytes at 8");
    RequireEqual(sceKernelLseek(file.Get(), 0, seekCurrent), 2, "offset after pwrite");
    char bytes[16] = {};
    RequireEqual(sceKernelRead(file.Get(), bytes, 3), std::int64_t{3}, "read 3 bytes");
    RequireEqual(std::string(bytes, 3), std::string("234"), "read payload");
    RequireEqual(sceKernelPread(file.Get(), bytes, sizeof(bytes), 0), std::int64_t{10}, "pread whole file");
    RequireEqual(std::string(bytes, 10), std::string("01234567XY"), "file contents");
}};

const Case preadAtEnd{"SceKernelPread_AtEndOfFile_ReturnsZero", [] {
    OpenDataFile file;
    char bytes[16] = {};
    RequireEqual(sceKernelPread(file.Get(), bytes, sizeof(bytes), 10), std::int64_t{0}, "pread at 10");
}};

const Case preadNegative{"SceKernelPread_NegativeOffset_FailsWithEinval", [] {
    OpenDataFile file;
    char bytes[16] = {};
    RequireEqual(sceKernelPread(file.Get(), bytes, 1, -1), std::int64_t{errorEinval}, "pread at -1");
}};

const Case pwriteNull{"SceKernelPwrite_NullBuffer_FailsWithEfault", [] {
    OpenDataFile file;
    RequireEqual(sceKernelPwrite(file.Get(), nullptr, 1, 0), std::int64_t{errorEfault}, "pwrite null buffer");
}};

const Case preadClosed{"SceKernelPread_ClosedDescriptor_FailsWithEbadf", [] {
    OpenDataFile file;
    RequireEqual(file.Close(), 0, "close");
    char bytes[16] = {};
    RequireEqual(sceKernelPread(file.Get(), bytes, 1, 0), std::int64_t{errorEbadf}, "pread after close");
}};

} // namespace
