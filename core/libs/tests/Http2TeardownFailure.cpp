#include "SceTypes.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <new>
#include <thread>

extern "C" {
int APS5_VABI sceHttp2Init(int, int, std::size_t, int);
int APS5_VABI sceHttp2CreateTemplate(int, const char*, int, int);
int APS5_VABI sceHttp2CreateRequestWithURL(int, const char*, const char*, std::uint64_t);
int APS5_VABI sceHttp2SendRequestAsync(int, const void*, std::size_t, Http2AsyncOption*, void*);
int APS5_VABI sceHttp2WaitAsync(int, Http2AsyncResult*, std::uint32_t*, void*);
int APS5_VABI sceHttp2SetMinSslVersion(int, std::uint32_t);
int APS5_VABI sceHttp2DeleteRequest(int);
int APS5_VABI sceHttp2DeleteTemplate(int);
int APS5_VABI sceHttp2Term(int);
int APS5_VABI sceHttp2CreateCookieBox(std::size_t);
int APS5_VABI sceHttp2DeleteCookieBox(int);
}

static thread_local int allocationBudget = -1;
static thread_local unsigned allocationAttempts = 0;

void* operator new(std::size_t bytes) {
    if (allocationBudget >= 0) {
        ++allocationAttempts;
        if (allocationBudget == 0) {
            allocationBudget = -1;
            throw std::bad_alloc();
        }
        --allocationBudget;
    }
    if (void* memory = std::malloc(bytes ? bytes : 1)) return memory;
    throw std::bad_alloc();
}

void operator delete(void* memory) noexcept { std::free(memory); }
void operator delete(void* memory, std::size_t) noexcept { ::operator delete(memory); }

static void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "%s\n", message);
        std::abort();
    }
}

constexpr int InvalidId = static_cast<int>(0x817B1100);
constexpr int NetworkError = static_cast<int>(0x80436063);

enum class Operation { None, Request, Template, Context };

static const char* Name(Operation operation) {
    switch (operation) {
    case Operation::None: break;
    case Operation::Request: return "DeleteRequest";
    case Operation::Template: return "DeleteTemplate";
    case Operation::Context: return "Term";
    }
    std::abort();
}

struct Tree {
    int context;
    std::array<int, 2> templates;
    std::array<int, 6> requests;

    Tree() : context(sceHttp2Init(1, 1, 0x10000, 4)) {
        Require(context > 0, "context creation failed");
        for (auto& id : templates) {
            id = sceHttp2CreateTemplate(context, "agent", 2, 0);
            Require(id > 0, "template creation failed");
        }
        for (std::size_t index = 0; index < requests.size(); ++index) {
            requests[index] = sceHttp2CreateRequestWithURL(templates[index / 3], "GET", "https://example.invalid/", 0);
            Require(requests[index] > 0, "request creation failed");
            for (int pending = 0; pending < 3; ++pending)
                Require(sceHttp2SendRequestAsync(requests[index], nullptr, 0, nullptr, nullptr) == 0, "completion enqueue failed");
        }
    }

    int Delete(Operation operation) const {
        switch (operation) {
        case Operation::None: break;
        case Operation::Request: return sceHttp2DeleteRequest(requests[0]);
        case Operation::Template: return sceHttp2DeleteTemplate(templates[0]);
        case Operation::Context: return sceHttp2Term(context);
        }
        std::abort();
    }
};

static bool TakeCompletion(int request) {
    Http2AsyncResult result{};
    result.req_id = -77;
    result.result = -88;
    try {
        Require(sceHttp2WaitAsync(request, &result, nullptr, nullptr) == 0, "completion wait failed");
        Require(result.req_id == request && result.result == NetworkError, "completion contents changed");
        return true;
    } catch (const std::exception&) {
        Require(result.req_id == -77 && result.result == -88, "refused wait changed its output");
        return false;
    }
}

