#include "prx/libc/include/general/ExportMacros.hpp"
#include "prx/libSceGifParser/include/InventoryExports.hpp"
#include "Allocator.hpp"
#include <cstddef>
#include <cstdint>
#include <cstring>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include <array>
#include <mutex>
#include <stdexcept>

namespace {

constexpr int kResult809D4054 = static_cast<int>(0x809D4054u);
constexpr std::size_t kConfigSize = 0x58;

std::mutex g_configMutex;
std::array<std::byte, kConfigSize> g_config{};
std::array<std::byte, kConfigSize> g_configCopy{};

} // namespace

GifParser::Allocator GifParser::GetAllocator_nid_no_patch() {
    const std::lock_guard<std::mutex> lock(g_configMutex);
    Allocator allocator{};
    std::memcpy(&allocator.allocate, g_config.data(), sizeof(allocator.allocate));
    std::memcpy(&allocator.release, g_config.data() + 0x18, sizeof(allocator.release));
    if (!allocator.allocate || !allocator.release)
        throw std::logic_error("GIF metadata: allocator has not been initialized");
    return allocator;
}

extern "C" {

APS5_DUMMY_FUN

}

extern "C" {

int APS5_VABI _ZN23sceMetadataReaderWriter9gifParser17gifParserFinalizeEv() {
    const std::lock_guard<std::mutex> lock(g_configMutex);
    g_config.fill(std::byte{0});
    return 0;
}

int APS5_VABI libSceGifParserUnknown_WvV54DtSzmQ_nid_no_patch(const void* config) {
    if (config == nullptr)
        throw std::invalid_argument("WvV54DtSzmQ: unreadable configuration");
    std::array<std::byte, kConfigSize> copy{};
    const bool copied = GuestCopyIn_nid_no_patch(copy.data(), config, copy.size());
    if (!copied)
        throw std::invalid_argument("WvV54DtSzmQ: unreadable configuration");
    const std::lock_guard<std::mutex> lock(g_configMutex);
    g_config = copy;
    g_configCopy = copy;
    return 0;
}
APS5_EXPORT("WvV54DtSzmQ", libSceGifParserUnknown_WvV54DtSzmQ_nid_no_patch);

int APS5_VABI libSceGifParserUnknown_qiEL_minus_fPgaAA_nid_no_patch() {
    return kResult809D4054;
}
APS5_EXPORT("qiEL-fPgaAA", libSceGifParserUnknown_qiEL_minus_fPgaAA_nid_no_patch);

int APS5_VABI libSceGifParserUnknown__minus_ZzjSDCInl4_nid_no_patch() {
    return kResult809D4054;
}
APS5_EXPORT("-ZzjSDCInl4", libSceGifParserUnknown__minus_ZzjSDCInl4_nid_no_patch);

int APS5_VABI _ZN23sceMetadataReaderWriter9gifParser19gifParserInitializeEv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN23sceMetadataReaderWriter9gifParser21gifParserReadMetadataERKSsRKNS_8MetadataERS3_() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN23sceMetadataReaderWriter9gifParser22gifParserWriteMetadataERKSsRKNS_8MetadataE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI libSceGifParserUnknown_YtzTVdv9KoE_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("YtzTVdv9KoE", libSceGifParserUnknown_YtzTVdv9KoE_nid_no_patch);

int APS5_VABI libSceGifParserUnknown_OTz_minus_uftEt84_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("OTz-uftEt84", libSceGifParserUnknown_OTz_minus_uftEt84_nid_no_patch);

int APS5_VABI _ZN23sceMetadataReaderWriter9gifParser25gifParserExtractThumbnailERKSsRNS_9ThumbnailE() {
    NotImplemented_nid_no_patch(__func__);
}

}
