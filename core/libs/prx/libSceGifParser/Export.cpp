#include "prx/libc/include/general/ExportMacros.hpp"
#include "prx/libSceGifParser/include/InventoryExports.hpp"
#include <cstddef>
#include <cstdint>
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

extern "C" {

APS5_DUMMY_FUN

}

extern "C" {

int APS5_VABI libSceGifParserUnknown_gtaHM_minus_0DRuA_nid_no_patch() {
    const std::lock_guard<std::mutex> lock(g_configMutex);
    g_config.fill(std::byte{0});
    return 0;
}
APS5_EXPORT("gtaHM-0DRuA", libSceGifParserUnknown_gtaHM_minus_0DRuA_nid_no_patch);

int APS5_VABI libSceGifParserUnknown_WvV54DtSzmQ_nid_no_patch(const void* config) {
    std::array<std::byte, kConfigSize> copy{};
    if (config == nullptr || !GuestCopyIn_nid_no_patch(copy.data(), config, copy.size()))
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

int APS5_VABI libSceGifParserUnknown_g2ESAZBTI9I_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("g2ESAZBTI9I", libSceGifParserUnknown_g2ESAZBTI9I_nid_no_patch);

int APS5_VABI libSceGifParserUnknown_75RtNTcq2p4_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("75RtNTcq2p4", libSceGifParserUnknown_75RtNTcq2p4_nid_no_patch);

int APS5_VABI libSceGifParserUnknown_4phPV_plus_C95_minus_0_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("4phPV+C95-0", libSceGifParserUnknown_4phPV_plus_C95_minus_0_nid_no_patch);

int APS5_VABI libSceGifParserUnknown_YtzTVdv9KoE_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("YtzTVdv9KoE", libSceGifParserUnknown_YtzTVdv9KoE_nid_no_patch);

int APS5_VABI libSceGifParserUnknown_OTz_minus_uftEt84_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("OTz-uftEt84", libSceGifParserUnknown_OTz_minus_uftEt84_nid_no_patch);

int APS5_VABI libSceGifParserUnknown_moN21TSFs4s_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("moN21TSFs4s", libSceGifParserUnknown_moN21TSFs4s_nid_no_patch);

}
