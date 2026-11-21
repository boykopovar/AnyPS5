// SPDX-FileCopyrightText: Copyright 2024-2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "SceTypes.hpp"
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <string>

extern "C" {
#include <libavformat/avio.h>
#include <libavutil/error.h>
#include <libavutil/mem.h>
}

namespace AvPlayer {

class FileReplacementStream {
public:
    explicit FileReplacementStream(const AvPlayerFileReplacement& callbacks) : file(callbacks) {}

    ~FileReplacementStream() {
        if (context) {
            av_freep(&context->buffer);
            avio_context_free(&context);
        }
        if (opened) file.close(file.object_ptr);
    }

    FileReplacementStream(const FileReplacementStream&) = delete;
    FileReplacementStream& operator=(const FileReplacementStream&) = delete;

    bool Open(const std::string& path) {
        if (file.open(file.object_ptr, path.c_str()) < 0) return false;
        opened = true;
        size = file.size(file.object_ptr);
        if (size > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())) return false;
        auto* buffer = static_cast<std::uint8_t*>(av_malloc(BufferSize));
        if (!buffer) return false;
        context = avio_alloc_context(buffer, BufferSize, 0, this, &FileReplacementStream::read, nullptr, &FileReplacementStream::seek);
        if (!context) av_free(buffer);
        return context != nullptr;
    }

    AVIOContext* Context() const { return context; }

private:
    static constexpr int BufferSize = 64 * 1024;

    static int read(void* opaque, std::uint8_t* buffer, int size) {
        auto* self = static_cast<FileReplacementStream*>(opaque);
        if (self->position >= self->size) return AVERROR_EOF;
        const auto length = static_cast<std::uint32_t>(std::min<std::uint64_t>(static_cast<std::uint64_t>(size), self->size - self->position));
        const auto read = self->file.read_offset(self->file.object_ptr, buffer, self->position, length);
        if (read < 0) return AVERROR(EIO);
        if (read == 0) return AVERROR_EOF;
        self->position += static_cast<std::uint64_t>(read);
        return read;
    }

    static std::int64_t seek(void* opaque, std::int64_t offset, int whence) {
        auto* self = static_cast<FileReplacementStream*>(opaque);
        if (whence & AVSEEK_SIZE) return static_cast<std::int64_t>(self->size);
        std::int64_t base = 0;
        switch (whence & ~AVSEEK_FORCE) {
        case SEEK_SET: base = 0; break;
        case SEEK_CUR: base = static_cast<std::int64_t>(self->position); break;
        case SEEK_END: base = static_cast<std::int64_t>(self->size); break;
        default: return -1;
        }
        const auto limit = static_cast<std::int64_t>(self->size);
        const auto target = offset > limit - base ? limit : offset < -base ? 0 : base + offset;
        self->position = static_cast<std::uint64_t>(target);
        return target;
    }

    AvPlayerFileReplacement file;
    bool opened = false;
    std::uint64_t size = 0;
    std::uint64_t position = 0;
    AVIOContext* context = nullptr;
};

}
