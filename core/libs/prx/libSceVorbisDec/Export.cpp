#include "prx/libSceVorbisDec/include/Api.hpp"
#include "prx/libSceVorbisDec/include/GuestVorbisFile.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/GuestArena.hpp"

#include <vorbis/vorbisfile.h>

#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

using namespace VorbisDec;

extern "C" {
int anyps5_ov_test_open64(OggVorbis_File* vf, ogg_int64_t (*tell64)(void*));
ogg_int64_t anyps5_ov_bitrate64(OggVorbis_File* vf, int link);
ogg_int64_t anyps5_ov_bitrate_instant64(OggVorbis_File* vf);
}

static_assert(sizeof(vorbis_comment) == sizeof(GuestComment));
static_assert(offsetof(vorbis_comment, user_comments) == offsetof(GuestComment, user_comments));
static_assert(offsetof(vorbis_comment, comment_lengths) == offsetof(GuestComment, comment_lengths));
static_assert(offsetof(vorbis_comment, comments) == offsetof(GuestComment, comments));
static_assert(offsetof(vorbis_comment, vendor) == offsetof(GuestComment, vendor));
static_assert(SEEK_SET == 0 && SEEK_CUR == 1 && SEEK_END == 2);

namespace {

struct Handle {
    OggVorbis_File file{};
    void* datasource = nullptr;
    GuestCallbacks callbacks{};
    std::vector<GuestInfo> infos;
    void* floatArena = nullptr;
    std::size_t floatArenaBytes = 0;
    std::vector<float*> floatChannelPtrs;
    std::vector<float> floatSamples;

