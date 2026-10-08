// SPDX-FileCopyrightText: Copyright 2024-2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "prx/libSceAvPlayer/include/AvPlayer.hpp"
#include "prx/libc/include/General.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstring>
#include <limits>
#include <optional>
#include <vector>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavformat/avio.h>
#include <libavutil/mathematics.h>
#include <libavutil/mem.h>
#include <libswresample/swresample.h>
#include <libswscale/swscale.h>
}

namespace AvPlayer {

namespace {

constexpr std::uint32_t VideoPitchAlignment = 256;
constexpr std::uint32_t VideoHeightAlignment = 16;
constexpr std::uint32_t VideoBufferAlignment = 0x100;
constexpr std::uint32_t AudioBufferAlignment = 0x10;
constexpr std::uint32_t AudioChunkSamples = 1024;
constexpr std::uint32_t AudioMaxChannels = 8;
constexpr std::uint32_t VideoDecodeAheadFrames = 4;
constexpr std::size_t VideoPacketLimit = 30;
constexpr std::size_t AudioPacketLimit = 8;
constexpr std::size_t AudioOnlyPacketLimit = 30;
constexpr int AvioBufferSize = 64 * 1024;

std::uint32_t AlignUp(std::uint32_t value, std::uint32_t alignment) {
    return (value + alignment - 1) / alignment * alignment;
}

std::uint64_t ToMillis(std::int64_t timestamp, AVRational timeBase) {
    if (timestamp == AV_NOPTS_VALUE || timestamp <= 0 || timeBase.num <= 0 || timeBase.den <= 0) return 0;
    const auto millis = av_rescale_q(timestamp, timeBase, AVRational{1, 1000});
    return millis > 0 ? static_cast<std::uint64_t>(millis) : 0;
}

std::uint64_t StreamDuration(const AVFormatContext& format, const AVStream& stream) {
    const auto duration = ToMillis(stream.duration, stream.time_base);
    return duration != 0 ? duration : ToMillis(format.duration, AVRational{1, AV_TIME_BASE});
}

std::uint64_t FrameMillis(const AVFrame& frame, AVRational timeBase) {
    auto timestamp = frame.best_effort_timestamp;
    if (timestamp == AV_NOPTS_VALUE) timestamp = frame.pts;
    if (timestamp == AV_NOPTS_VALUE) timestamp = frame.pkt_dts;
    return ToMillis(timestamp, timeBase);
}

bool IsStreamSupported(const AVStream& stream) {
    const auto codec = stream.codecpar->codec_id;
    return codec == AV_CODEC_ID_H264 || codec == AV_CODEC_ID_HEVC || codec == AV_CODEC_ID_AAC;
}

float DisplayAspect(int width, int height, AVRational sampleAspect) {
    if (width <= 0 || height <= 0) return 0.0f;
    const double pixel = sampleAspect.num > 0 && sampleAspect.den > 0 ? av_q2d(sampleAspect) : 1.0;
    return static_cast<float>(width * pixel / height);
}

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
        auto* buffer = static_cast<std::uint8_t*>(av_malloc(AvioBufferSize));
        if (!buffer) return false;
        context = avio_alloc_context(buffer, AvioBufferSize, 0, this, &FileReplacementStream::read, nullptr, &FileReplacementStream::seek);
        if (!context) av_free(buffer);
        return context != nullptr;
    }

    AVIOContext* Context() const { return context; }

private:
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
        const auto target = std::clamp<std::int64_t>(base + offset, 0, static_cast<std::int64_t>(self->size));
        self->position = static_cast<std::uint64_t>(target);
        return target;
    }

    AvPlayerFileReplacement file;
    bool opened = false;
    std::uint64_t size = 0;
    std::uint64_t position = 0;
    AVIOContext* context = nullptr;
};

class Clock {
public:
    double Now() const {
        if (!running || paused) return base;
        const std::chrono::duration<double, std::milli> elapsed = std::chrono::steady_clock::now() - wall;
        return std::max(0.0, base + elapsed.count() * speed / NormalSpeed);
    }

    void Rebase(double milliseconds) {
        base = milliseconds;
        wall = std::chrono::steady_clock::now();
        running = true;
    }

    void Reset(double milliseconds) {
        base = milliseconds;
        running = false;
    }

    void Pause() {
        base = Now();
        paused = true;
    }

    void Resume() {
        wall = std::chrono::steady_clock::now();
        paused = false;
    }

    void SetSpeed(std::int32_t percent) {
        base = Now();
        wall = std::chrono::steady_clock::now();
        speed = percent;
    }

private:
    double base = 0.0;
    std::chrono::steady_clock::time_point wall{};
    std::int32_t speed = NormalSpeed;
    bool running = false;
    bool paused = false;
};

enum class PacketKind { Data, Flush, Drain, End, Reopen };

struct PacketItem {
    PacketKind kind = PacketKind::Data;
    AVPacket* packet = nullptr;
    std::uint64_t epoch = 0;
    std::uint64_t skipBefore = 0;
    int stream = -1;
};

struct Frame {
    std::uint8_t* buffer = nullptr;
    AvPlayerFrameInfoEx info{};
    std::uint64_t epoch = 0;
    bool seamless = false;
};

