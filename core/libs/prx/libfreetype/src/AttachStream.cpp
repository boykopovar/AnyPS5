#include "prx/libfreetype/include/AttachStream.hpp"
#include "prx/libSceFreeType/include/GuestObjects.hpp"
#include "prx/libc/include/FilesystemError.hpp"

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <stdexcept>
#include <vector>

using namespace FreeType;

namespace {

struct AttachedStream {
    GuestStreamRec* guest = nullptr;
    FT_StreamRec_ host{};
    bool closed = false;
};

unsigned long StreamRead(FT_Stream stream, unsigned long offset, unsigned char* buffer, unsigned long count) {
    auto* attached = static_cast<AttachedStream*>(stream->descriptor.pointer);
    attached->guest->pos = stream->pos;
    const auto read = attached->guest->read(attached->guest, offset, buffer, count);
    return static_cast<unsigned long>(std::min<std::uint64_t>(read, count));
}

void StreamClose(FT_Stream stream) {
    auto* attached = static_cast<AttachedStream*>(stream->descriptor.pointer);
    if (attached->closed) return;
    attached->closed = true;
    attached->guest->pos = stream->pos;
    if (attached->guest->close) attached->guest->close(attached->guest);
}

bool ReadFontFile(const char* path, std::vector<unsigned char>& bytes) {
    std::filesystem::path hostPath;
    if (ResolvePathError_nid_no_patch(path, hostPath) != 0) return false;
    std::ifstream file(hostPath, std::ios::binary);
    if (!file) return false;
    bytes.assign(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
    return !file.bad();
}

}

extern "C" {

int APS5_VABI FT_Attach_Stream_nid_postfix(GuestFaceRec* face, const GuestOpenArgs* args) {
    GuestFace* owner = FaceOf(face);
    if (!owner) return FT_Err_Invalid_Face_Handle;
    if (!args) return FT_Err_Invalid_Argument;
    AttachedStream attached;
    std::vector<unsigned char> bytes;
    FT_Open_Args native{};
    native.flags = args->flags;
    if ((args->flags & FT_OPEN_STREAM) && args->stream) {
        auto* guest = static_cast<GuestStreamRec*>(args->stream);
        if (guest->size > std::numeric_limits<FT_ULong>::max() || guest->pos > std::numeric_limits<FT_ULong>::max()) {
            throw std::runtime_error("FT_Attach_Stream: a guest stream outside the host FT_ULong range is not implemented");
        }
        attached.guest = guest;
        attached.host.base = guest->base;
        attached.host.size = static_cast<FT_ULong>(guest->size);
        attached.host.pos = static_cast<FT_ULong>(guest->pos);
        attached.host.descriptor.pointer = &attached;
        attached.host.pathname.pointer = guest->pathname;
        attached.host.read = guest->read ? StreamRead : nullptr;
        attached.host.close = StreamClose;
        native.stream = &attached.host;
    }
    const std::uint32_t mode = args->flags & (FT_OPEN_MEMORY | FT_OPEN_STREAM | FT_OPEN_PATHNAME);
    if (mode == FT_OPEN_PATHNAME) {
        if (!args->pathname) return FT_Err_Invalid_Argument;
        if (!ReadFontFile(args->pathname, bytes)) return FT_Err_Cannot_Open_Resource;
        if (bytes.size() > static_cast<std::size_t>(std::numeric_limits<FT_Long>::max())) {
            throw std::runtime_error("FT_Attach_Stream: a file larger than the host FT_Long range is not implemented");
        }
        native.flags = FT_OPEN_MEMORY;
        native.memory_base = bytes.data();
        native.memory_size = static_cast<FT_Long>(bytes.size());
    } else if (mode == FT_OPEN_MEMORY) {
        if (args->memory_size < std::numeric_limits<FT_Long>::min() || args->memory_size > std::numeric_limits<FT_Long>::max()) {
            throw std::runtime_error("FT_Attach_Stream: a font buffer size outside the host FT_Long range is not implemented");
        }
        native.memory_base = args->memory_base;
        native.memory_size = static_cast<FT_Long>(args->memory_size);
    }
    const FT_Error error = FT_Attach_Stream(owner->host, &native);
    if (error == 0) SyncFace(*owner);
    return error;
}

}
