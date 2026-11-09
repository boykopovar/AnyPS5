#define SDL_MAIN_HANDLED
#include "prx/libSceAudioOut/include/AudioOutSdl.hpp"
#include "SDL.h"

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
int active = 0;
int peak = 0;
bool secondEntered = false;
bool secondReady = false;
bool validFlags = true;
}

extern "C" int SDLCALL SDL_InitSubSystem(Uint32 flags) {
    std::unique_lock lock(stateMutex);
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
    return call == 1 ? -1 : 0;
}

int main() {
    int firstResult = 99;
    int secondResult = 99;
    std::thread first([&] { firstResult = InitializeSdlAudio_nid_no_patch(); });
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
        secondResult = InitializeSdlAudio_nid_no_patch();
    });
    first.join();
    second.join();
    if (peak != 1 || calls != 2 || active != 0 || !validFlags || firstResult != -1 || secondResult != 0) {
        std::fprintf(stderr, "audio initialization overlap or return-value mismatch: peak=%d calls=%d results=%d,%d\n", peak, calls, firstResult, secondResult);
        return 1;
    }
    std::puts("Serialized audio initialization: PASS");
    return 0;
}
