#include "prx/libSceGvMp4Parser/libSceGvMp4Parser.hpp"
#include "prx/libkernel/File/include/File.hpp"
#include "prx/libkernel/File/include/FileFlags.hpp"

#include <algorithm>
#include <array>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <string>
#include <vector>

namespace {

constexpr int kNotInitialized = static_cast<int>(0x809D420Bu);
constexpr int kMissingPath = static_cast<int>(0x809D4203u);
constexpr int kUnsupportedFormat = static_cast<int>(0x809D4201u);

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "GvMp4Parser: %s\n", message);
    std::abort();
}

template <class TAction>
bool Rejects(TAction action) {
    try {
        action();
    } catch (const std::exception&) {
        return true;
    }
    return false;
}

struct GuestString {
    union {
        char bytes[16];
        const char* pointer;
    };
    std::uint64_t size;
    std::uint64_t capacity;
};

std::vector<std::size_t> allocations;
std::vector<void*> allocated;
std::vector<void*> released;
std::size_t refuseAt = 0;
std::size_t finalizeAt = 0;

void* APS5_VABI Allocate(std::size_t size) {
    allocations.push_back(size);
    if (allocations.size() == finalizeAt) Require(_ZN23sceMetadataReaderWriter11gvMp4Parser19gvMp4ParserFinalizeEv() == 0, "reentrant Finalize failed");
    if (refuseAt != 0 && allocations.size() == refuseAt) return nullptr;
    void* address = std::malloc(size);
    allocated.push_back(address);
    return address;
}

void APS5_VABI Release(void* address) {
    released.push_back(address);
    std::free(address);
}

GuestString Path(const std::string& text) {
    GuestString path{};
    path.size = text.size();
    path.capacity = std::max<std::size_t>(15, text.size());
    if (path.capacity == 15) std::memcpy(path.bytes, text.c_str(), text.size() + 1);
    else path.pointer = text.c_str();
    return path;
}


void RemoveGuestFile(const char* path, bool mustExist = false) {
    const int saved = errno;
    const int result = sceKernelUnlink(path);
    errno = saved;
    if (mustExist) Require(result == 0, "could not remove a guest parser fixture");
}

void MakeGuestFile(const char* path) {
    const int saved = errno;
    sceKernelUnlink(path);
    const int descriptor = sceKernelOpen(path, SCE_KERNEL_O_WRONLY | SCE_KERNEL_O_CREAT | SCE_KERNEL_O_TRUNC, 0600);
    Require(descriptor >= 0, "could not create a guest parser fixture");
    Require(sceKernelClose(descriptor) == 0, "could not close a guest parser fixture");
    errno = saved;
}

void RequireGates(const GuestString& path, const char* message) {
    const auto before = allocations.size();
    Require(libSceGvMp4ParserUnknown_PJigiYrhUwQ_nid_no_patch(&path, nullptr, nullptr) == kNotInitialized, message);
    Require(libSceGvMp4ParserUnknown_tcsvdFwGn9U_nid_no_patch(&path, nullptr) == kNotInitialized, message);
    Require(libSceGvMp4ParserUnknown_iPAZM6la8Gk_nid_no_patch(&path, 0, 0, nullptr) == kNotInitialized, message);
    Require(allocations.size() == before, "a gate allocated through the table");
}

}