struct Decoder {
    std::atomic<int> stream = -1;
    int switchTo = -1;
    bool video = false;
    AVCodecContext* context = nullptr;
    std::deque<PacketItem> packets;
    std::uint64_t queuedBytes = 0;
    std::deque<Frame> frames;
    std::vector<std::uint8_t*> free;
    std::vector<std::uint8_t*> allocated;
    std::optional<Frame> current;
    std::deque<std::uint8_t*> handedOut;
    std::size_t retained = 0;
    std::uint32_t bufferSize = 0;
    std::uint64_t epoch = 0;
    std::uint64_t skipBefore = 0;
    bool seamless = false;
    bool ended = false;
    std::thread thread;
};

class FfmpegSource final : public ISource {
public:
    FfmpegSource(const SourceSettings& sourceSettings, ISourceEvents& sourceEvents) : settings(sourceSettings), events(sourceEvents) {
        video.video = true;
    }

    ~FfmpegSource() override {
        Stop();
        avformat_close_input(&format);
        replacement.reset();
    }

    FfmpegSource(const FfmpegSource&) = delete;
    FfmpegSource& operator=(const FfmpegSource&) = delete;

    bool Open(const std::string& path) {
        format = avformat_alloc_context();
        if (!format) return false;
        std::string url;
        if (settings.file.open) {
            replacement = std::make_unique<FileReplacementStream>(settings.file);
            if (!replacement->Open(path)) {
                avformat_free_context(format);
                format = nullptr;
                return false;
            }
            format->pb = replacement->Context();
            format->flags |= AVFMT_FLAG_CUSTOM_IO;
        } else {
            const auto resolved = ResolvePath_nid_no_patch(path.c_str()).u8string();
            url = "file:" + std::string(resolved.begin(), resolved.end());
        }
        if (const int error = avformat_open_input(&format, replacement ? nullptr : url.c_str(), nullptr, nullptr); error < 0) {
            char reason[AV_ERROR_MAX_STRING_SIZE]{};
            av_strerror(error, reason, sizeof(reason));
            APS5_LOG_ERR("Could not open %s: %s", path.c_str(), reason);
            return false;
        }
        if (avformat_find_stream_info(format, nullptr) < 0) return true;
        for (unsigned index = 0; index < format->nb_streams; ++index) {
            const auto* stream = format->streams[index];
            if (!IsStreamSupported(*stream)) continue;
            streams.push_back({static_cast<int>(index), createStreamInfo(*stream), createStreamInfoEx(*stream)});
        }
        for (const auto& stream : streams) duration = std::max(duration, stream.info.duration);
        return true;
    }

    std::uint32_t StreamCount() const override { return static_cast<std::uint32_t>(streams.size()); }

    bool GetStreamInfo(std::uint32_t index, AvPlayerStreamInfo& info) const override {
        if (index >= streams.size()) return false;
        info = streams[index].info;
        return true;
    }

    bool GetStreamInfoEx(std::uint32_t index, AvPlayerStreamInfoEx& info) const override {
        if (index >= streams.size()) return false;
        info = streams[index].infoEx;
        return true;
    }

    bool EnableStream(std::uint32_t index) override {
        if (index >= streams.size() || started) return false;
        const auto type = streams[index].info.type;
        if (type == StreamTypeVideo) video.stream = streams[index].stream;
        if (type == StreamTypeAudio) audio.stream = streams[index].stream;
        return true;
    }

    bool DisableStream(std::uint32_t index) override {
        if (index >= streams.size() || started) return false;
        const auto stream = streams[index].stream;
        if (video.stream == stream) video.stream = -1;
        if (audio.stream == stream) audio.stream = -1;
        return true;
    }

    bool ChangeStream(std::uint32_t from, std::uint32_t to) override {
        if (from >= streams.size() || to >= streams.size()) return false;
        const auto& current = streams[from];
        const auto& next = streams[to];
        if (current.info.type != next.info.type) return false;
        auto& decoder = current.info.type == StreamTypeVideo ? video : audio;
        const auto* parameters = format->streams[next.stream]->codecpar;
        {
            std::lock_guard lock(mutex);
            const int active = decoder.switchTo >= 0 ? decoder.switchTo : decoder.stream.load();
            if (active != current.stream) return false;
            if (current.stream == next.stream) return true;
            if (!started) {
                decoder.stream = next.stream;
                return true;
            }
            if (decoder.video && (AlignUp(static_cast<std::uint32_t>(parameters->width), VideoPitchAlignment) > pitch || AlignUp(static_cast<std::uint32_t>(parameters->height), VideoHeightAlignment) > bufferHeight)) return false;
            if (!decoder.video && (parameters->ch_layout.nb_channels <= 0 || parameters->ch_layout.nb_channels > static_cast<int>(AudioMaxChannels))) return false;
            decoder.switchTo = next.stream;
            if (!reposition) reposition = Reposition{clockMillis(), false};
        }
        condition.notify_all();
        return true;
    }

    int Start() override {
        if (video.stream < 0 && audio.stream < 0) return SCE_AVPLAYER_ERROR_OPERATION_FAILED;
        for (auto* decoder : {&video, &audio}) {
            if (decoder->stream < 0) continue;
            const int result = openDecoder(*decoder);
            if (result != SCE_OK) {
                releaseDecoders();
                return result;
            }
        }
        addVideoDecodeAheadBuffers();
        const auto start = startOffset;
        startOffset = 0;
        seek(start);
        {
            std::lock_guard lock(mutex);
            stopping = false;
            reposition.reset();
            demuxEnded = false;
            demuxEpoch = 1;
            minEpoch = 1;
            clockEpoch = 0;
            audioDriving = false;
            presented = false;
            lastPresented = 0;
            presentationClock.Reset(static_cast<double>(start));
            for (auto* decoder : {&video, &audio}) {
                decoder->switchTo = -1;
                decoder->epoch = 1;
                decoder->skipBefore = start;
                decoder->seamless = false;
                decoder->ended = decoder->stream < 0;
            }
        }
        started = true;
        for (auto* decoder : {&video, &audio}) {
            if (decoder->stream >= 0) decoder->thread = std::thread([this, decoder] { decodeLoop(*decoder); });
        }
        demuxer = std::thread([this] { demuxLoop(); });
        return SCE_OK;
    }

