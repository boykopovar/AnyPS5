#include "prx/libfreetype/include/LegacyExports.hpp"
#include "prx/libfreetype/include/GuestStream.hpp"
#include "prx/libc/include/General.hpp"

#include <ft2build.h>
#include FT_BZIP2_H
#include FT_GZIP_H
#include FT_LZW_H
#include FT_SYSTEM_H

#include <algorithm>
#include <cstdint>
#include <limits>
#include <memory>
#include <mutex>
#include <unordered_map>

namespace {

using LegacyFreeType::GuestStreamRec;

struct State {
    std::mutex mutex;
    GuestStreamRec* source = nullptr;
    FT_MemoryRec_ memory{};
    FT_StreamRec input{};
    FT_StreamRec output{};
    bool closed = false;
};

std::mutex tableMutex;
std::unordered_map<GuestStreamRec*, std::shared_ptr<State>> table;

FreeType::GuestMemoryRec* GuestMemory(FT_Memory memory) {
    return static_cast<FreeType::GuestMemoryRec*>(memory->user);
}

void* Allocate(FT_Memory memory, long size) {
    auto* guest = GuestMemory(memory);
    return guest->alloc(guest, size);
}

void Release(FT_Memory memory, void* block) {
    auto* guest = GuestMemory(memory);
    guest->free(guest, block);
}

void* Reallocate(FT_Memory memory, long current, long size, void* block) {
    auto* guest = GuestMemory(memory);
    return guest->realloc(guest, current, size, block);
}

unsigned long SourceRead(FT_Stream stream, unsigned long offset, unsigned char* buffer, unsigned long count) {
    auto* state = static_cast<State*>(stream->descriptor.pointer);
    state->source->pos = stream->pos;
    const auto read = state->source->read(state->source, offset, buffer, count);
    return static_cast<unsigned long>(std::min<std::uint64_t>(read, count));
}

std::shared_ptr<State> Find(GuestStreamRec* stream) {
    const std::lock_guard lock(tableMutex);
    const auto found = table.find(stream);
    return found == table.end() ? nullptr : found->second;
}

std::uint64_t APS5_VABI OutputRead(GuestStreamRec* stream, std::uint64_t offset, unsigned char* buffer, std::uint64_t count) {
    const auto state = Find(stream);
    if (!state) return 0;
    const std::lock_guard lock(state->mutex);
    if (state->closed) return 0;
    if (offset > std::numeric_limits<unsigned long>::max() || count > std::numeric_limits<unsigned long>::max())
        NotImplemented_nid_no_patch(__func__);
    const auto result = state->output.read(&state->output, static_cast<unsigned long>(offset), buffer, static_cast<unsigned long>(count));
    state->source->pos = state->input.pos;
    return result;
}

void APS5_VABI OutputClose(GuestStreamRec* stream) {
    std::shared_ptr<State> state;
    {
        const std::lock_guard lock(tableMutex);
        const auto found = table.find(stream);
        if (found == table.end()) return;
        state = std::move(found->second);
        table.erase(found);
    }
    const std::lock_guard lock(state->mutex);
    if (state->closed) return;
    state->closed = true;
    if (state->output.close) state->output.close(&state->output);
    stream->read = nullptr;
    stream->close = nullptr;
    stream->base = nullptr;
    stream->descriptor.pointer = nullptr;
}

template <typename TOpen>
int Open(GuestStreamRec* stream, GuestStreamRec* source, TOpen open) {
    if (!stream || !source) return FT_Err_Invalid_Stream_Handle;
    if (stream == source || !source->memory || !source->memory->alloc || !source->memory->free || !source->memory->realloc)
        NotImplemented_nid_no_patch(__func__);
    if (source->size > std::numeric_limits<unsigned long>::max() ||
        source->pos > std::numeric_limits<unsigned long>::max())
        NotImplemented_nid_no_patch(__func__);
    if (auto previous = Find(stream)) OutputClose(stream);
    auto state = std::make_shared<State>();
    state->source = source;
    state->memory = {source->memory, Allocate, Release, Reallocate};
    state->input.base = source->base;
    state->input.size = static_cast<unsigned long>(source->size);
    state->input.pos = static_cast<unsigned long>(source->pos);
    state->input.descriptor.pointer = state.get();
    state->input.pathname.pointer = source->pathname.pointer;
    state->input.read = source->read ? SourceRead : nullptr;
    state->input.memory = &state->memory;
    const FT_Error error = open(&state->output, &state->input);
    source->pos = state->input.pos;
    if (error != FT_Err_Ok) return error;
    *stream = {};
    stream->base = state->output.base;
    stream->size = state->output.size;
    stream->pos = state->output.pos;
    stream->descriptor.pointer = state->output.descriptor.pointer;
    stream->memory = source->memory;
    stream->read = state->output.read ? OutputRead : nullptr;
    stream->close = OutputClose;
    {
        const std::lock_guard lock(tableMutex);
        table[stream] = std::move(state);
    }
    return FT_Err_Ok;
}

}

extern "C" {

int APS5_VABI FT_Stream_OpenGzip_nid_postfix(GuestStreamRec* stream, GuestStreamRec* source) {
    return Open(stream, source, FT_Stream_OpenGzip);
}

int APS5_VABI FT_Stream_OpenLZW_nid_postfix(GuestStreamRec* stream, GuestStreamRec* source) {
    return Open(stream, source, FT_Stream_OpenLZW);
}

int APS5_VABI FT_Stream_OpenBzip2_nid_postfix(GuestStreamRec* stream, GuestStreamRec* source) {
    return Open(stream, source, FT_Stream_OpenBzip2);
}

}
