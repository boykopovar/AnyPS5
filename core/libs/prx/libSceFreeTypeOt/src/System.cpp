// Derived from FreeType src/base/ftsystem.c and src/base/ftobjs.c (FreeType 2.4.12, commit cecf93ef90c660a8b0b45e5adbbfb5ea443fb6b9).
// Copyright 1996-2013 by David Turner, Robert Wilhelm, and Werner Lemberg.
// This file is part of the FreeType project, and may only be used, modified, and distributed
// under the terms of the FreeType project license, LICENSE.TXT. By continuing to use, modify,
// or distribute this file you indicate that you have read the license and understand and accept it fully.

#include "prx/libSceFreeTypeOt/include/System.hpp"
#include "prx/libSceFreeTypeOt/include/Memory.hpp"
#include "prx/libc/include/FilesystemError.hpp"
#include "prx/libc/include/General.hpp"

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <ios>
#include <limits>
#include <memory>
#include <system_error>

using namespace FreeType;

namespace {

constexpr int CannotOpenResource = 0x01;
constexpr int InvalidArgument = 0x06;
constexpr int InvalidLibraryHandle = 0x21;
constexpr int InvalidStreamHandle = 0x28;
constexpr int CannotOpenStream = 0x51;
constexpr std::uint32_t OpenMemory = 0x1;
constexpr std::uint32_t OpenStream = 0x2;
constexpr std::uint32_t OpenPathname = 0x4;

std::ifstream& File(GuestStreamRec* stream) {
    return *static_cast<std::ifstream*>(stream->descriptor);
}

void APS5_VABI StreamClose(GuestStreamRec* stream) {
    delete &File(stream);
    stream->descriptor = nullptr;
    stream->size = 0;
    stream->base = nullptr;
}

std::uint64_t APS5_VABI StreamIo(GuestStreamRec* stream, std::uint64_t offset, std::uint8_t* buffer, std::uint64_t count) {
    if (!count && offset > stream->size) return 1;
    auto& file = File(stream);
    file.clear();
    if (stream->pos != offset) {
        if (offset > static_cast<std::uint64_t>(std::numeric_limits<std::streamoff>::max())) NotImplemented_nid_no_patch("FT_Stream_Open");
        file.seekg(static_cast<std::streamoff>(offset), std::ios::beg);
    }
    if (!count || !file) return 0;
    const auto request = static_cast<std::streamsize>(std::min<std::uint64_t>(count, std::numeric_limits<std::streamsize>::max()));
    file.read(reinterpret_cast<char*>(buffer), request);
    return static_cast<std::uint64_t>(file.gcount());
}

}

extern "C" {

int APS5_VABI FT_Stream_Open_nid_postfix(GuestStreamRec* stream, const char* filepathname) {
    if (!stream) return InvalidStreamHandle;
    if (!filepathname) NotImplemented_nid_no_patch(__func__);
    stream->descriptor = nullptr;
    stream->pathname = const_cast<char*>(filepathname);
    stream->base = nullptr;
    stream->pos = 0;
    stream->read = nullptr;
    stream->close = nullptr;
    std::filesystem::path hostPath;
    if (ResolvePathError_nid_no_patch(filepathname, hostPath) != 0) return CannotOpenResource;
    std::error_code error;
    const auto status = std::filesystem::status(hostPath, error);
    if (error || !std::filesystem::exists(status)) return CannotOpenResource;
    if (!std::filesystem::is_regular_file(status)) NotImplemented_nid_no_patch(__func__);
    auto file = std::make_unique<std::ifstream>(hostPath, std::ios::binary);
    if (!*file) return CannotOpenResource;
    file->seekg(0, std::ios::end);
    const std::streamoff size = file->tellg();
    if (size < 0) NotImplemented_nid_no_patch(__func__);
    stream->size = static_cast<std::uint64_t>(size);
    if (!stream->size) return CannotOpenStream;
    file->seekg(0, std::ios::beg);
    stream->descriptor = file.release();
    stream->read = StreamIo;
    stream->close = StreamClose;
    return 0;
}

int APS5_VABI FT_Stream_New_nid_postfix(GuestRaccessLibrary* library, const GuestStreamOpenArgs* args, GuestStreamRec** astream) {
    if (!astream) NotImplemented_nid_no_patch(__func__);
    *astream = nullptr;
    if (!library) return InvalidLibraryHandle;
    if (!args) return InvalidArgument;
    GuestMemoryRec* memory = library->memory;
    if (!memory) NotImplemented_nid_no_patch(__func__);
    int error = 0;
    auto* stream = static_cast<GuestStreamRec*>(ft_mem_alloc_nid_postfix(memory, sizeof(GuestStreamRec), &error));
    if (error) return error;
    stream->memory = memory;
    if (args->flags & OpenMemory) {
        FT_Stream_OpenMemory_nid_postfix(stream, args->memoryBase, static_cast<std::uint64_t>(args->memorySize));
    } else if (args->flags & OpenPathname) {
        try {
            error = FT_Stream_Open_nid_postfix(stream, args->pathname);
        } catch (...) {
            ft_mem_free_nid_postfix(memory, stream);
            throw;
        }
        stream->pathname = args->pathname;
    } else if ((args->flags & OpenStream) && args->stream) {
        ft_mem_free_nid_postfix(memory, stream);
        stream = args->stream;
    } else {
        error = InvalidArgument;
    }
    if (error) {
        ft_mem_free_nid_postfix(memory, stream);
        stream = nullptr;
    } else {
        stream->memory = memory;
    }
    *astream = stream;
    return error;
}

}