int main() {
    errno = 71;
    Require(_ZN23sceMetadataReaderWriter11gvMp4Parser19gvMp4ParserFinalizeEv() == 0, "Finalize without Initialize did not return 0");
    Require(_ZN23sceMetadataReaderWriter11gvMp4Parser19gvMp4ParserFinalizeEv() == 0, "a second Finalize did not return 0");
    Require(errno == 71, "Finalize changed errno");

    const std::string shortName = "/data/clip1.mp4";
    const std::string longName = "/data/clip12.mp4";
    const auto shortPath = Path(shortName);
    const auto longPath = Path(longName);
    Require(shortName.size() == 15 && longName.size() == 16, "fixture path lengths");
    RequireGates(shortPath, "an uninitialized gate did not return 0x809D420B");
    Require(Rejects([&] { libSceGvMp4ParserUnknown_PJigiYrhUwQ_nid_no_patch(&longPath, nullptr, nullptr); }), "a long path before Initialize did not need the allocator");
    Require(allocations.empty(), "an uninitialized long path allocated");

    std::array<std::uintptr_t, 11> config{};
    config[0] = reinterpret_cast<std::uintptr_t>(Allocate);
    config[3] = reinterpret_cast<std::uintptr_t>(Release);
    Require(libSceGvMp4ParserUnknown_sSswLa3vxMA_nid_no_patch(config.data()) == 0, "Initialize did not return 0");
    Require(allocations == std::vector<std::size_t>{0x7b8, 0x28}, "Initialize did not allocate the 0x7b8- and 0x28-byte objects in order");
    Require(errno == 71, "Initialize changed errno");
    Require(libSceGvMp4ParserUnknown_sSswLa3vxMA_nid_no_patch(config.data()) == kNotInitialized, "a second Initialize did not return 0x809D420B");
    Require(libSceGvMp4ParserUnknown_sSswLa3vxMA_nid_no_patch(nullptr) == kNotInitialized, "a second Initialize read its table before the context check");
    Require(allocations.size() == 2 && released.empty(), "a refused Initialize allocated or released");

    const std::string missingName = "gv_missing.mp4";
    RemoveGuestFile(missingName.c_str());
    const auto missingPath = Path(missingName);
    Require(libSceGvMp4ParserUnknown_PJigiYrhUwQ_nid_no_patch(&missingPath, nullptr, nullptr) == kMissingPath, "a missing read path did not return 0x809D4203");

    const std::string unsupportedName = "gv_bad.mov";
    MakeGuestFile(unsupportedName.c_str());
    const auto unsupportedPath = Path(unsupportedName);
    Require(libSceGvMp4ParserUnknown_PJigiYrhUwQ_nid_no_patch(&unsupportedPath, nullptr, nullptr) == kUnsupportedFormat, "an existing .mov path did not return 0x809D4201");
    RemoveGuestFile(unsupportedName.c_str(), true);

    for (const std::string supportedName : {"gv_ok.mp4", "gv_ok.m4v", "gv_ok.MGV", "gv_ok.mnv"}) {
        MakeGuestFile(supportedName.c_str());
        const auto supportedPath = Path(supportedName);
        Require(Rejects([&] { libSceGvMp4ParserUnknown_PJigiYrhUwQ_nid_no_patch(&supportedPath, nullptr, nullptr); }), "a measured supported extension did not reach the unsupported parser body");
        RemoveGuestFile(supportedName.c_str(), true);
    }

    for (const std::string unmeasuredName : {"gv_unknown.ETS", "gv_unknown.3gp"}) {
        MakeGuestFile(unmeasuredName.c_str());
        const auto unmeasuredPath = Path(unmeasuredName);
        Require(Rejects([&] { libSceGvMp4ParserUnknown_PJigiYrhUwQ_nid_no_patch(&unmeasuredPath, nullptr, nullptr); }), "an unmeasured extension was assigned a firmware result");
        RemoveGuestFile(unmeasuredName.c_str(), true);
    }

    const std::string noExtensionName = "gv_noext";
    MakeGuestFile(noExtensionName.c_str());
    const auto noExtensionPath = Path(noExtensionName);
    Require(Rejects([&] { libSceGvMp4ParserUnknown_PJigiYrhUwQ_nid_no_patch(&noExtensionPath, nullptr, nullptr); }), "a no-extension path did not keep the measured exception gate");
    RemoveGuestFile(noExtensionName.c_str(), true);

    const std::string nonDirectoryName = "gv_parent.mp4";
    MakeGuestFile(nonDirectoryName.c_str());
    const std::string nonDirectoryChild = nonDirectoryName + "/child.mp4";
    const auto nonDirectoryPath = Path(nonDirectoryChild);
    Require(Rejects([&] { libSceGvMp4ParserUnknown_PJigiYrhUwQ_nid_no_patch(&nonDirectoryPath, nullptr, nullptr); }), "an unmeasured ENOTDIR stat result was mapped as missing");
    RemoveGuestFile(nonDirectoryName.c_str(), true);

    config[3] = 0;
    Require(_ZN23sceMetadataReaderWriter11gvMp4Parser19gvMp4ParserFinalizeEv() == 0, "an active Finalize did not return 0");
    config[3] = reinterpret_cast<std::uintptr_t>(Release);
    Require(released == allocated, "Finalize did not release both objects once");
    Require(_ZN23sceMetadataReaderWriter11gvMp4Parser19gvMp4ParserFinalizeEv() == 0 && released.size() == 2, "a repeated Finalize released again");
    RequireGates(shortPath, "a gate after Finalize did not return 0x809D420B");
    Require(Rejects([&] { libSceGvMp4ParserUnknown_tcsvdFwGn9U_nid_no_patch(&longPath, nullptr); }), "a long path after Finalize did not need the cleared allocator");

    Require(libSceGvMp4ParserUnknown_sSswLa3vxMA_nid_no_patch(config.data()) == 0, "Initialize after Finalize did not return 0");
    Require(allocations.size() == 4 && allocations[2] == 0x7b8 && allocations[3] == 0x28, "Initialize after Finalize did not allocate both objects again");
    Require(_ZN23sceMetadataReaderWriter11gvMp4Parser19gvMp4ParserFinalizeEv() == 0 && released == allocated, "the second lifecycle did not release its objects");

    for (unsigned index : {1u, 2u}) {
        refuseAt = allocations.size() + index;
        Require(Rejects([&] { libSceGvMp4ParserUnknown_sSswLa3vxMA_nid_no_patch(config.data()); }), "a refused allocation was not reported");
        Require(allocations.size() == refuseAt && released == allocated, "a refused allocation leaked or continued allocating");
        RequireGates(shortPath, "a failed Initialize left a parser behind");
    }
    refuseAt = 0;
    for (unsigned index : {1u, 2u}) {
        finalizeAt = allocations.size() + index;
        Require(libSceGvMp4ParserUnknown_sSswLa3vxMA_nid_no_patch(config.data()) == kNotInitialized, "a canceled Initialize reported success");
        Require(released == allocated, "a canceled Initialize leaked an object");
        RequireGates(shortPath, "a canceled Initialize published a parser");
    }
    finalizeAt = 0;
    Require(libSceGvMp4ParserUnknown_sSswLa3vxMA_nid_no_patch(config.data()) == 0, "Initialize after a refused allocation did not return 0");
    Require(_ZN23sceMetadataReaderWriter11gvMp4Parser19gvMp4ParserFinalizeEv() == 0, "the last Finalize did not return 0");
    Require(allocated.size() == released.size(), "allocations and releases do not balance");
    Require(errno == 71, "the lifecycle changed errno");
    std::puts("GvMp4Parser tests passed");
    return 0;
}