static bool CheckTree(const Tree& tree, Operation operation) {
    bool correct = true;
    for (std::size_t index = 0; index < tree.requests.size(); ++index) {
        const bool removed = operation == Operation::Context || (operation == Operation::Template ? index < 3 : operation == Operation::Request && index == 0);
        const int status = sceHttp2SetMinSslVersion(tree.requests[index], 0);
        const bool live = status == 0;
        if (status != (removed ? InvalidId : 0)) correct = false;
        if (removed) {
            if (TakeCompletion(tree.requests[index])) {
                std::fprintf(stderr, "completion survived deletion: request=%d live=%d\n", tree.requests[index], live);
                correct = false;
            }
        } else {
            for (int pending = 0; pending < 3; ++pending)
                if (!TakeCompletion(tree.requests[index])) correct = false;
            if (TakeCompletion(tree.requests[index])) {
                std::fprintf(stderr, "completion survived deletion: request=%d live=%d\n", tree.requests[index], live);
                correct = false;
            }
        }
    }
    const int contextStatus = operation == Operation::Context ? InvalidId : 0;
    if (sceHttp2SetMinSslVersion(tree.context, 0) != contextStatus) correct = false;
    for (std::size_t index = 0; index < tree.templates.size(); ++index) {
        const bool removed = operation == Operation::Context || (operation == Operation::Template && index == 0);
        if (sceHttp2SetMinSslVersion(tree.templates[index], 0) != (removed ? InvalidId : 0)) correct = false;
    }
    return correct;
}

static unsigned AllocationFailureCases(Operation operation) {
    unsigned failures = 0;
    for (int budget = 0; budget < 16; ++budget) {
        Tree tree;
        Tree independent;
        const int cookieBox = sceHttp2CreateCookieBox(0x10000);
        Require(cookieBox > 0, "cookie box creation failed");
        allocationAttempts = 0;
        allocationBudget = budget;
        bool threw = false;
        int result = -1;
        try { result = tree.Delete(operation); }
        catch (const std::bad_alloc&) { threw = true; }
        allocationBudget = -1;
        const bool correct = CheckTree(tree, operation);
        if (threw || result != 0 || !correct || allocationAttempts != 0) {
            std::fprintf(stderr, "%s budget=%d threw=%d result=%d ownership=%d allocation attempts=%u\n",
                         Name(operation), budget, threw, result, correct, allocationAttempts);
            ++failures;
        }
        Require(CheckTree(independent, Operation::None), "independent context changed");
        Require(sceHttp2SetMinSslVersion(cookieBox, 0) == 0, "unrelated cookie box was deleted");
        Require(sceHttp2DeleteCookieBox(cookieBox) == 0, "cookie box cleanup failed");
        Require(sceHttp2Term(independent.context) == 0, "independent context cleanup failed");
        if (sceHttp2SetMinSslVersion(tree.context, 0) == 0)
            Require(sceHttp2Term(tree.context) == 0, "context cleanup failed");
    }
    return failures;
}

static void ConcurrentCompletionCases(Operation operation) {
    for (int iteration = 0; iteration < 64; ++iteration) {
        Tree tree;
        std::atomic<bool> start{false};
        std::thread producer([&] {
            while (!start.load(std::memory_order_acquire)) std::this_thread::yield();
            for (int pending = 0; pending < 16; ++pending) {
                const int result = sceHttp2SendRequestAsync(tree.requests[0], nullptr, 0, nullptr, nullptr);
                Require(result == 0 || result == InvalidId, "concurrent submission returned an unexpected result");
            }
        });
        start.store(true, std::memory_order_release);
        Require(tree.Delete(operation) == 0, "concurrent deletion failed");
        producer.join();
        Require(CheckTree(tree, operation), "concurrent deletion left a live object or completion");
        Require(tree.Delete(operation) == InvalidId, "repeated deletion succeeded");
        if (operation != Operation::Context) Require(sceHttp2Term(tree.context) == 0, "concurrent fixture cleanup failed");
    }
}

int main() {
    unsigned failures = 0;
    for (const auto operation : {Operation::Request, Operation::Template, Operation::Context}) {
        failures += AllocationFailureCases(operation);
        ConcurrentCompletionCases(operation);
    }
    std::printf("48 allocation-pressure lifecycles, 192 concurrent lifecycles, %u failures\n", failures);
    return failures == 0 ? 0 : 1;
}
