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
#include <array>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <ios>
#include <limits>
#include <memory>
#include <string>
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
constexpr std::size_t Chunk = 4096;

void CopyIn(void* destination, const void* source, std::size_t bytes, const char* caller) {
    const int savedErrno = errno;
    const bool copied = GuestCopyIn_nid_no_patch(destination, source, bytes);
    errno = savedErrno;
    if (!copied) NotImplementedAt_nid_no_patch(caller, __FILE__, __LINE__);
}

void CopyOut(void* destination, const void* source, std::size_t bytes, const char* caller) {
    const int savedErrno = errno;
    const bool copied = GuestCopyOut_nid_no_patch(destination, source, bytes);
    errno = savedErrno;
    if (!copied) NotImplementedAt_nid_no_patch(caller, __FILE__, __LINE__);
}

GuestStreamRec ReadStream(const GuestStreamRec* stream, const char* caller) {
    GuestStreamRec record{};
    CopyIn(&record, stream, sizeof(record), caller);
    return record;
}

std::string ReadText(const char* source, const char* caller) {
    std::string text;
    std::array<char, Chunk> buffer;
    auto address = reinterpret_cast<std::uintptr_t>(source);
    for (;;) {
        const std::size_t count = Chunk - address % Chunk;
        CopyIn(buffer.data(), reinterpret_cast<const void*>(address), count, caller);
        const auto* end = static_cast<const char*>(std::memchr(buffer.data(), 0, count));
        text.append(buffer.data(), end ? static_cast<std::size_t>(end - buffer.data()) : count);
        if (end) return text;
        address += count;
    }
}

void APS5_VABI StreamClose(GuestStreamRec* stream) {
    GuestStreamRec record = ReadStream(stream, __func__);
    delete static_cast<std::ifstream*>(record.descriptor);
    record.descriptor = nullptr;
    record.size = 0;
    record.base = nullptr;
    CopyOut(stream, &record, sizeof(record), __func__);
}

std::uint64_t APS5_VABI StreamIo(GuestStreamRec* stream, std::uint64_t offset, std::uint8_t* buffer, std::uint64_t count) {
    const GuestStreamRec record = ReadStream(stream, __func__);
    if (!count && offset > record.size) return 1;
    auto& file = *static_cast<std::ifstream*>(record.descriptor);
    file.clear();
    if (record.pos != offset) {
        if (offset > static_cast<std::uint64_t>(std::numeric_limits<std::streamoff>::max())) NotImplemented_nid_no_patch("FT_Stream_Open");
        file.seekg(static_cast<std::streamoff>(offset), std::ios::beg);
    }
    if (!count || !file) return 0;
    const std::uint64_t request = std::min<std::uint64_t>(count, std::numeric_limits<std::streamsize>::max());
    std::array<char, Chunk> chunk;
    std::uint64_t total = 0;
    while (total < request) {
        const auto wanted = static_cast<std::streamsize>(std::min<std::uint64_t>(Chunk, request - total));
        file.read(chunk.data(), wanted);
        const auto got = static_cast<std::size_t>(file.gcount());
        CopyOut(buffer + total, chunk.data(), got, __func__);
        total += got;
        if (got < static_cast<std::size_t>(wanted)) break;
    }
    return total;
}

}

extern "C" {

int APS5_VABI FT_Stream_Open_nid_postfix(GuestStreamRec* stream, const char* filepathname) {
    if (!stream) return InvalidStreamHandle;
    if (!filepathname) NotImplemented_nid_no_patch(__func__);
    GuestStreamRec record = ReadStream(stream, __func__);
    record.descriptor = nullptr;
    record.pathname = const_cast<char*>(filepathname);
    record.base = nullptr;
    record.pos = 0;
    record.read = nullptr;
    record.close = nullptr;
    CopyOut(stream, &record, sizeof(record), __func__);
    const std::string path = ReadText(filepathname, __func__);
    std::filesystem::path hostPath;
    if (ResolvePathError_nid_no_patch(path.c_str(), hostPath) != 0) return CannotOpenResource;
    std::error_code error;
    const auto status = std::filesystem::status(hostPath, error);
    if (error || !std::filesystem::exists(status)) return CannotOpenResource;
    if (!std::filesystem::is_regular_file(status)) NotImplemented_nid_no_patch(__func__);
    auto file = std::make_unique<std::ifstream>(hostPath, std::ios::binary);
    if (!*file) return CannotOpenResource;
    file->seekg(0, std::ios::end);
    const std::streamoff size = file->tellg();
    if (size < 0) NotImplemented_nid_no_patch(__func__);
    record.size = static_cast<std::uint64_t>(size);
    if (!record.size) {
        CopyOut(stream, &record, sizeof(record), __func__);
        return CannotOpenStream;
    }
    file->seekg(0, std::ios::beg);
    record.descriptor = file.get();
    record.read = StreamIo;
    record.close = StreamClose;
    CopyOut(stream, &record, sizeof(record), __func__);
    file.release();
    return 0;
}

int APS5_VABI FT_Stream_New_nid_postfix(GuestRaccessLibrary* library, const GuestStreamOpenArgs* args, GuestStreamRec** astream) {
    if (!astream) NotImplemented_nid_no_patch(__func__);
    GuestStreamRec* stream = nullptr;
    CopyOut(astream, &stream, sizeof(stream), __func__);
    if (!library) return InvalidLibraryHandle;
    if (!args) return InvalidArgument;
    GuestRaccessLibrary owner{};
    CopyIn(&owner, library, sizeof(owner), __func__);
    GuestMemoryRec* memory = owner.memory;
    if (!memory) NotImplemented_nid_no_patch(__func__);
    GuestStreamOpenArgs open{};
    CopyIn(&open, args, sizeof(open), __func__);
    int error = 0;
    stream = static_cast<GuestStreamRec*>(ft_mem_alloc_nid_postfix(memory, sizeof(GuestStreamRec), &error));
    if (error) return error;
    CopyOut(&stream->memory, &memory, sizeof(memory), __func__);
    if (open.flags & OpenMemory) {
        FT_Stream_OpenMemory_nid_postfix(stream, open.memoryBase, static_cast<std::uint64_t>(open.memorySize));
    } else if (open.flags & OpenPathname) {
        try {
            error = FT_Stream_Open_nid_postfix(stream, open.pathname);
        } catch (...) {
            ft_mem_free_nid_postfix(memory, stream);
            throw;
        }
        CopyOut(&stream->pathname, &open.pathname, sizeof(open.pathname), __func__);
    } else if ((open.flags & OpenStream) && open.stream) {
        ft_mem_free_nid_postfix(memory, stream);
        stream = open.stream;
    } else {
        error = InvalidArgument;
    }
    if (error) {
        ft_mem_free_nid_postfix(memory, stream);
        stream = nullptr;
    } else {
        CopyOut(&stream->memory, &memory, sizeof(memory), __func__);
    }
    CopyOut(astream, &stream, sizeof(stream), __func__);
    return error;
}

}
