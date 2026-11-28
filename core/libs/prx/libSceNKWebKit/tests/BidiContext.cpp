#include "prx/libc/include/General.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

using NKWebKit::WebCoreAbi::BidiContext;
using NKWebKit::WebCoreAbi::BidiContextRef;

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "bidi context check failed at line %d\n", line);
        std::fflush(stderr);
        std::_Exit(1);
    }
}
#define Require(value) Check((value), __LINE__)

constexpr auto create = &_ZN7WebCore11BidiContext6createEh14UCharDirectionbNS_19BidiEmbeddingSourceEPS0_;
constexpr auto copyStack = &_ZN7WebCore11BidiContext41copyStackRemovingUnicodeEmbeddingContextsEv;

constexpr int kLTR = 0;
constexpr int kRTL = 1;
constexpr unsigned kStyle = 0;
constexpr unsigned kUnicode = 1;

template<typename TFunction>
static bool ThrowsInvalidArgument(TFunction&& function) {
    try {
        function();
    } catch (const std::invalid_argument&) {
        return true;
    }
    return false;
}

static std::uint32_t Bits(unsigned level, int direction, bool override, unsigned source) {
    return level | (static_cast<std::uint32_t>(direction) << 6) | ((override ? 1U : 0U) << 11) | (source << 12);
}

static BidiContext* Make(unsigned char level, int direction, bool override, unsigned source, BidiContext* parent) {
    alignas(8) unsigned char bytes[8];
    std::memset(bytes, 0xA5, sizeof(bytes));
    auto* reference = reinterpret_cast<BidiContextRef*>(bytes);
    Require(create(reference, level, direction, override, source, parent) == reference);
    Require(reference->ptr != nullptr);
    return reference->ptr;
}

static BidiContext* Copy(BidiContext* context) {
    alignas(8) unsigned char bytes[8];
    std::memset(bytes, 0xA5, sizeof(bytes));
    auto* reference = reinterpret_cast<BidiContextRef*>(bytes);
    Require(copyStack(reference, context) == reference);
    Require(reference->ptr != nullptr);
    return reference->ptr;
}

static void Release(BidiContext* context) {
    while (context) {
        Require(context->refCount != 0);
        if (--context->refCount) return;
        BidiContext* parent = context->parent;
        _ZN3WTF8fastFreeEPv(context);
        context = parent;
    }
}

struct Shared {
    BidiContext* ltr;
    BidiContext* ltrOverride;
    BidiContext* rtl;
    BidiContext* rtlOverride;
};

static Shared shared;

static void TestTheSharedContexts() {
    shared.ltr = Make(0, kRTL, false, kUnicode, nullptr);
    shared.ltrOverride = Make(0, 13, true, kUnicode, nullptr);
    shared.rtl = Make(7, kLTR, false, kUnicode, nullptr);
    shared.rtlOverride = Make(255, 13, true, kUnicode, nullptr);
    BidiContext* const all[] = {shared.ltr, shared.ltrOverride, shared.rtl, shared.rtlOverride};
    for (int first = 0; first < 4; ++first)
        for (int second = first + 1; second < 4; ++second) Require(all[first] != all[second]);
    Require(shared.ltr->bits == Bits(0, kLTR, false, kStyle) && shared.ltrOverride->bits == Bits(0, kLTR, true, kStyle));
    Require(shared.rtl->bits == Bits(1, kRTL, false, kStyle) && shared.rtlOverride->bits == Bits(1, kRTL, true, kStyle));
    for (BidiContext* context : all) Require(context->parent == nullptr && context->refCount == 2);

    Require(Make(0, kLTR, false, kStyle, nullptr) == shared.ltr && shared.ltr->refCount == 3);
    Require(Make(0, 21, false, kStyle, nullptr) == shared.ltr && shared.ltr->refCount == 4);
    Require(Make(0, kLTR, true, kStyle, nullptr) == shared.ltrOverride && shared.ltrOverride->refCount == 3);
    Require(Make(1, kRTL, false, kStyle, nullptr) == shared.rtl && shared.rtl->refCount == 3);
    Require(Make(255, kLTR, false, kStyle, nullptr) == shared.rtl && shared.rtl->refCount == 4);
    Require(Make(1, kRTL, true, kStyle, nullptr) == shared.rtlOverride && shared.rtlOverride->refCount == 3);
    Require(shared.ltr->bits == Bits(0, kLTR, false, kStyle) && shared.rtl->bits == Bits(1, kRTL, false, kStyle));
    shared.ltr->refCount -= 2;
    shared.rtl->refCount -= 2;
    --shared.ltrOverride->refCount;
    --shared.rtlOverride->refCount;
}