    void Stop() override {
        if (!started) return;
        {
            std::lock_guard lock(mutex);
            stopping = true;
        }
        condition.notify_all();
        demuxer.join();
        for (auto* decoder : {&video, &audio}) {
            if (decoder->thread.joinable()) decoder->thread.join();
        }
        releaseDecoders();
        for (auto* decoder : {&video, &audio}) {
            if (decoder->switchTo >= 0) decoder->stream = decoder->switchTo;
            decoder->switchTo = -1;
        }
        started = false;
        paused = false;
    }

    void Pause() override {
        std::lock_guard lock(mutex);
        paused = true;
        presentationClock.Pause();
    }

    void Resume() override {
        std::lock_guard lock(mutex);
        paused = false;
        presentationClock.Resume();
    }

    void Jump(std::uint64_t milliseconds) override {
        {
            std::lock_guard lock(mutex);
            if (!started) {
                startOffset = milliseconds;
                return;
            }
            reposition = Reposition{milliseconds, true};
        }
        condition.notify_all();
    }

    void SetLooping(bool enabled) override { looping = enabled; }

    void SetSpeed(std::int32_t percent) override {
        {
            std::lock_guard lock(mutex);
            const bool forwardAgain = speed < 0 && percent > 0;
            speed = percent;
            presentationClock.SetSpeed(percent);
            if (percent != NormalSpeed) audioDriving = false;
            if (forwardAgain && started && !reposition) reposition = Reposition{clockMillis(), false};
        }
        condition.notify_all();
    }

    void SetSyncMode(std::uint32_t mode) override { syncMode = mode; }

    void SetDemuxVideoBufferSize(std::uint32_t bytes) override {
        {
            std::lock_guard lock(mutex);
            demuxVideoBytes = bytes;
        }
        condition.notify_all();
    }

    bool GetVideoData(AvPlayerFrameInfoEx& info) override {
        bool taken = false;
        {
            std::lock_guard lock(mutex);
            taken = takeVideo(info);
        }
        condition.notify_all();
        return taken;
    }

    bool GetAudioData(AvPlayerFrameInfo& info) override {
        AvPlayerFrameInfoEx extended{};
        bool taken = false;
        {
            std::lock_guard lock(mutex);
            taken = takeAudio(extended);
        }
        condition.notify_all();
        if (!taken) return false;
        info = {};
        info.p_data = static_cast<std::uint8_t*>(extended.p_data);
        info.timestamp = extended.timestamp;
        info.details.audio.channel_count = extended.details.audio.channel_count;
        info.details.audio.sample_rate = extended.details.audio.sample_rate;
        info.details.audio.size = extended.details.audio.size;
        std::memcpy(info.details.audio.language_code, extended.details.audio.language_code, sizeof(info.details.audio.language_code));
        return true;
    }

    std::uint64_t CurrentTime() override {
        std::lock_guard lock(mutex);
        return clockMillis();
    }

    bool Finished() override {
        std::lock_guard lock(mutex);
        if (!started || !demuxEnded || reposition) return false;
        for (const auto* decoder : {&video, &audio}) {
            if (decoder->stream < 0) continue;
            if (!decoder->ended || !decoder->packets.empty()) return false;
            for (const auto& frame : decoder->frames) {
                if (frame.epoch >= minEpoch) return false;
            }
        }
        return true;
    }

private:
    struct StreamEntry {
        int stream;
        AvPlayerStreamInfo info;
        AvPlayerStreamInfoEx infoEx;
    };

    struct Reposition {
        std::uint64_t target;
        bool announce;
        bool operator==(const Reposition&) const = default;
    };

    std::uint64_t clockMillis() const {
        if (!started) return 0;
        const auto milliseconds = static_cast<std::uint64_t>(presentationClock.Now());
        return duration != 0 ? std::min(milliseconds, duration) : milliseconds;
    }

    bool takeVideo(AvPlayerFrameInfoEx& info) {
        if (!started || video.stream < 0 || speed < 0) return false;
        if (speed != NormalSpeed) dropLateAudio();
        auto& frames = video.frames;
        const bool synced = syncMode == SyncModeDefault;
        const auto stale = synced ? std::max(minEpoch, clockEpoch) : minEpoch;
        while (!frames.empty() && frames.front().epoch < stale) recycleFront(video);
        if (frames.empty()) return false;
        const auto& front = frames.front();
        if (paused && front.epoch <= clockEpoch) return false;
        if (front.epoch > clockEpoch) {
            if (synced && audioDriving && speed == NormalSpeed) return false;
            if (synced && front.seamless && presented && presentationClock.Now() < static_cast<double>(lastPresented + frameDuration())) return false;
            presentationClock.Rebase(static_cast<double>(front.info.timestamp));
            clockEpoch = front.epoch;
        }
        if (synced) {
            const auto now = presentationClock.Now();
            if (static_cast<double>(front.info.timestamp) > now) return false;
            while (frames.size() > 1 && frames[1].epoch == frames.front().epoch && static_cast<double>(frames[1].info.timestamp) <= now) recycleFront(video);
        }
        present(video, info);
        presented = true;
        lastPresented = info.timestamp;
        return true;
    }

