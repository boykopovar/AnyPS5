#include "prx/libSceWebTransport/include/Api.hpp"
#include <atomic>
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <mutex>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

namespace {

constexpr int sceWebTransportErrorInvalidMode = static_cast<int>(0x80100004);
constexpr int sceWebTransportErrorNullParam = static_cast<int>(0x80100005);

std::atomic<std::uint32_t> g_logFlags{0};
std::atomic<void*> g_logCallback{nullptr};
std::atomic<std::uint32_t> g_backendMode{0};
std::mutex g_pairMutex;
std::uint32_t g_pair0 = 0;
std::uint32_t g_pair1 = 0;

}

extern "C" {

APS5_EXPORT("yDQLZ5vLNAo", sceWebTransportUnknown00);
void APS5_VABI sceWebTransportUnknown00(std::uint32_t flags) {
    g_logFlags.store(flags, std::memory_order_relaxed);
}

APS5_EXPORT("aRuF-dyJTf0", sceWebTransportUnknown01);
void APS5_VABI sceWebTransportUnknown01(void* callback) {
    g_logCallback.store(callback, std::memory_order_relaxed);
}

APS5_EXPORT("sO-Qj8LdN0w", sceWebTransportUnknown02);
int APS5_VABI sceWebTransportUnknown02(void) {
    NotImplemented_nid_no_patch("sO-Qj8LdN0w");
    return 0;
}

APS5_EXPORT("7dZ6MtRYlxk", sceWebTransportUnknown03);
int APS5_VABI sceWebTransportUnknown03(const std::uint32_t* params) {
    std::uint32_t v0 = 0;
    std::uint32_t v1 = 0;
    if (params != nullptr) {
        std::memcpy(&v0, params, sizeof(v0));
        std::memcpy(&v1, params + 1, sizeof(v1));
    }
    std::lock_guard<std::mutex> lock(g_pairMutex);
    g_pair0 = v0;
    g_pair1 = v1;
    return 0;
}

APS5_EXPORT("RZ7msPQyMrw", sceWebTransportUnknown04);
int APS5_VABI sceWebTransportUnknown04(std::uint32_t* statsOut) {
    if (statsOut == nullptr) {
        return sceWebTransportErrorNullParam;
    }
    std::memset(statsOut, 0, sizeof(std::uint32_t) * 12);
    return 0;
}

APS5_EXPORT("FuwKcReb9Qs", sceWebTransportUnknown05);
void APS5_VABI sceWebTransportUnknown05(std::uint64_t* countersOut) {
    if (countersOut == nullptr) {
        return;
    }
    std::memset(countersOut, 0, sizeof(std::uint64_t) * 10);
}

APS5_EXPORT("gpxgv3ZGouY", sceWebTransportUnknown06);
int APS5_VABI sceWebTransportUnknown06(void) {
    return 0;
}

APS5_EXPORT("yFXDNq00zdw", sceWebTransportUnknown07);
int APS5_VABI sceWebTransportUnknown07(void) {
    NotImplemented_nid_no_patch("yFXDNq00zdw");
    return 0;
}

APS5_EXPORT("IF1M6OsaPxE", sceWebTransportUnknown08);
int APS5_VABI sceWebTransportUnknown08(void) {
    NotImplemented_nid_no_patch("IF1M6OsaPxE");
    return 0;
}

APS5_EXPORT("8baUEaMS3w4", sceWebTransportUnknown09);
int APS5_VABI sceWebTransportUnknown09(void) {
    NotImplemented_nid_no_patch("8baUEaMS3w4");
    return 0;
}

APS5_EXPORT("3rFEpghOT7g", sceWebTransportUnknown10);
int APS5_VABI sceWebTransportUnknown10(void) {
    NotImplemented_nid_no_patch("3rFEpghOT7g");
    return 0;
}

APS5_EXPORT("QDaDbEh27pc", sceWebTransportUnknown11);
int APS5_VABI sceWebTransportUnknown11(void) {
    NotImplemented_nid_no_patch("QDaDbEh27pc");
    return 0;
}

APS5_EXPORT("LuKemyYcbYA", sceWebTransportUnknown12);
int APS5_VABI sceWebTransportUnknown12(void) {
    NotImplemented_nid_no_patch("LuKemyYcbYA");
    return 0;
}

APS5_EXPORT("Dj9-Bo1dPQY", sceWebTransportUnknown13);
int APS5_VABI sceWebTransportUnknown13(void) {
    NotImplemented_nid_no_patch("Dj9-Bo1dPQY");
    return 0;
}

APS5_EXPORT("F1waH8GrH5c", sceWebTransportUnknown14);
void APS5_VABI sceWebTransportUnknown14(void* configOut) {
    if (configOut == nullptr) {
        return;
    }
    auto* bytes = static_cast<std::uint8_t*>(configOut);
    std::memset(bytes, 0, 0xf0);
    const std::uint64_t block50[4] = {
        0x403e000000000000ULL,
        0x403e000000000000ULL,
        0x4024000000000000ULL,
        0x3fd54fdf3b645a1dULL,
    };
    const std::uint64_t scalar70 = 0x3f9999999999999aULL;
    const std::uint64_t block78[4] = {
        3ULL,
        2ULL,
        100ULL,
        103ULL,
    };
    const std::uint64_t blocka8[4] = {
        0x100000ULL,
        0x100000ULL,
        0x100000ULL,
        0x100000ULL,
    };
    std::memcpy(bytes + 0x50, block50, sizeof(block50));
    std::memcpy(bytes + 0x70, &scalar70, sizeof(scalar70));
    std::memcpy(bytes + 0x78, block78, sizeof(block78));
    std::memcpy(bytes + 0xa8, blocka8, sizeof(blocka8));
}

APS5_EXPORT("HNbe4GmHVoE", sceWebTransportUnknown15);
int APS5_VABI sceWebTransportUnknown15(std::uint32_t mode) {
    if (mode > 1u) {
        return sceWebTransportErrorInvalidMode;
    }
    g_backendMode.store(mode, std::memory_order_relaxed);
    return 0;
}

APS5_EXPORT("YWjYfs3Y6AQ", sceWebTransportUnknown16);
int APS5_VABI sceWebTransportUnknown16(void) {
    NotImplemented_nid_no_patch("YWjYfs3Y6AQ");
    return 0;
}

APS5_EXPORT("g0VYmAgyKCo", sceWebTransportUnknown17);
int APS5_VABI sceWebTransportUnknown17(void) {
    NotImplemented_nid_no_patch("g0VYmAgyKCo");
    return 0;
}

APS5_EXPORT("8qhovA7ewGo", sceWebTransportUnknown18);
int APS5_VABI sceWebTransportUnknown18(void) {
    NotImplemented_nid_no_patch("8qhovA7ewGo");
    return 0;
}

APS5_EXPORT("a5Kvg7QX0Tk", sceWebTransportUnknown19);
int APS5_VABI sceWebTransportUnknown19(void) {
    NotImplemented_nid_no_patch("a5Kvg7QX0Tk");
    return 0;
}

APS5_EXPORT("CG4Vqer++Fk", sceWebTransportUnknown20);
int APS5_VABI sceWebTransportUnknown20(void) {
    NotImplemented_nid_no_patch("CG4Vqer++Fk");
    return 0;
}

APS5_EXPORT("dBA3C0BvxZs", sceWebTransportUnknown21);
int APS5_VABI sceWebTransportUnknown21(void) {
    NotImplemented_nid_no_patch("dBA3C0BvxZs");
    return 0;
}

APS5_EXPORT("iTqKfqqRYHU", sceWebTransportUnknown22);
int APS5_VABI sceWebTransportUnknown22(void) {
    NotImplemented_nid_no_patch("iTqKfqqRYHU");
    return 0;
}

APS5_EXPORT("MOAg2IAVJpo", sceWebTransportUnknown23);
int APS5_VABI sceWebTransportUnknown23(void) {
    NotImplemented_nid_no_patch("MOAg2IAVJpo");
    return 0;
}

APS5_EXPORT("jvJFNpJiiVA", sceWebTransportUnknown24);
int APS5_VABI sceWebTransportUnknown24(void) {
    NotImplemented_nid_no_patch("jvJFNpJiiVA");
    return 0;
}

APS5_EXPORT("Ghb9UWG20hE", sceWebTransportUnknown25);
int APS5_VABI sceWebTransportUnknown25(void) {
    NotImplemented_nid_no_patch("Ghb9UWG20hE");
    return 0;
}

APS5_EXPORT("vlYUVlW76Ow", sceWebTransportUnknown26);
int APS5_VABI sceWebTransportUnknown26(void) {
    NotImplemented_nid_no_patch("vlYUVlW76Ow");
    return 0;
}

APS5_EXPORT("KihuM8ez8MM", sceWebTransportUnknown27);
int APS5_VABI sceWebTransportUnknown27(void) {
    NotImplemented_nid_no_patch("KihuM8ez8MM");
    return 0;
}

APS5_EXPORT("OSA7vqVi-jM", sceWebTransportUnknown28);
int APS5_VABI sceWebTransportUnknown28(void) {
    NotImplemented_nid_no_patch("OSA7vqVi-jM");
    return 0;
}

APS5_EXPORT("dP3Qm8xDt0A", sceWebTransportUnknown29);
int APS5_VABI sceWebTransportUnknown29(void) {
    NotImplemented_nid_no_patch("dP3Qm8xDt0A");
    return 0;
}

APS5_EXPORT("GtIbnZITTDI", sceWebTransportUnknown30);
int APS5_VABI sceWebTransportUnknown30(void) {
    NotImplemented_nid_no_patch("GtIbnZITTDI");
    return 0;
}

APS5_EXPORT("Fr0-n+3Tq-U", sceWebTransportUnknown31);
int APS5_VABI sceWebTransportUnknown31(void) {
    NotImplemented_nid_no_patch("Fr0-n+3Tq-U");
    return 0;
}

APS5_EXPORT("+Qna9X-+lvw", sceWebTransportUnknown32);
int APS5_VABI sceWebTransportUnknown32(void) {
    NotImplemented_nid_no_patch("+Qna9X-+lvw");
    return 0;
}

APS5_EXPORT("f4Se8M9P5bg", sceWebTransportUnknown33);
int APS5_VABI sceWebTransportUnknown33(void) {
    NotImplemented_nid_no_patch("f4Se8M9P5bg");
    return 0;
}

APS5_EXPORT("Sgpyj6wMFos", sceWebTransportUnknown34);
int APS5_VABI sceWebTransportUnknown34(void) {
    NotImplemented_nid_no_patch("Sgpyj6wMFos");
    return 0;
}

APS5_EXPORT("jJnQEEW7-Yo", sceWebTransportUnknown35);
int APS5_VABI sceWebTransportUnknown35(void) {
    NotImplemented_nid_no_patch("jJnQEEW7-Yo");
    return 0;
}

APS5_EXPORT("R4-TLIrDf8A", sceWebTransportUnknown36);
int APS5_VABI sceWebTransportUnknown36(void) {
    NotImplemented_nid_no_patch("R4-TLIrDf8A");
    return 0;
}

APS5_EXPORT("Pr-kbxNLmKU", sceWebTransportUnknown37);
int APS5_VABI sceWebTransportUnknown37(void) {
    NotImplemented_nid_no_patch("Pr-kbxNLmKU");
    return 0;
}

APS5_EXPORT("ZdjsEa5FfsQ", sceWebTransportUnknown38);
int APS5_VABI sceWebTransportUnknown38(void) {
    NotImplemented_nid_no_patch("ZdjsEa5FfsQ");
    return 0;
}

APS5_EXPORT("x2YzXDecgB8", sceWebTransportUnknown39);
int APS5_VABI sceWebTransportUnknown39(void) {
    NotImplemented_nid_no_patch("x2YzXDecgB8");
    return 0;
}

APS5_EXPORT("3fFqxhwfC2o", sceWebTransportUnknown40);
int APS5_VABI sceWebTransportUnknown40(void) {
    NotImplemented_nid_no_patch("3fFqxhwfC2o");
    return 0;
}

APS5_EXPORT("8Fv0htSKkaE", sceWebTransportUnknown41);
int APS5_VABI sceWebTransportUnknown41(void) {
    NotImplemented_nid_no_patch("8Fv0htSKkaE");
    return 0;
}

APS5_EXPORT("HrzL1GZ4pfM", sceWebTransportUnknown42);
int APS5_VABI sceWebTransportUnknown42(void) {
    NotImplemented_nid_no_patch("HrzL1GZ4pfM");
    return 0;
}

APS5_EXPORT("vkq466ZVpbE", sceWebTransportUnknown43);
int APS5_VABI sceWebTransportUnknown43(void) {
    NotImplemented_nid_no_patch("vkq466ZVpbE");
    return 0;
}

APS5_EXPORT("-q1hzjCUx1s", sceWebTransportUnknown44);
int APS5_VABI sceWebTransportUnknown44(void) {
    NotImplemented_nid_no_patch("-q1hzjCUx1s");
    return 0;
}

APS5_EXPORT("KyZxudPoVVc", sceWebTransportUnknown45);
int APS5_VABI sceWebTransportUnknown45(void) {
    NotImplemented_nid_no_patch("KyZxudPoVVc");
    return 0;
}

APS5_EXPORT("178NfAAq3Gw", sceWebTransportUnknown46);
int APS5_VABI sceWebTransportUnknown46(void) {
    NotImplemented_nid_no_patch("178NfAAq3Gw");
    return 0;
}

APS5_EXPORT("+ZTz1325V6k", sceWebTransportUnknown47);
int APS5_VABI sceWebTransportUnknown47(void) {
    NotImplemented_nid_no_patch("+ZTz1325V6k");
    return 0;
}

APS5_EXPORT("79NNBScLv2Y", sceWebTransportUnknown48);
int APS5_VABI sceWebTransportUnknown48(void) {
    NotImplemented_nid_no_patch("79NNBScLv2Y");
    return 0;
}

APS5_EXPORT("LYJaqTy-6dE", sceWebTransportUnknown49);
int APS5_VABI sceWebTransportUnknown49(void) {
    NotImplemented_nid_no_patch("LYJaqTy-6dE");
    return 0;
}

APS5_EXPORT("L0qjFMFFq7I", sceWebTransportUnknown50);
int APS5_VABI sceWebTransportUnknown50(void) {
    NotImplemented_nid_no_patch("L0qjFMFFq7I");
    return 0;
}

APS5_EXPORT("2BXQTf6fkB8", sceWebTransportUnknown51);
int APS5_VABI sceWebTransportUnknown51(void) {
    NotImplemented_nid_no_patch("2BXQTf6fkB8");
    return 0;
}

APS5_EXPORT("QoyAAWe9z4c", sceWebTransportUnknown52);
int APS5_VABI sceWebTransportUnknown52(void) {
    NotImplemented_nid_no_patch("QoyAAWe9z4c");
    return 0;
}

}
