#include "prx/libSceAddressSanitizer/src/FakeStack.hpp"
#include "prx/libSceAddressSanitizer/src/Container.hpp"
#include "prx/libSceAddressSanitizer/src/ErrorReport.hpp"
#include "prx/libSceAddressSanitizer/src/ShadowRequire.hpp"
#include "prx/libSceAddressSanitizer/src/StackBounds.hpp"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <mutex>
#include <new>
#include <set>
#include <string>

namespace AsanRuntime::FakeStack {

namespace {

constexpr std::size_t kMinFrameSizeLog = 6;
constexpr std::size_t kClasses = 11;
constexpr std::size_t kFlagsOffset = 4096;
constexpr std::size_t kMinStackSizeLog = 16;
constexpr std::size_t kMaxStackSizeLog = 20;
constexpr std::size_t kPageSize = 4096;
constexpr std::uintptr_t kMaxExpectedCleanupSize = 64ull << 20;
constexpr std::uintptr_t kCurrentStackFrameMagic = 0x41B58AB3;
constexpr std::uint8_t kStackAfterReturnMagic = 0xf5;
constexpr bool kDetectStackUseAfterReturn = false;

struct FakeFrame {
    std::uintptr_t magic;
    std::uintptr_t descr;
    std::uintptr_t pc;
    std::uintptr_t realStack;
};

struct Stack {
    std::uintptr_t hint[kClasses];
    std::uintptr_t stackSizeLog;
    bool needsGc;
};

std::uintptr_t SizeRequiredForFlags(std::uintptr_t stackSizeLog) {
    return std::uintptr_t{1} << (stackSizeLog + 1 - kMinFrameSizeLog);
}

std::uintptr_t SizeRequiredForFrames(std::uintptr_t stackSizeLog) {
    return (std::uintptr_t{1} << stackSizeLog) * kClasses;
}

std::uintptr_t RequiredSize(std::uintptr_t stackSizeLog) {
    return kFlagsOffset + SizeRequiredForFlags(stackSizeLog) + SizeRequiredForFrames(stackSizeLog);
}

std::uintptr_t FlagsOffset(std::uintptr_t stackSizeLog, std::uintptr_t classId) {
    const std::uintptr_t t = kClasses - 1 - classId;
    const std::uintptr_t allOnes = (std::uintptr_t{1} << (kClasses - 1)) - 1;
    return ((allOnes >> t) << t) << (stackSizeLog - 15);
}

std::uintptr_t NumberOfFrames(std::uintptr_t stackSizeLog, std::uintptr_t classId) {
    return std::uintptr_t{1} << (stackSizeLog - kMinFrameSizeLog - classId);
}

std::uintptr_t BytesInSizeClass(std::uintptr_t classId) {
    return std::uintptr_t{1} << (classId + kMinFrameSizeLog);
}

std::uint8_t* GetFlags(Stack* stack, std::uintptr_t classId) {
    return reinterpret_cast<std::uint8_t*>(stack) + kFlagsOffset + FlagsOffset(stack->stackSizeLog, classId);
}

std::uint8_t* GetFrame(Stack* stack, std::uintptr_t classId, std::uintptr_t position) {
    return reinterpret_cast<std::uint8_t*>(stack) + kFlagsOffset + SizeRequiredForFlags(stack->stackSizeLog) + (std::uintptr_t{1} << stack->stackSizeLog) * classId + BytesInSizeClass(classId) * position;
}

std::uint8_t** SavedFlagPtr(std::uintptr_t frame, std::uintptr_t classId) {
    return reinterpret_cast<std::uint8_t**>(frame + BytesInSizeClass(classId) - sizeof(frame));
}

void SetShadow(std::uintptr_t pointer, std::uintptr_t size, std::uintptr_t classId, std::uint8_t magic) {
    if (classId <= 6) {
        PoisonShadow_nid_no_patch(pointer, BytesInSizeClass(classId), magic);
    } else {
        PoisonShadow_nid_no_patch(pointer, size, magic);
    }
}

struct Registry {
    std::mutex mutex;
    std::set<Stack*> stacks;
};

Registry& GetRegistry() {
    static Registry registry;
    return registry;
}

bool IsRegistered(void* stack) {
    Registry& registry = GetRegistry();
    std::lock_guard lock(registry.mutex);
    return registry.stacks.count(static_cast<Stack*>(stack)) != 0;
}

Stack* Create(std::uintptr_t stackSizeLog) {
    stackSizeLog = std::min<std::uintptr_t>(std::max<std::uintptr_t>(stackSizeLog, kMinStackSizeLog), kMaxStackSizeLog);
    const std::size_t size = RequiredSize(stackSizeLog);
    void* block = std::calloc(1, size);
    if (!block) {
        throw std::bad_alloc();
    }
    try {
        RequireShadowRange_nid_no_patch(reinterpret_cast<std::uintptr_t>(block), size, __func__);
    } catch (...) {
        std::free(block);
        throw;
    }
    auto* stack = static_cast<Stack*>(block);
    stack->stackSizeLog = stackSizeLog;
    Registry& registry = GetRegistry();
    std::lock_guard lock(registry.mutex);
    registry.stacks.insert(stack);
    return stack;
}

void Destroy(Stack* stack) {
    PoisonShadow_nid_no_patch(reinterpret_cast<std::uintptr_t>(stack), RequiredSize(stack->stackSizeLog), 0);
    {
        Registry& registry = GetRegistry();
        std::lock_guard lock(registry.mutex);
        registry.stacks.erase(stack);
    }
    std::free(stack);
}

struct ThreadState {
    std::uintptr_t bottom = 0;
    std::uintptr_t top = 0;
    std::uintptr_t nextBottom = 0;
    std::uintptr_t nextTop = 0;
    bool boundsKnown = false;
    bool switching = false;
    Stack* fakeStack = nullptr;

