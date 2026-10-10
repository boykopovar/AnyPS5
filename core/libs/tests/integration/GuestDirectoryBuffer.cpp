#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libkernel/File/include/FileFlags.hpp"
#include "prx/libkernel/KernelErrors.hpp"

#include <Testing/Test.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <fstream>
#include <string>
#include <string_view>

extern "C" {
int APS5_VABI sceKernelOpen(const char*, int, std::uint16_t);
int APS5_VABI sceKernelClose(int);
int APS5_VABI sceKernelGetdents(int, char*, int);
int APS5_VABI sceKernelGetdirentries(int, char*, int, std::int64_t*);
}

namespace {

using Testing::Case;
using Testing::Require;
using Testing::RequireEqual;

constexpr std::string_view entryName = "a-long-directory-entry-name.bin";
constexpr int dotRecordSize = 12;

using Buffer = std::array<char, 256>;

class OpenDirectory {
public:
    OpenDirectory() {
        std::ofstream file(directory.Path() / std::string(entryName));
        Require(static_cast<bool>(file), "create the directory entry");
        file.close();
        descriptor = sceKernelOpen(directory.Path().string().c_str(), SCE_KERNEL_O_RDONLY | SCE_KERNEL_O_DIRECTORY, 0);
        Require(descriptor >= 0, "open the directory");
        buffer.fill('x');
    }
    ~OpenDirectory() {
        if (descriptor >= 0) sceKernelClose(descriptor);
    }
    OpenDirectory(const OpenDirectory&) = delete;
    OpenDirectory& operator=(const OpenDirectory&) = delete;

    int Getdents(int size) { return sceKernelGetdents(descriptor, buffer.data(), size); }

    int Getdirentries(int size) {
        std::int64_t base = -1;
        return sceKernelGetdirentries(descriptor, buffer.data(), size, &base);
    }

    std::string_view EntryName() const { return std::string_view(buffer.data() + 8); }

    bool BufferUntouched() const {
        return std::all_of(buffer.begin(), buffer.end(), [](char byte) { return byte == 'x'; });
    }

    void ReadDotEntries() {
        RequireEqual(Getdents(20), dotRecordSize, "read . record");
        RequireEqual(EntryName(), std::string_view("."), "first entry");
        RequireEqual(Getdents(dotRecordSize), dotRecordSize, "read .. record");
        RequireEqual(EntryName(), std::string_view(".."), "second entry");
        buffer.fill('x');
    }

    int Close() {
        const int result = sceKernelClose(descriptor);
        descriptor = -1;
        return result;
    }

    int descriptor = -1;
    Buffer buffer{};

private:
    Testing::TemporaryDirectory directory;
};

int EntryRecordSize() {
    return static_cast<int>((8 + entryName.size() + 1 + 3) & ~std::size_t{3});
}

const Case smallDirentriesBuffer{"Getdirentries_BufferSmallerThanRecord_FailsWithEinvalAndLeavesBuffer", [] {
    OpenDirectory directory;
    RequireEqual(directory.Getdirentries(8), SCE_KERNEL_ERROR_EINVAL, "8 byte buffer");
    Require(directory.BufferUntouched(), "buffer untouched");
}};

const Case invalidBuffers{"Getdents_InvalidBufferOrSize_Fails", [] {
    OpenDirectory directory;
    RequireEqual(directory.Getdents(8), SCE_KERNEL_ERROR_EINVAL, "8 byte buffer");
    RequireEqual(sceKernelGetdents(directory.descriptor, nullptr, 256), SCE_KERNEL_ERROR_EFAULT, "null buffer");
    RequireEqual(directory.Getdents(0), SCE_KERNEL_ERROR_EINVAL, "zero size");
    RequireEqual(directory.Getdents(-1), SCE_KERNEL_ERROR_EINVAL, "negative size");
}};

const Case failedReadsKeepPosition{"Getdents_AfterRejectedBuffers_StillStartsWithDotEntries", [] {
    OpenDirectory directory;
    directory.Getdirentries(8);
    directory.Getdents(8);
    directory.Getdents(0);
    directory.ReadDotEntries();
}};

const Case dotEntries{"Getdents_ExactRecordSize_ReturnsOneRecordPerCall", [] {
    OpenDirectory directory;
    directory.ReadDotEntries();
}};

const Case smallRecordBuffer{"Getdents_BufferSmallerThanNextRecord_FailsWithEinvalAndLeavesBuffer", [] {
    OpenDirectory directory;
    directory.ReadDotEntries();
    RequireEqual(directory.Getdents(dotRecordSize), SCE_KERNEL_ERROR_EINVAL, "12 byte buffer for long entry");
    Require(directory.BufferUntouched(), "buffer untouched");
}};

const Case exactEntryRecord{"Getdirentries_ExactRecordSize_ReturnsNamedEntry", [] {
    OpenDirectory directory;
    directory.ReadDotEntries();
    directory.Getdents(dotRecordSize);
    RequireEqual(directory.Getdirentries(EntryRecordSize()), EntryRecordSize(), "entry record size");
    RequireEqual(directory.EntryName(), entryName, "entry name");
}};

const Case endOfDirectory{"Getdents_AtEndOfDirectory_ReturnsZero", [] {
    OpenDirectory directory;
    directory.ReadDotEntries();
    RequireEqual(directory.Getdirentries(EntryRecordSize()), EntryRecordSize(), "entry record");
    RequireEqual(directory.Getdents(8), 0, "8 byte buffer at end");
    RequireEqual(directory.Getdents(256), 0, "256 byte buffer at end");
    RequireEqual(directory.Close(), 0, "close");
}};

const Case invalidDescriptor{"Getdents_InvalidDescriptor_FailsWithEnotdir", [] {
    Buffer buffer{};
    RequireEqual(sceKernelGetdents(-1, buffer.data(), 256), SCE_KERNEL_ERROR_ENOTDIR, "descriptor -1");
}};

} // namespace