static void TestAContextWithAParent() {
    const unsigned before = shared.ltr->refCount;
    BidiContext* child = Make(3, kRTL, true, kUnicode, shared.ltr);
    Require(child != shared.ltr && child->refCount == 1 && child->parent == shared.ltr && shared.ltr->refCount == before + 1);
    Require(child->bits == Bits(3, kRTL, true, kUnicode));
    BidiContext* twin = Make(3, kRTL, true, kUnicode, shared.ltr);
    Require(twin != child && twin->refCount == 1 && shared.ltr->refCount == before + 2);
    BidiContext* grandchild = Make(0, kLTR, false, kStyle, child);
    Require(grandchild->bits == 0 && grandchild->parent == child && child->refCount == 2 && grandchild != shared.ltr);
    BidiContext* truncated = Make(70, 37, false, 3, twin);
    Require(truncated->bits == Bits(6, 5, false, 1) && twin->refCount == 2);
    BidiContext* widest = Make(63, 31, true, kUnicode, twin);
    Require(widest->bits == 0x1FFF);
    Release(widest);
    Release(truncated);
    Release(grandchild);
    Release(child);
    Release(twin);
    Require(shared.ltr->refCount == before);
}

struct Step {
    int direction;
    bool override;
    unsigned source;
};

static void RequireCopyOfAChain(BidiContext* root, const std::vector<Step>& steps) {
    const unsigned rootBefore = root->refCount;
    std::vector<BidiContext*> chain;
    BidiContext* parent = root;
    for (std::size_t index = 0; index < steps.size(); ++index) {
        parent = Make(static_cast<unsigned char>(40 + index), steps[index].direction, steps[index].override, steps[index].source, parent);
        chain.push_back(parent);
    }
    BidiContext* copy = Copy(chain.empty() ? root : chain.back());

    BidiContext* expectedRoot = (root->bits >> 6 & 31) == static_cast<unsigned>(kRTL)
        ? ((root->bits >> 11 & 1) ? shared.rtlOverride : shared.rtl)
        : ((root->bits >> 11 & 1) ? shared.ltrOverride : shared.ltr);
    std::vector<std::uint32_t> expected;
    unsigned level = expectedRoot->bits & 63;
    for (const Step& step : steps) {
        if (step.source == kUnicode) continue;
        level = static_cast<unsigned char>(step.direction == kRTL ? ((level + 1) | 1) : ((level + 2) & ~1U)) & 63;
        expected.push_back(Bits(level, step.direction, step.override, step.source));
    }

    BidiContext* walk = copy;
    for (std::size_t index = expected.size(); index; --index) {
        Require(walk != nullptr && walk->bits == expected[index - 1] && walk->refCount == 1);
        for (BidiContext* original : chain) Require(walk != original);
        walk = walk->parent;
    }
    Require(walk == expectedRoot && walk->parent == nullptr);
    for (std::size_t index = 0; index < chain.size(); ++index) Require(chain[index]->refCount == (index + 1 == chain.size() ? 1U : 2U));
    Require(root->refCount == rootBefore + (chain.empty() ? 0U : 1U) + (expectedRoot == root ? 1U : 0U));
    Release(copy);
    for (std::size_t index = chain.size(); index; --index) Release(chain[index - 1]);
    Require(root->refCount == rootBefore);
}

static void TestCopies() {
    RequireCopyOfAChain(shared.ltr, {});
    RequireCopyOfAChain(shared.rtl, {});
    RequireCopyOfAChain(shared.ltrOverride, {});
    RequireCopyOfAChain(shared.rtlOverride, {});
    RequireCopyOfAChain(shared.ltr, {{kRTL, false, kStyle}, {kLTR, false, kUnicode}, {kRTL, true, kStyle}});
    RequireCopyOfAChain(shared.ltr, {{kRTL, false, kStyle}, {kLTR, false, kUnicode}});
    RequireCopyOfAChain(shared.ltr, {{kLTR, false, kUnicode}, {kRTL, false, kUnicode}});
    RequireCopyOfAChain(shared.rtl, {{kLTR, false, kStyle}});
    RequireCopyOfAChain(shared.rtl, {{kRTL, true, kStyle}, {kLTR, true, kStyle}, {kLTR, false, kStyle}});
    RequireCopyOfAChain(shared.ltrOverride, {{kRTL, false, kStyle}});
    RequireCopyOfAChain(shared.rtlOverride, {{kLTR, false, kStyle}, {kRTL, false, kUnicode}, {kRTL, false, kStyle}});
    RequireCopyOfAChain(shared.ltr, {{13, false, kStyle}, {kRTL, false, kStyle}, {4, true, kStyle}});
    RequireCopyOfAChain(shared.ltr, {{21, false, kStyle}, {17, true, kStyle}, {kRTL, false, kStyle}, {31, false, kStyle}});
    std::vector<Step> deep;
    for (int index = 0; index < 70; ++index) deep.push_back({kRTL, false, kStyle});
    RequireCopyOfAChain(shared.ltr, deep);
    std::vector<Step> mixed;
    for (int index = 0; index < 90; ++index) mixed.push_back({index % 3 ? kRTL : kLTR, index % 5 == 0, index % 7 == 3 ? kUnicode : kStyle});
    RequireCopyOfAChain(shared.rtl, mixed);
}