    ~Handle() {
        if (floatArena) {
            GuestArena::GuestArenaReset_nid_postfix(floatArena, floatArenaBytes);
            GuestArena::GuestArenaRelease_nid_postfix(floatArena, floatArenaBytes);
        }
        file.datasource = nullptr;
        ov_clear(&file);
    }
};

void ReleaseFloatArena(Handle& handle) {
    if (!handle.floatArena) return;
    GuestArena::GuestArenaReset_nid_postfix(handle.floatArena, handle.floatArenaBytes);
    GuestArena::GuestArenaRelease_nid_postfix(handle.floatArena, handle.floatArenaBytes);
    handle.floatArena = nullptr;
    handle.floatArenaBytes = 0;
}

struct Table {
    std::mutex mutex;
    std::unordered_map<const void*, std::unique_ptr<Handle>> handles;
};

Table& Handles() {
    static auto* table = new Table;
    return *table;
}

Handle* Find(const void* vf) {
    auto& table = Handles();
    const std::lock_guard lock(table.mutex);
    const auto found = table.handles.find(vf);
    return found == table.handles.end() ? nullptr : found->second.get();
}

std::unique_ptr<Handle> Take(const void* vf) {
    auto& table = Handles();
    const std::lock_guard lock(table.mutex);
    const auto found = table.handles.find(vf);
    if (found == table.handles.end()) return nullptr;
    auto handle = std::move(found->second);
    table.handles.erase(found);
    return handle;
}

void Store(const void* vf, std::unique_ptr<Handle> handle) {
    auto& table = Handles();
    const std::lock_guard lock(table.mutex);
    table.handles[vf] = std::move(handle);
}

void SyncInfos(Handle& handle) {
    for (std::size_t link = 0; link < handle.infos.size(); ++link) {
        const vorbis_info& info = handle.file.vi[link];
        handle.infos[link] = {info.version, info.channels, info.rate, info.bitrate_upper, info.bitrate_nominal, info.bitrate_lower, info.bitrate_window, info.codec_setup};
    }
}

void RequireFile(const void* vf, const char* function) {
    if (!vf) throw std::invalid_argument(std::string(function) + ": a null OggVorbis_File is not implemented");
}

template <typename TCall>
auto WithHandle(const void* vf, const char* function, TCall call) {
    RequireFile(vf, function);
    if (Handle* handle = Find(vf)) {
        const auto result = call(*handle);
        SyncInfos(*handle);
        return result;
    }
    Handle closed;
    return call(closed);
}

std::size_t ReadSource(void* buffer, std::size_t size, std::size_t count, void* source) {
    const auto* handle = static_cast<const Handle*>(source);
    return handle->callbacks.read_func(buffer, size, count, handle->datasource);
}

int SeekSource(void* source, ogg_int64_t offset, int whence) {
    const auto* handle = static_cast<const Handle*>(source);
    return handle->callbacks.seek_func(handle->datasource, offset, whence);
}

int CloseSource(void* source) {
    const auto* handle = static_cast<const Handle*>(source);
    return handle->callbacks.close_func(handle->datasource);
}

ogg_int64_t TellSource(void* source) {
    const auto* handle = static_cast<const Handle*>(source);
    return handle->callbacks.tell_func(handle->datasource);
}

struct FilterContext {
    GuestFilter filter;
    void* param;
};

void RunFilter(float** pcm, long channels, long samples, void* context) {
    const auto* filter = static_cast<const FilterContext*>(context);
    filter->filter(pcm, channels, samples, filter->param);
}

bool CopyGuestIn(void* destination, const void* source, std::size_t size) {
    const int saved = errno;
    const bool ok = GuestCopyIn_nid_no_patch(destination, source, size);
    errno = saved;
    return ok;
}

bool CopyGuestOut(void* destination, const void* source, std::size_t size) {
    const int saved = errno;
    const bool ok = GuestCopyOut_nid_no_patch(destination, source, size);
    errno = saved;
    return ok;
}

bool WritableGuest(void* destination, std::size_t size) {
    if (size == 0) return true;
    const int saved = errno;
    const GuestArena::HostWrite guard(destination, size);
    const bool ok = guard.Open() && GuestWritable_nid_no_patch(destination, size);
    errno = saved;
    return ok;
}

template <typename TCall>
std::int64_t ReadPcm(void* vf, const char* function, char* buffer, int length, int* bitstream, TCall call) {
    RequireFile(vf, function);
    if (bitstream && !WritableGuest(bitstream, sizeof(*bitstream))) return OV_EFAULT;
    if (length > 0 && (!buffer || !WritableGuest(buffer, static_cast<std::size_t>(length)))) return OV_EFAULT;
    std::vector<char> hostBuffer;
    char* target = buffer;
    if (length > 0) {
        hostBuffer.resize(static_cast<std::size_t>(length));
        target = hostBuffer.data();
    }
    int localBitstream = 0;
    const std::int64_t result = WithHandle(vf, function, [&](Handle& handle) -> std::int64_t {
        return call(handle, target, bitstream ? &localBitstream : nullptr);
    });
    if (result > 0) {
        if (buffer && !CopyGuestOut(buffer, hostBuffer.data(), static_cast<std::size_t>(result))) return OV_EFAULT;
        if (bitstream && !CopyGuestOut(bitstream, &localBitstream, sizeof(localBitstream))) return OV_EFAULT;
    }
    return result;
}

}