    bool takeAudio(AvPlayerFrameInfoEx& info) {
        if (!started || paused || audio.stream < 0 || speed != NormalSpeed) return false;
        auto& frames = audio.frames;
        while (!frames.empty() && frames.front().epoch < minEpoch) recycleFront(audio);
        if (frames.empty()) return false;
        const auto epoch = frames.front().epoch;
        present(audio, info);
        audioDriving = true;
        clockEpoch = std::max(clockEpoch, epoch);
        presentationClock.Rebase(static_cast<double>(info.timestamp));
        return true;
    }

    AvPlayerStreamInfo createStreamInfo(const AVStream& stream) const {
        AvPlayerStreamInfo info{};
        const auto* parameters = stream.codecpar;
        info.type = parameters->codec_type == AVMEDIA_TYPE_VIDEO ? StreamTypeVideo : StreamTypeAudio;
        info.duration = StreamDuration(*format, stream);
        char language[4]{};
        if (const auto* entry = av_dict_get(stream.metadata, "language", nullptr, 0)) std::strncpy(language, entry->value, 3);
        if (info.type == StreamTypeVideo) {
            info.details.video.width = AlignUp(static_cast<std::uint32_t>(parameters->width), VideoHeightAlignment);
            info.details.video.height = AlignUp(static_cast<std::uint32_t>(parameters->height), VideoHeightAlignment);
            info.details.video.aspect_ratio = DisplayAspect(parameters->width, parameters->height, parameters->sample_aspect_ratio);
            std::memcpy(info.details.video.language_code, language, sizeof(language));
        } else {
            info.details.audio.channel_count = static_cast<std::uint16_t>(parameters->ch_layout.nb_channels);
            info.details.audio.sample_rate = static_cast<std::uint32_t>(parameters->sample_rate);
            info.details.audio.size = 0;
            std::memcpy(info.details.audio.language_code, language, sizeof(language));
        }
        return info;
    }

    AvPlayerStreamInfoEx createStreamInfoEx(const AVStream& stream) const {
        const auto basic = createStreamInfo(stream);
        AvPlayerStreamInfoEx info{};
        info.type = basic.type;
        info.duration = basic.duration;
        const auto* parameters = stream.codecpar;
        if (info.type == StreamTypeVideo) {
            auto& picture = info.details.video;
            const auto width = static_cast<std::uint32_t>(parameters->width);
            const auto height = static_cast<std::uint32_t>(parameters->height);
            const auto streamPitch = AlignUp(width, VideoPitchAlignment);
            picture.width = basic.details.video.width;
            picture.height = basic.details.video.height;
            picture.aspect_ratio = basic.details.video.aspect_ratio;
            std::memcpy(picture.language_code, basic.details.video.language_code, sizeof(picture.language_code));
            picture.crop_right_offset = streamPitch - width;
            picture.crop_bottom_offset = picture.height - height;
            picture.pitch = streamPitch;
            picture.luma_bit_depth = 8;
            picture.chroma_bit_depth = 8;
            picture.video_full_range_flag = parameters->color_range == AVCOL_RANGE_JPEG;
        } else {
            auto& sound = info.details.audio;
            sound.channel_count = basic.details.audio.channel_count;
            sound.sample_rate = basic.details.audio.sample_rate;
            std::memcpy(sound.language_code, basic.details.audio.language_code, sizeof(sound.language_code));
        }
        return info;
    }

    int openContext(Decoder& decoder, int index) {
        const auto* stream = format->streams[index];
        const auto* codec = avcodec_find_decoder(stream->codecpar->codec_id);
        if (!codec) return SCE_AVPLAYER_ERROR_NOT_SUPPORTED;
        decoder.context = avcodec_alloc_context3(codec);
        if (!decoder.context) return SCE_AVPLAYER_ERROR_NO_MEMORY;
        if (avcodec_parameters_to_context(decoder.context, stream->codecpar) < 0) return SCE_AVPLAYER_ERROR_OPERATION_FAILED;
        decoder.context->pkt_timebase = stream->time_base;
        if (decoder.video) decoder.context->thread_count = static_cast<int>(std::clamp(std::thread::hardware_concurrency(), 1u, 4u));
        if (avcodec_open2(decoder.context, codec, nullptr) < 0) return SCE_AVPLAYER_ERROR_OPERATION_FAILED;
        return SCE_OK;
    }

