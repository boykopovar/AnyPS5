#include "WriterState.hpp"
#include <cstring>
#include "prx/libc/include/general/ExportMacros.hpp"
#include "prx/libScePngParser/include/InventoryExports.hpp"
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include <array>
#include <mutex>
#include <stdexcept>

namespace {

constexpr int kResult809D4019 = static_cast<int>(0x809D4019u);
constexpr int kResult809D4054 = static_cast<int>(0x809D4054u);
constexpr std::size_t kConfigSize = 0x58;

std::mutex g_configMutex;
std::array<std::byte, kConfigSize> g_config{};
std::array<std::byte, kConfigSize> g_configCopy{};

} // namespace

extern "C" {

APS5_DUMMY_FUN

}

extern "C" {

int APS5_VABI _ZN23sceMetadataReaderWriter9pngParser17pngParserFinalizeEv() {
    const std::lock_guard<std::mutex> lock(g_configMutex);
    g_config.fill(std::byte{0});
    return 0;
}

int APS5_VABI libScePngParserUnknown_vJpgNFIBlCk_nid_no_patch(const void* config) {
    if (config == nullptr)
        throw std::invalid_argument("vJpgNFIBlCk: unreadable configuration");
    std::array<std::byte, kConfigSize> copy{};
    const int savedErrno = errno;
    const bool copied = GuestCopyIn_nid_no_patch(copy.data(), config, copy.size());
    errno = savedErrno;
    if (!copied)
        throw std::invalid_argument("vJpgNFIBlCk: unreadable configuration");
    const std::lock_guard<std::mutex> lock(g_configMutex);
    g_config = copy;
    g_configCopy = copy;
    return 0;
}
APS5_EXPORT("vJpgNFIBlCk", libScePngParserUnknown_vJpgNFIBlCk_nid_no_patch);

int APS5_VABI libScePngParserUnknown_0Imz9zU_minus_Wgk_nid_no_patch() {
    return kResult809D4019;
}
APS5_EXPORT("0Imz9zU-Wgk", libScePngParserUnknown_0Imz9zU_minus_Wgk_nid_no_patch);

int APS5_VABI libScePngParserUnknown_5XfcHLzGE6E_nid_no_patch() {
    return kResult809D4054;
}
APS5_EXPORT("5XfcHLzGE6E", libScePngParserUnknown_5XfcHLzGE6E_nid_no_patch);

int APS5_VABI _ZN23sceMetadataReaderWriter9pngParser19pngParserInitializeEv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN23sceMetadataReaderWriter9pngParser21pngParserReadMetadataERKSsRKNS_8MetadataERS3_() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN23sceMetadataReaderWriter9pngParser22pngParserWriteMetadataERKSsRKNS_8MetadataE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI libScePngParserUnknown_G6G_minus_KRVQEwI_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("G6G-KRVQEwI", libScePngParserUnknown_G6G_minus_KRVQEwI_nid_no_patch);

int APS5_VABI libScePngParserUnknown_ji_plus_m7tJEFUk_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("ji+m7tJEFUk", libScePngParserUnknown_ji_plus_m7tJEFUk_nid_no_patch);

int APS5_VABI _ZN23sceMetadataReaderWriter9pngParser25pngParserExtractThumbnailERKSsRNS_9ThumbnailE() {
    NotImplemented_nid_no_patch(__func__);
}

}

PngWriter::Allocator PngWriter::GetAllocator_nid_no_patch() {
    const std::lock_guard lock(g_configMutex);
    Allocator allocator{};
    std::memcpy(&allocator.allocate, g_config.data(), sizeof(allocator.allocate));
    std::memcpy(&allocator.release, g_config.data() + 0x18, sizeof(allocator.release));
    if (!allocator.allocate || !allocator.release) throw std::logic_error("PNG writer: missing configured allocator");
    return allocator;
}
