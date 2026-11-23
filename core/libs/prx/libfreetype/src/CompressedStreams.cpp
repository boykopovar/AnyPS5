#include "prx/libfreetype/include/LegacyExports.hpp"
#include "prx/libfreetype/include/GuestStream.hpp"
#include "prx/libc/include/General.hpp"

#include <ft2build.h>
#include FT_BZIP2_H
#include FT_GZIP_H
#include FT_LZW_H
#include FT_SYSTEM_H

#include <cerrno>
#include <cstddef>
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
    FreeType::GuestMemoryRec* guestMemory = nullptr;
    FT_MemoryRec_ memory{};
    FT_StreamRec input{};
    FT_StreamRec output{};
    std::size_t openAllocations = 0;
    bool opening = false;
    bool fileMissing = false;
    bool fault = false;
    bool closed = false;
};

std::mutex tableMutex;
std::unordered_map<GuestStreamRec*, std::shared_ptr<State>> table;

State& StateOf(FT_Memory memory) {
    return *static_cast<State*>(memory->user);
}

void* Allocate(FT_Memory memory, long size) {
    auto& state = StateOf(memory);
    auto* guest = state.guestMemory;
    if (!guest || !guest->alloc) {
        state.fault = true;
        return nullptr;
    }
    void* block = guest->alloc(guest, size);
    if (state.opening && state.openAllocations++ == 0) {
        state.fileMissing = !block;
    } else if (state.opening && state.fileMissing && block) {
        if (guest->free) guest->free(guest, block);
        state.fault = true;
        return nullptr;
    }
    return block;
}

void Release(FT_Memory memory, void* block) {
    auto& state = StateOf(memory);
    auto* guest = state.guestMemory;
    if (!guest || !guest->free) {
        state.fault = true;
        return;
    }
    guest->free(guest, block);
}

void* Reallocate(FT_Memory memory, long current, long size, void* block) {
    auto& state = StateOf(memory);
    auto* guest = state.guestMemory;
    if (!guest || !guest->realloc) {
        state.fault = true;
        return nullptr;
    }
    return guest->realloc(guest, current, size, block);
}

void WriteSourcePos(GuestStreamRec* source, unsigned long pos) {
    const std::uint64_t value = pos;
    GuestCopyOut_nid_no_patch(&source->pos, &value, sizeof(source->pos));
}

unsigned long SourceRead(FT_Stream stream, unsigned long offset, unsigned char* buffer, unsigned long count) {
    auto* state = static_cast<State*>(stream->descriptor.pointer);
    WriteSourcePos(state->source, stream->pos);
    GuestStreamRec sourceRec{};
    const bool copied = GuestCopyIn_nid_no_patch(&sourceRec, state->source, sizeof(sourceRec));
    if (!copied || !sourceRec.read) {
        state->fault = true;
        return 0;
    }
    const auto read = sourceRec.read(state->source, offset, buffer, count);
    if (!count) return read != 0;
    if (read > count) {
        state->fault = true;
        return 0;
    }
    return static_cast<unsigned long>(read);
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
    if (offset > std::numeric_limits<unsigned long>::max() || count > std::numeric_limits<unsigned long>::max() ||
        !state->output.descriptor.pointer)
        NotImplemented_nid_no_patch(__func__);
    const auto result = state->output.read(&state->output, static_cast<unsigned long>(offset), buffer, static_cast<unsigned long>(count));
    WriteSourcePos(state->source, state->input.pos);
    if (state->fault) NotImplemented_nid_no_patch(__func__);
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
    if (state->fault) NotImplemented_nid_no_patch(__func__);
    GuestStreamRec cleared{};
    const int savedErrno = errno;
    if (GuestCopyIn_nid_no_patch(&cleared, stream, sizeof(cleared))) {
        cleared.read = nullptr;
        cleared.close = nullptr;
        cleared.base = nullptr;
        cleared.descriptor.pointer = nullptr;
        GuestCopyOut_nid_no_patch(stream, &cleared, sizeof(cleared));
    }
    errno = savedErrno;
}

