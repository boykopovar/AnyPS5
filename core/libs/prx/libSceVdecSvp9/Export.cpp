#include "prx/libc/include/general/ExportMacros.hpp"
#include <algorithm>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

namespace {

constexpr std::uint32_t kHandleMagic = 0x4dec0deu;
constexpr std::uint32_t kFbAlign = 0x100u;

struct DimInfo {
    std::uint32_t w;
    std::uint32_t h;
    bool flag8;
    bool flag9;
};

struct MemBlock {
    std::uint64_t size;
    std::uint32_t align;
};

bool CopyGuestIn(void* dst, const void* src, std::size_t size) {
    const auto preserved = errno;
    const bool ok = GuestCopyIn_nid_no_patch(dst, src, size);
    errno = preserved;
    return ok;
}

bool CopyGuestOut(void* dst, const void* src, std::size_t size) {
    const auto preserved = errno;
    const bool ok = GuestCopyOut_nid_no_patch(dst, src, size);
    errno = preserved;
    return ok;
}

std::uint32_t ReadU32(const std::uint8_t* ptr) {
    std::uint32_t value = 0;
    std::memcpy(&value, ptr, sizeof(value));
    return value;
}

std::int32_t ReadI32(const std::uint8_t* ptr) {
    std::int32_t value = 0;
    std::memcpy(&value, ptr, sizeof(value));
    return value;
}

std::uint64_t ReadU64(const std::uint8_t* ptr) {
    std::uint64_t value = 0;
    std::memcpy(&value, ptr, sizeof(value));
    return value;
}

bool ReadHandleU32(const void* handle, std::size_t offset, std::uint32_t& outValue) {
    return CopyGuestIn(&outValue, static_cast<const std::uint8_t*>(handle) + offset, sizeof(outValue));
}

bool ReadHandleMagic(const void* handle) {
    std::uint32_t magic = 0;
    if (!ReadHandleU32(handle, 0, magic)) {
        return false;
    }
    return magic == kHandleMagic;
}

std::uint32_t ComputeInstanceCount(std::uint32_t count) {
    if (count == 0u) {
        return 6u;
    }
    return count < 7u ? count : 7u;
}

bool IsValidLevel(std::int32_t level) {
    if (level == 0x3e) {
        return true;
    }
    const std::uint32_t index = static_cast<std::uint32_t>(level) + 1u;
    return index <= 0x3eu && ((0x6038060180601801ULL >> index) & 1ULL) != 0ULL;
}

bool IsValidDpb(std::int32_t dpb) {
    return dpb >= -1 && dpb <= 8;
}

bool ComputeDimensions(const std::uint8_t* cfg, std::uint32_t recFlag2c, DimInfo& out) {
    const std::uint32_t limit = (recFlag2c != 0u) ? 0x1100u : 0x1000u;
    out.flag8 = (ReadU32(cfg + 0x08) == 1u);
    out.flag9 = (ReadU32(cfg + 0x00) == 2u);
    const std::int32_t wIn = ReadI32(cfg + 0x14);
    const std::int32_t hIn = ReadI32(cfg + 0x18);
    if (wIn == -1 || hIn == -1) {
        out.w = 0x2000u;
        out.h = limit;
        return true;
    }
    const std::int32_t hLimit = static_cast<std::int32_t>(limit >> 6);
    if (wIn < 0 || hIn < 0 || wIn > 0x80 || hIn > hLimit) {
        return false;
    }
    out.w = static_cast<std::uint32_t>(wIn) << 6;
    out.h = static_cast<std::uint32_t>(hIn) << 6;
    return true;
}

void ComputeFrameBufferLayout(const DimInfo& dim, std::uint64_t& outSize, std::uint32_t& outAlign) {
    const std::uint32_t pitch = dim.flag8
        ? (((((dim.w + 2u) / 3u) * 4u) + 0xfcu) & ~0xffu)
        : (((dim.w + 0xffu) & ~0xffu) << (dim.flag9 ? 1u : 0u));
    const std::uint32_t luma = pitch * dim.h;
    outSize = static_cast<std::uint64_t>(luma) + static_cast<std::uint64_t>(luma >> 1);
    outAlign = kFbAlign;
}

void ComputeMemoryBlocks(
    std::uint32_t w,
    std::uint32_t h,
    std::uint32_t profile,
    std::uint32_t rec18,
    std::int32_t rec08,
    std::int32_t dpbFrames,
    MemBlock (&blocks)[3]) {
    std::uint64_t cur[2] = {0ULL, 0x2268ULL};
    std::uint64_t cur2 = 0ULL;
    constexpr std::uint64_t cap = 0xffffffffULL;

    const auto alloc01 = [&](std::size_t idx, std::uint64_t size) {
        const std::uint64_t alignedSize = (size + 0xffULL) & ~0xffULL;
        const std::uint64_t next = ((cur[idx] + 0xffULL) & ~0xffULL) + alignedSize;
        if (next <= cap) {
            cur[idx] = next;
        }
    };

    const auto alloc2 = [&](std::uint64_t size, std::uint64_t align) {
        const std::uint64_t effectiveAlign = (align == 0ULL) ? 0x20ULL : align;
        const std::uint64_t next = ((cur2 + effectiveAlign - 1ULL) & ~(effectiveAlign - 1ULL)) + size;
        if (next <= cap) {
            cur2 = next;
        }
    };

    const std::uint32_t alignW = (w + 0x3fu) & ~0x3fu;
    const std::uint32_t alignH = (h + 0x3fu) & ~0x3fu;
    const std::uint32_t bpp8 = (profile != 0u) ? 10u : 8u;
    const std::uint32_t bppDiv8 = (bpp8 + 7u) >> 3;
    const std::uint32_t numInstances = ComputeInstanceCount(rec18);
    const std::int32_t dpbPos = (dpbFrames < 0) ? 0 : dpbFrames;
    const std::int32_t dpbClamped = (dpbPos < 8) ? dpbPos : 8;
    const std::uint32_t framesSum = static_cast<std::uint32_t>(rec08) + static_cast<std::uint32_t>(dpbClamped) + 1u;
    const std::uint32_t numFrames = std::min(framesSum, 20u);

    alloc2(0xba520ULL, 0ULL);
    alloc01(1, 0x2868ULL);
    alloc01(1, 0x23fffdcULL);
    alloc01(1, 0x5500ULL);
    alloc01(0, 0x2100ULL);

    const std::uint32_t sbW = alignW >> 6;
    const std::uint32_t sbH = alignH >> 6;
    const std::uint32_t sbCount = sbW * sbH;

    alloc01(0, static_cast<std::uint64_t>(sbCount) * 6144ULL);
    alloc01(0, static_cast<std::uint64_t>(sbCount) * 3072ULL);
    alloc01(0, static_cast<std::uint64_t>(sbCount) * 304ULL);
    alloc01(0, static_cast<std::uint64_t>(sbCount) * 6144ULL);

    const std::uint32_t lumaBufSz = alignW * alignH * 3u + 0x800u;
    alloc01(0, lumaBufSz);
    alloc2(0x5e80ULL, 0ULL);

    if (numFrames > 0u) {
        const std::uint32_t bh = alignH + 7u;
        const std::uint32_t eax = ((bh & 0xfffffff8u) + 0x7fu) & 0xffffc0u;
        const std::uint32_t edx = (((bh >> 3) * 4u + 0x3fu) & 0xffffe0u) + eax;
        const std::uint32_t ecx = (((((alignW + 7u) >> 3) * 4u + 0x20u) * bppDiv8) + 0xffu) & 0x7fffff00u;
        const std::uint64_t fbSz = static_cast<std::uint64_t>(edx * ecx) * 2ULL;
        for (std::uint32_t i = 0; i < numFrames; ++i) {
            alloc01(0, fbSz);
        }
    }

    const std::uint64_t miCnt = static_cast<std::uint64_t>((alignW >> 3) + 8u) * static_cast<std::uint64_t>((alignH >> 3) + 8u);
    for (std::uint32_t i = 0; i < 8u; ++i) {
        alloc01(1, miCnt * 0x58ULL);
        alloc2(miCnt * 8ULL, 0ULL);
    }
    for (int i = 0; i < 6; ++i) {
        alloc2(miCnt, 0x40ULL);
    }

    if (numInstances > 0u) {
        int sh = -1;
        do {
            ++sh;
        } while (static_cast<std::int32_t>(sbW >> sh) > 3);
        const std::uint32_t ebx = ((sbW * 48u) + 0x1fu) & ~0x1fu;
        const int sh2 = (sh < 2) ? 1 : sh;
        const std::uint32_t c2 = (sbW < 2u) ? sbW : 2u;
        const std::uint64_t s0 = (static_cast<std::uint64_t>(4u << (sh2 - 1)) << 6) * 5ULL;
        const std::uint64_t s1 = static_cast<std::uint64_t>(static_cast<std::int64_t>(static_cast<std::int32_t>(((sbW * 8u + 0x1fu) & ~0x1fu) + ebx)));
        const std::uint64_t s2 = static_cast<std::uint64_t>(static_cast<std::int64_t>(static_cast<std::int32_t>(lumaBufSz * bppDiv8)));
        const std::uint64_t s3 = static_cast<std::uint64_t>(static_cast<std::int64_t>(static_cast<std::int32_t>((sbCount << 9) + sbCount * 4u))) * 4ULL;
        const std::uint64_t s4 = static_cast<std::uint64_t>(sbCount) * 384ULL * 4ULL;
        const std::uint64_t s5 = static_cast<std::uint64_t>(sbCount) * 4ULL;
        const std::uint64_t tmp = static_cast<std::uint64_t>(static_cast<std::int64_t>(static_cast<std::int32_t>(((alignW >> 2) + (alignH >> 1) + 2u) * 4u + 1u)));
        const std::uint64_t s6 = tmp * 16ULL;
        const std::uint64_t s7 = tmp * 20ULL;
        const std::uint64_t s8 = static_cast<std::uint64_t>(static_cast<std::int64_t>(static_cast<std::int32_t>(sbW + c2 * (sbH - 1u) + 2u))) * 4ULL;
        const std::uint64_t s9 = static_cast<std::uint64_t>(static_cast<std::int64_t>(static_cast<std::int32_t>((alignH >> 3) * (alignW >> 3) + (alignH >> 2) * (alignW >> 2)))) * 4ULL;

        for (std::uint32_t i = 0; i < numInstances; ++i) {
            alloc2(0x5e80ULL, 0ULL);
            alloc2(s0, 0ULL);
            alloc2(s1, 0ULL);
            alloc01(1, s2);
            alloc01(1, s4);
            alloc01(1, s3);
            alloc01(1, s4);
            alloc01(1, s6);
            alloc2(s7, 0ULL);
            alloc01(1, s9);
            alloc01(1, s5);
            alloc2(s8, 0ULL);
        }
    }

    blocks[0] = {cur2 + 0x300ULL, 0x20u};
    blocks[1] = {cur[1], 0x100u};
    blocks[2] = {cur[0], 0x100u};
}

bool RunMemoryQuery(const std::uint8_t* cfg, const std::uint8_t* rec, MemBlock (&blocks)[3]) {
    const std::uint32_t profile = ReadU32(cfg + 0x00);
    if ((profile & ~2u) != 0u) {
        return false;
    }
    const std::int32_t level = ReadI32(cfg + 0x04);
    if (!IsValidLevel(level)) {
        return false;
    }
    const std::int32_t wIn = ReadI32(cfg + 0x14);
    const std::int32_t hIn = ReadI32(cfg + 0x18);
    if (hIn == 0 || wIn == 0 || wIn > 0x100 || hIn > 0x100) {
        return false;
    }
    const std::int32_t dpb = ReadI32(cfg + 0x0c);
    if (!IsValidDpb(dpb)) {
        return false;
    }
    DimInfo dim{};
    if (!ComputeDimensions(cfg, ReadU32(rec + 0x2c), dim)) {
        return false;
    }
    const std::int32_t dpbFrames = (dpb == -1) ? 8 : dpb;
    ComputeMemoryBlocks(dim.w, dim.h, profile, ReadU32(rec + 0x18), ReadI32(rec + 0x08), dpbFrames, blocks);
    return true;
}

} // namespace

