#include "prx/libSceHttp2/include/Http2.hpp"
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <new>
#include <thread>

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
            id = sceHttp2CreateTemplate(context, "agent", 3, 0);
            Require(id > 0, "template creation failed");
        }
        for (std::size_t index = 0; index < requests.size(); ++index) {
            requests[index] = sceHttp2CreateRequestWithURL(templates[index / 3], "GET", "https://example.invalid/", 0);
            Require(requests[index] > 0, "request creation failed");
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

static bool ContextExists(int context) {
    const int probe = sceHttp2CreateTemplate(context, "probe", 3, 0);
    if (probe == InvalidId) return false;
    Require(probe > 0, "context probe failed");
    Require(sceHttp2DeleteTemplate(probe) == 0, "context probe cleanup failed");
    return true;
}

static bool CheckTree(const Tree& tree, Operation operation) {
    bool correct = true;
    for (std::size_t index = 0; index < tree.requests.size(); ++index) {
        const bool removed = operation == Operation::Context || (operation == Operation::Template ? index < 3 : operation == Operation::Request && index == 0);
        const int status = sceHttp2SslEnableOption(tree.requests[index], 0);
        if (status != (removed ? InvalidId : 0)) correct = false;
    }
    if (ContextExists(tree.context) != (operation != Operation::Context)) correct = false;
    for (std::size_t index = 0; index < tree.templates.size(); ++index) {
        const bool removed = operation == Operation::Context || (operation == Operation::Template && index == 0);
        if (sceHttp2SslEnableOption(tree.templates[index], 0) != (removed ? InvalidId : 0)) correct = false;
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
        Http2CookieStats stats{};
        Require(sceHttp2GetCookieStats(cookieBox, &stats) == 0, "unrelated cookie box was deleted");
        Require(sceHttp2DeleteCookieBox(cookieBox) == 0, "cookie box cleanup failed");
        Require(sceHttp2Term(independent.context) == 0, "independent context cleanup failed");
        if (ContextExists(tree.context))
            Require(sceHttp2Term(tree.context) == 0, "context cleanup failed");
    }
    return failures;
}

static void ConcurrentCreationCases(Operation operation) {
    for (int iteration = 0; iteration < 64; ++iteration) {
        Tree tree;
        std::array<int, 16> created{};
        std::atomic<bool> start{false};
        std::thread producer([&] {
            while (!start.load(std::memory_order_acquire)) std::this_thread::yield();
            for (auto& request : created) {
                request = sceHttp2CreateRequestWithURL(tree.templates[0], "GET", "https://example.invalid/", 0);
                Require(request > 0 || request == InvalidId, "concurrent creation returned an unexpected result");
            }
        });
        start.store(true, std::memory_order_release);
        Require(tree.Delete(operation) == 0, "concurrent deletion failed");
        producer.join();
        Require(CheckTree(tree, operation), "concurrent deletion changed object ownership");
        for (const int request : created) {
            if (request == InvalidId) {
                Require(operation != Operation::Request, "request deletion removed its parent template");
                continue;
            }
            const int expected = operation == Operation::Request ? 0 : InvalidId;
            Require(sceHttp2SslEnableOption(request, 0) == expected, "concurrent creation escaped its parent's teardown");
        }
        Require(tree.Delete(operation) == InvalidId, "repeated deletion succeeded");
        if (operation != Operation::Context) Require(sceHttp2Term(tree.context) == 0, "concurrent fixture cleanup failed");
    }
}

int main() {
    unsigned failures = 0;
    for (const auto operation : {Operation::Request, Operation::Template, Operation::Context}) {
        failures += AllocationFailureCases(operation);
        ConcurrentCreationCases(operation);
    }
    std::printf("48 allocation-pressure lifecycles, 192 concurrent lifecycles, %u failures\n", failures);
    return failures == 0 ? 0 : 1;
}