    ~ThreadState() {
        if (fakeStack) {
            Destroy(fakeStack);
        }
    }
};

thread_local ThreadState threadState;

ThreadState& Thread() {
    if (!threadState.boundsKnown) {
        threadState.boundsKnown = true;
        if (!CurrentStackBounds_nid_no_patch(threadState.bottom, threadState.top)) {
            threadState.bottom = 0;
            threadState.top = 0;
        }
    }
    return threadState;
}

struct Bounds {
    std::uintptr_t bottom;
    std::uintptr_t top;
};

Bounds CurrentBounds() {
    ThreadState& thread = Thread();
    if (!thread.switching) {
        if (thread.bottom >= thread.top) {
            return {0, 0};
        }
        return {thread.bottom, thread.top};
    }
    char local;
    const auto current = reinterpret_cast<std::uintptr_t>(&local);
    if (current >= thread.nextBottom && current < thread.nextTop) {
        return {thread.nextBottom, thread.nextTop};
    }
    return {thread.bottom, thread.top};
}

Stack* GetOrCreate() {
    ThreadState& thread = Thread();
    if (thread.fakeStack) {
        return thread.fakeStack;
    }
    const Bounds bounds = CurrentBounds();
    const std::uintptr_t stackSize = bounds.top - bounds.bottom;
    if (stackSize == 0) {
        return nullptr;
    }
    std::uintptr_t log = 0;
    while ((std::uintptr_t{1} << log) < stackSize) {
        ++log;
    }
    thread.fakeStack = Create(log);
    return thread.fakeStack;
}

void CollectGarbage(Stack* stack, std::uintptr_t realStack) {
    const Bounds bounds = CurrentBounds();
    if (realStack < bounds.bottom || realStack > bounds.top) {
        return;
    }
    for (std::uintptr_t classId = 0; classId < kClasses; ++classId) {
        std::uint8_t* flags = GetFlags(stack, classId);
        for (std::uintptr_t i = 0, n = NumberOfFrames(stack->stackSizeLog, classId); i < n; ++i) {
            if (flags[i] == 0) {
                continue;
            }
            auto* frame = reinterpret_cast<FakeFrame*>(GetFrame(stack, classId, i));
            if (bounds.bottom < frame->realStack && frame->realStack < realStack) {
                flags[i] = 0;
                SetShadow(reinterpret_cast<std::uintptr_t>(frame), BytesInSizeClass(classId), classId, kStackAfterReturnMagic);
            }
        }
    }
    stack->needsGc = false;
}

FakeFrame* AllocateFrame(Stack* stack, std::uintptr_t classId, std::uintptr_t realStack) {
    if (stack->needsGc) {
        CollectGarbage(stack, realStack);
    }
    std::uintptr_t& hint = stack->hint[classId];
    const std::uintptr_t count = NumberOfFrames(stack->stackSizeLog, classId);
    std::uint8_t* flags = GetFlags(stack, classId);
    for (std::uintptr_t i = 0; i < count; ++i) {
        const std::uintptr_t position = hint++ & (count - 1);
        if (flags[position]) {
            continue;
        }
        flags[position] = 1;
        auto* frame = reinterpret_cast<FakeFrame*>(GetFrame(stack, classId, position));
        frame->realStack = realStack;
        *SavedFlagPtr(reinterpret_cast<std::uintptr_t>(frame), classId) = &flags[position];
        return frame;
    }
    return nullptr;
}

struct FrameLocation {
    std::uintptr_t frame;
    std::uintptr_t begin;
    std::uintptr_t end;
    std::uintptr_t classId;
    std::uintptr_t position;
};

bool LocateFrame(Stack* stack, std::uintptr_t pointer, FrameLocation& location) {
    const std::uintptr_t stackSizeLog = stack->stackSizeLog;
    const auto first = reinterpret_cast<std::uintptr_t>(GetFrame(stack, 0, 0));
    const std::uintptr_t end = reinterpret_cast<std::uintptr_t>(stack) + RequiredSize(stackSizeLog);
    if (pointer < first || pointer >= end) {
        return false;
    }
    location.classId = (pointer - first) >> stackSizeLog;
    const std::uintptr_t base = first + (location.classId << stackSizeLog);
    location.position = (pointer - base) >> (kMinFrameSizeLog + location.classId);
    location.frame = base + location.position * BytesInSizeClass(location.classId);
    location.end = location.frame + BytesInSizeClass(location.classId);
    location.begin = location.frame + sizeof(FakeFrame);
    return true;
}

}  // namespace

void* Malloc_nid_no_patch(std::size_t classId, std::size_t size, bool always, std::uintptr_t realStack) {
    if (classId >= kClasses) {
        InvalidArgument_nid_no_patch(__func__);
    }
    if (!always && !kDetectStackUseAfterReturn) {
        return nullptr;
    }
    Stack* stack = GetOrCreate();
    if (!stack) {
        return nullptr;
    }
    FakeFrame* frame = AllocateFrame(stack, classId, realStack);
    if (!frame) {
        return nullptr;
    }
    SetShadow(reinterpret_cast<std::uintptr_t>(frame), size, classId, 0);
    return frame;
}

void Free_nid_no_patch(std::uintptr_t pointer, std::size_t classId, std::size_t size) {
    Stack* stack = Thread().fakeStack;
    FrameLocation location{};
    if (classId >= kClasses || !stack || !LocateFrame(stack, pointer, location) || location.frame != pointer || location.classId != classId) {
        InvalidArgument_nid_no_patch(__func__);
    }
    GetFlags(stack, classId)[location.position] = 0;
    SetShadow(pointer, size, classId, kStackAfterReturnMagic);
}

void* Current_nid_no_patch() {
    return kDetectStackUseAfterReturn ? GetOrCreate() : nullptr;
}

void* AddrIsIn_nid_no_patch(void* fakeStack, void* address, void** begin, void** end) {
    if (!fakeStack) {
        return nullptr;
    }
    if (!IsRegistered(fakeStack)) {
        InvalidArgument_nid_no_patch(__func__);
    }
    FrameLocation location{};
    if (!LocateFrame(static_cast<Stack*>(fakeStack), reinterpret_cast<std::uintptr_t>(address), location)) {
        return nullptr;
    }
    const auto* header = reinterpret_cast<const FakeFrame*>(location.frame);
    if (header->magic != kCurrentStackFrameMagic) {
        return nullptr;
    }
    if (begin) {
        *begin = reinterpret_cast<void*>(location.begin);
    }
    if (end) {
        *end = reinterpret_cast<void*>(location.end);
    }
    return reinterpret_cast<void*>(header->realStack);
}

void HandleNoReturn_nid_no_patch() {
    ThreadState& thread = Thread();
    const Bounds bounds = CurrentBounds();
    if (bounds.top == 0) {
        StackUnavailable_nid_no_patch(__func__);
    }
    char local;
    const std::uintptr_t bottom = (reinterpret_cast<std::uintptr_t>(&local) - kPageSize) & ~(kPageSize - 1);
    if (bounds.top > bottom) {
        const std::uintptr_t size = bounds.top - bottom;
        if (size > kMaxExpectedCleanupSize) {
            static std::once_flag warned;
            std::call_once(warned, [&] {
                char buffer[400];
                std::snprintf(buffer, sizeof(buffer), "WARNING: ASan is ignoring requested __asan_handle_no_return: stack type: default top: %p; bottom %p; size: %p (%zd)\nFalse positive error reports may follow\nFor details see https://github.com/google/sanitizers/issues/189\n", reinterpret_cast<void*>(bounds.top), reinterpret_cast<void*>(bottom), reinterpret_cast<void*>(size), static_cast<std::ptrdiff_t>(size));
                EmitDiagnostic_nid_no_patch(buffer);
            });
        } else {
            RequireShadowRange_nid_no_patch(bottom, size, __func__);
            PoisonShadow_nid_no_patch(bottom, (size + kGranularity - 1) & ~(kGranularity - 1), 0);
        }
    }
    if (thread.fakeStack) {
        thread.fakeStack->needsGc = true;
    }
}

void StartSwitchFiber_nid_no_patch(void** save, const void* bottom, std::size_t size) {
    ThreadState& thread = Thread();
    if (thread.switching) {
        EmitDiagnostic_nid_no_patch("ERROR: starting fiber switch while in fiber switch\n");
        Die_nid_no_patch();
    }
    thread.nextBottom = reinterpret_cast<std::uintptr_t>(bottom);
    thread.nextTop = thread.nextBottom + size;
    thread.switching = true;
    Stack* current = thread.fakeStack;
    if (save) {
        *save = current;
    }
    thread.fakeStack = nullptr;
    if (!save && current) {
        Destroy(current);
    }
}

void FinishSwitchFiber_nid_no_patch(void* save, const void** bottomOld, std::size_t* sizeOld) {
    ThreadState& thread = Thread();
    if (!thread.switching) {
        EmitDiagnostic_nid_no_patch("ERROR: finishing a fiber switch that has not started\n");
        Die_nid_no_patch();
    }
    if (save) {
        thread.fakeStack = static_cast<Stack*>(save);
    }
    if (bottomOld) {
        *bottomOld = reinterpret_cast<const void*>(thread.bottom);
    }
    if (sizeOld) {
        *sizeOld = thread.top - thread.bottom;
    }
    thread.bottom = thread.nextBottom;
    thread.top = thread.nextTop;
    thread.switching = false;
    thread.nextTop = 0;
    thread.nextBottom = 0;
}

}  // namespace AsanRuntime::FakeStack