extern "C" {

APS5_DUMMY_FUN

}

extern "C" {

int APS5_VABI libSceVdecSvp9Unknown_2dO21ExX5hM_nid_no_patch(void* handle, int flag, void* outA, std::uint32_t* outStatus) {
    (void)flag;
    if (handle == nullptr || outA == nullptr || outStatus == nullptr) {
        return -1;
    }
    if (!ReadHandleMagic(handle)) {
        return -1;
    }
    std::uint32_t state = 0;
    if (!ReadHandleU32(handle, 0x228, state)) {
        return -1;
    }
    if (state > 2u) {
        return -2;
    }
    std::uint32_t queued22c = 0;
    if (!ReadHandleU32(handle, 0x22c, queued22c)) {
        return -1;
    }
    if (queued22c == 0u) {
        return -12;
    }
    const std::uint32_t zero = 0u;
    if (!CopyGuestOut(outStatus, &zero, sizeof(zero))) {
        return -1;
    }
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("2dO21ExX5hM", libSceVdecSvp9Unknown_2dO21ExX5hM_nid_no_patch);

int APS5_VABI libSceVdecSvp9Unknown_33wXf5Ne2xc_nid_no_patch(const void* cfg, const void* rec, void* outFb) {
    if (cfg == nullptr || rec == nullptr || outFb == nullptr) {
        return -1;
    }
    std::uint8_t cfgBuf[0x1c]{};
    std::uint8_t recBuf[0x30]{};
    if (!CopyGuestIn(cfgBuf, cfg, sizeof(cfgBuf)) || !CopyGuestIn(recBuf, rec, sizeof(recBuf))) {
        return -1;
    }
    DimInfo dim{};
    if (!ComputeDimensions(cfgBuf, ReadU32(recBuf + 0x2c), dim)) {
        return -1;
    }
    std::uint64_t fbSize = 0;
    std::uint32_t fbAlign = 0;
    ComputeFrameBufferLayout(dim, fbSize, fbAlign);
    auto* outBytes = static_cast<std::uint8_t*>(outFb);
    if (!CopyGuestOut(outBytes + 0x08, &fbSize, sizeof(fbSize)) ||
        !CopyGuestOut(outBytes + 0x10, &fbAlign, sizeof(fbAlign))) {
        return -1;
    }
    return 0;
}
APS5_EXPORT("33wXf5Ne2xc", libSceVdecSvp9Unknown_33wXf5Ne2xc_nid_no_patch);

int APS5_VABI libSceVdecSvp9Unknown_BFFONbbPMXw_nid_no_patch(const void* cfg, const void* rec, void* outMem) {
    if (cfg == nullptr || rec == nullptr || outMem == nullptr) {
        return -1;
    }
    std::uint8_t cfgBuf[0x1c]{};
    std::uint8_t recBuf[0x30]{};
    if (!CopyGuestIn(cfgBuf, cfg, sizeof(cfgBuf)) || !CopyGuestIn(recBuf, rec, sizeof(recBuf))) {
        return -1;
    }
    MemBlock blocks[3]{};
    if (!RunMemoryQuery(cfgBuf, recBuf, blocks)) {
        return -1;
    }
    auto* outBytes = static_cast<std::uint8_t*>(outMem);
    if (!CopyGuestOut(outBytes + 0x08, &blocks[0].size, sizeof(blocks[0].size)) ||
        !CopyGuestOut(outBytes + 0x10, &blocks[0].align, sizeof(blocks[0].align)) ||
        !CopyGuestOut(outBytes + 0x20, &blocks[1].size, sizeof(blocks[1].size)) ||
        !CopyGuestOut(outBytes + 0x28, &blocks[1].align, sizeof(blocks[1].align)) ||
        !CopyGuestOut(outBytes + 0x38, &blocks[2].size, sizeof(blocks[2].size)) ||
        !CopyGuestOut(outBytes + 0x40, &blocks[2].align, sizeof(blocks[2].align))) {
        return -1;
    }
    return 0;
}
APS5_EXPORT("BFFONbbPMXw", libSceVdecSvp9Unknown_BFFONbbPMXw_nid_no_patch);

int APS5_VABI libSceVdecSvp9Unknown_JawAvxR7M_plus_k_nid_no_patch(const void* cfg, const void* rec, const void* memInfo, const void* a4, void* outHandle) {
    if (cfg == nullptr || rec == nullptr || memInfo == nullptr || a4 == nullptr || outHandle == nullptr) {
        return -1;
    }
    std::uint8_t cfgBuf[0x1c]{};
    std::uint8_t recBuf[0x30]{};
    std::uint8_t memBuf[0x48]{};
    if (!CopyGuestIn(cfgBuf, cfg, sizeof(cfgBuf)) ||
        !CopyGuestIn(recBuf, rec, sizeof(recBuf)) ||
        !CopyGuestIn(memBuf, memInfo, sizeof(memBuf))) {
        return -1;
    }
    if (ReadU64(recBuf + 0x00) == 0ULL) {
        return -1;
    }
    const std::int32_t dpb = ReadI32(cfgBuf + 0x0c);
    if (!IsValidDpb(dpb)) {
        return -1;
    }
    MemBlock req[3]{};
    if (!RunMemoryQuery(cfgBuf, recBuf, req)) {
        return -1;
    }
    const std::uint64_t size0 = ReadU64(memBuf + 0x08);
    const std::uint32_t align0 = ReadU32(memBuf + 0x10);
    const std::uint64_t base1 = ReadU64(memBuf + 0x18);
    const std::uint64_t size1 = ReadU64(memBuf + 0x20);
    const std::uint32_t align1 = ReadU32(memBuf + 0x28);
    const std::uint64_t base2 = ReadU64(memBuf + 0x30);
    const std::uint64_t size2 = ReadU64(memBuf + 0x38);
    const std::uint32_t align2 = ReadU32(memBuf + 0x40);
    if (size0 < req[0].size || (align0 % req[0].align) != 0u ||
        size1 < req[1].size || (align1 % req[1].align) != 0u ||
        (base1 % static_cast<std::uint64_t>(req[1].align)) != 0ULL ||
        (align2 % req[2].align) != 0u ||
        (base2 % static_cast<std::uint64_t>(req[2].align)) != 0ULL ||
        size2 < req[2].size) {
        return -8;
    }
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("JawAvxR7M+k", libSceVdecSvp9Unknown_JawAvxR7M_plus_k_nid_no_patch);

int APS5_VABI libSceVdecSvp9Unknown_RuWQMaRBlEs_nid_no_patch(void* handle) {
    if (handle == nullptr || !ReadHandleMagic(handle)) {
        return -1;
    }
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("RuWQMaRBlEs", libSceVdecSvp9Unknown_RuWQMaRBlEs_nid_no_patch);

int APS5_VABI libSceVdecSvp9Unknown_VGbdvyOsLzg_nid_no_patch(const void* cfg, const void* rec, std::uint32_t* outCount) {
    if (cfg == nullptr || rec == nullptr || outCount == nullptr) {
        return -1;
    }
    std::uint32_t rec18 = 0u;
    if (!CopyGuestIn(&rec18, static_cast<const std::uint8_t*>(rec) + 0x18, sizeof(rec18))) {
        return -1;
    }
    const std::uint32_t count = ComputeInstanceCount(rec18);
    if (!CopyGuestOut(outCount, &count, sizeof(count))) {
        return -1;
    }
    return 0;
}
APS5_EXPORT("VGbdvyOsLzg", libSceVdecSvp9Unknown_VGbdvyOsLzg_nid_no_patch);

int APS5_VABI libSceVdecSvp9Unknown_VmDqnxqvIFg_nid_no_patch(void* handle, const void* a2, const void* a3, std::uint64_t* outStatus) {
    if (handle == nullptr || a2 == nullptr || a3 == nullptr || outStatus == nullptr) {
        return -1;
    }
    if (!ReadHandleMagic(handle)) {
        return -1;
    }
    const std::uint64_t one = 1ULL;
    if (!CopyGuestOut(outStatus, &one, sizeof(one))) {
        return -1;
    }
    std::uint32_t state = 0;
    if (!ReadHandleU32(handle, 0x228, state)) {
        return -1;
    }
    if (state == 4u) {
        return -2;
    }
    std::uint32_t queued = 0;
    std::uint32_t limit158 = 0;
    if (!ReadHandleU32(handle, 0x164, queued) || !ReadHandleU32(handle, 0x158, limit158)) {
        return -1;
    }
    if (queued > limit158) {
        return -22;
    }
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("VmDqnxqvIFg", libSceVdecSvp9Unknown_VmDqnxqvIFg_nid_no_patch);

int APS5_VABI libSceVdecSvp9Unknown_gktyGB_plus_7_minus_go_nid_no_patch(void* handle, int flag, void* outA, void* outB) {
    (void)flag;
    if (handle == nullptr || outA == nullptr || outB == nullptr) {
        return -1;
    }
    if (!ReadHandleMagic(handle)) {
        return -1;
    }
    std::uint32_t val128 = 0;
    if (!ReadHandleU32(handle, 0x128, val128)) {
        return -1;
    }
    if (static_cast<std::int32_t>(val128) == -1) {
        return -7;
    }
    std::uint32_t queued = 0;
    if (!ReadHandleU32(handle, 0x164, queued)) {
        return -1;
    }
    if (queued == 0u) {
        return -2;
    }
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("gktyGB+7-go", libSceVdecSvp9Unknown_gktyGB_plus_7_minus_go_nid_no_patch);

int APS5_VABI libSceVdecSvp9Unknown_hWezRodOVBI_nid_no_patch(void* handle, std::uint32_t* outCount) {
    if (handle == nullptr || outCount == nullptr) {
        return -1;
    }
    if (!ReadHandleMagic(handle)) {
        return -1;
    }
    const std::uint32_t zero = 0u;
    if (!CopyGuestOut(outCount, &zero, sizeof(zero))) {
        return -1;
    }
    std::uint32_t state = 0;
    if (!ReadHandleU32(handle, 0x228, state)) {
        return -1;
    }
    if (state == 4u) {
        return -2;
    }
    if (state == 0u) {
        return 0;
    }
    std::uint32_t queued = 0;
    if (!ReadHandleU32(handle, 0x164, queued)) {
        return -1;
    }
    if (queued != 0u) {
        return 0;
    }
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("hWezRodOVBI", libSceVdecSvp9Unknown_hWezRodOVBI_nid_no_patch);

int APS5_VABI libSceVdecSvp9Unknown_kUyX8iOM0E4_nid_no_patch(void* handle) {
    if (handle == nullptr || !ReadHandleMagic(handle)) {
        return -1;
    }
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("kUyX8iOM0E4", libSceVdecSvp9Unknown_kUyX8iOM0E4_nid_no_patch);

int APS5_VABI libSceVdecSvp9Unknown_x_plus_HO8XFm6io_nid_no_patch(void* handle, const void* inInfo) {
    if (handle == nullptr || inInfo == nullptr) {
        return -1;
    }
    if (!ReadHandleMagic(handle)) {
        return -1;
    }
    std::uint32_t state = 0;
    if (!ReadHandleU32(handle, 0x228, state)) {
        return -1;
    }
    if (state > 2u) {
        return -2;
    }
    std::uint32_t queued22c = 0;
    if (!ReadHandleU32(handle, 0x22c, queued22c)) {
        return -1;
    }
    if (queued22c != 0u) {
        return -22;
    }
    std::uint8_t infoBuf[0x14]{};
    if (!CopyGuestIn(infoBuf, inInfo, sizeof(infoBuf))) {
        return -1;
    }
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("x+HO8XFm6io", libSceVdecSvp9Unknown_x_plus_HO8XFm6io_nid_no_patch);

}
