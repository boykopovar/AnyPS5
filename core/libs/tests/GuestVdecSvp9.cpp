#include "prx/libc/include/GuestArena.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

#include <array>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <utility>

extern "C" {

int APS5_VABI libSceVdecSvp9Unknown_2dO21ExX5hM_nid_no_patch(void* handle, int flag, void* outA, std::uint32_t* outStatus);
int APS5_VABI libSceVdecSvp9Unknown_33wXf5Ne2xc_nid_no_patch(const void* cfg, const void* rec, void* outFb);
int APS5_VABI libSceVdecSvp9Unknown_BFFONbbPMXw_nid_no_patch(const void* cfg, const void* rec, void* outMem);
int APS5_VABI libSceVdecSvp9Unknown_JawAvxR7M_plus_k_nid_no_patch(const void* cfg, const void* rec, const void* memInfo, const void* a4, void* outHandle);
int APS5_VABI libSceVdecSvp9Unknown_RuWQMaRBlEs_nid_no_patch(void* handle);
int APS5_VABI libSceVdecSvp9Unknown_VGbdvyOsLzg_nid_no_patch(const void* cfg, const void* rec, std::uint32_t* outCount);
int APS5_VABI libSceVdecSvp9Unknown_VmDqnxqvIFg_nid_no_patch(void* handle, const void* a2, const void* a3, std::uint64_t* outStatus);
int APS5_VABI libSceVdecSvp9Unknown_gktyGB_plus_7_minus_go_nid_no_patch(void* handle, int flag, void* outA, void* outB);
int APS5_VABI libSceVdecSvp9Unknown_hWezRodOVBI_nid_no_patch(void* handle, std::uint32_t* outCount);
int APS5_VABI libSceVdecSvp9Unknown_kUyX8iOM0E4_nid_no_patch(void* handle);
int APS5_VABI libSceVdecSvp9Unknown_x_plus_HO8XFm6io_nid_no_patch(void* handle, const void* inInfo);

}

namespace {

constexpr std::uint32_t kHandleMagic = 0x4dec0deu;
constexpr int kPreservedErrno = 71;

void Require(bool ok, const char* description) {
    if (ok) {
        return;
    }
    std::fprintf(stderr, "libSceVdecSvp9 test failed: %s\n", description);
    std::abort();
}

void ClobberErrno(std::uintptr_t, std::size_t) {
    errno = 99;
}

template <typename Fn>
void RequireNotImplemented(Fn fn, const char* description) {
    try {
        fn();
    } catch (const std::runtime_error&) {
        return;
    }
    Require(false, description);
}

void WriteU32(std::uint8_t* base, std::size_t offset, std::uint32_t value) {
    std::memcpy(base + offset, &value, sizeof(value));
}

void WriteI32(std::uint8_t* base, std::size_t offset, std::int32_t value) {
    std::memcpy(base + offset, &value, sizeof(value));
}

void WriteU64(std::uint8_t* base, std::size_t offset, std::uint64_t value) {
    std::memcpy(base + offset, &value, sizeof(value));
}

std::uint32_t ReadU32(const std::uint8_t* base, std::size_t offset) {
    std::uint32_t value = 0;
    std::memcpy(&value, base + offset, sizeof(value));
    return value;
}

std::uint64_t ReadU64(const std::uint8_t* base, std::size_t offset) {
    std::uint64_t value = 0;
    std::memcpy(&value, base + offset, sizeof(value));
    return value;
}

bool AllBytesEqual(const std::uint8_t* data, std::size_t start, std::size_t end, std::uint8_t expected) {
    for (std::size_t i = start; i < end; ++i) {
        if (data[i] != expected) {
            return false;
        }
    }
    return true;
}

struct ConfigBuf {
    std::array<std::uint8_t, 0x20> bytes{};

    void Init(std::uint32_t profile, std::int32_t level, std::uint32_t flag8, std::int32_t dpb, std::int32_t wMb, std::int32_t hMb) {
        bytes.fill(0);
        WriteU32(bytes.data(), 0x00, profile);
        WriteI32(bytes.data(), 0x04, level);
        WriteU32(bytes.data(), 0x08, flag8);
        WriteI32(bytes.data(), 0x0c, dpb);
        WriteI32(bytes.data(), 0x14, wMb);
        WriteI32(bytes.data(), 0x18, hMb);
    }
};

struct RecBuf {
    std::array<std::uint8_t, 0x30> bytes{};

