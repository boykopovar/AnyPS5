#include "prx/libc/include/general/ExportMacros.hpp"
#include "prx/libScePngParser/include/InventoryExports.hpp"
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

int APS5_VABI libScePngParserUnknown_suBT1aYALqA_nid_no_patch() {
    const std::lock_guard<std::mutex> lock(g_configMutex);
    g_config.fill(std::byte{0});
    return 0;
}
APS5_EXPORT("suBT1aYALqA", libScePngParserUnknown_suBT1aYALqA_nid_no_patch);

int APS5_VABI libScePngParserUnknown_vJpgNFIBlCk_nid_no_patch(const void* config) {
    std::array<std::byte, kConfigSize> copy{};
    if (config == nullptr || !GuestCopyIn_nid_no_patch(copy.data(), config, copy.size()))
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

int APS5_VABI libScePngParserUnknown_zfzpwUnVvYE_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("zfzpwUnVvYE", libScePngParserUnknown_zfzpwUnVvYE_nid_no_patch);

int APS5_VABI libScePngParserUnknown_70kLUNq_plus_dvQ_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("70kLUNq+dvQ", libScePngParserUnknown_70kLUNq_plus_dvQ_nid_no_patch);

int APS5_VABI libScePngParserUnknown_I58FzHBd1OE_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("I58FzHBd1OE", libScePngParserUnknown_I58FzHBd1OE_nid_no_patch);

int APS5_VABI libScePngParserUnknown_G6G_minus_KRVQEwI_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("G6G-KRVQEwI", libScePngParserUnknown_G6G_minus_KRVQEwI_nid_no_patch);

int APS5_VABI libScePngParserUnknown_ji_plus_m7tJEFUk_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("ji+m7tJEFUk", libScePngParserUnknown_ji_plus_m7tJEFUk_nid_no_patch);

int APS5_VABI libScePngParserUnknown_Ena5vQ_plus_C5Vs_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("Ena5vQ+C5Vs", libScePngParserUnknown_Ena5vQ_plus_C5Vs_nid_no_patch);

}
