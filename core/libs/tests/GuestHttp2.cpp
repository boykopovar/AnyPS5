#include "prx/libkernel/include/Api.hpp"
#include "prx/libSceHttp2/include/Http2.hpp"
#include "SceTypes.hpp"
#include <cstdint>
#include <cstdlib>
#include <stdexcept>

static void Require(bool value) { if (!value) std::abort(); }

static constexpr int InvalidId = static_cast<int>(0x817B1100);

static void RequireSslSetters(int id, int expected) {
    Require(sceHttp2SetMinSslVersion(id, 0) == expected);
    Require(sceHttp2SslEnableOption(id, 0) == expected);
    Require(sceHttp2SslDisableOption(id, 0) == expected);
}

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
    Require(sceHttp2CreateCookieBox(0) == static_cast<int>(0x817b121du));
    Require(sceHttp2CreateCookieBox(0xffff) == static_cast<int>(0x817b121du));
    const int independentBox = sceHttp2CreateCookieBox(0x10000);
    Require(independentBox > 0);
    Require(sceHttp2DeleteCookieBox(independentBox) == 0);
    const int context = sceHttp2Init(1, 1, 0x10000, 4);
    Require(context > 0);
    Http2MemoryPoolStats stats{1, 1, 1, 1};
    Require(sceHttp2GetMemoryPoolStats(context, &stats) == 0);
    Require(stats.pool_size == 0x10000 && stats.max_inuse_size == 0 && stats.current_inuse_size == 0 && stats.reserved == 0);
    const int box = sceHttp2CreateCookieBox(0x10000);
    const int other = sceHttp2CreateCookieBox(0x10000);
    Require(box > 0 && other > 0 && box != other && box != context);
    const int discarded = sceHttp2CreateCookieBox(0x10000);
    Require(discarded > 0 && discarded != box && discarded != other);
    Require(sceHttp2DeleteCookieBox(discarded) == 0);
    Require(sceHttp2DeleteCookieBox(discarded) == InvalidId);
    Require(sceHttp2DeleteCookieBox(context) == InvalidId);
    const int tmpl = sceHttp2CreateTemplate(context, "agent", 2, 0);
    Require(tmpl > 0 && tmpl != box && tmpl != other);
    Require(sceHttp2SetResolveRetry(tmpl, 3) == 0);
    Require(sceHttp2SetCookieBox(tmpl, box) == 0);
    Require(sceHttp2SetCookieBox(tmpl, 0) == 0);
    const int request = sceHttp2CreateRequestWithURL(tmpl, "POST", "https://example.com/", 16);
    Require(request > 0);
    Require(sceHttp2SetCookieBox(request, other) == 0);
    Require(sceHttp2SetRequestNoContentLength(request) == 0);
    Require(Throws([] { sceHttp2WebSocketSetPingInterval(0, 1000000); }));
    Require(sceHttp2CookieFlush(context) == 0);
    Require(sceHttp2SendRequest(request, nullptr, 0) == static_cast<int>(0x80436063));

    RequireSslSetters(0, InvalidId);
    RequireSslSetters(-1, InvalidId);
    RequireSslSetters(0x7fffffff, InvalidId);
    RequireSslSetters(context, 0);
    RequireSslSetters(box, 0);
    RequireSslSetters(other, 0);
    RequireSslSetters(tmpl, 0);
    RequireSslSetters(request, 0);

    const int doomedRequest = sceHttp2CreateRequestWithURL(tmpl, "GET", "https://example.com/doomed", 0);
    Require(doomedRequest > 0);
    Require(sceHttp2DeleteRequest(doomedRequest) == 0);
    RequireSslSetters(doomedRequest, InvalidId);
    Require(sceHttp2DeleteRequest(doomedRequest) == InvalidId);

    const int doomedTemplate = sceHttp2CreateTemplate(context, "agent", 2, 0);
    Require(doomedTemplate > 0);
    const int doomedTemplateRequest = sceHttp2CreateRequestWithURL(doomedTemplate, "GET", "https://example.com/child", 0);
    Require(doomedTemplateRequest > 0);
    Require(sceHttp2DeleteTemplate(doomedTemplate) == 0);
    RequireSslSetters(doomedTemplate, InvalidId);
    RequireSslSetters(doomedTemplateRequest, InvalidId);
    Require(sceHttp2DeleteTemplate(doomedTemplate) == InvalidId);

    const int doomedContext = sceHttp2Init(1, 1, 0x2000, 4);
    Require(doomedContext > 0);
    const int doomedContextBox = sceHttp2CreateCookieBox(0x10001);
    const int doomedContextTemplate = sceHttp2CreateTemplate(doomedContext, "agent", 2, 0);
    Require(doomedContextBox > 0 && doomedContextTemplate > 0);
    const int doomedContextRequest = sceHttp2CreateRequestWithURL(doomedContextTemplate, "GET", "https://example.com/descendant", 0);
    Require(doomedContextRequest > 0);
    RequireSslSetters(doomedContext, 0);
    Require(sceHttp2Term(doomedContext) == 0);
    RequireSslSetters(doomedContext, InvalidId);
    RequireSslSetters(doomedContextBox, 0);
    Require(sceHttp2DeleteCookieBox(doomedContextBox) == 0);
    Require(sceHttp2DeleteCookieBox(doomedContextBox) == InvalidId);
    RequireSslSetters(doomedContextTemplate, InvalidId);
    RequireSslSetters(doomedContextRequest, InvalidId);
    Require(sceHttp2Term(doomedContext) == InvalidId);

    const int freshContext = sceHttp2Init(1, 1, 0x2000, 4);
    Require(freshContext > 0);
    RequireSslSetters(freshContext, 0);
    Require(sceHttp2Term(freshContext) == 0);

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
    Require(sceHttp2SendRequestAsync(request, nullptr, 0, nullptr, nullptr) == 0);
    std::uint32_t wait_timeout = 2000000;
    Require(sceHttp2WaitAsync(request, &result, &wait_timeout, nullptr) == 0);
    Require(result.req_id == request && result.result == static_cast<int>(0x80436063));
    Require(Throws([&] { sceHttp2WaitAsync(request, &result, nullptr, nullptr); }));
    const int invalidRequests[] = {0, -1, 0x7fffffff, context, tmpl, box, doomedRequest, doomedTemplateRequest, doomedContextRequest};
    for (const int invalid : invalidRequests) {
        Require(sceHttp2SendRequestAsync(invalid, nullptr, 0, nullptr, nullptr) == InvalidId);
        Require(Throws([&] { sceHttp2WaitAsync(invalid, &result, nullptr, nullptr); }));
    }
    const int independent = sceHttp2CreateRequestWithURL(tmpl, "GET", "https://example.com/", 0);
    Require(independent > 0 && independent != request);
    Require(sceHttp2SendRequestAsync(request, nullptr, 0, nullptr, nullptr) == 0);
    Require(sceHttp2SendRequestAsync(independent, nullptr, 0, nullptr, nullptr) == 0);
    Require(sceHttp2SendRequestAsync(request, nullptr, 0, nullptr, nullptr) == 0);
    Require(Throws([&] { sceHttp2SendRequestAsync(request, nullptr, 0, nullptr, &tag); }));
    Require(sceHttp2WaitAsync(independent, &result, nullptr, nullptr) == 0);
    Require(result.req_id == independent && result.result == static_cast<int>(0x80436063));
    Require(Throws([&] { sceHttp2WaitAsync(independent, &result, nullptr, nullptr); }));
    for (int i = 0; i < 2; ++i) {
        Require(sceHttp2WaitAsync(request, &result, nullptr, nullptr) == 0);
        Require(result.req_id == request && result.result == static_cast<int>(0x80436063));
    }
    Require(Throws([&] { sceHttp2WaitAsync(request, &result, nullptr, nullptr); }));
    Require(sceHttp2SendRequestAsync(independent, nullptr, 0, nullptr, nullptr) == 0);
    Require(sceHttp2DeleteRequest(independent) == 0);
    Require(sceHttp2SendRequestAsync(independent, nullptr, 0, nullptr, nullptr) == InvalidId);
    Require(Throws([&] { sceHttp2WaitAsync(independent, &result, nullptr, nullptr); }));
    Require(sceKernelDeleteUserEvent(eq, request) == 0);
    Require(Throws([&] { sceHttp2SendRequestAsync(request, nullptr, 0, &option, nullptr); }));
    Require(Throws([&] { sceHttp2WaitAsync(request, &result, nullptr, nullptr); }));
    Require(sceHttp2DeleteRequest(request) == 0);
    Require(sceKernelDeleteEqueue(eq) == 0);
    Require(sceHttp2DeleteCookieBox(box) == 0 && sceHttp2DeleteCookieBox(other) == 0);
    Require(sceHttp2Term(context) == 0);

    KernelEqueue templateEqueue = 0;
    Require(sceKernelCreateEqueue(&templateEqueue, "http2-template") == 0);
    const int cascadeContext = sceHttp2Init(1, 1, 0x3000, 4);
    Require(cascadeContext > 0);
    const int cascadeTemplate = sceHttp2CreateTemplate(cascadeContext, "agent", 2, 0);
    Require(cascadeTemplate > 0);
    const int cascadeRequest = sceHttp2CreateRequestWithURL(cascadeTemplate, "GET", "https://example.com/cascade", 0);
    Require(cascadeRequest > 0);
    Require(sceKernelAddUserEventEdge(templateEqueue, cascadeRequest) == 0);
    Http2AsyncOption cascadeOption{};
    cascadeOption.equeue = templateEqueue;
    cascadeOption.user_event_id = cascadeRequest;
    cascadeOption.user_data = &tag;
    Require(sceHttp2SendRequestAsync(cascadeRequest, nullptr, 0, &cascadeOption, nullptr) == 0);
    KernelEvent cascadeEvent{};
    int cascadeCount = 0;
    Require(sceKernelWaitEqueue(templateEqueue, &cascadeEvent, 1, &cascadeCount, &timeout) == 0 && cascadeCount == 1);
    Require(sceHttp2DeleteTemplate(cascadeTemplate) == 0);
    Require(Throws([&] { sceHttp2WaitAsync(cascadeRequest, &result, nullptr, nullptr); }));
    Require(sceHttp2SendRequestAsync(cascadeRequest, nullptr, 0, &cascadeOption, nullptr) == InvalidId);
    Require(Throws([&] { sceHttp2WaitAsync(cascadeRequest, &result, nullptr, nullptr); }));
    Require(sceHttp2DeleteRequest(cascadeRequest) == InvalidId);
    RequireSslSetters(cascadeRequest, InvalidId);
    Require(sceHttp2Term(cascadeContext) == 0);
    Require(sceKernelDeleteUserEvent(templateEqueue, cascadeRequest) == 0);
    Require(sceKernelDeleteEqueue(templateEqueue) == 0);

    KernelEqueue termEqueue = 0;
    Require(sceKernelCreateEqueue(&termEqueue, "http2-term") == 0);
    const int terminatedContext = sceHttp2Init(1, 1, 0x4000, 4);
    Require(terminatedContext > 0);
    const int terminatedTemplate = sceHttp2CreateTemplate(terminatedContext, "agent", 2, 0);
    Require(terminatedTemplate > 0);
    const int terminatedRequest = sceHttp2CreateRequestWithURL(terminatedTemplate, "GET", "https://example.com/terminated", 0);
    Require(terminatedRequest > 0);
    Require(sceKernelAddUserEventEdge(termEqueue, terminatedRequest) == 0);
    Http2AsyncOption termOption{};
    termOption.equeue = termEqueue;
    termOption.user_event_id = terminatedRequest;
    termOption.user_data = &tag;
    Require(sceHttp2SendRequestAsync(terminatedRequest, nullptr, 0, &termOption, nullptr) == 0);
    KernelEvent termEvent{};
    int termCount = 0;
    Require(sceKernelWaitEqueue(termEqueue, &termEvent, 1, &termCount, &timeout) == 0 && termCount == 1);
    Require(sceHttp2Term(terminatedContext) == 0);
    Require(sceHttp2SendRequestAsync(terminatedRequest, nullptr, 0, &termOption, nullptr) == InvalidId);
    Require(Throws([&] { sceHttp2WaitAsync(terminatedRequest, &result, nullptr, nullptr); }));
    Require(sceHttp2DeleteRequest(terminatedRequest) == InvalidId);
    Require(sceHttp2DeleteTemplate(terminatedTemplate) == InvalidId);
    RequireSslSetters(terminatedContext, InvalidId);
    RequireSslSetters(terminatedRequest, InvalidId);
    Require(sceKernelDeleteUserEvent(termEqueue, terminatedRequest) == 0);
    Require(sceKernelDeleteEqueue(termEqueue) == 0);
}
