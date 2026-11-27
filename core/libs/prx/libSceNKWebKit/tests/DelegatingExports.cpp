#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "DelegatingExports check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

using Call = std::uint64_t (APS5_VABI*)(void*, std::uint64_t, std::uint64_t, std::uint64_t, std::uint64_t, std::uint64_t);

struct Entry {
    Call call;
    std::size_t offset;
    std::size_t slot;
};

static const Entry Entries[] = {
    {reinterpret_cast<Call>(&_ZN7WebCore11MediaPlayer10cancelLoadEv), 0x50, 0x20},
    {reinterpret_cast<Call>(&_ZN7WebCore11MediaPlayer10readyStateEv), 0x50, 0x1b0},
    {reinterpret_cast<Call>(&_ZN7WebCore11MediaPlayer11naturalSizeEv), 0x50, 0x90},
    {reinterpret_cast<Call>(&_ZN7WebCore11MediaPlayer11rateChangedEv), 0x18, 0x40},
    {reinterpret_cast<Call>(&_ZN7WebCore11MediaPlayer11sizeChangedEv), 0x18, 0x60},
    {reinterpret_cast<Call>(&_ZN7WebCore11MediaPlayer11timeChangedEv), 0x18, 0x30},
    {reinterpret_cast<Call>(&_ZN7WebCore11MediaPlayer12addTextTrackERNS_22InbandTextTrackPrivateE), 0x18, 0x128},
    {reinterpret_cast<Call>(&_ZN7WebCore11MediaPlayer12networkStateEv), 0x50, 0x1a8},
    {reinterpret_cast<Call>(&_ZN7WebCore11MediaPlayer13addAudioTrackERNS_17AudioTrackPrivateE), 0x18, 0x120},
    {reinterpret_cast<Call>(&_ZN7WebCore11MediaPlayer13addVideoTrackERNS_17VideoTrackPrivateE), 0x18, 0x130},
    {reinterpret_cast<Call>(&_ZN7WebCore11MediaPlayer13tracksChangedEv), 0x50, 0x2f0},
    {reinterpret_cast<Call>(&_ZN7WebCore11MediaPlayer15durationChangedEv), 0x18, 0x38},
    {reinterpret_cast<Call>(&_ZN7WebCore11MediaPlayer15removeTextTrackERNS_22InbandTextTrackPrivateE), 0x18, 0x140},
    {reinterpret_cast<Call>(&_ZN7WebCore11MediaPlayer16removeAudioTrackERNS_17AudioTrackPrivateE), 0x18, 0x138},
    {reinterpret_cast<Call>(&_ZN7WebCore11MediaPlayer16removeVideoTrackERNS_17VideoTrackPrivateE), 0x18, 0x148},
    {reinterpret_cast<Call>(&_ZN7WebCore11MediaPlayer17seekWithToleranceERKN3WTF9MediaTimeES4_S4_), 0x50, 0x118},
    {reinterpret_cast<Call>(&_ZN7WebCore11MediaPlayer18liveUpdateIntervalEv), 0x50, 0x1f0},
    {reinterpret_cast<Call>(&_ZN7WebCore11MediaPlayer18mediaEngineUpdatedEv), 0x18, 0x68},
    {reinterpret_cast<Call>(&_ZN7WebCore11MediaPlayer18setBufferingPolicyENS_16MediaPlayerEnums15BufferingPolicyE), 0x50, 0x58},
    {reinterpret_cast<Call>(&_ZN7WebCore11MediaPlayer19syncTextTrackBoundsEv), 0x50, 0x2e8},
    {reinterpret_cast<Call>(&_ZN7WebCore11MediaPlayer20cachedResourceLoaderEv), 0x18, 0xf8},
    {reinterpret_cast<Call>(&_ZN7WebCore11MediaPlayer20playbackStateChangedEv), 0x18, 0x48},
    {reinterpret_cast<Call>(&_ZN7WebCore11MediaPlayer20resourceNotSupportedEv), 0x18, 0x50},
    {reinterpret_cast<Call>(&_ZN7WebCore11MediaPlayer21characteristicChangedEv), 0x18, 0x78},
    {reinterpret_cast<Call>(&_ZN7WebCore11MediaPlayer22performTaskAtMediaTimeEON3WTF8FunctionIFvvEEERKNS1_9MediaTimeE), 0x50, 0x358},
    {reinterpret_cast<Call>(&_ZN7WebCore11MediaPlayer24firstVideoFrameAvailableEv), 0x18, 0x70},
    {reinterpret_cast<Call>(&_ZN7WebCore11MediaPlayer24remoteEngineFailedToLoadEv), 0x18, 0x178},
    {reinterpret_cast<Call>(&_ZN7WebCore11MediaPlayer24setClosedCaptionsVisibleEb), 0x50, 0x190},
    {reinterpret_cast<Call>(&_ZN7WebCore11MediaPlayer25bufferedTimeRangesChangedEv), 0x18, 0x1b8},
    {reinterpret_cast<Call>(&_ZN7WebCore11MediaPlayer25seekableTimeRangesChangedEv), 0x18, 0x1c0},
    {reinterpret_cast<Call>(&_ZN7WebCore11MediaPlayer25shouldIgnoreIntrinsicSizeEv), 0x50, 0x360},
    {reinterpret_cast<Call>(&_ZN7WebCore11MediaPlayer26activeSourceBuffersChangedEv), 0x18, 0x98},
    {reinterpret_cast<Call>(&_ZN7WebCore11MediaPlayer26applicationDidBecomeActiveEv), 0x50, 0x350},
    {reinterpret_cast<Call>(&_ZN7WebCore11MediaPlayer26setTextTrackRepresentationEPNS_23TextTrackRepresentationE), 0x50, 0x2e0},
    {reinterpret_cast<Call>(&_ZN7WebCore11MediaPlayer27applicationWillResignActiveEv), 0x50, 0x348},
    {reinterpret_cast<Call>(&_ZN7WebCore11MediaPlayer28setShouldMaintainAspectRatioEb), 0x50, 0x268},
    {reinterpret_cast<Call>(&_ZN7WebCore11MediaPlayer29acceleratedCompositingEnabledEv), 0x18, 0x90},
    {reinterpret_cast<Call>(&_ZN7WebCore11MediaPlayer32acceleratedRenderingStateChangedEv), 0x50, 0x260},
    {reinterpret_cast<Call>(&_ZN7WebCore11MediaPlayer34seekableTimeRangesLastModifiedTimeEv), 0x50, 0x1e8},
    {reinterpret_cast<Call>(&_ZN7WebCore11MediaPlayer4playEv), 0x50, 0x48},
    {reinterpret_cast<Call>(&_ZN7WebCore11MediaPlayer4seekERKN3WTF9MediaTimeE), 0x50, 0x110},
    {reinterpret_cast<Call>(&_ZN7WebCore11MediaPlayer5paintERNS_15GraphicsContextERKNS_9FloatRectE), 0x50, 0x218},
    {reinterpret_cast<Call>(&_ZN7WebCore11MediaPlayer5pauseEv), 0x50, 0x50},
    {reinterpret_cast<Call>(&_ZN7WebCore11MediaPlayer7repaintEv), 0x18, 0x58},
    {reinterpret_cast<Call>(&_ZN7WebCore11MediaPlayer7setRateEd), 0x50, 0x140},
    {reinterpret_cast<Call>(&_ZN7WebCore16HTMLInputElement11selectColorEN3WTF10StringViewE), 0xf8, 0x348},
    {reinterpret_cast<Call>(&_ZN7WebCore16HTMLInputElement5filesEv), 0xf8, 0x278},
    {reinterpret_cast<Call>(&_ZN7WebCore18RenderLayerBacking30setIsTrackingDisplayListReplayEb), 0x38, 0x2b0},
    {reinterpret_cast<Call>(&_ZN7WebCore22CanvasRenderingContext5derefEv), 0x18, 0x18},
    {reinterpret_cast<Call>(&_ZN7WebCore4Page23clearUndoRedoOperationsEv), 0x78, 0x128},
    {reinterpret_cast<Call>(&_ZN7WebCore6Chrome23enableSuddenTerminationEv), 0x10, 0x388},
    {reinterpret_cast<Call>(&_ZN7WebCore6Chrome24disableSuddenTerminationEv), 0x10, 0x390},
    {reinterpret_cast<Call>(&_ZNK7WebCore11FrameLoader6pageIDEv), 0x8, 0x28},
    {reinterpret_cast<Call>(&_ZNK7WebCore11FrameLoader7frameIDEv), 0x8, 0x30},
    {reinterpret_cast<Call>(&_ZNK7WebCore11MediaPlayer13isVideoPlayerEv), 0x18, 0xd0},
    {reinterpret_cast<Call>(&_ZNK7WebCore11MediaPlayer13movieLoadTypeEv), 0x50, 0x288},
    {reinterpret_cast<Call>(&_ZNK7WebCore11MediaPlayer13platformLayerEv), 0x50, 0x38},
    {reinterpret_cast<Call>(&_ZNK7WebCore11MediaPlayer13requestedRateEv), 0x18, 0x180},
    {reinterpret_cast<Call>(&_ZNK7WebCore11MediaPlayer16canSaveMediaDataEv), 0x50, 0x88},
    {reinterpret_cast<Call>(&_ZNK7WebCore11MediaPlayer16supportsScanningEv), 0x50, 0x70},
    {reinterpret_cast<Call>(&_ZNK7WebCore11MediaPlayer17decodedFrameCountEv), 0x50, 0x2a8},
    {reinterpret_cast<Call>(&_ZNK7WebCore11MediaPlayer17doesHaveAttributeERKN3WTF10AtomStringEPS2_), 0x18, 0x108},
    {reinterpret_cast<Call>(&_ZNK7WebCore11MediaPlayer17droppedFrameCountEv), 0x50, 0x2b0},
    {reinterpret_cast<Call>(&_ZNK7WebCore11MediaPlayer17hasClosedCaptionsEv), 0x50, 0x188},
    {reinterpret_cast<Call>(&_ZNK7WebCore11MediaPlayer18maxFastForwardRateEv), 0x50, 0x198},
    {reinterpret_cast<Call>(&_ZNK7WebCore11MediaPlayer18minFastReverseRateEv), 0x50, 0x1a0},
    {reinterpret_cast<Call>(&_ZNK7WebCore11MediaPlayer18shouldDisableSleepEv), 0x18, 0x1a0},
    {reinterpret_cast<Call>(&_ZNK7WebCore11MediaPlayer18supportsFullscreenEv), 0x50, 0x68},
    {reinterpret_cast<Call>(&_ZNK7WebCore11MediaPlayer19mediaCacheDirectoryEv), 0x18, 0x118},
    {reinterpret_cast<Call>(&_ZNK7WebCore11MediaPlayer19playerContentsScaleEv), 0x18, 0xe0},
    {reinterpret_cast<Call>(&_ZNK7WebCore11MediaPlayer20playerContentBoxRectEv), 0x18, 0xd8},
    {reinterpret_cast<Call>(&_ZNK7WebCore11MediaPlayer20renderingModeChangedEv), 0x18, 0x88},
    {reinterpret_cast<Call>(&_ZNK7WebCore11MediaPlayer21audioDecodedByteCountEv), 0x50, 0x2b8},
    {reinterpret_cast<Call>(&_ZNK7WebCore11MediaPlayer21videoDecodedByteCountEv), 0x50, 0x2c0},
    {reinterpret_cast<Call>(&_ZNK7WebCore11MediaPlayer22didPassCORSAccessCheckEv), 0x50, 0x278},
    {reinterpret_cast<Call>(&_ZNK7WebCore11MediaPlayer22hasAvailableVideoFrameEv), 0x50, 0x250},
    {reinterpret_cast<Call>(&_ZNK7WebCore11MediaPlayer23hasSingleSecurityOriginEv), 0x50, 0x270},
    {reinterpret_cast<Call>(&_ZNK7WebCore11MediaPlayer24shouldUsePersistentCacheEv), 0x18, 0x110},
    {reinterpret_cast<Call>(&_ZNK7WebCore11MediaPlayer24supportsPictureInPictureEv), 0x50, 0x60},
    {reinterpret_cast<Call>(&_ZNK7WebCore11MediaPlayer25renderingCanBeAcceleratedEv), 0x18, 0x80},
    {reinterpret_cast<Call>(&_ZNK7WebCore11MediaPlayer26shouldCheckHardwareSupportEv), 0x18, 0x1b0},
    {reinterpret_cast<Call>(&_ZNK7WebCore11MediaPlayer28requiresImmediateCompositingEv), 0x50, 0x80},
    {reinterpret_cast<Call>(&_ZNK7WebCore11MediaPlayer28supportsAcceleratedRenderingEv), 0x50, 0x258},
    {reinterpret_cast<Call>(&_ZNK7WebCore11MediaPlayer31maximumDurationToCacheMediaTimeEv), 0x50, 0x2a0},
    {reinterpret_cast<Call>(&_ZNK7WebCore11MediaPlayer31requiresTextTrackRepresentationEv), 0x50, 0x2d8},
    {reinterpret_cast<Call>(&_ZNK7WebCore11MediaPlayer35platformVolumeConfigurationRequiredEv), 0x18, 0xe8},
    {reinterpret_cast<Call>(&_ZNK7WebCore11MediaPlayer41mediaContentTypesRequiringHardwareSupportEv), 0x18, 0x1a8},
    {reinterpret_cast<Call>(&_ZNK7WebCore11MediaPlayer4rateEv), 0x50, 0x148},
    {reinterpret_cast<Call>(&_ZNK7WebCore11MediaPlayer5endedEv), 0x50, 0x320},
    {reinterpret_cast<Call>(&_ZNK7WebCore11MediaPlayer6pausedEv), 0x50, 0x168},
    {reinterpret_cast<Call>(&_ZNK7WebCore11MediaPlayer7seekingEv), 0x50, 0x120},
    {reinterpret_cast<Call>(&_ZNK7WebCore11MediaPlayer8hasAudioEv), 0x50, 0xa0},
    {reinterpret_cast<Call>(&_ZNK7WebCore11MediaPlayer8hasVideoEv), 0x50, 0x98},
    {reinterpret_cast<Call>(&_ZNK7WebCore11MediaPlayer9isLoopingEv), 0x18, 0xf0},
    {reinterpret_cast<Call>(&_ZNK7WebCore16HTMLInputElement11valueAsDateEv), 0xf8, 0x58},
    {reinterpret_cast<Call>(&_ZNK7WebCore16HTMLInputElement13valueAsNumberEv), 0xf8, 0x68},
    {reinterpret_cast<Call>(&_ZNK7WebCore16HTMLInputElement21autoFillButtonElementEv), 0xf8, 0x218},
    {reinterpret_cast<Call>(&_ZNK7WebCore16HTMLInputElement5widthEv), 0xf8, 0x370},
    {reinterpret_cast<Call>(&_ZNK7WebCore16HTMLInputElement6heightEv), 0xf8, 0x368},
    {reinterpret_cast<Call>(&_ZNK7WebCore18RenderLayerBacking12tiledBackingEv), 0x38, 0x2e0},
    {reinterpret_cast<Call>(&_ZNK7WebCore21BackForwardController12forwardCountEv), 0x8, 0x30},
    {reinterpret_cast<Call>(&_ZNK7WebCore21BackForwardController9backCountEv), 0x8, 0x28},
    {reinterpret_cast<Call>(&_ZNK7WebCore6Chrome13setWindowRectERKNS_9FloatRectE), 0x10, 0x8},
    {reinterpret_cast<Call>(&_ZNK7WebCore6Chrome4showEv), 0x10, 0x58},
};

