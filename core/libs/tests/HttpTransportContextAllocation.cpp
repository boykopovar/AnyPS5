#include "SceTypes.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceHttp/src/HttpErrors.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <new>

extern "C" {
int APS5_VABI sceHttpInit_nid_postfix(int, int, std::uint64_t);
int APS5_VABI sceHttpTerm_nid_postfix(int);
int APS5_VABI sceHttpAuthCacheFlush(int);
int APS5_VABI sceHttpGetNonblock(int, int*);
int APS5_VABI sceHttpCreateTemplate(int, const char*, int, int);
int APS5_VABI sceHttpDeleteTemplate(int);
}

namespace {

int g_allocationBudget = -1;

void Require(const bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "%s\n", message);
        std::abort();
    }
}

bool Inactive() {
    int nonblock = -1;
    return sceHttpAuthCacheFlush(-1) == ERROR_BEFORE_INIT && sceHttpGetNonblock(-1, &nonblock) == ERROR_BEFORE_INIT;
}

void* operatorNew(const std::size_t size) {
    if (g_allocationBudget >= 0) {
        if (g_allocationBudget == 0) {
            g_allocationBudget = -1;
            throw std::bad_alloc();
        }
        --g_allocationBudget;
    }
    if (void* memory = std::malloc(size ? size : 1)) return memory;
    throw std::bad_alloc();
}

int CheckInitWithoutExistingContext() {
    unsigned rejected = 0;
    for (int budget = 0; budget < 16; ++budget) {
        Require(Inactive(), "HTTP service was active before init fault injection");
        g_allocationBudget = budget;
        bool failed = false;
        int context = 0;
        try {
            context = sceHttpInit_nid_postfix(0, 0, 0x10000);
        } catch (const std::bad_alloc&) {
            failed = true;
        }
        g_allocationBudget = -1;
        rejected += failed;

        if (failed) {
            Require(Inactive(), "failed HTTP init published context or options state");
            const int probeContext = sceHttpInit_nid_postfix(0, 0, 0x10000);
            Require(probeContext > 0, "probe HTTP context creation failed");
            Require(sceHttpTerm_nid_postfix(probeContext) == 0 && Inactive(), "probe context cleanup failed");
        } else {
            Require(context > 0 && sceHttpAuthCacheFlush(context) == 0, "successful HTTP init did not publish its context");
            Require(sceHttpTerm_nid_postfix(context) == 0 && Inactive(), "successful HTTP context cleanup failed");
        }
    }
    Require(rejected != 0 && Inactive(), "HTTP init fault injection did not reject an allocation");
    std::printf("HTTP transport init without existing context: 16 allocation budgets, %u rejected\n", rejected);
    return 0;
}

int CheckInitWithExistingContext() {
    unsigned rejected = 0;
    for (int budget = 0; budget < 16; ++budget) {
        const int existing = sceHttpInit_nid_postfix(0, 0, 0x10000);
        Require(existing > 0, "existing HTTP context creation failed");
        g_allocationBudget = budget;
        bool failed = false;
        int created = 0;
        try {
            created = sceHttpInit_nid_postfix(0, 0, 0x10000);
        } catch (const std::bad_alloc&) {
            failed = true;
        }
        g_allocationBudget = -1;
        rejected += failed;

        if (failed) {
            Require(sceHttpAuthCacheFlush(existing) == 0, "failed HTTP init damaged an existing context");
            const int failedContext = existing + 1;
            Require(sceHttpCreateTemplate(failedContext, "allocation-test", 1, 0) == ERROR_INVALID_ID,
                "failed HTTP init left a transport context registered");
            created = sceHttpInit_nid_postfix(0, 0, 0x10000);
            Require(created > failedContext && sceHttpAuthCacheFlush(created) == 0, "HTTP init retry did not create a valid context");
        } else {
            Require(created == existing + 1 && sceHttpAuthCacheFlush(created) == 0, "successful HTTP init did not register its context");
        }

        const int templateId = sceHttpCreateTemplate(created, "allocation-test", 1, 0);
        Require(templateId > 0, "template creation failed for a live HTTP transport context");
        Require(sceHttpDeleteTemplate(templateId) == 0, "template cleanup failed");
        Require(sceHttpTerm_nid_postfix(created) == 0, "new context cleanup failed");
        Require(sceHttpAuthCacheFlush(existing) == 0, "new context cleanup damaged an existing context");
        Require(sceHttpTerm_nid_postfix(existing) == 0 && Inactive(), "existing context cleanup failed");
    }
    Require(rejected != 0 && Inactive(), "HTTP init fault injection did not reject an allocation with a live context");
    std::printf("HTTP transport init with existing context: 16 allocation budgets, %u rejected\n", rejected);
    return 0;
}

}

void* operator new(const std::size_t size) { return operatorNew(size); }
void* operator new[](const std::size_t size) { return operatorNew(size); }
void operator delete(void* memory) noexcept { std::free(memory); }
void operator delete[](void* memory) noexcept { std::free(memory); }
void operator delete(void* memory, std::size_t) noexcept { std::free(memory); }
void operator delete[](void* memory, std::size_t) noexcept { std::free(memory); }

int main() {
    if (const int result = CheckInitWithoutExistingContext()) return result;
    return CheckInitWithExistingContext();
}
