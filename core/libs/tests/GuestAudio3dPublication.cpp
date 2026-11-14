#include "prx/libSceAudio3d/include/Audio3d.hpp"
#include "prx/libSceAudioOut/include/AudioOut.hpp"
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>

namespace {

constexpr int Handle = 137;
bool nativeLive = false;
bool inject = false;
bool armed = false;
bool reached = false;
bool throwClose = false;
unsigned opens = 0;
unsigned closes = 0;

struct CloseFailure {};

void Require(bool value, const char* message) {
    if (value) return;
    std::fprintf(stderr, "audio3d publication: %s\n", message);
    std::abort();
}

}

void* operator new(std::size_t bytes) {
    if (armed) {
        armed = false;
        reached = true;
        std::fprintf(stderr, "audio3d association stage=post_open_allocation qualified=%d bytes=%zu opens=%u closes=%u\n", nativeLive && opens == 1 && closes == 0, bytes, opens, closes);
        throw std::bad_alloc();
    }
    if (void* result = std::malloc(bytes ? bytes : 1)) return result;
    throw std::bad_alloc();
}

void operator delete(void* pointer) noexcept { std::free(pointer); }
void operator delete(void* pointer, std::size_t) noexcept { std::free(pointer); }

int APS5_VABI sceAudioOutInit() { return 0; }

int APS5_VABI sceAudioOutOpen(int, int, int, std::uint32_t, std::uint32_t, std::uint32_t) {
    Require(!nativeLive, "a native handle cannot be reopened while live");
    nativeLive = true;
    ++opens;
    armed = inject;
    return Handle;
}

int APS5_VABI sceAudioOutClose(int handle) {
    Require(handle == Handle && nativeLive, "close receives the live handle once");
    nativeLive = false;
    ++closes;
    if (throwClose) throw CloseFailure{};
    return 0;
}

int main(int argc, char** argv) {
    Require(argc == 2 && (std::strcmp(argv[1], "normal") == 0 || std::strcmp(argv[1], "throwing-close") == 0), "test mode");
    throwClose = std::strcmp(argv[1], "throwing-close") == 0;
    Require(sceAudio3dInitialize(0) == 0, "initialize");
    Audio3dOpenParameters parameters{};
    sceAudio3dGetDefaultOpenParameters(&parameters);
    std::uint32_t id = 7;
    Require(sceAudio3dPortOpen(0xFF, &parameters, &id) == 0 && id == 0, "open the parent port before fault injection");
    inject = true;
    bool caught = false;
    try {
        sceAudio3dAudioOutOpen(0, 0xFF, 0, 0, parameters.granularity, 48000, 1);
    } catch (const std::bad_alloc&) {
        caught = true;
    }
    armed = inject = false;
    Require(caught && reached && opens == 1, "a publication allocation throws after successful native open");
    std::fprintf(stderr, "audio3d association cleanup qualified=1 opens=%u closes=%u live=%d\n", opens, closes, nativeLive);
    Require(closes == 1 && !nativeLive, "the unpublished native handle is closed once");
    Require(sceAudio3dAudioOutClose(Handle) == static_cast<int>(0x80EA0002), "the failed association was not published");
    throwClose = false;
    const int handle = sceAudio3dAudioOutOpen(0, 0xFF, 0, 0, parameters.granularity, 48000, 1);
    Require(handle == Handle && nativeLive && opens == 2, "the valid association can be retried");
    Require(sceAudio3dAudioOutClose(handle) == 0 && !nativeLive && closes == 2, "ordinary association closes once");
    Require(sceAudio3dPortClose(0) == 0, "close the parent port");
    Require(sceAudio3dTerminate() == 0, "terminate");
    return 0;
}