struct Seen {
    void* self;
    std::uint64_t args[5];
    int calls;
};

static Seen seen;

static std::uint64_t APS5_VABI Target(void* self, std::uint64_t a, std::uint64_t b, std::uint64_t c, std::uint64_t d, std::uint64_t e) {
    seen.self = self;
    seen.args[0] = a;
    seen.args[1] = b;
    seen.args[2] = c;
    seen.args[3] = d;
    seen.args[4] = e;
    ++seen.calls;
    return 0x5151515151515151ULL;
}

static std::uint64_t APS5_VABI Wrong(void*, std::uint64_t, std::uint64_t, std::uint64_t, std::uint64_t, std::uint64_t) {
    std::abort();
}

template<typename F> static bool Throws(F f) {
    try {
        f();
    } catch (const std::invalid_argument&) {
        return true;
    }
    return false;
}

int main() {
    static void* vtable[512];
    for (auto& entry : vtable) entry = reinterpret_cast<void*>(&Wrong);
    struct Delegate {
        void** vptr;
    } delegate{vtable};
    alignas(8) unsigned char object[512];
    const auto before = NotImplementedCalls_nid_no_patch();
    for (const Entry& entry : Entries) {
        Require(entry.offset + sizeof(void*) <= sizeof(object) && entry.slot / sizeof(void*) < 512 && entry.slot % sizeof(void*) == 0);
        std::memset(object, 0, sizeof(object));
        void* pointer = &delegate;
        std::memcpy(object + entry.offset, &pointer, sizeof(pointer));
        vtable[entry.slot / sizeof(void*)] = reinterpret_cast<void*>(&Target);
        seen = {};
        Require(entry.call(object, 1, 2, 3, 4, 5) == 0x5151515151515151ULL);
        Require(seen.calls == 1 && seen.self == &delegate);
        for (int i = 0; i < 5; ++i) Require(seen.args[i] == static_cast<std::uint64_t>(i + 1));
        vtable[entry.slot / sizeof(void*)] = reinterpret_cast<void*>(&Wrong);
        std::memset(object + entry.offset, 0, sizeof(void*));
        Require(Throws([&] { entry.call(object, 0, 0, 0, 0, 0); }));
        Require(Throws([&] { entry.call(nullptr, 0, 0, 0, 0, 0); }));
    }
    Require(sizeof(Entries) / sizeof(Entries[0]) == 104);
    Require(NotImplementedCalls_nid_no_patch() == before);
    return 0;
}
