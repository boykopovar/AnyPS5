#include "SceTypes.hpp"
#include <cstdint>
#include <cstdlib>
#include <stdexcept>

extern "C" {
int APS5_VABI sceHttp2Init(int, int, std::size_t, int);
int APS5_VABI sceHttp2CreateTemplate(int, const char*, int, int);
int APS5_VABI sceHttp2CreateRequestWithURL(int, const char*, const char*, std::uint64_t);
int APS5_VABI sceHttp2CreateCookieBox(int);
int APS5_VABI sceHttp2SetCookieBox(int, int);
int APS5_VABI sceHttp2CookieFlush(int);
int APS5_VABI sceHttp2SetRequestNoContentLength(int);
int APS5_VABI sceHttp2SendRequest(int, const void*, std::size_t);
int APS5_VABI sceHttp2SendRequestAsync(int, const void*, std::size_t, Http2AsyncOption*, void*);
int APS5_VABI sceHttp2WaitAsync(int, Http2AsyncResult*, std::uint32_t*, void*);
int APS5_VABI sceHttp2DeleteRequest(int);
int APS5_VABI sceHttp2Term(int);
int APS5_VABI sceKernelCreateEqueue(KernelEqueue* eq, const char* name);
int APS5_VABI sceKernelDeleteEqueue(KernelEqueue eq);
int APS5_VABI sceKernelWaitEqueue(KernelEqueue eq, KernelEvent* ev, int num, int* out, const KernelUseconds* timo);
int APS5_VABI sceKernelAddUserEventEdge(KernelEqueue eq, int id);
int APS5_VABI sceKernelDeleteUserEvent(KernelEqueue eq, int id);
uintptr_t APS5_VABI sceKernelGetEventId(const KernelEvent* ev);
void* APS5_VABI sceKernelGetEventUserData(const KernelEvent* ev);
}

struct Http2MemoryPoolStats {
    std::size_t pool_size;
    std::size_t max_inuse_size;
    std::size_t current_inuse_size;
    std::int32_t reserved;
};

extern "C" {
int APS5_VABI sceHttp2GetMemoryPoolStats(int, Http2MemoryPoolStats*);
int APS5_VABI sceHttp2SetResolveRetry(int, std::int32_t);
}

static void Require(bool value) { if (!value) std::abort(); }

template <typename TCall>
static bool Throws(TCall call) {
    try {
        call();
    } catch (const std::exception&) {
        return true;
    }
    return false;
}

int main() {
    const int context = sceHttp2Init(1, 1, 0x10000, 4);
    Require(context > 0);
    Http2MemoryPoolStats stats{1, 1, 1, 1};
    Require(sceHttp2GetMemoryPoolStats(context, &stats) == 0);
    Require(stats.pool_size == 0x10000 && stats.max_inuse_size == 0 && stats.current_inuse_size == 0 && stats.reserved == 0);
    const int box = sceHttp2CreateCookieBox(context);
    const int other = sceHttp2CreateCookieBox(context);
    Require(box > 0 && other > 0 && box != other && box != context);
    const int tmpl = sceHttp2CreateTemplate(context, "agent", 2, 0);
    Require(tmpl > 0 && tmpl != box && tmpl != other);
    Require(sceHttp2SetResolveRetry(tmpl, 3) == 0);
    Require(sceHttp2SetCookieBox(tmpl, box) == 0);
    Require(sceHttp2SetCookieBox(tmpl, 0) == 0);
    const int request = sceHttp2CreateRequestWithURL(tmpl, "POST", "https://example.com/", 16);
    Require(request > 0);
    Require(sceHttp2SetCookieBox(request, other) == 0);
    Require(sceHttp2SetRequestNoContentLength(request) == 0);
    Require(sceHttp2CookieFlush(context) == 0);
    Require(sceHttp2SendRequest(request, nullptr, 0) == static_cast<int>(0x80436063));

    KernelEqueue eq = 0;
    Require(sceKernelCreateEqueue(&eq, "http2") == 0);
    Require(sceKernelAddUserEventEdge(eq, request) == 0);
    int tag = 0;
    Http2AsyncOption option{};
    option.equeue = eq;
    option.user_event_id = request;
    option.user_data = &tag;
    Require(sceHttp2SendRequestAsync(request, nullptr, 0, &option, nullptr) == 0);
    KernelEvent event{};
    int count = 0;
    const KernelUseconds timeout = 1000000;
    Require(sceKernelWaitEqueue(eq, &event, 1, &count, &timeout) == 0 && count == 1);
    Require(sceKernelGetEventId(&event) == static_cast<uintptr_t>(request));
    Require(sceKernelGetEventUserData(&event) == &tag);
    Http2AsyncResult result{};
    Require(sceHttp2WaitAsync(request, &result, nullptr, nullptr) == 0);
    Require(result.req_id == request && result.result == static_cast<int>(0x80436063));
    Require(Throws([&] { sceHttp2WaitAsync(request, &result, nullptr, nullptr); }));
    Require(Throws([&] { sceHttp2SendRequestAsync(request, nullptr, 0, nullptr, nullptr); }));
    Require(sceKernelDeleteUserEvent(eq, request) == 0);
    Require(Throws([&] { sceHttp2SendRequestAsync(request, nullptr, 0, &option, nullptr); }));
    Require(Throws([&] { sceHttp2WaitAsync(request, &result, nullptr, nullptr); }));
    Require(sceHttp2DeleteRequest(request) == 0);
    Require(sceKernelDeleteEqueue(eq) == 0);
    Require(sceHttp2Term(context) == 0);
}