    int openDecoder(Decoder& decoder) {
        if (const int result = openContext(decoder, decoder.stream); result != SCE_OK) return result;
        std::uint32_t count = 0;
        bool texture = false;
        std::uint32_t alignment = 0;
        if (decoder.video) {
            if (decoder.context->width <= 0 || decoder.context->height <= 0) return SCE_AVPLAYER_ERROR_NOT_SUPPORTED;
            pitch = AlignUp(static_cast<std::uint32_t>(decoder.context->width), VideoPitchAlignment);
            bufferHeight = AlignUp(static_cast<std::uint32_t>(decoder.context->height), VideoHeightAlignment);
            decoder.bufferSize = pitch * bufferHeight * 3 / 2;
            count = settings.videoBuffers;
            decoder.retained = settings.videoBuffers > 3 ? settings.videoBuffers - 3 : 0;
            texture = true;
            alignment = VideoBufferAlignment;
        } else {
            if (decoder.context->ch_layout.nb_channels <= 0 || decoder.context->ch_layout.nb_channels > static_cast<int>(AudioMaxChannels)) return SCE_AVPLAYER_ERROR_NOT_SUPPORTED;
            decoder.bufferSize = AudioMaxChannels * AudioChunkSamples * sizeof(std::int16_t);
            count = settings.videoBuffers * 2;
            alignment = AudioBufferAlignment;
        }
        const auto& memory = settings.memory;
        for (std::uint32_t index = 0; index < count; ++index) {
            void* buffer = texture ? memory.allocate_texture(memory.object_ptr, alignment, decoder.bufferSize) : memory.allocate(memory.object_ptr, alignment, decoder.bufferSize);
            if (!buffer) return SCE_AVPLAYER_ERROR_NO_MEMORY;
            decoder.allocated.push_back(static_cast<std::uint8_t*>(buffer));
            decoder.free.push_back(static_cast<std::uint8_t*>(buffer));
        }
        return SCE_OK;
    }

    void addVideoDecodeAheadBuffers() {
        if (video.stream < 0) return;

        const auto& memory = settings.memory;
        std::vector<std::uint8_t*> extra;
        extra.reserve(VideoDecodeAheadFrames);
        for (std::uint32_t index = 0; index < VideoDecodeAheadFrames; ++index) {
            auto* buffer = static_cast<std::uint8_t*>(memory.allocate_texture(
                memory.object_ptr, VideoBufferAlignment, video.bufferSize));
            if (!buffer) {
                for (auto* allocated : extra) memory.deallocate_texture(memory.object_ptr, allocated);
                return;
            }
            extra.push_back(buffer);
        }
        for (auto* buffer : extra) {
            video.allocated.push_back(buffer);
            video.free.push_back(buffer);
        }
    }

    void releaseDecoders() {
        const auto& memory = settings.memory;
        for (auto* decoder : {&video, &audio}) {
            for (auto& item : decoder->packets) av_packet_free(&item.packet);
            decoder->packets.clear();
            decoder->queuedBytes = 0;
            decoder->frames.clear();
            decoder->current.reset();
            decoder->handedOut.clear();
            decoder->free.clear();
            for (auto* buffer : decoder->allocated) {
                if (decoder->video) {
                    memory.deallocate_texture(memory.object_ptr, buffer);
                } else {
                    memory.deallocate(memory.object_ptr, buffer);
                }
            }
            decoder->allocated.clear();
            avcodec_free_context(&decoder->context);
        }
        sws_freeContext(scaler);
        scaler = nullptr;
        swr_free(&resampler);
        av_channel_layout_uninit(&resamplerLayout);
        resamplerFormat = -1;
        resamplerRate = 0;
    }

