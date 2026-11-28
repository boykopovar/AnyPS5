#include "prx/libSceHttp2/include/Http2.hpp"
#include "prx/libSceHttp2/Transport.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <mutex>
#include <thread>

namespace {

using Clock = std::chrono::steady_clock;

constexpr int TimedOutError = static_cast<int>(0x817b1068);
constexpr int DnsLookupError = static_cast<int>(0x804101e6);

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "HTTP2 DNS wait: %s\n", message);
    std::abort();
}

std::mutex gateMutex;
std::condition_variable gateCv;
bool hookEntered = false;
bool hookRelease = false;
std::atomic<int> activeHooks{0};

struct ActiveHookGuard {
    ActiveHookGuard() { activeHooks.fetch_add(1, std::memory_order_acq_rel); }
    ~ActiveHookGuard() {
        {
            std::lock_guard lock(gateMutex);
            activeHooks.fetch_sub(1, std::memory_order_acq_rel);
        }
        gateCv.notify_all();
    }
};

int BlockingLookup(const char*) {
    ActiveHookGuard active;
    std::unique_lock lock(gateMutex);
    hookEntered = true;
    gateCv.notify_all();
    gateCv.wait(lock, [] { return hookRelease; });
    return -1;
}

void ResetLookupGate() {
    std::unique_lock lock(gateMutex);
    hookRelease = true;
    gateCv.notify_all();
    gateCv.wait(lock, [] { return activeHooks.load(std::memory_order_acquire) == 0; });
    hookEntered = false;
    hookRelease = false;
}

} // namespace

int main() {
    const int context = sceHttp2Init(1, 1, 0x10000, 4);
    Require(context > 0, "sceHttp2Init failed");
    const int tmpl = sceHttp2CreateTemplate(context, "agent", 3, 0);
    Require(tmpl > 0, "sceHttp2CreateTemplate failed");
    Require(sceHttp2SetResolveRetry(tmpl, 0) == 0, "sceHttp2SetResolveRetry failed");

    Http2SetLookupHook_nid_no_patch(BlockingLookup);

    auto shortest = Clock::duration::max();
    for (int attempt = 0; attempt < 5; ++attempt) {
        ResetLookupGate();
        const int request = sceHttp2CreateRequestWithURL(tmpl, "GET", "http://example.invalid/", 0);
        Require(request > 0, "sceHttp2CreateRequestWithURL failed");
        Require(sceHttp2SetTimeOut(request, 2000) == 0, "sceHttp2SetTimeOut failed");

        const auto start = Clock::now();
        const int result = sceHttp2SendRequest(request, nullptr, 0);
        const auto elapsed = Clock::now() - start;

        ResetLookupGate();
        Require(result == TimedOutError, "expected TimedOut during blocked DNS lookup");
        Require(elapsed >= std::chrono::milliseconds(2), "timeout returned early");
        shortest = std::min(shortest, elapsed);
        Require(sceHttp2DeleteRequest(request) == 0, "sceHttp2DeleteRequest failed");
    }
    Require(shortest < std::chrono::milliseconds(10), "2 ms DNS wait hit a coarse timer tick every time");

    ResetLookupGate();
    const int wakeRequest = sceHttp2CreateRequestWithURL(tmpl, "GET", "http://example.invalid/", 0);
    Require(wakeRequest > 0, "create wakeRequest failed");
    Require(sceHttp2SetTimeOut(wakeRequest, 5000000) == 0, "set 5 s timeout failed");
    std::thread releaser([] {
        std::unique_lock lock(gateMutex);
        gateCv.wait(lock, [] { return hookEntered; });
        hookRelease = true;
        gateCv.notify_all();
    });
    const auto wakeStart = Clock::now();
    const int wakeResult = sceHttp2SendRequest(wakeRequest, nullptr, 0);
    const auto wakeElapsed = Clock::now() - wakeStart;
    releaser.join();
    ResetLookupGate();
    Require(wakeResult == DnsLookupError, "expected DNS failure error when lookup completes");
    Require(wakeElapsed < std::chrono::seconds(1), "DNS completion wakeup stalled");
    Require(sceHttp2DeleteRequest(wakeRequest) == 0, "delete wakeRequest failed");

    Http2SetLookupHook_nid_no_patch(nullptr);
    Require(sceHttp2DeleteTemplate(tmpl) == 0, "sceHttp2DeleteTemplate failed");
    Require(sceHttp2Term(context) == 0, "sceHttp2Term failed");
    return 0;
}