static void TestHandBuiltRoots() {
    BidiContext leftRoot{1, Bits(5, kLTR, false, kStyle), nullptr};
    RequireCopyOfAChain(&leftRoot, {});
    RequireCopyOfAChain(&leftRoot, {{kRTL, false, kStyle}});
    BidiContext rightRoot{1, Bits(5, kRTL, true, kStyle), nullptr};
    RequireCopyOfAChain(&rightRoot, {{kLTR, false, kStyle}});
    Require(leftRoot.refCount == 1 && rightRoot.refCount == 1);

    BidiContext unicodeRoot{1, Bits(2, kLTR, false, kUnicode), nullptr};
    BidiContext unicodeChild{1, Bits(3, kRTL, false, kUnicode), &unicodeRoot};
    alignas(8) unsigned char bytes[8];
    std::memset(bytes, 0xA5, sizeof(bytes));
    auto* reference = reinterpret_cast<BidiContextRef*>(bytes);
    Require(ThrowsInvalidArgument([&] { copyStack(reference, &unicodeChild); }));
    Require(ThrowsInvalidArgument([&] { copyStack(reference, &unicodeRoot); }));
    for (const unsigned char byte : bytes) Require(byte == 0xA5);
    Require(unicodeRoot.refCount == 1 && unicodeChild.refCount == 1);

    BidiContext styleChild{1, Bits(9, kRTL, true, kStyle), &unicodeRoot};
    BidiContext* copy = Copy(&styleChild);
    Require(copy == shared.rtlOverride);
    --copy->refCount;
}

static void TestInvalidArguments() {
    BidiContextRef reference{};
    Require(ThrowsInvalidArgument([&] { create(nullptr, 0, kLTR, false, kStyle, nullptr); }));
    Require(ThrowsInvalidArgument([&] { create(nullptr, 1, kRTL, false, kStyle, shared.ltr); }));
    Require(ThrowsInvalidArgument([&] { copyStack(nullptr, shared.ltr); }));
    Require(ThrowsInvalidArgument([&] { copyStack(&reference, nullptr); }));
    Require(reference.ptr == nullptr);
}

struct GuardedPages {
    unsigned char* base = nullptr;
    std::size_t pageSize = 0;

