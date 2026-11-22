#define SDL_MAIN_HANDLED
#include "prx/libSceAudioOut/include/AudioOutSdl.hpp"
#include "SDL.h"

#include <cerrno>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <mutex>
#include <thread>

namespace {
std::mutex stateMutex;
std::condition_variable stateChanged;
int calls = 0;
int wasInitCalls = 0;
int audioRefCount = 0;
int active = 0;
int peak = 0;
bool secondEntered = false;
bool secondReady = false;
bool validFlags = true;
}

extern "C" Uint32 SDLCALL SDL_WasInit(Uint32 flags) {
    std::lock_guard lock(stateMutex);
    errno = ENOENT;
    validFlags = validFlags && flags == SDL_INIT_AUDIO;
    ++wasInitCalls;
    return audioRefCount > 0 ? SDL_INIT_AUDIO : 0;
}

extern "C" int SDLCALL SDL_InitSubSystem(Uint32 flags) {
    std::unique_lock lock(stateMutex);
    errno = EINVAL;
    validFlags = validFlags && flags == SDL_INIT_AUDIO;
    const int call = ++calls;
    ++active;
    if (active > peak) peak = active;
    stateChanged.notify_all();
    if (call == 1) {
        stateChanged.wait(lock, [] { return secondReady; });
        stateChanged.wait_for(lock, std::chrono::milliseconds(500), [] { return secondEntered; });
    } else {
        secondEntered = true;
        stateChanged.notify_all();
    }
    --active;
    if (call == 1) {
        return -1;
    }
    ++audioRefCount;
    return 0;
}

int main() {
    int firstResult = 99;
    int secondResult = 99;
    int firstErrno = 0;
    int secondErrno = 0;
    std::thread first([&] {
        errno = 71;
        firstResult = InitializeSdlAudio_nid_no_patch();
        firstErrno = errno;
    });
    {
        std::unique_lock lock(stateMutex);
        stateChanged.wait(lock, [] { return calls == 1; });
    }
    std::thread second([&] {
        {
            std::lock_guard lock(stateMutex);
            secondReady = true;
            stateChanged.notify_all();
        }
        errno = 71;
        secondResult = InitializeSdlAudio_nid_no_patch();
        secondErrno = errno;
    });
    first.join();
    second.join();
    errno = 71;
    const int thirdResult = InitializeSdlAudio_nid_no_patch();
    const int thirdErrno = errno;
    if (peak != 1 || calls != 2 || wasInitCalls != 3 || audioRefCount != 1 || active != 0 || !validFlags || firstResult != -1 || secondResult != 0 || thirdResult != 0 || firstErrno != 71 || secondErrno != 71 || thirdErrno != 71) {
        std::fprintf(stderr, "audio initialization mismatch: peak=%d calls=%d wasInitCalls=%d refCount=%d results=%d,%d,%d errnos=%d,%d,%d\n", peak, calls, wasInitCalls, audioRefCount, firstResult, secondResult, thirdResult, firstErrno, secondErrno, thirdErrno);
        return 1;
    }
    std::puts("Serialized audio initialization: PASS");
    return 0;
}
