#include "prx/libSceImageUtil/include/LegacyExports.hpp"
#include "prx/libc/include/general/ExportMacros.hpp"
#include "prx/libSceImageUtil/include/InventoryExports.hpp"
#include "SceTypes.hpp"
#include "prx/libSceJpegDec/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libkernel/File/include/File.hpp"
#include "prx/libkernel/include/Api.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

struct GuestString {
    union {
        char bytes[16];
        const char* pointer;
    };
    std::uint64_t size;
    std::uint64_t capacity;
};
static_assert(sizeof(GuestString) == 32);

std::string ReadGuestPath(const void* address) {
    GuestString path{};
    if (!GuestCopyIn_nid_no_patch(&path, address, sizeof(path)) || path.size > path.capacity)
        throw std::invalid_argument("JPEG dimensions: invalid guest path");

    const auto* bytes = path.capacity < sizeof(path.bytes) ? path.bytes : path.pointer;
    std::string text(path.size, '\0');
    if (!text.empty() && !GuestCopyIn_nid_no_patch(text.data(), bytes, text.size()))
        throw std::invalid_argument("JPEG dimensions: unreadable guest path");
    return text;
}

class JpegFile {
public:
    explicit JpegFile(const char* path) : fd(sceKernelOpen(path, 0, 0)) {
        if (fd < 0) throw std::runtime_error("JPEG dimensions: file open failed after stat");
    }
    ~JpegFile() { sceKernelClose(fd); }
    JpegFile(const JpegFile&) = delete;
    JpegFile& operator=(const JpegFile&) = delete;

    void Read(std::span<std::uint8_t> bytes) const {
        std::int64_t offset = 0;
        while (!bytes.empty()) {
            const auto size = sceKernelPread(fd, bytes.data(), bytes.size(), offset);
            if (size <= 0) throw std::runtime_error("JPEG dimensions: incomplete file read");
            bytes = bytes.subspan(size);
            offset += size;
        }
    }

private:
    const int fd;
};

}

extern "C" {

APS5_DUMMY_FUN

}

extern "C" {

int APS5_VABI libSceImageUtilUnknown_CttPwxlzc1A_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("CttPwxlzc1A", libSceImageUtilUnknown_CttPwxlzc1A_nid_no_patch);

int APS5_VABI libSceImageUtilUnknown_o9hdefxVx_minus_0_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("o9hdefxVx-0", libSceImageUtilUnknown_o9hdefxVx_minus_0_nid_no_patch);

int APS5_VABI libSceImageUtilUnknown_pjWv_plus__plus_MzaJY_nid_no_patch(const void* path, std::uint64_t* dimensions) {
    if (!path || !dimensions) return static_cast<int>(0x809D4013u);

    const auto text = ReadGuestPath(path);
    FileStat stat{};
    if (sceKernelStat(text.c_str(), &stat) != 0) return static_cast<int>(0x809D401Eu);
    if (stat.st_size <= 0 || stat.st_size > UINT32_MAX)
        throw std::length_error("JPEG dimensions: unsupported file size");

    const JpegFile file(text.c_str());
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(stat.st_size));
    file.Read(bytes);
    const JpegDecParseParam request{bytes.data(), static_cast<std::uint32_t>(bytes.size()), 0, 1};
    JpegDecImageInfo info{};
    if (JpegDecParseHeader_nid_no_patch(&request, &info) != 0) return static_cast<int>(0x809D4050u);

    const std::array<std::uint64_t, 2> result{info.image_width, info.image_height};
    if (!GuestCopyOut_nid_no_patch(dimensions, result.data(), sizeof(result)))
        throw std::invalid_argument("JPEG dimensions: unwritable output");
    return 0;
}
APS5_EXPORT("pjWv++MzaJY", libSceImageUtilUnknown_pjWv_plus__plus_MzaJY_nid_no_patch);

int APS5_VABI libSceImageUtilUnknown_rSoaNeazErw_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("rSoaNeazErw", libSceImageUtilUnknown_rSoaNeazErw_nid_no_patch);

int APS5_VABI libSceImageUtilUnknown_yjU_minus_5r3TDtc_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("yjU-5r3TDtc", libSceImageUtilUnknown_yjU_minus_5r3TDtc_nid_no_patch);

}

extern "C" {

int APS5_VABI libSceImageUtilUnknown_J805kfu8t4w_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("J805kfu8t4w", libSceImageUtilUnknown_J805kfu8t4w_nid_no_patch);

int APS5_VABI libSceImageUtilUnknown_PaJr2GfStvA_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("PaJr2GfStvA", libSceImageUtilUnknown_PaJr2GfStvA_nid_no_patch);

int APS5_VABI libSceImageUtilUnknown_TBLqZYmwmnU_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("TBLqZYmwmnU", libSceImageUtilUnknown_TBLqZYmwmnU_nid_no_patch);

int APS5_VABI libSceImageUtilUnknown_WePG8ETDGLg_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("WePG8ETDGLg", libSceImageUtilUnknown_WePG8ETDGLg_nid_no_patch);

int APS5_VABI libSceImageUtilUnknown_dBpqY1nOvJQ_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("dBpqY1nOvJQ", libSceImageUtilUnknown_dBpqY1nOvJQ_nid_no_patch);

int APS5_VABI libSceImageUtilUnknown_eMlkZeoHuxM_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("eMlkZeoHuxM", libSceImageUtilUnknown_eMlkZeoHuxM_nid_no_patch);

int APS5_VABI libSceImageUtilUnknown_oOjUpbj5GyI_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("oOjUpbj5GyI", libSceImageUtilUnknown_oOjUpbj5GyI_nid_no_patch);

int APS5_VABI libSceImageUtilUnknown_q_minus_Jp4vQAAXM_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("q-Jp4vQAAXM", libSceImageUtilUnknown_q_minus_Jp4vQAAXM_nid_no_patch);

}

extern "C" {

int APS5_VABI imageUtilLegacyUnknown_3Wr_minus_d3LSAAs_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("3Wr-d3LSAAs", imageUtilLegacyUnknown_3Wr_minus_d3LSAAs_nid_no_patch);

int APS5_VABI imageUtilLegacyUnknown_YpFwL2gtFWo_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("YpFwL2gtFWo", imageUtilLegacyUnknown_YpFwL2gtFWo_nid_no_patch);

int APS5_VABI imageUtilLegacyUnknown_hsXeUWEBufA_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("hsXeUWEBufA", imageUtilLegacyUnknown_hsXeUWEBufA_nid_no_patch);

int APS5_VABI imageUtilLegacyUnknown_vUK2ZahdWkQ_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("vUK2ZahdWkQ", imageUtilLegacyUnknown_vUK2ZahdWkQ_nid_no_patch);

int APS5_VABI imageUtilLegacyUnknown_wxDgXPkLn8o_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("wxDgXPkLn8o", imageUtilLegacyUnknown_wxDgXPkLn8o_nid_no_patch);

}
