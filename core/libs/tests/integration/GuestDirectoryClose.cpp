#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libkernel/File/include/FileFlags.hpp"

#include <Testing/Test.hpp>

#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>

extern "C" {
int APS5_VABI sceKernelOpen(const char*, int, std::uint16_t);
int APS5_VABI sceKernelGetdents(int, char*, int);
int APS5_VABI close_nid_postfix(int);
int APS5_VABI _close_nid_postfix(int);
}

namespace {

using Testing::Case;
using Testing::Require;
using Testing::RequireEqual;

constexpr int ErrorEnotdir = static_cast<int>(0x80020014u);

using CloseFunction = int (APS5_VABI *)(int);

void RequireDescriptorReuse(CloseFunction closeDescriptor) {
    const Testing::TemporaryDirectory root;
    const auto filePath = root.Path() / "entry.txt";
    {
        std::ofstream file(filePath, std::ios::binary);
        Require(file.is_open(), "create entry file");
        file << "entry";
        Require(static_cast<bool>(file), "write entry file");
    }

    const int directory = sceKernelOpen(root.Path().string().c_str(), SCE_KERNEL_O_RDONLY | SCE_KERNEL_O_DIRECTORY, 0);
    Require(directory >= 0, "open directory");
    RequireEqual(closeDescriptor(directory), 0, "close directory");

    const int file = sceKernelOpen(filePath.string().c_str(), SCE_KERNEL_O_RDONLY, 0);
    Require(file >= 0, "open file");
    std::array<char, 256> entries{};
    const int getdents = sceKernelGetdents(file, entries.data(), static_cast<int>(entries.size()));
    const int closed = closeDescriptor(file);
    RequireEqual(file, directory, "file reuses the directory descriptor");
    RequireEqual(getdents, ErrorEnotdir, "getdents on reused file descriptor");
    RequireEqual(closed, 0, "close file");
}

const Case close{"Close_DirectoryDescriptor_ReleasesItForFileReuse", [] {
    RequireDescriptorReuse(close_nid_postfix);
}};

const Case underscoreClose{"UnderscoreClose_DirectoryDescriptor_ReleasesItForFileReuse", [] {
    RequireDescriptorReuse(_close_nid_postfix);
}};

} // namespace
