#include "prx/libc/include/general/ExportMacros.hpp"
#include "prx/libSceTtsCoreG3Bc/include/InventoryExports.hpp"
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

APS5_DUMMY_FUN

}

extern "C" {

int APS5_VABI libSceTtsCoreG3BcUnknown__plus_b5_minus_z8Ff4cw_nid_no_patch() {
    return 100;
}
APS5_EXPORT("+b5-z8Ff4cw", libSceTtsCoreG3BcUnknown__plus_b5_minus_z8Ff4cw_nid_no_patch);

int APS5_VABI libSceTtsCoreG3BcUnknown_1scTM6aHK5I_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("1scTM6aHK5I", libSceTtsCoreG3BcUnknown_1scTM6aHK5I_nid_no_patch);

int APS5_VABI libSceTtsCoreG3BcUnknown_9tnCCarXqdY_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("9tnCCarXqdY", libSceTtsCoreG3BcUnknown_9tnCCarXqdY_nid_no_patch);

int APS5_VABI libSceTtsCoreG3BcUnknown_EmF4qOwLp5w_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("EmF4qOwLp5w", libSceTtsCoreG3BcUnknown_EmF4qOwLp5w_nid_no_patch);

int APS5_VABI libSceTtsCoreG3BcUnknown_H0VOaNk61es_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("H0VOaNk61es", libSceTtsCoreG3BcUnknown_H0VOaNk61es_nid_no_patch);

int APS5_VABI libSceTtsCoreG3BcUnknown_LQ65KkmJ48g_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("LQ65KkmJ48g", libSceTtsCoreG3BcUnknown_LQ65KkmJ48g_nid_no_patch);

int APS5_VABI libSceTtsCoreG3BcUnknown_PRmv_minus_UzXa1w_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("PRmv-UzXa1w", libSceTtsCoreG3BcUnknown_PRmv_minus_UzXa1w_nid_no_patch);

void APS5_VABI libSceTtsCoreG3BcUnknown_RByzVVv6e0A_nid_no_patch(void* object) {
    if (object == nullptr) return;
    using DeletingDtor = void(APS5_VABI*)(void*);
    void* const* vtable = nullptr;
    std::memcpy(&vtable, object, sizeof(vtable));
    if (vtable == nullptr) throw std::invalid_argument("RByzVVv6e0A: vtable must not be null");
    DeletingDtor dtor = nullptr;
    std::memcpy(&dtor, vtable + 1, sizeof(dtor));
    if (dtor == nullptr) throw std::invalid_argument("RByzVVv6e0A: vtable[1] must not be null");
    dtor(object);
}
APS5_EXPORT("RByzVVv6e0A", libSceTtsCoreG3BcUnknown_RByzVVv6e0A_nid_no_patch);

void APS5_VABI libSceTtsCoreG3BcUnknown_Sq5GSDo_minus_Rhk_nid_no_patch(void* engine, const void* params) {
    if (engine == nullptr || params == nullptr) {
        throw std::invalid_argument("Sq5GSDo-Rhk: engine and params must not be null");
    }
    std::memcpy(static_cast<std::byte*>(engine) + 0xe58, params, 16);
}
APS5_EXPORT("Sq5GSDo-Rhk", libSceTtsCoreG3BcUnknown_Sq5GSDo_minus_Rhk_nid_no_patch);

void APS5_VABI libSceTtsCoreG3BcUnknown_V_plus_nR_plus_xHS2ag_nid_no_patch() {}
APS5_EXPORT("V+nR+xHS2ag", libSceTtsCoreG3BcUnknown_V_plus_nR_plus_xHS2ag_nid_no_patch);

int APS5_VABI libSceTtsCoreG3BcUnknown_bapD6H6e7Yk_nid_no_patch(const void* engine) {
    if (engine == nullptr) return 0;
    const std::byte* ctx = nullptr;
    std::memcpy(&ctx, static_cast<const std::byte*>(engine) + 0x5f8, sizeof(ctx));
    if (ctx == nullptr) return 0x5622;
    std::int32_t activeCount = 0;
    std::int32_t activeIndex = 0;
    std::memcpy(&activeCount, ctx + 0x48, sizeof(activeCount));
    std::memcpy(&activeIndex, ctx + 0x38, sizeof(activeIndex));
    if (activeCount == 0 || activeIndex == -1) return 0x5622;
    const std::byte* slot = nullptr;
    std::memcpy(&slot, ctx + 0x40, sizeof(slot));
    if (slot == nullptr) return 0x5622;
    const std::byte* voiceData = nullptr;
    std::memcpy(&voiceData, slot + 0x38, sizeof(voiceData));
    if (voiceData == nullptr) return 0x5622;
    std::uint32_t sampleRate = 0;
    std::memcpy(&sampleRate, voiceData + 0x7e4, sizeof(sampleRate));
    return static_cast<int>(sampleRate);
}
APS5_EXPORT("bapD6H6e7Yk", libSceTtsCoreG3BcUnknown_bapD6H6e7Yk_nid_no_patch);

int APS5_VABI libSceTtsCoreG3BcUnknown_fF3Q3LdPUtk_nid_no_patch(const void* engine, void* outInfo) {
    if (engine == nullptr || outInfo == nullptr) {
        throw std::invalid_argument("fF3Q3LdPUtk: engine and outInfo must not be null");
    }
    const std::byte* inner = nullptr;
    std::memcpy(&inner, static_cast<const std::byte*>(engine) + 0x20, sizeof(inner));
    if (inner == nullptr) throw std::invalid_argument("fF3Q3LdPUtk: engine inner state must not be null");
    std::uint32_t sampleRate = 0;
    std::memcpy(&sampleRate, inner + 0x7e4, sizeof(sampleRate));
    if (sampleRate == 0) throw std::invalid_argument("fF3Q3LdPUtk: sample rate must be non-zero");
    auto* outBytes = static_cast<std::byte*>(outInfo);
    std::memset(outBytes, 0, 0x80);
    const std::uint32_t rawPeriod = 1000000000u / sampleRate;
    const std::uint32_t period = (rawPeriod == 0xf424u) ? 0xf423u : rawPeriod;
    const std::uint32_t fixedValue = 0x40u;
    std::memcpy(outBytes + 0x80, &period, sizeof(period));
    std::memcpy(outBytes + 0x84, &fixedValue, sizeof(fixedValue));
    return 0;
}
APS5_EXPORT("fF3Q3LdPUtk", libSceTtsCoreG3BcUnknown_fF3Q3LdPUtk_nid_no_patch);

void APS5_VABI libSceTtsCoreG3BcUnknown_iW0DoaHZE5Y_nid_no_patch(void* engine, std::uint8_t flag) {
    if (engine == nullptr) throw std::invalid_argument("iW0DoaHZE5Y: engine must not be null");
    std::memcpy(static_cast<std::byte*>(engine) + 0xe79, &flag, sizeof(flag));
}
APS5_EXPORT("iW0DoaHZE5Y", libSceTtsCoreG3BcUnknown_iW0DoaHZE5Y_nid_no_patch);

int APS5_VABI libSceTtsCoreG3BcUnknown_k1_minus_orvzQrHw_nid_no_patch() {
    return 8;
}
APS5_EXPORT("k1-orvzQrHw", libSceTtsCoreG3BcUnknown_k1_minus_orvzQrHw_nid_no_patch);

void APS5_VABI libSceTtsCoreG3BcUnknown_lQLzsMt_minus_6NU_nid_no_patch() {}
APS5_EXPORT("lQLzsMt-6NU", libSceTtsCoreG3BcUnknown_lQLzsMt_minus_6NU_nid_no_patch);

int APS5_VABI libSceTtsCoreG3BcUnknown_pacG6ffHAvU_nid_no_patch(const void* engine) {
    if (engine == nullptr) throw std::invalid_argument("pacG6ffHAvU: engine must not be null");
    std::int32_t level = 0;
    std::memcpy(&level, static_cast<const std::byte*>(engine) + 0xe6c, sizeof(level));
    if (level == 10) return 27;
    if (level > 10 || level < -100) {
        throw std::invalid_argument("pacG6ffHAvU: pitch level out of range");
    }
    float scale = 1.0f;
    for (std::int32_t i = level; i < 10; ++i) {
        scale *= 1.11f;
    }
    return static_cast<int>(scale * 27.47f);
}
APS5_EXPORT("pacG6ffHAvU", libSceTtsCoreG3BcUnknown_pacG6ffHAvU_nid_no_patch);

int APS5_VABI libSceTtsCoreG3BcUnknown_pzhkpUfnlqY_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("pzhkpUfnlqY", libSceTtsCoreG3BcUnknown_pzhkpUfnlqY_nid_no_patch);

int APS5_VABI libSceTtsCoreG3BcUnknown_stLW2n_minus_xJ6M_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("stLW2n-xJ6M", libSceTtsCoreG3BcUnknown_stLW2n_minus_xJ6M_nid_no_patch);

}