    void seek(std::uint64_t milliseconds) {
        const auto target = static_cast<std::int64_t>(std::min<std::uint64_t>(milliseconds, static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max() / 1000)) * 1000);
        if (avformat_seek_file(format, -1, std::numeric_limits<std::int64_t>::min(), target, target, 0) < 0) {
            avformat_seek_file(format, -1, std::numeric_limits<std::int64_t>::min(), 0, 0, 0);
        }
    }

    std::uint64_t frameDuration() const {
        const auto* stream = format->streams[video.stream];
        const auto rate = stream->avg_frame_rate.num > 0 && stream->avg_frame_rate.den > 0 ? stream->avg_frame_rate : stream->r_frame_rate;
        if (rate.num <= 0 || rate.den <= 0) return 0;
        return static_cast<std::uint64_t>(av_rescale(1000, rate.den, rate.num));
    }

    static void recycleFront(Decoder& decoder) {
        decoder.free.push_back(decoder.frames.front().buffer);
        decoder.frames.pop_front();
    }

    void returnBuffer(Decoder& decoder, std::uint8_t* buffer) {
        {
            std::lock_guard lock(mutex);
            decoder.free.push_back(buffer);
        }
        condition.notify_all();
    }

    void present(Decoder& decoder, AvPlayerFrameInfoEx& info) {
        if (decoder.current) decoder.handedOut.push_back(decoder.current->buffer);
        while (decoder.handedOut.size() > decoder.retained) {
            decoder.free.push_back(decoder.handedOut.front());
            decoder.handedOut.pop_front();
        }
        decoder.current = std::move(decoder.frames.front());
        decoder.frames.pop_front();
        info = decoder.current->info;
    }

    void dropLateAudio() {
        if (audio.stream < 0) return;
        const auto now = presentationClock.Now();
        while (!audio.frames.empty() && (audio.frames.front().epoch < clockEpoch || static_cast<double>(audio.frames.front().info.timestamp) <= now)) recycleFront(audio);
    }

    bool demuxerSaturated() const {
        const bool videoFull = video.stream < 0 || (demuxVideoBytes != 0 ? video.queuedBytes >= demuxVideoBytes : video.packets.size() > VideoPacketLimit);
        const bool audioFull = audio.stream < 0 || audio.packets.size() > (video.stream < 0 ? AudioOnlyPacketLimit : AudioPacketLimit);
        return videoFull && audioFull;
    }

    void pushAll(PacketKind kind, std::uint64_t epoch, std::uint64_t skipBefore) {
        for (auto* decoder : {&video, &audio}) {
            if (decoder->stream < 0) continue;
            decoder->packets.push_back({kind, nullptr, epoch, skipBefore});
        }
    }

    void demuxLoop() {
        AVPacket* packet = av_packet_alloc();
        bool ended = false;
        for (;;) {
            std::optional<Reposition> requested;
            {
                std::unique_lock lock(mutex);
                condition.wait(lock, [&] { return stopping || reposition.has_value() || (!ended && !demuxerSaturated()); });
                if (stopping) break;
                requested = reposition;
            }
            if (requested) {
                seek(requested->target);
                {
                    std::lock_guard lock(mutex);
                    if (reposition == requested) reposition.reset();
                    presentationClock.Rebase(static_cast<double>(requested->target));
                    ++demuxEpoch;
                    minEpoch = demuxEpoch;
                    for (auto* decoder : {&video, &audio}) {
                        if (decoder->stream < 0) continue;
                        for (auto& item : decoder->packets) av_packet_free(&item.packet);
                        decoder->packets.clear();
                        decoder->queuedBytes = 0;
                        while (!decoder->frames.empty()) recycleFront(*decoder);
                    }
                    for (auto* decoder : {&video, &audio}) {
                        if (decoder->switchTo < 0) continue;
                        decoder->stream = decoder->switchTo;
                        decoder->packets.push_back({PacketKind::Reopen, nullptr, demuxEpoch, 0, decoder->switchTo});
                        decoder->switchTo = -1;
                    }
                    pushAll(PacketKind::Flush, demuxEpoch, requested->target);
                    demuxEnded = false;
                    audioDriving = false;
                }
                condition.notify_all();
                if (requested->announce) events.OnWarning(SCE_AVPLAYER_ERROR_WAR_JUMP_COMPLETE);
                ended = false;
                continue;
            }
            const int result = av_read_frame(format, packet);
            if (result < 0) {
                if (result != AVERROR_EOF && !avio_feof(format->pb)) {
                    APS5_LOG_ERR("Could not read a packet: %d", result);
                    events.OnError();
                } else if (looping) {
                    seek(0);
                    {
                        std::lock_guard lock(mutex);
                        ++demuxEpoch;
                        pushAll(PacketKind::Drain, demuxEpoch, 0);
                    }
                    condition.notify_all();
                    events.OnWarning(SCE_AVPLAYER_ERROR_WAR_LOOPING_BACK);
                    continue;
                }
                {
                    std::lock_guard lock(mutex);
                    pushAll(PacketKind::End, demuxEpoch, 0);
                    demuxEnded = true;
                }
                condition.notify_all();
                ended = true;
                continue;
            }
            Decoder* target = packet->stream_index == video.stream ? &video : packet->stream_index == audio.stream ? &audio : nullptr;
            if (!target) {
                av_packet_unref(packet);
                continue;
            }
            AVPacket* owned = av_packet_alloc();
            av_packet_move_ref(owned, packet);
            {
                std::lock_guard lock(mutex);
                target->packets.push_back({PacketKind::Data, owned, demuxEpoch, 0});
                target->queuedBytes += static_cast<std::uint64_t>(owned->size);
            }
            condition.notify_all();
        }
        av_packet_free(&packet);
    }

    void decodeLoop(Decoder& decoder) {
        AVFrame* frame = av_frame_alloc();
        for (;;) {
            PacketItem item;
            {
                std::unique_lock lock(mutex);
                condition.wait(lock, [&] { return stopping || !decoder.packets.empty(); });
                if (stopping) break;
                item = decoder.packets.front();
                decoder.packets.pop_front();
                if (item.packet) decoder.queuedBytes -= static_cast<std::uint64_t>(item.packet->size);
            }
            switch (item.kind) {
            case PacketKind::Data:
                if (!decoder.context) break;
                for (;;) {
                    const int result = avcodec_send_packet(decoder.context, item.packet);
                    if (result == AVERROR(EAGAIN) && receiveFrames(decoder, frame)) continue;
                    if (result < 0 && result != AVERROR(EAGAIN)) APS5_LOG_ERR("Could not decode a packet: %d", result);
                    if (result >= 0) receiveFrames(decoder, frame);
                    break;
                }
                break;
            case PacketKind::Reopen:
                avcodec_free_context(&decoder.context);
                if (openContext(decoder, item.stream) != SCE_OK) {
                    avcodec_free_context(&decoder.context);
                    events.OnError();
                }
                break;
            case PacketKind::Flush: {
                if (decoder.context) avcodec_flush_buffers(decoder.context);
                std::lock_guard lock(mutex);
                decoder.epoch = item.epoch;
                decoder.skipBefore = item.skipBefore;
                decoder.seamless = false;
                decoder.ended = false;
                break;
            }
            case PacketKind::Drain:
            case PacketKind::End: {
                if (decoder.context) {
                    avcodec_send_packet(decoder.context, nullptr);
                    receiveFrames(decoder, frame);
                    avcodec_flush_buffers(decoder.context);
                }
                std::lock_guard lock(mutex);
                if (item.kind == PacketKind::End) {
                    decoder.ended = true;
                } else {
                    decoder.epoch = item.epoch;
                    decoder.skipBefore = 0;
                    decoder.seamless = true;
                }
                break;
            }
            }
            av_packet_free(&item.packet);
            condition.notify_all();
        }
        av_frame_free(&frame);
    }

    bool receiveFrames(Decoder& decoder, AVFrame* frame) {
        for (;;) {
            const int result = avcodec_receive_frame(decoder.context, frame);
            if (result == AVERROR(EAGAIN) || result == AVERROR_EOF) return true;
            if (result < 0) {
                APS5_LOG_ERR("Could not receive a decoded frame: %d", result);
                events.OnError();
                return false;
            }
            const bool delivered = decoder.video ? deliverVideo(decoder, *frame) : deliverAudio(decoder, *frame);
            av_frame_unref(frame);
            if (!delivered) return false;
        }
    }

    enum class Acquire { Buffer, Stale, Stopping };

    Acquire acquireBuffer(Decoder& decoder, std::uint64_t epoch, std::uint8_t*& buffer) {
        std::unique_lock lock(mutex);
        condition.wait(lock, [&] { return stopping || !decoder.free.empty() || epoch < minEpoch; });
        if (stopping) return Acquire::Stopping;
        if (epoch < minEpoch) return Acquire::Stale;
        buffer = decoder.free.back();
        decoder.free.pop_back();
        return Acquire::Buffer;
    }

    void publish(Decoder& decoder, Frame frame) {
        {
            std::lock_guard lock(mutex);
            if (frame.epoch < minEpoch) {
                decoder.free.push_back(frame.buffer);
            } else {
                decoder.frames.push_back(frame);
            }
        }
        condition.notify_all();
    }

    bool deliverVideo(Decoder& decoder, const AVFrame& frame) {
        const int index = decoder.stream;
        const auto timestamp = FrameMillis(frame, format->streams[index]->time_base);
        std::uint64_t epoch = 0;
        std::uint64_t skipBefore = 0;
        bool seamless = false;
        {
            std::lock_guard lock(mutex);
            epoch = decoder.epoch;
            skipBefore = decoder.skipBefore;
            seamless = decoder.seamless;
            decoder.seamless = false;
        }
        if (timestamp + frameDuration() <= skipBefore) {
            std::lock_guard lock(mutex);
            decoder.seamless = seamless;
            return true;
        }
        const auto width = static_cast<std::uint32_t>(frame.width);
        const auto height = static_cast<std::uint32_t>(frame.height);
        if (AlignUp(width, VideoPitchAlignment) > pitch || AlignUp(height, VideoHeightAlignment) > bufferHeight) {
            APS5_LOG_ERR("Video frame %ux%u exceeds the %ux%u output buffers", width, height, pitch, bufferHeight);
            events.OnError();
            return false;
        }
        std::uint8_t* buffer = nullptr;
        if (const auto acquired = acquireBuffer(decoder, epoch, buffer); acquired != Acquire::Buffer) return acquired == Acquire::Stale;
        auto* chroma = buffer + static_cast<std::size_t>(pitch) * bufferHeight;
        scaler = sws_getCachedContext(scaler, frame.width, frame.height, static_cast<AVPixelFormat>(frame.format), frame.width, frame.height, AV_PIX_FMT_NV12, SWS_BILINEAR, nullptr, nullptr, nullptr);
        std::uint8_t* planes[4] = {buffer, chroma, nullptr, nullptr};
        const int strides[4] = {static_cast<int>(pitch), static_cast<int>(pitch), 0, 0};
        if (!scaler || sws_scale(scaler, frame.data, frame.linesize, 0, frame.height, planes, strides) < 0) {
            APS5_LOG_CHARS_ERR("Could not convert a video frame to NV12");
            returnBuffer(decoder, buffer);
            events.OnError();
            return false;
        }
        const auto chromaHeight = (height + 1) / 2;
        for (std::uint32_t row = height; row < bufferHeight; ++row) std::memcpy(buffer + static_cast<std::size_t>(row) * pitch, buffer + static_cast<std::size_t>(height - 1) * pitch, pitch);
        for (std::uint32_t row = chromaHeight; row < bufferHeight / 2; ++row) std::memcpy(chroma + static_cast<std::size_t>(row) * pitch, chroma + static_cast<std::size_t>(chromaHeight - 1) * pitch, pitch);
        Frame output{buffer, {}, epoch, seamless};
        auto& info = output.info;
        const auto alignedWidth = AlignUp(width, VideoHeightAlignment);
        const auto alignedHeight = AlignUp(height, VideoHeightAlignment);
        info.p_data = buffer;
        info.timestamp = timestamp;
        info.details.video.width = alignedWidth;
        info.details.video.height = alignedHeight;
        info.details.video.aspect_ratio = DisplayAspect(frame.width, frame.height, frame.sample_aspect_ratio);
        std::memcpy(info.details.video.language_code, streamLanguage(index), sizeof(info.details.video.language_code));
        info.details.video.crop_left_offset = static_cast<std::uint32_t>(frame.crop_left);
        info.details.video.crop_right_offset = static_cast<std::uint32_t>(frame.crop_right) + pitch - width;
        info.details.video.crop_top_offset = static_cast<std::uint32_t>(frame.crop_top);
        info.details.video.crop_bottom_offset = static_cast<std::uint32_t>(frame.crop_bottom) + alignedHeight - height;
        info.details.video.pitch = pitch;
        info.details.video.luma_bit_depth = 8;
        info.details.video.chroma_bit_depth = 8;
        info.details.video.video_full_range_flag = frame.color_range == AVCOL_RANGE_JPEG;
        publish(decoder, output);
        return true;
    }

    bool deliverAudio(Decoder& decoder, const AVFrame& frame) {
        const int index = decoder.stream;
        const auto timestamp = FrameMillis(frame, format->streams[index]->time_base);
        const auto channels = frame.ch_layout.nb_channels;
        if (channels <= 0 || channels > static_cast<int>(AudioMaxChannels) || frame.sample_rate <= 0) {
            events.OnError();
            return false;
        }
        std::uint64_t epoch = 0;
        std::uint64_t skipBefore = 0;
        {
            std::lock_guard lock(mutex);
            epoch = decoder.epoch;
            skipBefore = decoder.skipBefore;
        }
        if (timestamp + static_cast<std::uint64_t>(frame.nb_samples) * 1000 / static_cast<std::uint64_t>(frame.sample_rate) <= skipBefore) return true;
        if (!resampler || resamplerFormat != frame.format || resamplerRate != frame.sample_rate || av_channel_layout_compare(&resamplerLayout, &frame.ch_layout) != 0) {
            swr_free(&resampler);
            av_channel_layout_uninit(&resamplerLayout);
            if (swr_alloc_set_opts2(&resampler, &frame.ch_layout, AV_SAMPLE_FMT_S16, frame.sample_rate, &frame.ch_layout, static_cast<AVSampleFormat>(frame.format), frame.sample_rate, 0, nullptr) < 0 || swr_init(resampler) < 0) {
                swr_free(&resampler);
                events.OnError();
                return false;
            }
            av_channel_layout_copy(&resamplerLayout, &frame.ch_layout);
            resamplerFormat = frame.format;
            resamplerRate = frame.sample_rate;
        }
        sampleBuffer.resize(static_cast<std::size_t>(frame.nb_samples) * static_cast<std::size_t>(channels));
        auto* output = reinterpret_cast<std::uint8_t*>(sampleBuffer.data());
        const int converted = swr_convert(resampler, &output, frame.nb_samples, frame.extended_data, frame.nb_samples);
        if (converted < 0) {
            events.OnError();
            return false;
        }
        for (int offset = 0; offset < converted; offset += static_cast<int>(AudioChunkSamples)) {
            const auto samples = std::min<int>(converted - offset, static_cast<int>(AudioChunkSamples));
            std::uint8_t* buffer = nullptr;
            if (const auto acquired = acquireBuffer(decoder, epoch, buffer); acquired != Acquire::Buffer) return acquired == Acquire::Stale;
            const auto size = static_cast<std::uint32_t>(samples * channels * static_cast<int>(sizeof(std::int16_t)));
            std::memcpy(buffer, sampleBuffer.data() + static_cast<std::size_t>(offset) * static_cast<std::size_t>(channels), size);
            Frame chunk{buffer, {}, epoch, false};
            chunk.info.p_data = buffer;
            chunk.info.timestamp = timestamp + static_cast<std::uint64_t>(offset) * 1000 / static_cast<std::uint64_t>(frame.sample_rate);
            chunk.info.details.audio.channel_count = static_cast<std::uint16_t>(channels);
            chunk.info.details.audio.sample_rate = static_cast<std::uint32_t>(frame.sample_rate);
            chunk.info.details.audio.size = size;
            std::memcpy(chunk.info.details.audio.language_code, streamLanguage(index), sizeof(chunk.info.details.audio.language_code));
            publish(decoder, chunk);
        }
        return true;
    }

    const std::uint8_t* streamLanguage(int index) const {
        for (const auto& stream : streams) {
            if (stream.stream != index) continue;
            return reinterpret_cast<const std::uint8_t*>(stream.info.type == StreamTypeVideo ? stream.info.details.video.language_code : stream.info.details.audio.language_code);
        }
        static constexpr std::uint8_t none[4]{};
        return none;
    }

    SourceSettings settings;
    ISourceEvents& events;
    std::unique_ptr<FileReplacementStream> replacement;
    AVFormatContext* format = nullptr;
    std::vector<StreamEntry> streams;
    std::uint64_t duration = 0;

    std::mutex mutex;
    std::condition_variable condition;
    Decoder video;
    Decoder audio;
    std::thread demuxer;
    bool started = false;
    bool stopping = false;
    bool paused = false;
    bool demuxEnded = false;
    std::atomic_bool looping = false;
    std::atomic<std::uint32_t> syncMode = SyncModeDefault;
    std::int32_t speed = NormalSpeed;
    std::optional<Reposition> reposition;
    std::uint32_t demuxVideoBytes = 0;
    std::uint64_t startOffset = 0;
    std::uint64_t demuxEpoch = 0;
    std::uint64_t minEpoch = 0;
    std::uint64_t clockEpoch = 0;
    bool audioDriving = false;
    bool presented = false;
    std::uint64_t lastPresented = 0;
    Clock presentationClock;

    std::uint32_t pitch = 0;
    std::uint32_t bufferHeight = 0;
    SwsContext* scaler = nullptr;
    SwrContext* resampler = nullptr;
    AVChannelLayout resamplerLayout{};
    int resamplerFormat = -1;
    int resamplerRate = 0;
    std::vector<std::int16_t> sampleBuffer;
};

}

std::unique_ptr<ISource> OpenSource(const SourceSettings& settings, const std::string& path, ISourceEvents& events) {
    auto source = std::make_unique<FfmpegSource>(settings, events);
    if (!source->Open(path)) return nullptr;
    return source;
}

}
