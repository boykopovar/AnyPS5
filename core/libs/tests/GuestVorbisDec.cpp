#include "prx/libSceVorbisDec/include/Api.hpp"
#include "prx/libSceVorbisDec/include/GuestVorbisFile.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

#include <vorbis/vorbisenc.h>
#include <vorbis/vorbisfile.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <vector>

using namespace VorbisDec;

namespace {

constexpr int FirstSerial = 0x1234abcd;
constexpr int SecondSerial = -0x61c88647;
constexpr int FirstChannels = 3;
constexpr int SecondChannels = 2;
constexpr long FirstRate = 44100;
constexpr long SecondRate = 22050;
constexpr long FirstSamples = 44100;
constexpr long SecondSamples = 11025;
constexpr std::int64_t DecodedBytes = (FirstSamples * FirstChannels + SecondSamples * SecondChannels) * 2;
constexpr unsigned char Pattern = 0xA5;

void Check(bool value, int line) {
    if (value) return;
    std::fprintf(stderr, "VorbisDec check failed at line %d\n", line);
    std::abort();
}
#define Require(value) Check((value), __LINE__)

template <typename TException, typename TCall>
bool Throws(TCall call) {
    try {
        call();
    } catch (const TException&) {
        return true;
    }
    return false;
}

struct Source {
    const std::vector<unsigned char>* bytes = nullptr;
    std::size_t position = 0;
    int closes = 0;
    void* probe = nullptr;
    std::int64_t probed = -1;
    bool throwOnRead = false;
    bool throwOnClose = false;
};

std::size_t ReadBytes(Source& source, void* buffer, std::size_t size, std::size_t count) {
    if (source.throwOnRead) throw std::runtime_error("read callback");
    if (source.probe) source.probed = sceVorbisDecStreams(source.probe);
    if (size == 0) return 0;
    const std::size_t items = std::min(count, (source.bytes->size() - source.position) / size);
    if (items != 0) std::memcpy(buffer, source.bytes->data() + source.position, items * size);
    source.position += items * size;
    return items;
}

int SeekBytes(Source& source, std::int64_t offset, int whence) {
    const auto size = static_cast<std::int64_t>(source.bytes->size());
    const std::int64_t base = whence == SEEK_SET ? 0 : whence == SEEK_CUR ? static_cast<std::int64_t>(source.position) : size;
    if (offset < -base || offset > size - base) return -1;
    source.position = static_cast<std::size_t>(base + offset);
    return 0;
}

std::size_t APS5_VABI GuestRead(void* buffer, std::size_t size, std::size_t count, void* source) {
    return ReadBytes(*static_cast<Source*>(source), buffer, size, count);
}

int APS5_VABI GuestSeek(void* source, std::int64_t offset, int whence) {
    return SeekBytes(*static_cast<Source*>(source), offset, whence);
}

int APS5_VABI GuestClose(void* source) {
    auto& closing = *static_cast<Source*>(source);
    ++closing.closes;
    if (closing.throwOnClose) throw std::runtime_error("close callback");
    return 0;
}

std::int64_t APS5_VABI GuestTell(void* source) {
    return static_cast<std::int64_t>(static_cast<Source*>(source)->position);
}

std::size_t HostRead(void* buffer, std::size_t size, std::size_t count, void* source) {
    return ReadBytes(*static_cast<Source*>(source), buffer, size, count);
}

int HostSeek(void* source, ogg_int64_t offset, int whence) {
    return SeekBytes(*static_cast<Source*>(source), offset, whence);
}

int HostClose(void* source) {
    ++static_cast<Source*>(source)->closes;
    return 0;
}

long HostTell(void* source) {
    return static_cast<long>(static_cast<Source*>(source)->position);
}

constexpr GuestCallbacks SeekableCallbacks{GuestRead, GuestSeek, GuestClose, GuestTell};
constexpr GuestCallbacks StreamingCallbacks{GuestRead, nullptr, GuestClose, nullptr};
const ov_callbacks HostSeekableCallbacks{HostRead, HostSeek, HostClose, HostTell};
const ov_callbacks HostStreamingCallbacks{HostRead, nullptr, HostClose, nullptr};

struct GuestFile {
    alignas(16) std::array<unsigned char, 1024> bytes;

    GuestFile() { bytes.fill(Pattern); }

    void* Get() { return bytes.data(); }