extern "C" {

int APS5_VABI sceVorbisDecOpenCallbacks(void* datasource, void* vf, const char* initial, std::int64_t ibytes, GuestCallbacks callbacks) {
    RequireFile(vf, __func__);
    if (initial && (ibytes < std::numeric_limits<long>::min() || ibytes > std::numeric_limits<long>::max())) {
        throw std::runtime_error(std::string(__func__) + ": an initial byte count outside the host long range is not implemented");
    }
    std::vector<char> initialCopy;
    if (initial && ibytes > 0) {
        initialCopy.resize(static_cast<std::size_t>(ibytes));
        if (!CopyGuestIn(initialCopy.data(), initial, initialCopy.size())) return OV_EFAULT;
    }
    Take(vf).reset();
    auto handle = std::make_unique<Handle>();
    handle->datasource = datasource;
    handle->callbacks = callbacks;
    const ov_callbacks host{
        callbacks.read_func ? ReadSource : nullptr,
        callbacks.seek_func ? SeekSource : nullptr,
        callbacks.close_func ? CloseSource : nullptr,
        nullptr,
    };
    const char* initialBytes = initial ? (ibytes > 0 ? initialCopy.data() : initial) : nullptr;
    const int headers = ov_test_callbacks(datasource ? handle.get() : nullptr, &handle->file, initialBytes, initial ? static_cast<long>(ibytes) : 0, host);
    if (headers != 0) return headers;
    const int result = anyps5_ov_test_open64(&handle->file, callbacks.tell_func ? TellSource : nullptr);
    if (result != 0) return result;
    handle->infos.resize(static_cast<std::size_t>(handle->file.links));
    SyncInfos(*handle);
    Store(vf, std::move(handle));
    return 0;
}

int APS5_VABI sceVorbisDecClear(void* vf) {
    const std::unique_ptr<Handle> handle = Take(vf);
    if (!handle) {
        Handle closed;
        return ov_clear(&closed.file);
    }
    void* const source = handle->file.datasource;
    handle->file.datasource = nullptr;
    const int result = ov_clear(&handle->file);
    if (source && handle->callbacks.close_func) handle->callbacks.close_func(handle->datasource);
    return result;
}

std::int64_t APS5_VABI sceVorbisDecBitrate(void* vf, int link) {
    return WithHandle(vf, __func__, [&](Handle& handle) -> std::int64_t { return anyps5_ov_bitrate64(&handle.file, link); });
}

std::int64_t APS5_VABI sceVorbisDecBitrateInstant(void* vf) {
    return WithHandle(vf, __func__, [](Handle& handle) -> std::int64_t { return anyps5_ov_bitrate_instant64(&handle.file); });
}

GuestComment* APS5_VABI sceVorbisDecComment(void* vf, int link) {
    return WithHandle(vf, __func__, [&](Handle& handle) { return reinterpret_cast<GuestComment*>(ov_comment(&handle.file, link)); });
}

int APS5_VABI sceVorbisDecCrossLap(void* first, void* second) {
    if (first != second) RequireFile(first, __func__);
    Handle* from = Find(first);
    if (first != second && from) RequireFile(second, __func__);
    Handle* to = second == first ? from : Find(second);
    Handle closedFrom;
    Handle closedTo;
    OggVorbis_File* fromFile = from ? &from->file : &closedFrom.file;
    OggVorbis_File* toFile = second == first ? fromFile : to ? &to->file : &closedTo.file;
    const int result = ov_crosslap(fromFile, toFile);
    if (from) SyncInfos(*from);
    if (to) SyncInfos(*to);
    return result;
}

int APS5_VABI sceVorbisDecGetDecodePriority(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceVorbisDecGetLastDecodeError(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceVorbisDecHalfrate(void* vf, int flag) {
    return WithHandle(vf, __func__, [&](Handle& handle) { return ov_halfrate(&handle.file, flag); });
}

int APS5_VABI sceVorbisDecHalfrateP(void* vf) {
    return WithHandle(vf, __func__, [](Handle& handle) { return ov_halfrate_p(&handle.file); });
}

GuestInfo* APS5_VABI sceVorbisDecInfo(void* vf, int link) {
    return WithHandle(vf, __func__, [&](Handle& handle) -> GuestInfo* {
        const vorbis_info* info = ov_info(&handle.file, link);
        return info ? &handle.infos.at(static_cast<std::size_t>(info - handle.file.vi)) : nullptr;
    });
}

int APS5_VABI sceVorbisDecPcmSeek(void* vf, std::int64_t position) {
    return WithHandle(vf, __func__, [&](Handle& handle) { return ov_pcm_seek(&handle.file, position); });
}

int APS5_VABI sceVorbisDecPcmSeekLap(void* vf, std::int64_t position) {
    return WithHandle(vf, __func__, [&](Handle& handle) { return ov_pcm_seek_lap(&handle.file, position); });
}

int APS5_VABI sceVorbisDecPcmSeekPage(void* vf, std::int64_t position) {
    return WithHandle(vf, __func__, [&](Handle& handle) { return ov_pcm_seek_page(&handle.file, position); });
}

int APS5_VABI sceVorbisDecPcmSeekPageLap(void* vf, std::int64_t position) {
    return WithHandle(vf, __func__, [&](Handle& handle) { return ov_pcm_seek_page_lap(&handle.file, position); });
}

std::int64_t APS5_VABI sceVorbisDecPcmTell(void* vf) {
    return WithHandle(vf, __func__, [](Handle& handle) -> std::int64_t { return ov_pcm_tell(&handle.file); });
}

std::int64_t APS5_VABI sceVorbisDecPcmTotal(void* vf, int link) {
    return WithHandle(vf, __func__, [&](Handle& handle) -> std::int64_t { return ov_pcm_total(&handle.file, link); });
}

int APS5_VABI sceVorbisDecRawSeek(void* vf, std::int64_t position) {
    return WithHandle(vf, __func__, [&](Handle& handle) { return ov_raw_seek(&handle.file, position); });
}

int APS5_VABI sceVorbisDecRawSeekLap(void* vf, std::int64_t position) {
    return WithHandle(vf, __func__, [&](Handle& handle) { return ov_raw_seek_lap(&handle.file, position); });
}

std::int64_t APS5_VABI sceVorbisDecRawTell(void* vf) {
    return WithHandle(vf, __func__, [](Handle& handle) -> std::int64_t { return ov_raw_tell(&handle.file); });
}

std::int64_t APS5_VABI sceVorbisDecRawTotal(void* vf, int link) {
    return WithHandle(vf, __func__, [&](Handle& handle) -> std::int64_t { return ov_raw_total(&handle.file, link); });
}

std::int64_t APS5_VABI sceVorbisDecRead(void* vf, char* buffer, int length, int bigEndian, int word, int isSigned, int* bitstream) {
    return ReadPcm(vf, __func__, buffer, length, bitstream, [&](Handle& handle, char* target, int* targetBitstream) -> std::int64_t {
        return ov_read(&handle.file, target, length, bigEndian, word, isSigned, targetBitstream);
    });
}

std::int64_t APS5_VABI sceVorbisDecReadFilter(void* vf, char* buffer, int length, int bigEndian, int word, int isSigned, int* bitstream, GuestFilter filter, void* filterParam) {
    FilterContext context{filter, filterParam};
    return ReadPcm(vf, __func__, buffer, length, bitstream, [&](Handle& handle, char* target, int* targetBitstream) -> std::int64_t {
        return ov_read_filter(&handle.file, target, length, bigEndian, word, isSigned, targetBitstream, filter ? RunFilter : nullptr, &context);
    });
}

std::int64_t APS5_VABI sceVorbisDecReadFloat(void* vf, float*** pcmChannels, int samples, int* bitstream) {
    RequireFile(vf, __func__);
    if (pcmChannels && !WritableGuest(pcmChannels, sizeof(*pcmChannels))) return OV_EFAULT;
    if (bitstream && !WritableGuest(bitstream, sizeof(*bitstream))) return OV_EFAULT;
    float** guestChannels = nullptr;
    int localBitstream = 0;
    bool copyOutFailed = false;
    const std::int64_t result = WithHandle(vf, __func__, [&](Handle& handle) -> std::int64_t {
        float** localChannels = nullptr;
        const std::int64_t decoded = ov_read_float(&handle.file, pcmChannels ? &localChannels : nullptr, samples, bitstream ? &localBitstream : nullptr);
        if (decoded <= 0 || !pcmChannels || !localChannels) return decoded;
        const vorbis_info* info = ov_info(&handle.file, -1);
        if (!info || info->channels <= 0) return OV_EINVAL;
        const auto channelCount = static_cast<std::size_t>(info->channels);
        const auto sampleCount = static_cast<std::size_t>(decoded);
        const std::size_t ptrBytes = channelCount * sizeof(float*);
        const std::size_t totalBytes = ptrBytes + channelCount * sampleCount * sizeof(float);
        if (GuestArena::GuestArenaAvailable_nid_postfix()) {
            constexpr std::uint32_t ReadWriteProtection = 0x04;
            constexpr std::size_t ArenaGranularity = 65536;
            const std::size_t arenaBytes = (totalBytes + ArenaGranularity - 1) & ~(ArenaGranularity - 1);
            if (handle.floatArenaBytes < totalBytes) {
                ReleaseFloatArena(handle);
                void* const allocated = GuestArena::GuestArenaAllocate_nid_postfix(arenaBytes, alignof(float*));
                if (!allocated) throw std::bad_alloc();
                try {
                    GuestArena::GuestArenaCommit_nid_postfix(allocated, arenaBytes, ReadWriteProtection, arenaBytes);
                } catch (...) {
                    GuestArena::GuestArenaReset_nid_postfix(allocated, arenaBytes);
                    GuestArena::GuestArenaRelease_nid_postfix(allocated, arenaBytes);
                    throw;
                }
                handle.floatArena = allocated;
                handle.floatArenaBytes = arenaBytes;
            }
            auto* const guestBase = static_cast<std::uint8_t*>(handle.floatArena);
            std::vector<float*> ptrs(channelCount);
            for (std::size_t channel = 0; channel < channelCount; ++channel) {
                ptrs[channel] = reinterpret_cast<float*>(guestBase + ptrBytes + channel * sampleCount * sizeof(float));
                if (!CopyGuestOut(ptrs[channel], localChannels[channel], sampleCount * sizeof(float))) {
                    copyOutFailed = true;
                    return OV_EFAULT;
                }
            }
            if (!CopyGuestOut(guestBase, ptrs.data(), ptrBytes)) {
                copyOutFailed = true;
                return OV_EFAULT;
            }
            guestChannels = reinterpret_cast<float**>(guestBase);
        } else {
            handle.floatSamples.resize(channelCount * sampleCount);
            handle.floatChannelPtrs.resize(channelCount);
            for (std::size_t channel = 0; channel < channelCount; ++channel) {
                float* const destination = handle.floatSamples.data() + channel * sampleCount;
                std::memcpy(destination, localChannels[channel], sampleCount * sizeof(float));
                handle.floatChannelPtrs[channel] = destination;
            }
            guestChannels = handle.floatChannelPtrs.data();
        }
        return decoded;
    });
    if (copyOutFailed) return OV_EFAULT;
    if (result > 0) {
        if (pcmChannels && !CopyGuestOut(pcmChannels, &guestChannels, sizeof(guestChannels))) return OV_EFAULT;
        if (bitstream && !CopyGuestOut(bitstream, &localBitstream, sizeof(localBitstream))) return OV_EFAULT;
    }
    return result;
}

std::int64_t APS5_VABI sceVorbisDecSeekable(void* vf) {
    return WithHandle(vf, __func__, [](Handle& handle) -> std::int64_t { return ov_seekable(&handle.file); });
}

std::int64_t APS5_VABI sceVorbisDecSerialNumber(void* vf, int link) {
    return WithHandle(vf, __func__, [&](Handle& handle) -> std::int64_t { return ov_serialnumber(&handle.file, link); });
}

int APS5_VABI sceVorbisDecSetDecodePriority(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

std::int64_t APS5_VABI sceVorbisDecStreams(void* vf) {
    return WithHandle(vf, __func__, [](Handle& handle) -> std::int64_t { return ov_streams(&handle.file); });
}

int APS5_VABI sceVorbisDecTimeSeek(void* vf, double position) {
    return WithHandle(vf, __func__, [&](Handle& handle) { return ov_time_seek(&handle.file, position); });
}

int APS5_VABI sceVorbisDecTimeSeekLap(void* vf, double position) {
    return WithHandle(vf, __func__, [&](Handle& handle) { return ov_time_seek_lap(&handle.file, position); });
}

int APS5_VABI sceVorbisDecTimeSeekPage(void* vf, double position) {
    return WithHandle(vf, __func__, [&](Handle& handle) { return ov_time_seek_page(&handle.file, position); });
}

int APS5_VABI sceVorbisDecTimeSeekPageLap(void* vf, double position) {
    return WithHandle(vf, __func__, [&](Handle& handle) { return ov_time_seek_page_lap(&handle.file, position); });
}

double APS5_VABI sceVorbisDecTimeTell(void* vf) {
    return WithHandle(vf, __func__, [](Handle& handle) { return ov_time_tell(&handle.file); });
}

double APS5_VABI sceVorbisDecTimeTotal(void* vf, int link) {
    return WithHandle(vf, __func__, [&](Handle& handle) { return ov_time_total(&handle.file, link); });
}

}