    void Init(std::uint64_t tag, std::int32_t rec08, std::uint32_t rec18, std::uint32_t rec2c) {
        bytes.fill(0);
        WriteU64(bytes.data(), 0x00, tag);
        WriteI32(bytes.data(), 0x08, rec08);
        WriteU32(bytes.data(), 0x18, rec18);
        WriteU32(bytes.data(), 0x2c, rec2c);
    }
};

void TestInstanceCountQuery() {
    ConfigBuf cfg{};
    cfg.Init(0u, 40, 0u, 8, 30, 17);
    RecBuf rec{};
    rec.Init(1ULL, 0, 0u, 0u);
    std::uint32_t outCount = 0xA5A5A5A5u;

    errno = kPreservedErrno;
    Require(libSceVdecSvp9Unknown_VGbdvyOsLzg_nid_no_patch(nullptr, rec.bytes.data(), &outCount) == -1, "VGbdvyOsLzg null cfg");
    Require(errno == kPreservedErrno, "VGbdvyOsLzg null cfg errno");
    Require(libSceVdecSvp9Unknown_VGbdvyOsLzg_nid_no_patch(cfg.bytes.data(), nullptr, &outCount) == -1, "VGbdvyOsLzg null rec");
    Require(libSceVdecSvp9Unknown_VGbdvyOsLzg_nid_no_patch(cfg.bytes.data(), rec.bytes.data(), nullptr) == -1, "VGbdvyOsLzg null out");
    Require(outCount == 0xA5A5A5A5u, "VGbdvyOsLzg null touched output");

    const std::array<std::pair<std::uint32_t, std::uint32_t>, 10> cases{{
        {0u, 6u},
        {1u, 1u},
        {2u, 2u},
        {3u, 3u},
        {4u, 4u},
        {5u, 5u},
        {6u, 6u},
        {7u, 7u},
        {100u, 7u},
        {0xffffffffu, 7u},
    }};
    for (const auto& entry : cases) {
        rec.Init(1ULL, 0, entry.first, 0u);
        outCount = 0xA5A5A5A5u;
        errno = kPreservedErrno;
        Require(libSceVdecSvp9Unknown_VGbdvyOsLzg_nid_no_patch(cfg.bytes.data(), rec.bytes.data(), &outCount) == 0, "VGbdvyOsLzg valid");
        Require(outCount == entry.second, "VGbdvyOsLzg mapped value");
        Require(errno == kPreservedErrno, "VGbdvyOsLzg errno preserved");
    }
}

void TestFrameBufferQuery() {
    ConfigBuf cfg{};
    cfg.Init(0u, 40, 0u, 8, 30, 17);
    RecBuf rec{};
    rec.Init(1ULL, 0, 1u, 0u);
    std::array<std::uint8_t, 0x20> outFb{};
    outFb.fill(0xA5);

    errno = kPreservedErrno;
    Require(libSceVdecSvp9Unknown_33wXf5Ne2xc_nid_no_patch(nullptr, rec.bytes.data(), outFb.data()) == -1, "33wXf5Ne2xc null cfg");
    Require(libSceVdecSvp9Unknown_33wXf5Ne2xc_nid_no_patch(cfg.bytes.data(), nullptr, outFb.data()) == -1, "33wXf5Ne2xc null rec");
    Require(libSceVdecSvp9Unknown_33wXf5Ne2xc_nid_no_patch(cfg.bytes.data(), rec.bytes.data(), nullptr) == -1, "33wXf5Ne2xc null out");
    Require(AllBytesEqual(outFb.data(), 0, outFb.size(), 0xA5), "33wXf5Ne2xc null touched out");
    Require(errno == kPreservedErrno, "33wXf5Ne2xc null errno");

    cfg.Init(0u, 40, 0u, 8, -2, 17);
    Require(libSceVdecSvp9Unknown_33wXf5Ne2xc_nid_no_patch(cfg.bytes.data(), rec.bytes.data(), outFb.data()) == -1, "33wXf5Ne2xc width = -2");
    cfg.Init(0u, 40, 0u, 8, 30, -2);
    Require(libSceVdecSvp9Unknown_33wXf5Ne2xc_nid_no_patch(cfg.bytes.data(), rec.bytes.data(), outFb.data()) == -1, "33wXf5Ne2xc height = -2");
    cfg.Init(0u, 40, 0u, 8, 30, 0x04000000);
    Require(libSceVdecSvp9Unknown_33wXf5Ne2xc_nid_no_patch(cfg.bytes.data(), rec.bytes.data(), outFb.data()) == -1, "33wXf5Ne2xc height << 6 overflow");
    cfg.Init(0u, 40, 0u, 8, 0x81, 17);
    Require(libSceVdecSvp9Unknown_33wXf5Ne2xc_nid_no_patch(cfg.bytes.data(), rec.bytes.data(), outFb.data()) == -1, "33wXf5Ne2xc width > 0x80");
    cfg.Init(0u, 40, 0u, 8, 30, 0x41);
    Require(libSceVdecSvp9Unknown_33wXf5Ne2xc_nid_no_patch(cfg.bytes.data(), rec.bytes.data(), outFb.data()) == -1, "33wXf5Ne2xc height > 4096");
    rec.Init(1ULL, 0, 1u, 1u);
    cfg.Init(0u, 40, 0u, 8, 30, 0x45);
    Require(libSceVdecSvp9Unknown_33wXf5Ne2xc_nid_no_patch(cfg.bytes.data(), rec.bytes.data(), outFb.data()) == -1, "33wXf5Ne2xc height > 4352");

    struct FbCase {
        std::uint32_t profile;
        std::uint32_t flag8;
        std::int32_t wMb;
        std::int32_t hMb;
        std::uint32_t rec2c;
        std::uint64_t expectedSize;
    };
    const std::array<FbCase, 7> cases{{
        {0u, 0u, 30, 17, 0u, 0x330000ULL},
        {2u, 0u, 30, 17, 0u, 0x660000ULL},
        {0u, 1u, 30, 17, 0u, 0x3fc000ULL},
        {0u, 0u, -1, 17, 0u, 0x3000000ULL},
        {0u, 0u, 30, -1, 1u, 0x3300000ULL},
        {0u, 0u, 0x80, 0x40, 0u, 0x3000000ULL},
        {0u, 0u, 0x80, 0x44, 1u, 0x3300000ULL},
    }};
    for (const auto& entry : cases) {
        cfg.Init(entry.profile, 40, entry.flag8, 8, entry.wMb, entry.hMb);
        rec.Init(1ULL, 0, 1u, entry.rec2c);
        outFb.fill(0xA5);
        errno = kPreservedErrno;
        Require(libSceVdecSvp9Unknown_33wXf5Ne2xc_nid_no_patch(cfg.bytes.data(), rec.bytes.data(), outFb.data()) == 0, "33wXf5Ne2xc valid");
        Require(errno == kPreservedErrno, "33wXf5Ne2xc errno preserved");
        Require(AllBytesEqual(outFb.data(), 0, 0x08, 0xA5), "33wXf5Ne2xc base untouched");
        Require(ReadU64(outFb.data(), 0x08) == entry.expectedSize, "33wXf5Ne2xc size");
        Require(ReadU32(outFb.data(), 0x10) == 0x100u, "33wXf5Ne2xc alignment");
        Require(AllBytesEqual(outFb.data(), 0x14, outFb.size(), 0xA5), "33wXf5Ne2xc tail untouched");
    }
}

void TestMemoryQuery() {
    ConfigBuf cfg{};
    cfg.Init(0u, 40, 0u, 8, 30, 17);
    RecBuf rec{};
    rec.Init(1ULL, 0, 0u, 0u);
    std::array<std::uint8_t, 0x48> outMem{};
    outMem.fill(0xA5);

    errno = kPreservedErrno;
    Require(libSceVdecSvp9Unknown_BFFONbbPMXw_nid_no_patch(nullptr, rec.bytes.data(), outMem.data()) == -1, "BFFONbbPMXw null cfg");
    Require(libSceVdecSvp9Unknown_BFFONbbPMXw_nid_no_patch(cfg.bytes.data(), nullptr, outMem.data()) == -1, "BFFONbbPMXw null rec");
    Require(libSceVdecSvp9Unknown_BFFONbbPMXw_nid_no_patch(cfg.bytes.data(), rec.bytes.data(), nullptr) == -1, "BFFONbbPMXw null out");
    Require(errno == kPreservedErrno, "BFFONbbPMXw null errno");

    cfg.Init(1u, 40, 0u, 8, 30, 17);
    Require(libSceVdecSvp9Unknown_BFFONbbPMXw_nid_no_patch(cfg.bytes.data(), rec.bytes.data(), outMem.data()) == -1, "BFFONbbPMXw profile 1");
    cfg.Init(3u, 40, 0u, 8, 30, 17);
    Require(libSceVdecSvp9Unknown_BFFONbbPMXw_nid_no_patch(cfg.bytes.data(), rec.bytes.data(), outMem.data()) == -1, "BFFONbbPMXw profile 3");

    for (std::int32_t badLevel : {0, 9, 63, -2}) {
        cfg.Init(0u, badLevel, 0u, 8, 30, 17);
        Require(libSceVdecSvp9Unknown_BFFONbbPMXw_nid_no_patch(cfg.bytes.data(), rec.bytes.data(), outMem.data()) == -1, "BFFONbbPMXw bad level");
    }

    cfg.Init(0u, 40, 0u, 8, 0, 17);
    Require(libSceVdecSvp9Unknown_BFFONbbPMXw_nid_no_patch(cfg.bytes.data(), rec.bytes.data(), outMem.data()) == -1, "BFFONbbPMXw w=0");
    cfg.Init(0u, 40, 0u, 8, 30, 0);
    Require(libSceVdecSvp9Unknown_BFFONbbPMXw_nid_no_patch(cfg.bytes.data(), rec.bytes.data(), outMem.data()) == -1, "BFFONbbPMXw h=0");
    cfg.Init(0u, 40, 0u, 8, -2, 17);
    Require(libSceVdecSvp9Unknown_BFFONbbPMXw_nid_no_patch(cfg.bytes.data(), rec.bytes.data(), outMem.data()) == -1, "BFFONbbPMXw w=-2");
    cfg.Init(0u, 40, 0u, 8, 30, -2);
    Require(libSceVdecSvp9Unknown_BFFONbbPMXw_nid_no_patch(cfg.bytes.data(), rec.bytes.data(), outMem.data()) == -1, "BFFONbbPMXw h=-2");
    cfg.Init(0u, 40, 0u, 8, 30, 0x04000000);
    Require(libSceVdecSvp9Unknown_BFFONbbPMXw_nid_no_patch(cfg.bytes.data(), rec.bytes.data(), outMem.data()) == -1, "BFFONbbPMXw h=0x04000000");
    cfg.Init(0u, 40, 0u, 8, 0x101, 17);
    Require(libSceVdecSvp9Unknown_BFFONbbPMXw_nid_no_patch(cfg.bytes.data(), rec.bytes.data(), outMem.data()) == -1, "BFFONbbPMXw w=0x101");
    cfg.Init(0u, 40, 0u, 8, 0x81, 17);
    Require(libSceVdecSvp9Unknown_BFFONbbPMXw_nid_no_patch(cfg.bytes.data(), rec.bytes.data(), outMem.data()) == -1, "BFFONbbPMXw w=0x81");
    cfg.Init(0u, 40, 0u, 8, 30, 0x41);
    Require(libSceVdecSvp9Unknown_BFFONbbPMXw_nid_no_patch(cfg.bytes.data(), rec.bytes.data(), outMem.data()) == -1, "BFFONbbPMXw h=0x41");
    cfg.Init(0u, 40, 0u, -2, 30, 17);
    Require(libSceVdecSvp9Unknown_BFFONbbPMXw_nid_no_patch(cfg.bytes.data(), rec.bytes.data(), outMem.data()) == -1, "BFFONbbPMXw dpb=-2");
    cfg.Init(0u, 40, 0u, INT32_MIN, 30, 17);
    Require(libSceVdecSvp9Unknown_BFFONbbPMXw_nid_no_patch(cfg.bytes.data(), rec.bytes.data(), outMem.data()) == -1, "BFFONbbPMXw dpb=INT32_MIN");
    cfg.Init(0u, 40, 0u, 9, 30, 17);
    Require(libSceVdecSvp9Unknown_BFFONbbPMXw_nid_no_patch(cfg.bytes.data(), rec.bytes.data(), outMem.data()) == -1, "BFFONbbPMXw dpb=9");
    Require(AllBytesEqual(outMem.data(), 0, outMem.size(), 0xA5), "BFFONbbPMXw invalid touched out");

    for (std::int32_t validLevel : {-1, 10, 11, 20, 21, 30, 31, 40, 41, 50, 51, 52, 60, 61, 62}) {
        cfg.Init(0u, validLevel, 0u, 8, 30, 17);
        rec.Init(1ULL, 0, 0u, 0u);
        outMem.fill(0xA5);
        Require(libSceVdecSvp9Unknown_BFFONbbPMXw_nid_no_patch(cfg.bytes.data(), rec.bytes.data(), outMem.data()) == 0, "BFFONbbPMXw valid level");
    }

    struct MemCase {
        std::uint32_t profile;
        std::int32_t dpb;
        std::int32_t wMb;
        std::int32_t hMb;
        std::int32_t rec08;
        std::uint32_t rec18;
        std::uint32_t rec2c;
        std::uint64_t size0;
        std::uint64_t size1;
        std::uint64_t size2;
    };
    const std::array<MemCase, 4> cases{{
        {0u, -1, 30, 17, 0, 0u, 0u, 0x3c8d00ULL, 0x72ffb00ULL, 0x2bfaf00ULL},
        {2u, 8, 30, 17, 0, 0u, 0u, 0x3c8d00ULL, 0x96deb00ULL, 0x4a5af00ULL},
        {0u, 4, -1, 17, 3, 2u, 0u, 0x25576c0ULL, 0x2b126300ULL, 0x278c2900ULL},
        {2u, 8, 30, -1, 4, 7u, 1u, 0x29871e0ULL, 0x8ad60000ULL, 0x65394900ULL},
    }};
    for (const auto& entry : cases) {
        cfg.Init(entry.profile, 40, 0u, entry.dpb, entry.wMb, entry.hMb);
        rec.Init(1ULL, entry.rec08, entry.rec18, entry.rec2c);
        outMem.fill(0xA5);
        errno = kPreservedErrno;
        Require(libSceVdecSvp9Unknown_BFFONbbPMXw_nid_no_patch(cfg.bytes.data(), rec.bytes.data(), outMem.data()) == 0, "BFFONbbPMXw concrete");
        Require(errno == kPreservedErrno, "BFFONbbPMXw errno preserved");
        Require(AllBytesEqual(outMem.data(), 0x00, 0x08, 0xA5), "BFFONbbPMXw block0 base untouched");
        Require(ReadU64(outMem.data(), 0x08) == entry.size0, "BFFONbbPMXw block0 size");
        Require(ReadU32(outMem.data(), 0x10) == 0x20u, "BFFONbbPMXw block0 align");
        Require(AllBytesEqual(outMem.data(), 0x14, 0x20, 0xA5), "BFFONbbPMXw block1 base/pad untouched");
        Require(ReadU64(outMem.data(), 0x20) == entry.size1, "BFFONbbPMXw block1 size");
        Require(ReadU32(outMem.data(), 0x28) == 0x100u, "BFFONbbPMXw block1 align");
        Require(AllBytesEqual(outMem.data(), 0x2c, 0x38, 0xA5), "BFFONbbPMXw block2 base/pad untouched");
        Require(ReadU64(outMem.data(), 0x38) == entry.size2, "BFFONbbPMXw block2 size");
        Require(ReadU32(outMem.data(), 0x40) == 0x100u, "BFFONbbPMXw block2 align");
        Require(AllBytesEqual(outMem.data(), 0x44, 0x48, 0xA5), "BFFONbbPMXw tail untouched");
    }

    cfg.Init(2u, 40, 0u, 8, 0x80, 0x44);
    rec.Init(1ULL, 11, 7u, 1u);
    outMem.fill(0xA5);
    Require(libSceVdecSvp9Unknown_BFFONbbPMXw_nid_no_patch(cfg.bytes.data(), rec.bytes.data(), outMem.data()) == 0, "BFFONbbPMXw max dims");
    Require(ReadU64(outMem.data(), 0x08) > 0ULL && ReadU64(outMem.data(), 0x08) <= 0xFFFFFFFFULL, "BFFONbbPMXw max dims block0 range");
    Require(ReadU32(outMem.data(), 0x10) == 0x20u, "BFFONbbPMXw max dims block0 align");
    Require(ReadU64(outMem.data(), 0x20) > 0ULL && ReadU64(outMem.data(), 0x20) <= 0xFFFFFFFFULL, "BFFONbbPMXw max dims block1 range");
    Require(ReadU32(outMem.data(), 0x28) == 0x100u, "BFFONbbPMXw max dims block1 align");
    Require(ReadU64(outMem.data(), 0x38) > 0ULL && ReadU64(outMem.data(), 0x38) <= 0xFFFFFFFFULL, "BFFONbbPMXw max dims block2 range");
    Require(ReadU32(outMem.data(), 0x40) == 0x100u, "BFFONbbPMXw max dims block2 align");
}

void TestPreDecoderValidation() {
    ConfigBuf cfg{};
    cfg.Init(0u, 40, 0u, 8, 30, 17);
    RecBuf rec{};
    rec.Init(1ULL, 0, 0u, 0u);
    std::array<std::uint8_t, 0x48> memInfo{};
    Require(libSceVdecSvp9Unknown_BFFONbbPMXw_nid_no_patch(cfg.bytes.data(), rec.bytes.data(), memInfo.data()) == 0, "query memInfo");
    WriteU64(memInfo.data(), 0x00, 0x10000ULL);
    WriteU64(memInfo.data(), 0x18, 0x20000ULL);
    WriteU64(memInfo.data(), 0x30, 0x30000ULL);

    std::uint8_t dummyA4 = 0;
    std::array<std::uint8_t, 0x2f0> handle{};
    errno = kPreservedErrno;

    Require(libSceVdecSvp9Unknown_JawAvxR7M_plus_k_nid_no_patch(nullptr, rec.bytes.data(), memInfo.data(), &dummyA4, handle.data()) == -1, "JawAvxR7M+k null cfg");
    Require(libSceVdecSvp9Unknown_JawAvxR7M_plus_k_nid_no_patch(cfg.bytes.data(), nullptr, memInfo.data(), &dummyA4, handle.data()) == -1, "JawAvxR7M+k null rec");
    Require(libSceVdecSvp9Unknown_JawAvxR7M_plus_k_nid_no_patch(cfg.bytes.data(), rec.bytes.data(), nullptr, &dummyA4, handle.data()) == -1, "JawAvxR7M+k null mem");
    Require(libSceVdecSvp9Unknown_JawAvxR7M_plus_k_nid_no_patch(cfg.bytes.data(), rec.bytes.data(), memInfo.data(), nullptr, handle.data()) == -1, "JawAvxR7M+k null a4");
    Require(libSceVdecSvp9Unknown_JawAvxR7M_plus_k_nid_no_patch(cfg.bytes.data(), rec.bytes.data(), memInfo.data(), &dummyA4, nullptr) == -1, "JawAvxR7M+k null handle");

    RecBuf zeroRec{};
    zeroRec.Init(0ULL, 0, 0u, 0u);
    Require(libSceVdecSvp9Unknown_JawAvxR7M_plus_k_nid_no_patch(cfg.bytes.data(), zeroRec.bytes.data(), memInfo.data(), &dummyA4, handle.data()) == -1, "JawAvxR7M+k rec[0]==0");

    ConfigBuf badCfg{};
    badCfg.Init(0u, 40, 0u, 8, -2, 17);
    Require(libSceVdecSvp9Unknown_JawAvxR7M_plus_k_nid_no_patch(badCfg.bytes.data(), rec.bytes.data(), memInfo.data(), &dummyA4, handle.data()) == -1, "JawAvxR7M+k w=-2");
    badCfg.Init(0u, 40, 0u, 8, 30, -2);
    Require(libSceVdecSvp9Unknown_JawAvxR7M_plus_k_nid_no_patch(badCfg.bytes.data(), rec.bytes.data(), memInfo.data(), &dummyA4, handle.data()) == -1, "JawAvxR7M+k h=-2");
    badCfg.Init(0u, 40, 0u, 8, 30, 0x04000000);
    Require(libSceVdecSvp9Unknown_JawAvxR7M_plus_k_nid_no_patch(badCfg.bytes.data(), rec.bytes.data(), memInfo.data(), &dummyA4, handle.data()) == -1, "JawAvxR7M+k h=0x04000000");
    badCfg.Init(0u, 40, 0u, INT32_MIN, 30, 17);
    Require(libSceVdecSvp9Unknown_JawAvxR7M_plus_k_nid_no_patch(badCfg.bytes.data(), rec.bytes.data(), memInfo.data(), &dummyA4, handle.data()) == -1, "JawAvxR7M+k dpb=INT32_MIN");

    auto checkMemReject = [&](std::size_t off, std::uint64_t badVal, bool isU32, const char* msg) {
        auto badMem = memInfo;
        if (isU32) {
            WriteU32(badMem.data(), off, static_cast<std::uint32_t>(badVal));
        } else {
            WriteU64(badMem.data(), off, badVal);
        }
        Require(libSceVdecSvp9Unknown_JawAvxR7M_plus_k_nid_no_patch(cfg.bytes.data(), rec.bytes.data(), badMem.data(), &dummyA4, handle.data()) == -8, msg);
    };
    checkMemReject(0x08, ReadU64(memInfo.data(), 0x08) - 1ULL, false, "JawAvxR7M+k size0 small");
    checkMemReject(0x10, 0x30ULL, true, "JawAvxR7M+k align0 bad");
    checkMemReject(0x20, ReadU64(memInfo.data(), 0x20) - 1ULL, false, "JawAvxR7M+k size1 small");
    checkMemReject(0x28, 0x180ULL, true, "JawAvxR7M+k align1 bad");
    checkMemReject(0x18, 0x20080ULL, false, "JawAvxR7M+k base1 misaligned");
    checkMemReject(0x38, ReadU64(memInfo.data(), 0x38) - 1ULL, false, "JawAvxR7M+k size2 small");
    checkMemReject(0x40, 0x180ULL, true, "JawAvxR7M+k align2 bad");
    checkMemReject(0x30, 0x30040ULL, false, "JawAvxR7M+k base2 misaligned");
    for (std::size_t alignOff : {std::size_t{0x10}, std::size_t{0x28}, std::size_t{0x40}}) {
        auto zeroAlignMem = memInfo;
        WriteU32(zeroAlignMem.data(), alignOff, 0u);
        RequireNotImplemented([&] {
            libSceVdecSvp9Unknown_JawAvxR7M_plus_k_nid_no_patch(cfg.bytes.data(), rec.bytes.data(), zeroAlignMem.data(), &dummyA4, handle.data());
        }, "JawAvxR7M+k align == 0 passes validation");
    }
    {
        auto reqAlignMem = memInfo;
        WriteU64(reqAlignMem.data(), 0x00, 0x13ULL);
        WriteU32(reqAlignMem.data(), 0x28, 0x200u);
        WriteU64(reqAlignMem.data(), 0x18, 0x20100ULL);
        WriteU32(reqAlignMem.data(), 0x40, 0x200u);
        WriteU64(reqAlignMem.data(), 0x30, 0x30100ULL);
        RequireNotImplemented([&] {
            libSceVdecSvp9Unknown_JawAvxR7M_plus_k_nid_no_patch(cfg.bytes.data(), rec.bytes.data(), reqAlignMem.data(), &dummyA4, handle.data());
        }, "JawAvxR7M+k base0 unchecked while base1/base2 checked against req align");
    }
    RequireNotImplemented([&] {
        libSceVdecSvp9Unknown_JawAvxR7M_plus_k_nid_no_patch(cfg.bytes.data(), rec.bytes.data(), memInfo.data(), &dummyA4, handle.data());
    }, "JawAvxR7M+k valid reaches NotImplemented");

    handle.fill(0);
    std::array<std::uint8_t, 0x20> bufA{};
    std::array<std::uint8_t, 0x20> bufB{};
    std::uint32_t outCount = 0xA5A5A5A5u;
    std::uint32_t outStatus32 = 0xA5A5A5A5u;
    std::uint64_t outStatus64 = 0xA5A5A5A5A5A5A5A5ULL;

    Require(libSceVdecSvp9Unknown_kUyX8iOM0E4_nid_no_patch(nullptr) == -1, "kUyX8iOM0E4 null");
    Require(libSceVdecSvp9Unknown_kUyX8iOM0E4_nid_no_patch(handle.data()) == -1, "kUyX8iOM0E4 bad magic");
    Require(libSceVdecSvp9Unknown_RuWQMaRBlEs_nid_no_patch(nullptr) == -1, "RuWQMaRBlEs null");
    Require(libSceVdecSvp9Unknown_RuWQMaRBlEs_nid_no_patch(handle.data()) == -1, "RuWQMaRBlEs bad magic");
    Require(libSceVdecSvp9Unknown_hWezRodOVBI_nid_no_patch(nullptr, &outCount) == -1, "hWezRodOVBI null");
    Require(libSceVdecSvp9Unknown_hWezRodOVBI_nid_no_patch(handle.data(), nullptr) == -1, "hWezRodOVBI null out");
    Require(libSceVdecSvp9Unknown_hWezRodOVBI_nid_no_patch(handle.data(), &outCount) == -1, "hWezRodOVBI bad magic");
    Require(outCount == 0xA5A5A5A5u, "hWezRodOVBI bad magic untouched outCount");
    Require(libSceVdecSvp9Unknown_x_plus_HO8XFm6io_nid_no_patch(nullptr, bufA.data()) == -1, "x+HO8XFm6io null");
    Require(libSceVdecSvp9Unknown_x_plus_HO8XFm6io_nid_no_patch(handle.data(), nullptr) == -1, "x+HO8XFm6io null info");
    Require(libSceVdecSvp9Unknown_x_plus_HO8XFm6io_nid_no_patch(handle.data(), bufA.data()) == -1, "x+HO8XFm6io bad magic");
    Require(libSceVdecSvp9Unknown_gktyGB_plus_7_minus_go_nid_no_patch(nullptr, 0, bufA.data(), bufB.data()) == -1, "gktyGB+7-go null");
    Require(libSceVdecSvp9Unknown_gktyGB_plus_7_minus_go_nid_no_patch(handle.data(), 0, nullptr, bufB.data()) == -1, "gktyGB+7-go null outA");
    Require(libSceVdecSvp9Unknown_gktyGB_plus_7_minus_go_nid_no_patch(handle.data(), 0, bufA.data(), nullptr) == -1, "gktyGB+7-go null outB");
    Require(libSceVdecSvp9Unknown_gktyGB_plus_7_minus_go_nid_no_patch(handle.data(), 0, bufA.data(), bufB.data()) == -1, "gktyGB+7-go bad magic");
    Require(libSceVdecSvp9Unknown_2dO21ExX5hM_nid_no_patch(nullptr, 0, bufA.data(), &outStatus32) == -1, "2dO21ExX5hM null");
    Require(libSceVdecSvp9Unknown_2dO21ExX5hM_nid_no_patch(handle.data(), 0, nullptr, &outStatus32) == -1, "2dO21ExX5hM null outA");
    Require(libSceVdecSvp9Unknown_2dO21ExX5hM_nid_no_patch(handle.data(), 0, bufA.data(), nullptr) == -1, "2dO21ExX5hM null outStatus");
    Require(libSceVdecSvp9Unknown_2dO21ExX5hM_nid_no_patch(handle.data(), 0, bufA.data(), &outStatus32) == -1, "2dO21ExX5hM bad magic");
    Require(libSceVdecSvp9Unknown_VmDqnxqvIFg_nid_no_patch(nullptr, bufA.data(), bufB.data(), &outStatus64) == -1, "VmDqnxqvIFg null");
    Require(libSceVdecSvp9Unknown_VmDqnxqvIFg_nid_no_patch(handle.data(), nullptr, bufB.data(), &outStatus64) == -1, "VmDqnxqvIFg null a2");
    Require(libSceVdecSvp9Unknown_VmDqnxqvIFg_nid_no_patch(handle.data(), bufA.data(), nullptr, &outStatus64) == -1, "VmDqnxqvIFg null a3");
    Require(libSceVdecSvp9Unknown_VmDqnxqvIFg_nid_no_patch(handle.data(), bufA.data(), bufB.data(), nullptr) == -1, "VmDqnxqvIFg null out");
    Require(libSceVdecSvp9Unknown_VmDqnxqvIFg_nid_no_patch(handle.data(), bufA.data(), bufB.data(), &outStatus64) == -1, "VmDqnxqvIFg bad magic");
    Require(outStatus64 == 0xA5A5A5A5A5A5A5A5ULL, "VmDqnxqvIFg bad magic untouched outStatus");

    WriteU32(handle.data(), 0x00, kHandleMagic);
    RequireNotImplemented([&] { libSceVdecSvp9Unknown_kUyX8iOM0E4_nid_no_patch(handle.data()); }, "kUyX8iOM0E4 valid magic");
    RequireNotImplemented([&] { libSceVdecSvp9Unknown_RuWQMaRBlEs_nid_no_patch(handle.data()); }, "RuWQMaRBlEs valid magic");

    WriteU32(handle.data(), 0x228, 4u);
    outCount = 0xA5A5A5A5u;
    Require(libSceVdecSvp9Unknown_hWezRodOVBI_nid_no_patch(handle.data(), &outCount) == -2 && outCount == 0u, "hWezRodOVBI state 4");
    WriteU32(handle.data(), 0x228, 0u);
    outCount = 0xA5A5A5A5u;
    Require(libSceVdecSvp9Unknown_hWezRodOVBI_nid_no_patch(handle.data(), &outCount) == 0 && outCount == 0u, "hWezRodOVBI state 0");
    WriteU32(handle.data(), 0x228, 1u);
    WriteU32(handle.data(), 0x164, 2u);
    outCount = 0xA5A5A5A5u;
    Require(libSceVdecSvp9Unknown_hWezRodOVBI_nid_no_patch(handle.data(), &outCount) == 0 && outCount == 0u, "hWezRodOVBI queued != 0");
    WriteU32(handle.data(), 0x164, 0u);
    RequireNotImplemented([&] { libSceVdecSvp9Unknown_hWezRodOVBI_nid_no_patch(handle.data(), &outCount); }, "hWezRodOVBI unqueued reaches NotImplemented");

    WriteU32(handle.data(), 0x228, 3u);
    Require(libSceVdecSvp9Unknown_x_plus_HO8XFm6io_nid_no_patch(handle.data(), bufA.data()) == -2, "x+HO8XFm6io state > 2");
    WriteU32(handle.data(), 0x228, 2u);
    WriteU32(handle.data(), 0x22c, 1u);
    Require(libSceVdecSvp9Unknown_x_plus_HO8XFm6io_nid_no_patch(handle.data(), bufA.data()) == -22, "x+HO8XFm6io queued22c != 0");
    WriteU32(handle.data(), 0x22c, 0u);
    RequireNotImplemented([&] { libSceVdecSvp9Unknown_x_plus_HO8XFm6io_nid_no_patch(handle.data(), bufA.data()); }, "x+HO8XFm6io valid reaches NotImplemented");

    WriteI32(handle.data(), 0x128, -1);
    Require(libSceVdecSvp9Unknown_gktyGB_plus_7_minus_go_nid_no_patch(handle.data(), 0, bufA.data(), bufB.data()) == -7, "gktyGB+7-go fd == -1");
    WriteI32(handle.data(), 0x128, 0);
    WriteU32(handle.data(), 0x164, 0u);
    Require(libSceVdecSvp9Unknown_gktyGB_plus_7_minus_go_nid_no_patch(handle.data(), 0, bufA.data(), bufB.data()) == -2, "gktyGB+7-go queued == 0");
    WriteU32(handle.data(), 0x164, 1u);
    RequireNotImplemented([&] { libSceVdecSvp9Unknown_gktyGB_plus_7_minus_go_nid_no_patch(handle.data(), 0, bufA.data(), bufB.data()); }, "gktyGB+7-go valid reaches NotImplemented");

    WriteU32(handle.data(), 0x228, 3u);
    Require(libSceVdecSvp9Unknown_2dO21ExX5hM_nid_no_patch(handle.data(), 0, bufA.data(), &outStatus32) == -2, "2dO21ExX5hM state > 2");
    WriteU32(handle.data(), 0x228, 2u);
    WriteU32(handle.data(), 0x22c, 0u);
    Require(libSceVdecSvp9Unknown_2dO21ExX5hM_nid_no_patch(handle.data(), 0, bufA.data(), &outStatus32) == -12, "2dO21ExX5hM queued22c == 0");
    Require(outStatus32 == 0xA5A5A5A5u, "2dO21ExX5hM -12 untouched outStatus");
    WriteU32(handle.data(), 0x22c, 1u);
    RequireNotImplemented([&] { libSceVdecSvp9Unknown_2dO21ExX5hM_nid_no_patch(handle.data(), 0, bufA.data(), &outStatus32); }, "2dO21ExX5hM valid reaches NotImplemented");
    Require(outStatus32 == 0u, "2dO21ExX5hM zeroed outStatus");

    WriteU32(handle.data(), 0x228, 4u);
    outStatus64 = 0xA5A5A5A5A5A5A5A5ULL;
    Require(libSceVdecSvp9Unknown_VmDqnxqvIFg_nid_no_patch(handle.data(), bufA.data(), bufB.data(), &outStatus64) == -2 && outStatus64 == 1ULL, "VmDqnxqvIFg state 4");
    WriteU32(handle.data(), 0x228, 2u);
    WriteU32(handle.data(), 0x158, 3u);
    WriteU32(handle.data(), 0x164, 4u);
    outStatus64 = 0xA5A5A5A5A5A5A5A5ULL;
    Require(libSceVdecSvp9Unknown_VmDqnxqvIFg_nid_no_patch(handle.data(), bufA.data(), bufB.data(), &outStatus64) == -22 && outStatus64 == 1ULL, "VmDqnxqvIFg queued > limit");
    WriteU32(handle.data(), 0x164, 3u);
    outStatus64 = 0xA5A5A5A5A5A5A5A5ULL;
    RequireNotImplemented([&] { libSceVdecSvp9Unknown_VmDqnxqvIFg_nid_no_patch(handle.data(), bufA.data(), bufB.data(), &outStatus64); }, "VmDqnxqvIFg valid reaches NotImplemented");
    Require(outStatus64 == 1ULL, "VmDqnxqvIFg set outStatus 1");
    Require(errno == kPreservedErrno, "PreDecoderValidation errno preserved");
}

} // namespace

int main() {
    GuestArena::GuestArenaSetHostReadHook_nid_no_patch(ClobberErrno);
    GuestArena::GuestArenaSetHostWriteHook_nid_no_patch(ClobberErrno);
    TestInstanceCountQuery();
    TestFrameBufferQuery();
    TestMemoryQuery();
    TestPreDecoderValidation();
    GuestArena::GuestArenaSetHostReadHook_nid_no_patch(nullptr);
    GuestArena::GuestArenaSetHostWriteHook_nid_no_patch(nullptr);
    std::puts("libSceVdecSvp9 tests passed");
    return 0;
}
