#include "Ngs2Test.hpp"
#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <new>

static std::atomic<bool> observe{false};
static std::atomic<std::size_t> liveBytes{0}, allocationCalls{0};
struct alignas(std::max_align_t) Allocation {
    std::size_t bytes;
    bool counted;
};

void* operator new(std::size_t bytes) {
    if (bytes > SIZE_MAX - sizeof(Allocation)) throw std::bad_alloc();
    auto* allocation = static_cast<Allocation*>(std::malloc(sizeof(Allocation) + bytes));
    if (!allocation) throw std::bad_alloc();
    allocation->bytes = bytes;
    allocation->counted = observe && bytes >= 8192;
    if (allocation->counted) { liveBytes += bytes; ++allocationCalls; }
    return allocation + 1;
}
void operator delete(void* pointer) noexcept {
    if (!pointer) return;
    auto* allocation = static_cast<Allocation*>(pointer) - 1;
    if (allocation->counted) liveBytes -= allocation->bytes;
    std::free(allocation);
}
void operator delete(void* pointer, std::size_t) noexcept { ::operator delete(pointer); }
void* operator new[](std::size_t bytes) { return ::operator new(bytes); }
void operator delete[](void* pointer) noexcept { ::operator delete(pointer); }
void operator delete[](void* pointer, std::size_t) noexcept { ::operator delete(pointer); }

static void Play(uintptr_t system, uintptr_t voice, std::vector<std::int16_t>& pcm, bool streaming = false, bool slowFirst = false) {
    std::fill(pcm.begin(), pcm.end(), 8192);
    const Ngs2WaveformBlock block{0, pcm.size() * sizeof(std::int16_t), 0, 0, static_cast<std::uint32_t>(pcm.size()) + (streaming ? Grain : 0), 0, 0};
    Control(voice, SCE_NGS2_SAMPLER_VOICE_PARAM_ADD_WAVEFORM_BLOCKS,
        Ngs2SamplerVoiceWaveformBlocksParam{{}, pcm.data(), SCE_NGS2_WAVEFORM_BLOCKS_FLAG_RESET | (streaming ? SCE_NGS2_WAVEFORM_BLOCKS_FLAG_CONTINUE : 0), 1, &block});
    Control(voice, SCE_NGS2_SAMPLER_VOICE_PARAM_PITCH,
        Ngs2SamplerVoicePitchParam{{}, slowFirst ? 1.0f : static_cast<float>(pcm.size() / Grain)});
    Event(voice, SCE_NGS2_VOICE_EVENT_PLAY);
    float output[Grain]{};
    const Ngs2RenderBufferInfo info{output, sizeof(output), SCE_NGS2_WAVEFORM_TYPE_PCM_F32L, 1};
    Require(sceNgs2SystemRender(system, &info, 1) == SCE_NGS2_OK);
    for (float sample : output) Require(sample == 0.25f);
    if (slowFirst) {
        Require(liveBytes >= pcm.size() * sizeof(std::int16_t));
        std::fill(pcm.begin(), pcm.end(), -8192);
        Control(voice, SCE_NGS2_SAMPLER_VOICE_PARAM_PITCH,
            Ngs2SamplerVoicePitchParam{{}, static_cast<float>(pcm.size() / Grain)});
        Require(sceNgs2SystemRender(system, &info, 1) == SCE_NGS2_OK);
        for (float sample : output) Require(sample == 0.25f);
    }
    if (streaming) Require((Flags(voice) & SCE_NGS2_VOICE_STATE_FLAG_PLAYING) != 0);
    else Require(Flags(voice) == 0);
}

int main() {
    const auto system = CreateSystem();
    const auto voice = Voice(CreateRack(system, SCE_NGS2_RACK_ID_SAMPLER));
    Control(voice, SCE_NGS2_SAMPLER_VOICE_PARAM_SETUP,
        Ngs2SamplerVoiceSetupParam{{}, {SCE_NGS2_WAVEFORM_TYPE_PCM_I16L, 1, 48000, 0, 0, 0}});
    Patch(voice, Mastering(system, 1));
    std::vector<std::int16_t> large(1024 * 1024, 8192);
    std::vector<std::int16_t> ordinary(8192, 8192);
    observe = true;
    Play(system, voice, large, false, true);
    const auto largeCalls = allocationCalls.load();
    const auto afterLarge = liveBytes.load();
    Play(system, voice, large, true, true);
    const auto afterDrainedStream = liveBytes.load();
    const auto ordinaryStart = allocationCalls.load();
    for (int i = 0; i < 100; ++i) Play(system, voice, ordinary);
    const auto ordinaryCalls = allocationCalls.load() - ordinaryStart;
    const auto afterOrdinary = liveBytes.load();
    observe = false;
    Require(sceNgs2SystemDestroy(system, nullptr) == SCE_NGS2_OK);
    std::printf("large calls=%zu retained after large=%zu, drained stream=%zu; ordinary calls over 100 plays=%zu retained=%zu; after destroy=%zu\n",
        largeCalls, afterLarge, afterDrainedStream, ordinaryCalls, afterOrdinary, liveBytes.load());
    return largeCalls != 0 && afterLarge == 0 && afterDrainedStream == 0 && ordinaryCalls <= 2 && afterOrdinary <= 128 * 1024 && liveBytes == 0 ? 0 : 1;
}
