#include "SceTypes.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceHttp/src/HttpErrors.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>

extern "C" {
int APS5_VABI sceHttpInit_nid_postfix(int, int, std::uint64_t);
int APS5_VABI sceHttpTerm_nid_postfix(int);
int APS5_VABI sceHttpAuthCacheFlush(int);
int APS5_VABI sceHttpGetNonblock(int, int*);
int APS5_VABI sceHttpCreateEpoll(int, HttpEpollHandle*);
int APS5_VABI sceHttpDestroyEpoll(int, HttpEpollHandle);
int APS5_VABI sceHttpWaitRequest(HttpEpollHandle, HttpNBEvent*, int, int);
}

static int allocationBudget = -1;
static unsigned allocationAttempts = 0;

void* operator new(std::size_t size) {
    if (allocationBudget >= 0) {
        ++allocationAttempts;
        if (allocationBudget == 0) {
            allocationBudget = -1;
            throw std::bad_alloc();
        }
        --allocationBudget;
    }
    if (void* memory = std::malloc(size ? size : 1)) return memory;
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

static bool Inactive() {
    int enabled = -1;
    return sceHttpAuthCacheFlush(-1) == ERROR_BEFORE_INIT && sceHttpGetNonblock(-1, &enabled) == ERROR_BEFORE_INIT;
}

static int InitCases(bool withExistingContext) {
    unsigned rejectedCases = 0;
    for (int budget = 0; budget < 8; ++budget) {
        Require(Inactive(), "service was already initialized");
        const int existing = withExistingContext ? sceHttpInit_nid_postfix(0, 0, 0x10000) : 0;
        Require(!withExistingContext || existing > 0, "existing context creation failed");
        allocationAttempts = 0;
        allocationBudget = budget;
        bool rejected = false;
        int context = 0;
        try { context = sceHttpInit_nid_postfix(0, 0, 0x10000); }
        catch (const std::bad_alloc&) { rejected = true; }
        allocationBudget = -1;
        rejectedCases += rejected;
        if (rejected) {
            if (withExistingContext) {
                Require(sceHttpAuthCacheFlush(existing) == 0, "failed init damaged an existing context");
                Require(sceHttpTerm_nid_postfix(existing) == 0, "existing context cleanup failed");
            }
            if (!Inactive()) {
                std::fprintf(stderr, "init budget=%d attempts=%u published a context after failure\n", budget, allocationAttempts);
                return 1;
            }
        } else {
            Require(context > 0 && sceHttpAuthCacheFlush(context) == 0, "successful context is not live");
            Require(sceHttpTerm_nid_postfix(context) == 0, "context cleanup failed");
            if (withExistingContext) {
                Require(sceHttpAuthCacheFlush(existing) == 0, "new context cleanup damaged an existing context");
                Require(sceHttpTerm_nid_postfix(existing) == 0, "existing context cleanup failed");
            }
            Require(Inactive(), "context remained after cleanup");
        }
    }
    std::printf("HTTP init existing=%d: 8 allocation budgets, %u rejected, no partial publication\n", withExistingContext, rejectedCases);
    return 0;
}

static int CreateCases() {
    unsigned failures = 0, rejectedCases = 0;
    for (int budget = 0; budget < 8; ++budget) {
        const int context = sceHttpInit_nid_postfix(0, 0, 0x10000);
        Require(context > 0, "context creation failed");
        alignas(8) std::uint64_t sentinel = 0;
        const auto unchanged = reinterpret_cast<HttpEpollHandle>(&sentinel);
        HttpEpollHandle handle = unchanged;
        allocationAttempts = 0;
        allocationBudget = budget;
        bool rejected = false;
        int result = 0;
        try { result = sceHttpCreateEpoll(context, &handle); }
        catch (const std::bad_alloc&) { rejected = true; }
        allocationBudget = -1;
        rejectedCases += rejected;
        if (rejected) {
            if (handle != unchanged) {
                std::fprintf(stderr, "epoll budget=%d attempts=%u changed output after failure\n", budget, allocationAttempts);
                ++failures;
            }
        } else {
            Require(result == 0 && handle != nullptr && handle != unchanged, "successful epoll was not published");
            HttpNBEvent event{};
            Require(sceHttpWaitRequest(handle, &event, 1, 0) == 0, "published epoll is invalid");
            Require(sceHttpDestroyEpoll(context, handle) == 0, "epoll deletion failed");
        }
        Require(sceHttpAuthCacheFlush(context) == 0, "epoll creation lost its context");
        Require(sceHttpTerm_nid_postfix(context) == 0 && Inactive(), "context cleanup failed");
    }
    std::printf("HTTP epoll creation: 8 allocation budgets, %u rejected, %u changed failed outputs\n", rejectedCases, failures);
    return failures == 0 ? 0 : 1;
}

static int TermCase(int budget) {
    const int context = sceHttpInit_nid_postfix(0, 0, 0x10000);
    Require(context > 0, "context creation failed");
    std::array<HttpEpollHandle, 3> epolls{};
    for (auto& handle : epolls) Require(sceHttpCreateEpoll(context, &handle) == 0, "epoll creation failed");
    allocationAttempts = 0;
    allocationBudget = budget;
    bool rejected = false;
    int result = 0;
    try { result = sceHttpTerm_nid_postfix(context); }
    catch (const std::bad_alloc&) { rejected = true; }
    allocationBudget = -1;
    if (rejected) {
        if (sceHttpAuthCacheFlush(context) != 0) {
            std::fprintf(stderr, "term budget=%d attempts=%u lost the context before cleanup completed\n", budget, allocationAttempts);
            return 1;
        }
        HttpNBEvent event{};
        for (auto handle : epolls) Require(sceHttpWaitRequest(handle, &event, 1, 0) == 0, "failed termination lost an epoll");
        Require(sceHttpTerm_nid_postfix(context) == 0, "termination retry failed");
    } else {
        Require(result == 0 && Inactive(), "successful termination left a context live");
    }
    const int freshContext = sceHttpInit_nid_postfix(0, 0, 0x10000);
    Require(freshContext > 0, "context recreation failed");
    for (auto handle : epolls)
        Require(sceHttpDestroyEpoll(freshContext, handle) == ERROR_INVALID_ID, "termination retained an epoll");
    Require(sceHttpTerm_nid_postfix(freshContext) == 0 && Inactive(), "fresh context cleanup failed");
    std::printf("HTTP termination: budget=%d rejected=%d attempts=%u, lifecycle consistent\n", budget, rejected, allocationAttempts);
    return 0;
}

int main(int argc, char** argv) {
    if (argc == 2 && std::strcmp(argv[1], "init") == 0) return InitCases(false);
    if (argc == 2 && std::strcmp(argv[1], "init-live") == 0) return InitCases(true);
    if (argc == 2 && std::strcmp(argv[1], "create") == 0) return CreateCases();
    if (argc == 3 && std::strcmp(argv[1], "term") == 0) return TermCase(std::atoi(argv[2]));
    return 1;
}
