#include "prx/libSceVdecCore.native/include/Api.hpp"
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

namespace {

constexpr int InvalidArgument = static_cast<int>(0x80C00001u);
constexpr int UnsupportedCodec = static_cast<int>(0x80C00002u);
constexpr int InvalidParam = static_cast<int>(0x80C00007u);
constexpr int MemoryLimit = static_cast<int>(0x80C00015u);

template <typename T>
bool TryRead(const void* base, std::size_t offset, T& out) {
    if (base == nullptr) {
        return false;
    }
    const auto addr = reinterpret_cast<std::uintptr_t>(base);
    if (offset > UINTPTR_MAX - addr) {
        return false;
    }
    const int savedErrno = errno;
    const bool ok = GuestCopyIn_nid_no_patch(&out, reinterpret_cast<const void*>(addr + offset), sizeof(out));
    errno = savedErrno;
    return ok;
}

int CreateDecoderCommon(const void* config, const void* memory, void** decoder, const char* func) {
    if (decoder == nullptr) {
        return InvalidArgument;
    }
    std::uint32_t codecType = 0;
    if (!TryRead(config, 0, codecType)) {
        return InvalidArgument;
    }
    if (codecType > 0xbu) {
        return UnsupportedCodec;
    }
    std::uint64_t mem8 = 0;
    std::uint64_t mem18 = 0;
    std::uint64_t mem28 = 0;
    if (!TryRead(memory, 0x8, mem8) || !TryRead(memory, 0x18, mem18) || !TryRead(memory, 0x28, mem28)) {
        return InvalidArgument;
    }
    const std::uint32_t bit = 1u << codecType;
    if ((0xdf1u & bit) != 0) {
        if (mem8 == 0 || mem28 == 0) {
            return InvalidParam;
        }
    } else if ((0x20cu & bit) != 0) {
        if (mem8 == 0 || mem28 == 0 || mem18 == 0) {
            return InvalidParam;
        }
    } else {
        if (mem8 == 0 || mem18 == 0) {
            return InvalidParam;
        }
    }
    NotImplemented_nid_no_patch(func);
    return 0;
}

}

extern "C" {

int APS5_VABI sceVdecCoreCreateDecoderBid(std::int32_t bid, const void* config, const void* memory, void** decoder) {
    (void)bid;
    return CreateDecoderCommon(config, memory, decoder, __func__);
}

int APS5_VABI sceVdecCoreCreateDecoderSvp(std::int32_t svpParam, const void* config, const void* memory, void** decoder) {
    (void)svpParam;
    return CreateDecoderCommon(config, memory, decoder, __func__);
}

int APS5_VABI sceVdecCoreMapMemory(void* decoder, const void* address, std::size_t size) {
    (void)size;
    std::uint8_t count = 0;
    if (!TryRead(decoder, 0x138, count)) {
        return InvalidArgument;
    }
    if (count >= 0x10u) {
        return MemoryLimit;
    }
    if (address == nullptr) {
        return InvalidArgument;
    }
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceVdecCoreSetDecodeInputSvp(void* decoder, const void* pictureInfo, const void* auInfo, void* userData) {
    (void)pictureInfo;
    (void)userData;
    if (decoder == nullptr) {
        return InvalidArgument;
    }
    std::uint64_t auAddr = 0;
    if (!TryRead(auInfo, 0x10, auAddr)) {
        return InvalidArgument;
    }
    if ((auAddr & 0x3fu) != 0) {
        return InvalidParam;
    }
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

}