    GuardedPages() {
#ifdef _WIN32
        SYSTEM_INFO info{};
        GetSystemInfo(&info);
        pageSize = info.dwPageSize;
        base = static_cast<unsigned char*>(VirtualAlloc(nullptr, pageSize * 3, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
        Require(base != nullptr);
        DWORD previous = 0;
        Require(VirtualProtect(base + pageSize, pageSize, PAGE_READONLY, &previous) != 0);
        Require(VirtualProtect(base + pageSize * 2, pageSize, PAGE_NOACCESS, &previous) != 0);
#else
        const long size = sysconf(_SC_PAGESIZE);
        Require(size > 0);
        pageSize = static_cast<std::size_t>(size);
        void* mapped = mmap(nullptr, pageSize * 3, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        Require(mapped != MAP_FAILED);
        base = static_cast<unsigned char*>(mapped);
        Require(mprotect(base + pageSize, pageSize, PROT_READ) == 0);
        Require(mprotect(base + pageSize * 2, pageSize, PROT_NONE) == 0);
#endif
    }

    GuardedPages(const GuardedPages&) = delete;
    GuardedPages& operator=(const GuardedPages&) = delete;

    ~GuardedPages() {
        if (!base) return;
#ifdef _WIN32
        VirtualFree(base, 0, MEM_RELEASE);
#else
        munmap(base, pageSize * 3);
#endif
    }

    unsigned char* ReadOnly() const { return base + pageSize; }
    unsigned char* Guard() const { return base + pageSize * 2; }
};

template<typename TFunction>
static void RequireNotImplemented(TFunction&& function) {
    const auto before = NotImplementedCalls_nid_no_patch();
    bool threw = false;
    try {
        function();
    } catch (const std::runtime_error&) {
        threw = true;
    }
    Require(threw);
    Require(NotImplementedCalls_nid_no_patch() == before + 1);
}

static void TestGuestPointers() {
    GuardedPages pages;
    const unsigned ltrBefore = shared.ltr->refCount;
    BidiContextRef reference{shared.rtl};

    RequireNotImplemented([&] {
        create(reinterpret_cast<BidiContextRef*>(pages.Guard()), 0, kLTR, false, kStyle, nullptr);
    });
    Require(shared.ltr->refCount == ltrBefore);
    RequireNotImplemented([&] {
        create(reinterpret_cast<BidiContextRef*>(pages.ReadOnly()), 0, kLTR, false, kStyle, shared.ltr);
    });
    Require(shared.ltr->refCount == ltrBefore);

    BidiContext customParent{1, Bits(2, kLTR, false, kStyle), nullptr};
    RequireNotImplemented([&] {
        create(reinterpret_cast<BidiContextRef*>(pages.ReadOnly() - 4), 1, kRTL, false, kStyle, &customParent);
    });
    Require(customParent.refCount == 1);
    RequireNotImplemented([&] {
        create(reinterpret_cast<BidiContextRef*>(pages.ReadOnly() - 4), 0, kLTR, false, kStyle, nullptr);
    });
    Require(shared.ltr->refCount == ltrBefore);

    RequireNotImplemented([&] {
        create(&reference, 1, kRTL, false, kStyle, reinterpret_cast<BidiContext*>(pages.Guard()));
    });
    Require(reference.ptr == shared.rtl);

    RequireNotImplemented([&] {
        create(&reference, 1, kRTL, false, kStyle, reinterpret_cast<BidiContext*>(pages.ReadOnly()));
    });
    Require(reference.ptr == shared.rtl);

    RequireNotImplemented([&] {
        copyStack(reinterpret_cast<BidiContextRef*>(pages.Guard()), shared.ltr);
    });
    RequireNotImplemented([&] {
        copyStack(reinterpret_cast<BidiContextRef*>(pages.ReadOnly()), shared.ltr);
    });
    Require(shared.ltr->refCount == ltrBefore);

    BidiContext deepParent{1, Bits(1, kRTL, false, kStyle), shared.ltr};
    BidiContext deepChild{1, Bits(2, kLTR, false, kStyle), &deepParent};
    RequireNotImplemented([&] {
        copyStack(reinterpret_cast<BidiContextRef*>(pages.ReadOnly() - 4), &deepChild);
    });
    Require(shared.ltr->refCount == ltrBefore);
    Require(deepParent.refCount == 1 && deepChild.refCount == 1);

    RequireNotImplemented([&] {
        copyStack(&reference, reinterpret_cast<BidiContext*>(pages.Guard()));
    });
    Require(reference.ptr == shared.rtl);

    BidiContext brokenChild{1, Bits(2, kRTL, false, kStyle), reinterpret_cast<BidiContext*>(pages.Guard())};
    RequireNotImplemented([&] {
        copyStack(&reference, &brokenChild);
    });
    Require(reference.ptr == shared.rtl);

    BidiContext cyclic{1, Bits(1, kRTL, false, kStyle), nullptr};
    cyclic.parent = &cyclic;
    RequireNotImplemented([&] {
        copyStack(&reference, &cyclic);
    });
    Require(reference.ptr == shared.rtl);
}

int main() {
    TestTheSharedContexts();
    const unsigned counts[] = {shared.ltr->refCount, shared.ltrOverride->refCount, shared.rtl->refCount, shared.rtlOverride->refCount};
    TestAContextWithAParent();
    TestCopies();
    TestHandBuiltRoots();
    TestInvalidArguments();
    TestGuestPointers();
    Require(shared.ltr->refCount == counts[0] && shared.ltrOverride->refCount == counts[1]);
    Require(shared.rtl->refCount == counts[2] && shared.rtlOverride->refCount == counts[3]);
    std::puts("libSceNKWebKit bidi context tests passed");
    return 0;
}