bool Overlaps(const GuestStreamRec* stream, const GuestStreamRec* source) {
    const auto first = reinterpret_cast<std::uintptr_t>(stream);
    const auto second = reinterpret_cast<std::uintptr_t>(source);
    return first < second + sizeof(GuestStreamRec) && second < first + sizeof(GuestStreamRec);
}

void Publish(GuestStreamRec* stream, const State& state) {
    GuestStreamRec localStream{};
    localStream.base = state.output.base;
    localStream.size = state.output.size;
    localStream.pos = state.output.pos;
    localStream.descriptor.pointer = state.output.descriptor.pointer;
    localStream.memory = state.guestMemory;
    localStream.read = state.output.read ? OutputRead : nullptr;
    localStream.close = state.output.close ? OutputClose : nullptr;
    GuestCopyOut_nid_no_patch(stream, &localStream, sizeof(localStream));
}

template <typename TOpen>
int Open(GuestStreamRec* stream, GuestStreamRec* source, TOpen open, const char* function) {
    if (!stream || !source) return FT_Err_Invalid_Stream_Handle;
    const int savedErrno = errno;
    const bool streamOk = GuestWritable_nid_no_patch(stream, sizeof(*stream));
    const bool sourceOk = GuestWritable_nid_no_patch(source, sizeof(*source));
    errno = savedErrno;
    if (!streamOk || !sourceOk) return FT_Err_Invalid_Stream_Handle;
    GuestStreamRec sourceRec{};
    if (!GuestCopyIn_nid_no_patch(&sourceRec, source, sizeof(sourceRec))) {
        errno = savedErrno;
        return FT_Err_Invalid_Stream_Handle;
    }
    errno = savedErrno;
    if (sourceRec.size > std::numeric_limits<unsigned long>::max() || sourceRec.pos > std::numeric_limits<unsigned long>::max())
        NotImplemented_nid_no_patch(function);
    auto state = std::make_shared<State>();
    state->source = source;
    state->guestMemory = sourceRec.memory;
    state->memory = {state.get(), Allocate, Release, Reallocate};
    state->input.base = sourceRec.base;
    state->input.size = static_cast<unsigned long>(sourceRec.size);
    state->input.pos = static_cast<unsigned long>(sourceRec.pos);
    state->input.descriptor.pointer = state.get();
    state->input.pathname.pointer = sourceRec.pathname.pointer;
    state->input.read = sourceRec.read ? SourceRead : nullptr;
    state->input.memory = &state->memory;
    const bool aliased = stream == source;
    FT_StreamRec& output = aliased ? state->input : state->output;
    output.descriptor.pointer = state.get();
    state->opening = true;
    const FT_Error error = open(&output, &state->input);
    state->opening = false;
    if (state->fault) NotImplemented_nid_no_patch(function);
    if (output.descriptor.pointer == state.get()) {
        WriteSourcePos(source, state->input.pos);
        return error;
    }
    if (aliased ? error == FT_Err_Ok : Overlaps(stream, source)) NotImplemented_nid_no_patch(function);
    if (aliased) {
        state->output = state->input;
    } else {
        WriteSourcePos(source, state->input.pos);
    }
    Publish(stream, *state);
    const std::lock_guard lock(tableMutex);
    table.erase(stream);
    if (state->output.read || state->output.close) table.emplace(stream, std::move(state));
    return error;
}

}

extern "C" {

int APS5_VABI FT_Stream_OpenGzip_nid_postfix(GuestStreamRec* stream, GuestStreamRec* source) {
    return Open(stream, source, FT_Stream_OpenGzip, __func__);
}

int APS5_VABI FT_Stream_OpenLZW_nid_postfix(GuestStreamRec* stream, GuestStreamRec* source) {
    return Open(stream, source, FT_Stream_OpenLZW, __func__);
}

#ifdef FT_CONFIG_OPTION_USE_BZIP2
int APS5_VABI FT_Stream_OpenBzip2_nid_postfix(GuestStreamRec* stream, GuestStreamRec* source) {
    return Open(stream, source, FT_Stream_OpenBzip2, __func__);
}
#else
int APS5_VABI FT_Stream_OpenBzip2_nid_postfix(GuestStreamRec*, GuestStreamRec*) {
    return FT_Err_Unimplemented_Feature;
}
#endif

}