    bool Untouched() const {
        return std::all_of(bytes.begin(), bytes.end(), [](unsigned char value) { return value == Pattern; });
    }
};

struct FilterLog {
    int calls = 0;
    std::int64_t firstChannels = 0;
    std::int64_t lastChannels = 0;
    std::int64_t samples = 0;
};

void Halve(float** pcm, std::int64_t channels, std::int64_t samples) {
    for (std::int64_t channel = 0; channel < channels; ++channel)
        for (std::int64_t index = 0; index < samples; ++index) pcm[channel][index] *= 0.5f;
}

void APS5_VABI GuestHalve(float** pcm, std::int64_t channels, std::int64_t samples, void* param) {
    auto* log = static_cast<FilterLog*>(param);
    if (log->calls++ == 0) log->firstChannels = channels;
    log->lastChannels = channels;
    log->samples += samples;
    Halve(pcm, channels, samples);
}

void HostHalve(float** pcm, long channels, long samples, void*) {
    Halve(pcm, channels, samples);
}

void AppendPage(std::vector<unsigned char>& out, const ogg_page& page) {
    out.insert(out.end(), page.header, page.header + page.header_len);
    out.insert(out.end(), page.body, page.body + page.body_len);
}

void EncodeLink(std::vector<unsigned char>& out, int serial, int channels, long rate, long samples, const char* title) {
    vorbis_info info;
    vorbis_info_init(&info);
    Require(vorbis_encode_init_vbr(&info, channels, rate, 0.1f) == 0);
    vorbis_comment comment;
    vorbis_comment_init(&comment);
    vorbis_comment_add_tag(&comment, "TITLE", title);
    vorbis_comment_add_tag(&comment, "ARTIST", "AnyPS5");
    vorbis_dsp_state dsp;
    Require(vorbis_analysis_init(&dsp, &info) == 0);
    vorbis_block block;
    vorbis_block_init(&dsp, &block);
    ogg_stream_state stream;
    ogg_stream_init(&stream, serial);
    ogg_packet identification;
    ogg_packet comments;
    ogg_packet codebooks;
    vorbis_analysis_headerout(&dsp, &comment, &identification, &comments, &codebooks);
    ogg_stream_packetin(&stream, &identification);
    ogg_stream_packetin(&stream, &comments);
    ogg_stream_packetin(&stream, &codebooks);
    ogg_page page;
    while (ogg_stream_flush(&stream, &page) != 0) AppendPage(out, page);
    for (long done = 0;;) {
        const long count = std::min(1024L, samples - done);
        if (count > 0) {
            float** buffer = vorbis_analysis_buffer(&dsp, static_cast<int>(count));
            for (int channel = 0; channel < channels; ++channel) {
                const double frequency = 220.0 * (channel + 1) + 37.0 * serial / 1e9;
                for (long index = 0; index < count; ++index) {
                    buffer[channel][index] = static_cast<float>(0.3 * std::sin(6.283185307179586 * frequency * static_cast<double>(done + index) / static_cast<double>(rate)));
                }
            }
        }
        vorbis_analysis_wrote(&dsp, static_cast<int>(count));
        done += count;
        while (vorbis_analysis_blockout(&dsp, &block) == 1) {
            vorbis_analysis(&block, nullptr);
            vorbis_bitrate_addblock(&block);
            ogg_packet packet;
            while (vorbis_bitrate_flushpacket(&dsp, &packet) == 1) {
                ogg_stream_packetin(&stream, &packet);
                while (ogg_stream_pageout_fill(&stream, &page, 1024) != 0) AppendPage(out, page);
            }
        }
        if (count == 0) break;
    }
    while (ogg_stream_flush(&stream, &page) != 0) AppendPage(out, page);
    ogg_stream_clear(&stream);
    vorbis_block_clear(&block);
    vorbis_dsp_clear(&dsp);
    vorbis_comment_clear(&comment);
    vorbis_info_clear(&info);
}

std::int64_t RequireSameRead(void* vf, OggVorbis_File& reference, int bigEndian = 0, int word = 2, int isSigned = 1) {
    std::array<char, 4096> guest{};
    std::array<char, 4096> host{};
    int guestStream = -1;
    int hostStream = -1;
    const std::int64_t got = sceVorbisDecRead(vf, guest.data(), static_cast<int>(guest.size()), bigEndian, word, isSigned, &guestStream);
    const long expected = ov_read(&reference, host.data(), static_cast<int>(host.size()), bigEndian, word, isSigned, &hostStream);
    Require(got == expected);
    Require(got <= 0 || (guestStream == hostStream && std::memcmp(guest.data(), host.data(), static_cast<std::size_t>(got)) == 0));
    return got;
}

std::int64_t DecodeTogether(void* vf, OggVorbis_File& reference) {
    std::int64_t total = 0;
    for (;;) {
        const std::int64_t got = RequireSameRead(vf, reference);
        Require(got >= 0 || got == OV_HOLE);
        if (got == 0) return total;
        if (got > 0) total += got;
    }
}

void RequireSameTells(void* vf, OggVorbis_File& reference) {
    Require(sceVorbisDecPcmTell(vf) == ov_pcm_tell(&reference));
    Require(sceVorbisDecRawTell(vf) == ov_raw_tell(&reference));
    Require(sceVorbisDecTimeTell(vf) == ov_time_tell(&reference));
}

void TestSeekable(const std::vector<unsigned char>& stream) {
    GuestFile guest;
    GuestFile probe;
    void* vf = guest.Get();
    Source source{&stream};
    source.probe = probe.Get();
    Require(sceVorbisDecOpenCallbacks(&source, vf, nullptr, 0, SeekableCallbacks) == 0);
    Require(source.probed == 0 && source.closes == 0);
    Source referenceSource{&stream};
    OggVorbis_File reference{};
    Require(ov_open_callbacks(&referenceSource, &reference, nullptr, 0, HostSeekableCallbacks) == 0);

    Require(sceVorbisDecStreams(vf) == 2 && sceVorbisDecSeekable(vf) == 1);
    Require(sceVorbisDecSerialNumber(vf, 0) == FirstSerial && sceVorbisDecSerialNumber(vf, 1) == SecondSerial);
    Require(sceVorbisDecSerialNumber(vf, -1) == FirstSerial && sceVorbisDecSerialNumber(vf, 7) == SecondSerial);
    Require(sceVorbisDecPcmTotal(vf, 0) == FirstSamples && sceVorbisDecPcmTotal(vf, 1) == SecondSamples);
    Require(sceVorbisDecPcmTotal(vf, -1) == FirstSamples + SecondSamples && sceVorbisDecPcmTotal(vf, 2) == OV_EINVAL);
    Require(sceVorbisDecRawTotal(vf, -1) == ov_raw_total(&reference, -1) && sceVorbisDecRawTotal(vf, -1) > 0);
    Require(sceVorbisDecRawTotal(vf, 0) == ov_raw_total(&reference, 0) && sceVorbisDecRawTotal(vf, 1) == ov_raw_total(&reference, 1));
    Require(sceVorbisDecTimeTotal(vf, 0) == 1.0 && sceVorbisDecTimeTotal(vf, 1) == 0.5 && sceVorbisDecTimeTotal(vf, -1) == 1.5);
    Require(sceVorbisDecBitrate(vf, -1) == ov_bitrate(&reference, -1) && sceVorbisDecBitrate(vf, 1) == ov_bitrate(&reference, 1));
    Require(sceVorbisDecBitrate(vf, -1) > 0 && sceVorbisDecBitrate(vf, 2) == OV_EINVAL);
    Require(sceVorbisDecBitrateInstant(vf) == OV_FALSE);

    GuestInfo* first = sceVorbisDecInfo(vf, 0);
    GuestInfo* second = sceVorbisDecInfo(vf, 1);
    Require(first && second && first != second && sceVorbisDecInfo(vf, 0) == first);
    Require(sceVorbisDecInfo(vf, -1) == first && sceVorbisDecInfo(vf, 2) == nullptr);
    Require(first->version == 0 && first->channels == FirstChannels && first->rate == FirstRate);
    Require(second->version == 0 && second->channels == SecondChannels && second->rate == SecondRate);
    for (int link = 0; link < 2; ++link) {
        const GuestInfo* info = sceVorbisDecInfo(vf, link);
        const vorbis_info* host = ov_info(&reference, link);
        Require(info->bitrate_upper == host->bitrate_upper && info->bitrate_nominal == host->bitrate_nominal);
        Require(info->bitrate_lower == host->bitrate_lower && info->bitrate_window == host->bitrate_window && info->codec_setup != nullptr);
    }
    const GuestComment* comment = sceVorbisDecComment(vf, 1);
    Require(comment && comment->comments == 2 && comment->comment_lengths[0] == 12 && comment->comment_lengths[1] == 13);
    Require(std::strcmp(comment->user_comments[0], "TITLE=second") == 0 && std::strcmp(comment->user_comments[1], "ARTIST=AnyPS5") == 0);
    Require(std::strcmp(comment->vendor, ov_comment(&reference, 1)->vendor) == 0);
    Require(sceVorbisDecComment(vf, -1) == sceVorbisDecComment(vf, 0) && sceVorbisDecComment(vf, 2) == nullptr);
    Require(std::strcmp(sceVorbisDecComment(vf, 0)->user_comments[0], "TITLE=first") == 0);

    const std::int64_t firstRead = RequireSameRead(vf, reference);
    Require(firstRead > 0 && sceVorbisDecBitrateInstant(vf) == ov_bitrate_instant(&reference));
    Require(sceVorbisDecInfo(vf, -1) == first);
    Require(firstRead + DecodeTogether(vf, reference) == DecodedBytes);
    Require(sceVorbisDecPcmTell(vf) == FirstSamples + SecondSamples && sceVorbisDecTimeTell(vf) == 1.5);
    RequireSameTells(vf, reference);
    Require(sceVorbisDecInfo(vf, -1) == second && sceVorbisDecSerialNumber(vf, -1) == SecondSerial);

    Require(sceVorbisDecPcmSeek(vf, 0) == 0 && ov_pcm_seek(&reference, 0) == 0);
    Require(DecodeTogether(vf, reference) == DecodedBytes);

    Require(sceVorbisDecPcmSeek(vf, 1000) == 0 && ov_pcm_seek(&reference, 1000) == 0);
    Require(sceVorbisDecPcmTell(vf) == 1000);
    RequireSameTells(vf, reference);
    Require(RequireSameRead(vf, reference) > 0 && RequireSameRead(vf, reference, 1, 2, 0) > 0 && RequireSameRead(vf, reference, 0, 1, 0) > 0);
    Require(sceVorbisDecPcmSeekPage(vf, FirstSamples + 3000) == ov_pcm_seek_page(&reference, FirstSamples + 3000));
    RequireSameTells(vf, reference);
    Require(RequireSameRead(vf, reference) > 0 && sceVorbisDecInfo(vf, -1) == second);
    Require(sceVorbisDecTimeSeek(vf, 0.25) == 0 && ov_time_seek(&reference, 0.25) == 0);
    Require(sceVorbisDecPcmTell(vf) == FirstRate / 4);
    RequireSameTells(vf, reference);
    Require(RequireSameRead(vf, reference) > 0 && sceVorbisDecInfo(vf, -1) == first);
    Require(sceVorbisDecTimeSeekPage(vf, 1.2) == ov_time_seek_page(&reference, 1.2));
    RequireSameTells(vf, reference);
    Require(RequireSameRead(vf, reference) > 0);
    const auto middle = static_cast<std::int64_t>(stream.size() / 2);
    Require(sceVorbisDecRawSeek(vf, middle) == 0 && ov_raw_seek(&reference, middle) == 0);
    RequireSameTells(vf, reference);
    Require(RequireSameRead(vf, reference) > 0);

    Require(sceVorbisDecPcmSeekLap(vf, 5000) == 0 && ov_pcm_seek_lap(&reference, 5000) == 0);
    RequireSameTells(vf, reference);
    Require(RequireSameRead(vf, reference) > 0 && RequireSameRead(vf, reference) > 0);
    Require(sceVorbisDecPcmSeekPageLap(vf, 30000) == ov_pcm_seek_page_lap(&reference, 30000));
    RequireSameTells(vf, reference);
    Require(RequireSameRead(vf, reference) > 0);
    Require(sceVorbisDecRawSeekLap(vf, middle / 2) == ov_raw_seek_lap(&reference, middle / 2));
    RequireSameTells(vf, reference);
    Require(RequireSameRead(vf, reference) > 0);
    Require(sceVorbisDecTimeSeekLap(vf, 1.1) == 0 && ov_time_seek_lap(&reference, 1.1) == 0);
    RequireSameTells(vf, reference);
    Require(RequireSameRead(vf, reference) > 0 && sceVorbisDecInfo(vf, -1) == second);
    Require(sceVorbisDecTimeSeekPageLap(vf, 0.75) == ov_time_seek_page_lap(&reference, 0.75));
    RequireSameTells(vf, reference);
    Require(RequireSameRead(vf, reference) > 0 && sceVorbisDecInfo(vf, -1) == first);

    Require(sceVorbisDecPcmSeek(vf, -1) == OV_EINVAL && sceVorbisDecPcmSeek(vf, FirstSamples + SecondSamples + 1) == OV_EINVAL);
    Require(sceVorbisDecTimeSeek(vf, -0.5) == OV_EINVAL && sceVorbisDecTimeSeek(vf, 2.0) == OV_EINVAL);
    Require(sceVorbisDecRawSeek(vf, -1) == OV_EINVAL && sceVorbisDecRawSeek(vf, middle * 3) == OV_EINVAL);

    Require(source.closes == 0 && sceVorbisDecClear(vf) == 0 && source.closes == 1);
    Require(sceVorbisDecStreams(vf) == 0 && sceVorbisDecInfo(vf, 0) == nullptr && sceVorbisDecPcmTell(vf) == OV_EINVAL);
    Require(sceVorbisDecClear(vf) == 0 && source.closes == 1);
    Require(ov_clear(&reference) == 0 && referenceSource.closes == 1);
    Require(source.probed == 0 && guest.Untouched() && probe.Untouched());
}

void TestStreaming(const std::vector<unsigned char>& stream) {
    GuestFile guest;
    void* vf = guest.Get();
    const auto* initial = reinterpret_cast<const char*>(stream.data());
    Source source{&stream, 64};
    Require(sceVorbisDecOpenCallbacks(&source, vf, initial, 64, StreamingCallbacks) == 0);
    Source referenceSource{&stream, 64};
    OggVorbis_File reference{};
    Require(ov_open_callbacks(&referenceSource, &reference, initial, 64, HostStreamingCallbacks) == 0);

    Require(sceVorbisDecSeekable(vf) == 0 && sceVorbisDecStreams(vf) == 1);
    Require(sceVorbisDecPcmTotal(vf, -1) == OV_EINVAL && sceVorbisDecRawTotal(vf, 0) == OV_EINVAL && sceVorbisDecTimeTotal(vf, -1) == OV_EINVAL);
    Require(sceVorbisDecRawSeek(vf, 0) == OV_ENOSEEK && sceVorbisDecPcmSeek(vf, 0) == OV_ENOSEEK && sceVorbisDecTimeSeekPage(vf, 0.0) == OV_ENOSEEK);
    Require(sceVorbisDecBitrate(vf, 0) == ov_bitrate(&reference, 0) && sceVorbisDecBitrate(vf, 1) == ov_bitrate(&reference, 1));
    Require(sceVorbisDecSerialNumber(vf, 3) == FirstSerial);
    GuestInfo* info = sceVorbisDecInfo(vf, 0);
    Require(info && info == sceVorbisDecInfo(vf, 5) && info == sceVorbisDecInfo(vf, -1));
    Require(info->channels == FirstChannels && info->rate == FirstRate);
    const GuestComment* comment = sceVorbisDecComment(vf, 4);
    Require(comment && std::strcmp(comment->user_comments[0], "TITLE=first") == 0);

    Require(DecodeTogether(vf, reference) == DecodedBytes);
    Require(info->channels == SecondChannels && info->rate == SecondRate && sceVorbisDecInfo(vf, 0) == info);
    Require(std::strcmp(comment->user_comments[0], "TITLE=second") == 0);
    Require(sceVorbisDecSerialNumber(vf, 0) == SecondSerial);
    RequireSameTells(vf, reference);
    Require(sceVorbisDecClear(vf) == 0 && source.closes == 1);
    Require(ov_clear(&reference) == 0 && referenceSource.closes == 1);
    Require(guest.Untouched());
}

void TestFloatAndFilter(const std::vector<unsigned char>& stream) {
    GuestFile guest;
    void* vf = guest.Get();
    Source source{&stream};
    Require(sceVorbisDecOpenCallbacks(&source, vf, nullptr, 0, SeekableCallbacks) == 0);
    Source referenceSource{&stream};
    OggVorbis_File reference{};
    Require(ov_open_callbacks(&referenceSource, &reference, nullptr, 0, HostSeekableCallbacks) == 0);

    for (int round = 0; round < 6; ++round) {
        float** pcm = nullptr;
        float** hostPcm = nullptr;
        int guestStream = -1;
        int hostStream = -1;
        const std::int64_t got = sceVorbisDecReadFloat(vf, &pcm, 300, &guestStream);
        const long expected = ov_read_float(&reference, &hostPcm, 300, &hostStream);
        Require(got == expected && got > 0 && got <= 300 && guestStream == hostStream && guestStream == 0);
        for (int channel = 0; channel < FirstChannels; ++channel) {
            Require(std::memcmp(pcm[channel], hostPcm[channel], static_cast<std::size_t>(got) * sizeof(float)) == 0);
        }
    }
    Require(sceVorbisDecReadFloat(vf, nullptr, 50, nullptr) == ov_read_float(&reference, nullptr, 50, nullptr));
    RequireSameTells(vf, reference);

    FilterLog log;
    std::int64_t total = 0;
    for (;;) {
        std::array<char, 4096> guestBytes{};
        std::array<char, 4096> hostBytes{};
        int guestStream = -1;
        int hostStream = -1;
        const std::int64_t got = sceVorbisDecReadFilter(vf, guestBytes.data(), static_cast<int>(guestBytes.size()), 0, 2, 1, &guestStream, GuestHalve, &log);
        const long expected = ov_read_filter(&reference, hostBytes.data(), static_cast<int>(hostBytes.size()), 0, 2, 1, &hostStream, HostHalve, nullptr);
        Require(got == expected && got >= 0);
        if (got == 0) break;
        Require(guestStream == hostStream && std::memcmp(guestBytes.data(), hostBytes.data(), static_cast<std::size_t>(got)) == 0);
        total += got;
    }
    Require(total > 0 && log.calls > 0 && log.samples > 0 && log.firstChannels == FirstChannels && log.lastChannels == SecondChannels);

    Require(sceVorbisDecPcmSeek(vf, 0) == 0 && ov_pcm_seek(&reference, 0) == 0);
    std::array<char, 4096> filtered{};
    std::array<char, 4096> plain{};
    int bitstream = -1;
    const int calls = log.calls;
    const std::int64_t got = sceVorbisDecReadFilter(vf, filtered.data(), static_cast<int>(filtered.size()), 0, 2, 1, &bitstream, nullptr, &log);
    Require(got > 0 && got == ov_read(&reference, plain.data(), static_cast<int>(plain.size()), 0, 2, 1, &bitstream));
    Require(std::memcmp(filtered.data(), plain.data(), static_cast<std::size_t>(got)) == 0 && log.calls == calls);
    Require(sceVorbisDecClear(vf) == 0 && source.closes == 1 && ov_clear(&reference) == 0);
    Require(guest.Untouched());
}

void TestHalfrate(const std::vector<unsigned char>& stream) {
    GuestFile guest;
    void* vf = guest.Get();
    Source source{&stream};
    Require(sceVorbisDecOpenCallbacks(&source, vf, nullptr, 0, SeekableCallbacks) == 0);
    Source referenceSource{&stream};
    OggVorbis_File reference{};
    Require(ov_open_callbacks(&referenceSource, &reference, nullptr, 0, HostSeekableCallbacks) == 0);

    Require(sceVorbisDecHalfrateP(vf) == 0);
    Require(RequireSameRead(vf, reference) > 0 && RequireSameRead(vf, reference) > 0);
    Require(sceVorbisDecHalfrate(vf, 1) == 0 && ov_halfrate(&reference, 1) == 0);
    Require(sceVorbisDecHalfrateP(vf) == 1 && ov_halfrate_p(&reference) == 1);
    RequireSameTells(vf, reference);
    const std::int64_t half = DecodeTogether(vf, reference);
    Require(half > 0 && half < DecodedBytes / 2);
    RequireSameTells(vf, reference);
    Require(sceVorbisDecPcmTell(vf) > FirstSamples);
    Require(sceVorbisDecHalfrate(vf, 0) == 0 && sceVorbisDecHalfrateP(vf) == 0);
    Require(sceVorbisDecClear(vf) == 0 && source.closes == 1 && ov_clear(&reference) == 0);
    Require(guest.Untouched());
}

void TestCrossLap(const std::vector<unsigned char>& stream) {
    GuestFile first;
    GuestFile second;
    GuestFile closed;
    GuestFile otherClosed;
    Source firstSource{&stream};
    Source secondSource{&stream};
    Require(sceVorbisDecOpenCallbacks(&secondSource, second.Get(), nullptr, 0, SeekableCallbacks) == 0);
    firstSource.probe = second.Get();
    Require(sceVorbisDecOpenCallbacks(&firstSource, first.Get(), nullptr, 0, SeekableCallbacks) == 0);
    Require(firstSource.probed == 2);
    Source firstReferenceSource{&stream};
    Source secondReferenceSource{&stream};
    OggVorbis_File firstReference{};
    OggVorbis_File secondReference{};
    Require(ov_open_callbacks(&firstReferenceSource, &firstReference, nullptr, 0, HostSeekableCallbacks) == 0);
    Require(ov_open_callbacks(&secondReferenceSource, &secondReference, nullptr, 0, HostSeekableCallbacks) == 0);

    Require(RequireSameRead(first.Get(), firstReference) > 0 && RequireSameRead(first.Get(), firstReference) > 0);
    Require(sceVorbisDecPcmSeek(second.Get(), FirstSamples + 2000) == 0 && ov_pcm_seek(&secondReference, FirstSamples + 2000) == 0);
    Require(sceVorbisDecCrossLap(first.Get(), second.Get()) == 0 && ov_crosslap(&firstReference, &secondReference) == 0);
    for (int round = 0; round < 3; ++round) {
        Require(RequireSameRead(second.Get(), secondReference) > 0);
        Require(RequireSameRead(first.Get(), firstReference) > 0);
    }
    RequireSameTells(first.Get(), firstReference);
    RequireSameTells(second.Get(), secondReference);
    Require(sceVorbisDecCrossLap(first.Get(), first.Get()) == 0);
    Require(sceVorbisDecCrossLap(first.Get(), closed.Get()) == OV_EINVAL && sceVorbisDecCrossLap(closed.Get(), second.Get()) == OV_EINVAL);
    Require(sceVorbisDecCrossLap(closed.Get(), otherClosed.Get()) == OV_EINVAL && sceVorbisDecCrossLap(closed.Get(), closed.Get()) == 0);
    Require(sceVorbisDecCrossLap(nullptr, nullptr) == 0);
    OggVorbis_File zeroed{};
    Require(sceVorbisDecCrossLap(closed.Get(), nullptr) == OV_EINVAL && ov_crosslap(&zeroed, nullptr) == OV_EINVAL);
    Require(Throws<std::invalid_argument>([&] { sceVorbisDecCrossLap(first.Get(), nullptr); }));
    Require(Throws<std::invalid_argument>([&] { sceVorbisDecCrossLap(nullptr, closed.Get()); }));
    Require(sceVorbisDecClear(first.Get()) == 0 && sceVorbisDecClear(second.Get()) == 0);
    Require(firstSource.closes == 1 && secondSource.closes == 1);
    Require(ov_clear(&firstReference) == 0 && ov_clear(&secondReference) == 0);
    Require(first.Untouched() && second.Untouched() && closed.Untouched() && otherClosed.Untouched());
}

void TestNeverOpened() {
    GuestFile guest;
    void* vf = guest.Get();
    Require(sceVorbisDecStreams(vf) == 0 && sceVorbisDecSeekable(vf) == 0);
    Require(sceVorbisDecSerialNumber(vf, -1) == 0 && sceVorbisDecSerialNumber(vf, 0) == 0 && sceVorbisDecSerialNumber(vf, 3) == 0);
    Require(sceVorbisDecBitrate(vf, -1) == OV_EINVAL && sceVorbisDecBitrate(vf, 0) == OV_EINVAL && sceVorbisDecBitrateInstant(vf) == OV_EINVAL);
    Require(sceVorbisDecRawTotal(vf, -1) == OV_EINVAL && sceVorbisDecPcmTotal(vf, 0) == OV_EINVAL && sceVorbisDecTimeTotal(vf, -1) == OV_EINVAL);
    Require(sceVorbisDecRawSeek(vf, 0) == OV_EINVAL && sceVorbisDecPcmSeek(vf, 0) == OV_EINVAL && sceVorbisDecPcmSeekPage(vf, 0) == OV_EINVAL);
    Require(sceVorbisDecTimeSeek(vf, 0.0) == OV_EINVAL && sceVorbisDecTimeSeekPage(vf, 0.0) == OV_EINVAL);
    Require(sceVorbisDecRawSeekLap(vf, 0) == OV_EINVAL && sceVorbisDecPcmSeekLap(vf, 0) == OV_EINVAL && sceVorbisDecPcmSeekPageLap(vf, 0) == OV_EINVAL);
    Require(sceVorbisDecTimeSeekLap(vf, 0.0) == OV_EINVAL && sceVorbisDecTimeSeekPageLap(vf, 0.0) == OV_EINVAL);
    Require(sceVorbisDecRawTell(vf) == OV_EINVAL && sceVorbisDecPcmTell(vf) == OV_EINVAL && sceVorbisDecTimeTell(vf) == OV_EINVAL);
    Require(sceVorbisDecInfo(vf, -1) == nullptr && sceVorbisDecInfo(vf, 0) == nullptr);
    Require(sceVorbisDecComment(vf, -1) == nullptr && sceVorbisDecComment(vf, 0) == nullptr);
    std::array<char, 64> buffer{};
    int bitstream = 7;
    Require(sceVorbisDecRead(vf, buffer.data(), static_cast<int>(buffer.size()), 0, 2, 1, &bitstream) == OV_EINVAL && bitstream == 7);
    float** pcm = nullptr;
    Require(sceVorbisDecReadFloat(vf, &pcm, 64, &bitstream) == OV_EINVAL && pcm == nullptr && bitstream == 7);
    FilterLog log;
    Require(sceVorbisDecReadFilter(vf, buffer.data(), static_cast<int>(buffer.size()), 0, 2, 1, &bitstream, GuestHalve, &log) == OV_EINVAL);
    Require(log.calls == 0 && bitstream == 7);
    Require(sceVorbisDecHalfrate(vf, 1) == OV_EINVAL && sceVorbisDecHalfrateP(vf) == OV_EINVAL);
    Require(sceVorbisDecClear(vf) == 0 && sceVorbisDecClear(nullptr) == 0);
    Require(guest.Untouched());

    Source source;
    Require(Throws<std::invalid_argument>([&] { sceVorbisDecOpenCallbacks(&source, nullptr, nullptr, 0, SeekableCallbacks); }));
    Require(Throws<std::invalid_argument>([] { sceVorbisDecStreams(nullptr); }));
    Require(Throws<std::invalid_argument>([] { sceVorbisDecInfo(nullptr, 0); }));
    Require(Throws<std::invalid_argument>([] { sceVorbisDecPcmSeek(nullptr, 0); }));
}

void TestFailedOpens(const std::vector<unsigned char>& stream) {
    GuestFile guest;
    void* vf = guest.Get();
    std::vector<unsigned char> noise(8192);
    std::uint32_t state = 0x2545f491;
    for (auto& value : noise) {
        state = state * 1664525u + 1013904223u;
        value = static_cast<unsigned char>(state >> 24);
    }
    Source noiseSource{&noise};
    Require(sceVorbisDecOpenCallbacks(&noiseSource, vf, nullptr, 0, SeekableCallbacks) == OV_ENOTVORBIS);
    Require(noiseSource.closes == 0 && sceVorbisDecStreams(vf) == 0 && sceVorbisDecInfo(vf, 0) == nullptr);
    Require(sceVorbisDecClear(vf) == 0 && noiseSource.closes == 0);

    Source untold{&stream};
    constexpr GuestCallbacks NoTell{GuestRead, GuestSeek, GuestClose, nullptr};
    Require(sceVorbisDecOpenCallbacks(&untold, vf, nullptr, 0, NoTell) == OV_EINVAL);
    Require(untold.closes == 0 && sceVorbisDecStreams(vf) == 0 && sceVorbisDecClear(vf) == 0 && untold.closes == 0);

    constexpr GuestCallbacks NoRead{nullptr, GuestSeek, GuestClose, GuestTell};
    Source unread{&stream};
    OggVorbis_File reference{};
    const ov_callbacks hostNoRead{nullptr, HostSeek, HostClose, HostTell};
    Require(sceVorbisDecOpenCallbacks(&unread, vf, nullptr, 0, NoRead) == ov_open_callbacks(&unread, &reference, nullptr, 0, hostNoRead));
    Require(unread.closes == 0 && sceVorbisDecStreams(vf) == 0);

    Require(sceVorbisDecOpenCallbacks(nullptr, vf, nullptr, 0, SeekableCallbacks) == OV_ENOTVORBIS);

    Source previous{&stream};
    Source next{&stream};
    Require(sceVorbisDecOpenCallbacks(&previous, vf, nullptr, 0, SeekableCallbacks) == 0);
    Require(sceVorbisDecOpenCallbacks(&next, vf, nullptr, 0, StreamingCallbacks) == 0);
    Require(previous.closes == 0 && sceVorbisDecSeekable(vf) == 0 && sceVorbisDecStreams(vf) == 1);
    Require(sceVorbisDecClear(vf) == 0 && previous.closes == 0 && next.closes == 1);

    Source unsized{&stream};
    Require(sceVorbisDecOpenCallbacks(&unsized, vf, nullptr, std::int64_t{1} << 32, SeekableCallbacks) == 0);
    Require(sceVorbisDecStreams(vf) == 2 && sceVorbisDecClear(vf) == 0 && unsized.closes == 1);

    const auto* initial = reinterpret_cast<const char*>(stream.data());
    const auto size = static_cast<std::int64_t>(stream.size());
    Require(sceVorbisDecOpenCallbacks(nullptr, vf, initial, size, SeekableCallbacks) == 0);
    Require(ov_open_callbacks(nullptr, &reference, initial, static_cast<long>(size), HostSeekableCallbacks) == 0);
    Require(sceVorbisDecSeekable(vf) == 0 && DecodeTogether(vf, reference) == DecodedBytes);
    Require(sceVorbisDecClear(vf) == 0 && ov_clear(&reference) == 0);
    Require(guest.Untouched());
}

void TestThrowingCallbacks(const std::vector<unsigned char>& stream) {
    GuestFile guest;
    Source source{&stream};
    Require(sceVorbisDecOpenCallbacks(&source, guest.Get(), nullptr, 0, SeekableCallbacks) == 0);
    std::array<char, 4096> buffer{};
    int bitstream = -1;
    source.throwOnRead = true;
    bool threw = false;
    for (int call = 0; call < 1024 && !threw; ++call) {
        threw = Throws<std::runtime_error>([&] { sceVorbisDecRead(guest.Get(), buffer.data(), static_cast<int>(buffer.size()), 0, 2, 1, &bitstream); });
    }
    Require(threw);
    source.throwOnRead = false;
    source.throwOnClose = true;
    Require(Throws<std::runtime_error>([&] { sceVorbisDecClear(guest.Get()); }));
    Require(source.closes == 1 && sceVorbisDecStreams(guest.Get()) == 0);
    Require(sceVorbisDecClear(guest.Get()) == 0 && source.closes == 1);
    Require(guest.Untouched());
}

}

int main() {
    std::vector<unsigned char> stream;
    EncodeLink(stream, FirstSerial, FirstChannels, FirstRate, FirstSamples, "first");
    EncodeLink(stream, SecondSerial, SecondChannels, SecondRate, SecondSamples, "second");

    TestSeekable(stream);
    TestStreaming(stream);
    TestFloatAndFilter(stream);
    TestHalfrate(stream);
    TestCrossLap(stream);
    TestNeverOpened();
    TestFailedOpens(stream);
    TestThrowingCallbacks(stream);

    Require(Throws<std::runtime_error>([] { sceVorbisDecGetDecodePriority(); }));
    Require(Throws<std::runtime_error>([] { sceVorbisDecSetDecodePriority(); }));
    Require(Throws<std::runtime_error>([] { sceVorbisDecGetLastDecodeError(); }));

    std::puts("All VorbisDec guest tests passed");
    return 0;
}
