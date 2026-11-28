#include "prx/libSceAudioPropagation/AudioPropagation.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <vector>

extern "C" {

int APS5_VABI libSceAudioPropagationUnknown_TrQMW7ARZXY_nid_no_patch(AudioPropagationHandle system, AudioPropagationHandle* outMaterials, std::size_t size);
int APS5_VABI libSceAudioPropagationUnknown_fOTrQvqzABM_nid_no_patch(AudioPropagationHandle system, AudioPropagationHandle* outPortals, std::size_t size);
int APS5_VABI libSceAudioPropagationUnknown_mdH4mANi9GI_nid_no_patch(AudioPropagationHandle system, AudioPropagationHandle* outRooms, std::uint32_t capacity);
int APS5_VABI libSceAudioPropagationUnknown_o0gGIp1vd6A_nid_no_patch(AudioPropagationHandle system, AudioPropagationHandle* outSources, std::uint32_t capacity);

}

namespace {

void require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "AudioPropagation InventoryExports: %s\n", message);
        std::abort();
    }
}

template <typename TCall>
void requireNotImplemented(TCall call, const char* message) {
    try {
        call();
    } catch (const std::runtime_error&) {
        return;
    } catch (...) {
        require(false, message);
    }
    require(false, message);
}

void testInvalidSystemGates() {
    std::array<AudioPropagationHandle, 4> handles{};
    std::memset(handles.data(), 0xA5, handles.size() * sizeof(AudioPropagationHandle));
    const auto untouched = [&] {
        return std::all_of(handles.begin(), handles.end(), [](AudioPropagationHandle value) {
            return value == 0xA5A5A5A5A5A5A5A5ull;
        });
    };

    for (const AudioPropagationHandle bad : {AudioPropagationHandle{0}, AudioPropagationHandle{0xdeadbeef}}) {
        require(libSceAudioPropagationUnknown_TrQMW7ARZXY_nid_no_patch(bad, handles.data(), handles.size() * sizeof(AudioPropagationHandle)) == AudioPropagation::ErrorInvalidHandle, "TrQMW7ARZXY invalid system");
        require(libSceAudioPropagationUnknown_TrQMW7ARZXY_nid_no_patch(bad, nullptr, 0) == AudioPropagation::ErrorInvalidHandle, "TrQMW7ARZXY invalid system null output");
        require(libSceAudioPropagationUnknown_fOTrQvqzABM_nid_no_patch(bad, handles.data(), handles.size() * sizeof(AudioPropagationHandle)) == AudioPropagation::ErrorInvalidHandle, "fOTrQvqzABM invalid system");
        require(libSceAudioPropagationUnknown_fOTrQvqzABM_nid_no_patch(bad, nullptr, 0) == AudioPropagation::ErrorInvalidHandle, "fOTrQvqzABM invalid system null output");
        require(libSceAudioPropagationUnknown_mdH4mANi9GI_nid_no_patch(bad, handles.data(), static_cast<std::uint32_t>(handles.size())) == AudioPropagation::ErrorInvalidHandle, "mdH4mANi9GI invalid system");
        require(libSceAudioPropagationUnknown_mdH4mANi9GI_nid_no_patch(bad, nullptr, 0) == AudioPropagation::ErrorInvalidHandle, "mdH4mANi9GI invalid system null output");
        require(libSceAudioPropagationUnknown_o0gGIp1vd6A_nid_no_patch(bad, handles.data(), static_cast<std::uint32_t>(handles.size())) == AudioPropagation::ErrorInvalidHandle, "o0gGIp1vd6A invalid system");
        require(libSceAudioPropagationUnknown_o0gGIp1vd6A_nid_no_patch(bad, nullptr, 0) == AudioPropagation::ErrorInvalidHandle, "o0gGIp1vd6A invalid system null output");
    }
    require(untouched(), "invalid system gate modified output buffer");
}

void testLiveAndDestroyedSystemGates() {
    const AudioPropagation::SystemOptions options{{AudioPropagation::SystemOptionsId, AudioPropagation::SystemOptionsSize}, 2, 2, 6, 1, 256, 343.0f, 343.0f, 0, {}};
    AudioPropagationSystemMemory memory{};
    memory.desc = {AudioPropagation::SystemMemoryId, AudioPropagation::SystemMemorySize};
    require(sceAudioPropagationSystemQueryMemory(&options, &memory) == 0, "query memory failed");
    std::vector<std::uint64_t> arena((memory.size_cpu_mem + 7) / 8);
    memory.p_cpu_mem = arena.data();
    AudioPropagationHandle system = 0;
    require(sceAudioPropagationSystemCreate(&options, &memory, &system) == 0 && system != 0, "system create failed");

    std::array<AudioPropagationHandle, 4> handles{};
    std::memset(handles.data(), 0xA5, handles.size() * sizeof(AudioPropagationHandle));

    require(libSceAudioPropagationUnknown_TrQMW7ARZXY_nid_no_patch(system, nullptr, 0) == AudioPropagation::ErrorInvalidHandle, "TrQMW7ARZXY null output on live system");
    require(libSceAudioPropagationUnknown_fOTrQvqzABM_nid_no_patch(system, nullptr, 0) == AudioPropagation::ErrorInvalidHandle, "fOTrQvqzABM null output on live system");

    requireNotImplemented([&] { libSceAudioPropagationUnknown_TrQMW7ARZXY_nid_no_patch(system, handles.data(), handles.size() * sizeof(AudioPropagationHandle)); }, "TrQMW7ARZXY live enumeration");
    requireNotImplemented([&] { libSceAudioPropagationUnknown_fOTrQvqzABM_nid_no_patch(system, handles.data(), handles.size() * sizeof(AudioPropagationHandle)); }, "fOTrQvqzABM live enumeration");
    requireNotImplemented([&] { libSceAudioPropagationUnknown_mdH4mANi9GI_nid_no_patch(system, nullptr, 0); }, "mdH4mANi9GI live null count");
    requireNotImplemented([&] { libSceAudioPropagationUnknown_o0gGIp1vd6A_nid_no_patch(system, nullptr, 0); }, "o0gGIp1vd6A live null count");

    require(std::all_of(handles.begin(), handles.end(), [](AudioPropagationHandle value) { return value == 0xA5A5A5A5A5A5A5A5ull; }), "live stubs modified output buffer");

    require(sceAudioPropagationSystemDestroy(system) == 0, "system destroy failed");
    require(libSceAudioPropagationUnknown_TrQMW7ARZXY_nid_no_patch(system, handles.data(), handles.size() * sizeof(AudioPropagationHandle)) == AudioPropagation::ErrorInvalidHandle, "TrQMW7ARZXY destroyed system");
    require(libSceAudioPropagationUnknown_fOTrQvqzABM_nid_no_patch(system, handles.data(), handles.size() * sizeof(AudioPropagationHandle)) == AudioPropagation::ErrorInvalidHandle, "fOTrQvqzABM destroyed system");
    require(libSceAudioPropagationUnknown_mdH4mANi9GI_nid_no_patch(system, handles.data(), static_cast<std::uint32_t>(handles.size())) == AudioPropagation::ErrorInvalidHandle, "mdH4mANi9GI destroyed system");
    require(libSceAudioPropagationUnknown_o0gGIp1vd6A_nid_no_patch(system, handles.data(), static_cast<std::uint32_t>(handles.size())) == AudioPropagation::ErrorInvalidHandle, "o0gGIp1vd6A destroyed system");
}

}

int main() {
    testInvalidSystemGates();
    testLiveAndDestroyedSystemGates();
    std::puts("libSceAudioPropagation InventoryExports tests passed");
    return 0;
}
