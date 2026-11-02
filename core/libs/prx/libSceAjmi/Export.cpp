#include "prx/libc/include/general/ExportMacros.hpp"
#include "prx/libSceAjmi/include/InventoryExports.hpp"
#include "prx/libSceAjmi/include/Api.hpp"
#include <cstdint>
#include <cstddef>
#include <cstring>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

APS5_DUMMY_FUN

void* APS5_VABI sceAjmBatchJobInlineBuffer_nid_disambig1(void* position, const void* data_input, size_t data_input_size, const void** batch_address) {
    const std::size_t size = (data_input_size + 7) & ~std::size_t{7};
    auto* at = static_cast<std::uint8_t*>(position);
    std::uint32_t words[2];
    std::memcpy(words, at, sizeof(words));
    words[0] = (words[0] & ~0x1Fu) | 7u;
    words[1] = static_cast<std::uint32_t>(size);
    std::memcpy(at, words, sizeof(words));
    at += sizeof(words);
    if (data_input_size) std::memcpy(at, data_input, data_input_size);
    *batch_address = at;
    return at + size;
}

static std::uint8_t* KeepChunk(std::uint8_t* at, std::uint32_t keep, std::uint32_t low, std::uint32_t word) {
    std::uint32_t words[2];
    std::memcpy(words, at, sizeof(words));
    words[0] = (words[0] & keep) | low;
    words[1] = word;
    std::memcpy(at, words, sizeof(words));
    return at + sizeof(words);
}

static std::uint8_t* KeepBuffer(std::uint8_t* at, std::uint32_t kind, const void* address, std::size_t size) {
    at = KeepChunk(at, 0xFFFFFFE0u, kind, static_cast<std::uint32_t>(size));
    std::memcpy(at, &address, sizeof(address));
    return at + sizeof(address);
}

void* APS5_VABI sceAjmBatchJobRunBufferRa_nid_disambig1(void* position, uint32_t instance, uint64_t flags, const void* data_input, size_t data_input_size, void* data_output, size_t data_output_size, void* sideband_output, size_t sideband_output_size, void* return_address) {
    auto* job = static_cast<std::uint8_t*>(position);
    auto* at = job + 2 * sizeof(std::uint32_t);
    if (return_address) at = KeepBuffer(at, 6, return_address, 0);
    at = KeepBuffer(at, 1, data_input, data_input_size);
    const std::uint64_t kept = flags & 0x0000E00000001FFF;
    at = KeepChunk(at, 0xFC000000u, 4 | ((static_cast<std::uint32_t>(kept >> 32) & 0xFFFFFu) << 6), static_cast<std::uint32_t>(kept));
    at = KeepBuffer(at, 0x11, data_output, data_output_size);
    at = KeepBuffer(at, 0x12, sideband_output, sideband_output_size);
    KeepChunk(job, 0xFC000000u, (instance & 0xFFFFFu) << 6, static_cast<std::uint32_t>(at - job) - 2 * sizeof(std::uint32_t));
    return at;
}

}

extern "C" {

int APS5_VABI sceAjmiBatchCancel() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceAjmiBatchWait() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI libSceAjmiUnknown_4MsUK9kAnKg_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("4MsUK9kAnKg", libSceAjmiUnknown_4MsUK9kAnKg_nid_no_patch);

int APS5_VABI libSceAjmiUnknown_6fY_minus_ci3au2U_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("6fY-ci3au2U", libSceAjmiUnknown_6fY_minus_ci3au2U_nid_no_patch);

int APS5_VABI libSceAjmiUnknown_9zA4YMlkR9M_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("9zA4YMlkR9M", libSceAjmiUnknown_9zA4YMlkR9M_nid_no_patch);

int APS5_VABI libSceAjmiUnknown_BD002VBfhuE_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("BD002VBfhuE", libSceAjmiUnknown_BD002VBfhuE_nid_no_patch);

int APS5_VABI libSceAjmiUnknown_EYIibEpcHuw_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("EYIibEpcHuw", libSceAjmiUnknown_EYIibEpcHuw_nid_no_patch);

int APS5_VABI libSceAjmiUnknown_J3UwQ5e6sNU_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("J3UwQ5e6sNU", libSceAjmiUnknown_J3UwQ5e6sNU_nid_no_patch);

int APS5_VABI libSceAjmiUnknown_LESEGgm133w_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("LESEGgm133w", libSceAjmiUnknown_LESEGgm133w_nid_no_patch);

int APS5_VABI libSceAjmiUnknown_MTdd8ApwO50_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("MTdd8ApwO50", libSceAjmiUnknown_MTdd8ApwO50_nid_no_patch);

int APS5_VABI libSceAjmiUnknown_QhA7rTjJbro_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("QhA7rTjJbro", libSceAjmiUnknown_QhA7rTjJbro_nid_no_patch);

int APS5_VABI libSceAjmiUnknown_RbLGSlJB6dc_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("RbLGSlJB6dc", libSceAjmiUnknown_RbLGSlJB6dc_nid_no_patch);

int APS5_VABI libSceAjmiUnknown_Uqtj4RSP_minus_JQ_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("Uqtj4RSP-JQ", libSceAjmiUnknown_Uqtj4RSP_minus_JQ_nid_no_patch);

int APS5_VABI libSceAjmiUnknown_X8sT_minus_xi_minus_dVE_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("X8sT-xi-dVE", libSceAjmiUnknown_X8sT_minus_xi_minus_dVE_nid_no_patch);

int APS5_VABI libSceAjmiUnknown_YtubBgSeNH0_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("YtubBgSeNH0", libSceAjmiUnknown_YtubBgSeNH0_nid_no_patch);

int APS5_VABI libSceAjmiUnknown_cWZnCK7na5Y_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("cWZnCK7na5Y", libSceAjmiUnknown_cWZnCK7na5Y_nid_no_patch);

int APS5_VABI libSceAjmiUnknown_dTsNpEt1AqQ_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("dTsNpEt1AqQ", libSceAjmiUnknown_dTsNpEt1AqQ_nid_no_patch);

int APS5_VABI libSceAjmiUnknown_dmwcwCeItmE_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("dmwcwCeItmE", libSceAjmiUnknown_dmwcwCeItmE_nid_no_patch);

int APS5_VABI libSceAjmiUnknown_m0xisYC6U2w_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("m0xisYC6U2w", libSceAjmiUnknown_m0xisYC6U2w_nid_no_patch);

int APS5_VABI libSceAjmiUnknown_ofRX8Vq0ss0_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("ofRX8Vq0ss0", libSceAjmiUnknown_ofRX8Vq0ss0_nid_no_patch);

int APS5_VABI libSceAjmiUnknown_pfPwiigOE3A_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("pfPwiigOE3A", libSceAjmiUnknown_pfPwiigOE3A_nid_no_patch);

int APS5_VABI libSceAjmiUnknown_r7NMFYcJi08_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("r7NMFYcJi08", libSceAjmiUnknown_r7NMFYcJi08_nid_no_patch);

}
