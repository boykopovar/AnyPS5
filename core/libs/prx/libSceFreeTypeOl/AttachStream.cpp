#include "prx/libSceFreeTypeOl/libSceFreeTypeOl.hpp"
#include "prx/libSceFreeType/include/GuestObjects.hpp"
#include "prx/libc/include/FilesystemError.hpp"

#include <algorithm>
#include <bit>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <stdexcept>
#include <system_error>
#include <vector>

using namespace FreeType;

namespace {

constexpr FT_UInt SourceFlags = FT_OPEN_MEMORY | FT_OPEN_STREAM | FT_OPEN_PATHNAME;

unsigned long StreamRead(FT_Stream stream, unsigned long offset, unsigned char* buffer, unsigned long count) {
    auto* bridge = static_cast<GuestStreamBridge*>(stream->descriptor.pointer);
    bridge->guest->pos = stream->pos;
    const std::uint64_t read = bridge->guest->read(bridge->guest, offset, buffer, count);
    if (count == 0) return static_cast<unsigned long>(std::min<std::uint64_t>(read, std::numeric_limits<unsigned long>::max()));
    return static_cast<unsigned long>(std::min<std::uint64_t>(read, count));
}

void StreamClose(FT_Stream stream) {
    auto* bridge = static_cast<GuestStreamBridge*>(stream->descriptor.pointer);
    if (bridge->closed) return;
    bridge->closed = true;
    bridge->guest->pos = stream->pos;
    bridge->guest->cursor = stream->cursor;
    bridge->guest->limit = stream->limit;
    if (bridge->guest->close) bridge->guest->close(bridge->guest);
}

FT_Error ReadAttachedFile(const char* path, std::vector<unsigned char>& bytes) {
    std::filesystem::path hostPath;
    if (ResolvePathError_nid_no_patch(path, hostPath) != 0) return FT_Err_Cannot_Open_Resource;
    std::error_code error;
    const auto status = std::filesystem::status(hostPath, error);
    if (error || !std::filesystem::exists(status)) return FT_Err_Cannot_Open_Resource;
    if (!std::filesystem::is_regular_file(status)) {
        throw std::runtime_error("FT_Attach_Stream: attaching a path that is not a regular file is not implemented");
    }
    std::ifstream file(hostPath, std::ios::binary);
    if (!file) return FT_Err_Cannot_Open_Resource;
    bytes.assign(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
    if (file.bad() || bytes.empty()) return FT_Err_Cannot_Open_Stream;
    if (bytes.size() > static_cast<std::size_t>(std::numeric_limits<FT_Long>::max())) {
        throw std::runtime_error("FT_Attach_Stream: a file larger than the host FT_Long range is not implemented");
    }
    return 0;
}

void SyncAttachedMetrics(GuestFace& owner) {
    const FT_Face host = owner.host;
    owner.rec.face_flags = host->face_flags;
    owner.rec.bbox = {host->bbox.xMin, host->bbox.yMin, host->bbox.xMax, host->bbox.yMax};
    owner.rec.ascender = host->ascender;
    owner.rec.descender = host->descender;
}

}

extern "C" {

int APS5_VABI sceFreeTypeOlAttachStream_nid_postfix(GuestFaceRec* face, const GuestOpenArgs* parameters) {
    auto* owner = face ? static_cast<GuestFace*>(face->internal) : nullptr;
    if (!owner || !owner->host) return FT_Attach_Stream(nullptr, nullptr);
    if (!parameters) return FT_Attach_Stream(owner->host, nullptr);
    const FT_UInt mode = parameters->flags & SourceFlags;
    if (std::popcount(mode) > 1) {
        throw std::runtime_error("FT_Attach_Stream: mixed source flags are not implemented");
    }
    FT_Open_Args native{};
    native.flags = mode;
    std::vector<unsigned char> bytes;
    GuestStreamBridge bridge;
    if (mode == FT_OPEN_MEMORY) {
        const auto size = static_cast<std::uint64_t>(parameters->memory_size);
        if (size > std::numeric_limits<FT_ULong>::max()) {
            throw std::runtime_error("FT_Attach_Stream: a buffer size outside the host FT_ULong range is not implemented");
        }
        native.memory_base = parameters->memory_base;
        native.memory_size = static_cast<FT_Long>(static_cast<FT_ULong>(size));
    } else if (mode == FT_OPEN_PATHNAME) {
        if (!parameters->pathname) throw std::runtime_error("FT_Attach_Stream: a null path is not implemented");
        if (const FT_Error error = ReadAttachedFile(parameters->pathname, bytes)) return error;
        native.flags = FT_OPEN_MEMORY;
        native.memory_base = bytes.data();
        native.memory_size = static_cast<FT_Long>(bytes.size());
    } else if (mode == FT_OPEN_STREAM && parameters->stream) {
        auto* guest = static_cast<GuestStreamRec*>(parameters->stream);
        if (guest->size > std::numeric_limits<FT_ULong>::max() || guest->pos > std::numeric_limits<FT_ULong>::max()) {
            throw std::runtime_error("FT_Attach_Stream: a guest stream outside the host FT_ULong range is not implemented");
        }
        bridge.guest = guest;
        bridge.host.base = guest->base;
        bridge.host.size = static_cast<FT_ULong>(guest->size);
        bridge.host.pos = static_cast<FT_ULong>(guest->pos);
        bridge.host.descriptor.pointer = &bridge;
        bridge.host.pathname.pointer = guest->pathname;
        bridge.host.read = guest->read ? StreamRead : nullptr;
        bridge.host.close = StreamClose;
        native.stream = &bridge.host;
    }
    const FT_Error error = FT_Attach_Stream(owner->host, &native);
    SyncAttachedMetrics(*owner);
    return error;
}
APS5_EXPORT("ZqvTX8u1bEc", sceFreeTypeOlAttachStream_nid_postfix);

}
