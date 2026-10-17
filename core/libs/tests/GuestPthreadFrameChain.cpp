#include "SceTypes.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>

extern "C" {
int APS5_VABI scePthreadCreate(Pthread* thread, const PthreadAttr* attr, PthreadEntry entry, void* arg, const char* name);
int APS5_VABI scePthreadJoin(Pthread thread, void** retval);
}

static constexpr int SCE_OK = 0;
static constexpr std::uintptr_t ChainReach = std::uintptr_t{64} << 10;

static void Require(bool value, const char* what) {
    if (value) return;
    std::fprintf(stderr, "guest pthread frame chain: %s\n", what);
    std::abort();
}

struct Walk {
    int frames = 0;
    bool leftStack = false;
    bool ended = false;
    bool endedOnZeroRecord = false;
};

static Walk g_walk;

[[gnu::noinline]] static void WalkFrameChain() {
    auto* frame = static_cast<std::uintptr_t*>(__builtin_frame_address(0));
    const auto low = reinterpret_cast<std::uintptr_t>(frame);
    for (int depth = 0; depth < 100; ++depth) {
        const auto address = reinterpret_cast<std::uintptr_t>(frame);
        if (address < low || address - low > ChainReach) {
            g_walk.leftStack = true;
            return;
        }
        ++g_walk.frames;
        if (frame[1] == 0) {
            g_walk.ended = true;
            g_walk.endedOnZeroRecord = frame[0] == 0;
            return;
        }
        auto* next = reinterpret_cast<std::uintptr_t*>(frame[0]);
        if (reinterpret_cast<std::uintptr_t>(next) <= address) {
            g_walk.leftStack = true;
            return;
        }
        frame = next;
    }
}

static void* APS5_VABI WalkingEntry(void* arg) {
    WalkFrameChain();
    return arg;
}

int main() {
    Pthread thread = nullptr;
    Require(scePthreadCreate(&thread, nullptr, WalkingEntry, &g_walk, "frame chain") == SCE_OK, "create a guest thread");
    void* result = nullptr;
    Require(scePthreadJoin(thread, &result) == SCE_OK, "join the guest thread");
    Require(result == &g_walk, "the guest entry gets its argument and its return value reaches scePthreadJoin");
    Require(!g_walk.leftStack, "the frame chain of a guest thread stays on its stack and climbs it");
    Require(g_walk.ended, "the frame chain of a guest thread ends at a frame whose return address is 0");
    Require(g_walk.endedOnZeroRecord, "the last frame of a guest thread's chain also has a saved frame pointer of 0");
    Require(g_walk.frames == 3, "the chain is the walker's frame, the guest entry's frame and the zero record");
    std::puts("guest pthread frame chain tests passed");
    return 0;
}
