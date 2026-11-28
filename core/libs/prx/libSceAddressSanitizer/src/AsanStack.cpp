/* Derived from LLVM compiler-rt lib/asan (asan_fake_stack.cpp, asan_fake_stack.h, asan_thread.cpp, asan_descriptions.cpp, asan_report.cpp, asan_debugging.cpp) at llvmorg-16.0.6 (af2ed9f794c422923414d162dd1f48d986e2d6e3) */
/* Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
 * See https://llvm.org/LICENSE.txt for license information.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception */
#include "prx/libSceAddressSanitizer/src/AsanRuntime.hpp"
#include "prx/libSceAddressSanitizer/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/GuestHeap.hpp"
#include "prx/libkernel/include/Api.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <stdexcept>
#include <string>
#include <vector>

namespace Asan::Runtime {

namespace {

constexpr uptr MinStackFrameSizeLog = 6;
constexpr uptr MaxStackFrameSizeLog = 16;
constexpr uptr NumberOfSizeClasses = MaxStackFrameSizeLog - MinStackFrameSizeLog + 1;
constexpr uptr FlagsOffset = 4096;
constexpr uptr MinStackSizeLog = 16;
constexpr uptr MaxStackSizeLog = 28;
constexpr uptr CurrentStackFrameMagic = 0x41B58AB3;
constexpr std::uint64_t Magic8 = 0xf5f5f5f5f5f5f5f5ull;

static_assert(NumberOfSizeClasses == 11);

struct FakeFrame {
    uptr magic;
    uptr descr;
    uptr pc;
    uptr realStack;
};

class FakeStack {
public:
    static uptr SizeRequiredForFlags(uptr stackSizeLog) { return uptr{1} << (stackSizeLog + 1 - MinStackFrameSizeLog); }

    static uptr SizeRequiredForFrames(uptr stackSizeLog) { return (uptr{1} << stackSizeLog) * NumberOfSizeClasses; }

    static uptr RequiredSize(uptr stackSizeLog) { return FlagsOffset + SizeRequiredForFlags(stackSizeLog) + SizeRequiredForFrames(stackSizeLog); }

    static uptr ClassFlagsOffset(uptr stackSizeLog, uptr classId) {
        const uptr shift = NumberOfSizeClasses - 1 - classId;
        const uptr allOnes = (uptr{1} << (NumberOfSizeClasses - 1)) - 1;
        return ((allOnes >> shift) << shift) << (stackSizeLog - 15);
    }

    static uptr NumberOfFrames(uptr stackSizeLog, uptr classId) { return uptr{1} << (stackSizeLog - MinStackFrameSizeLog - classId); }

    static uptr BytesInSizeClass(uptr classId) { return uptr{1} << (classId + MinStackFrameSizeLog); }

    static u8** SavedFlagPtr(uptr frame, uptr classId) { return reinterpret_cast<u8**>(frame + BytesInSizeClass(classId) - sizeof(frame)); }

    static FakeStack* Create(uptr stackSizeLog) {
        stackSizeLog = std::clamp(stackSizeLog, MinStackSizeLog, MaxStackSizeLog);
        const uptr size = RequiredSize(stackSizeLog);
        auto* memory = static_cast<u8*>(GuestHeap::GuestHeapAlign_nid_postfix(FlagsOffset, size));
        std::memset(memory, 0, FlagsOffset + SizeRequiredForFlags(stackSizeLog));
        auto* stack = reinterpret_cast<FakeStack*>(memory);
        stack->stackSizeLog = stackSizeLog;
        stack->needsGc = false;
        std::fill(std::begin(stack->hintPosition), std::end(stack->hintPosition), uptr{0});
        PoisonShadow(reinterpret_cast<uptr>(memory), size, 0);
        return stack;
    }

    void Destroy() {
        PoisonShadow(reinterpret_cast<uptr>(this), RequiredSize(stackSizeLog), 0);
        GuestHeap::GuestHeapFree_nid_postfix(this);
    }

    u8* Flags(uptr classId) { return reinterpret_cast<u8*>(this) + FlagsOffset + ClassFlagsOffset(stackSizeLog, classId); }

    u8* Frame(uptr classId, uptr position) {
        return reinterpret_cast<u8*>(this) + FlagsOffset + SizeRequiredForFlags(stackSizeLog) + (uptr{1} << stackSizeLog) * classId + BytesInSizeClass(classId) * position;
    }

    FakeFrame* Allocate(uptr classId, uptr realStack) {
        if (needsGc) Gc(realStack);
        uptr& hint = hintPosition[classId];
        const uptr frames = NumberOfFrames(stackSizeLog, classId);
        u8* const flags = Flags(classId);
        for (uptr attempt = 0; attempt < frames; ++attempt) {
            const uptr position = hint++ & (frames - 1);
            if (flags[position]) continue;
            flags[position] = 1;
            auto* frame = reinterpret_cast<FakeFrame*>(Frame(classId, position));
            frame->realStack = realStack;
            *SavedFlagPtr(reinterpret_cast<uptr>(frame), classId) = &flags[position];
            return frame;
        }
        return nullptr;
    }

    uptr AddrIsInFakeStack(uptr address, uptr* frameBegin, uptr* frameEnd) {
        const uptr begin = reinterpret_cast<uptr>(Frame(0, 0));
        const uptr end = reinterpret_cast<uptr>(this) + RequiredSize(stackSizeLog);
        if (address < begin || address >= end) return 0;
        const uptr classId = (address - begin) >> stackSizeLog;
        const uptr base = begin + (classId << stackSizeLog);
        const uptr position = (address - base) >> (MinStackFrameSizeLog + classId);
        const uptr frame = base + position * BytesInSizeClass(classId);
        *frameEnd = frame + BytesInSizeClass(classId);
        *frameBegin = frame + sizeof(FakeFrame);
        return frame;
    }

    void HandleNoReturn() { needsGc = true; }

    void Gc(uptr realStack) {
        for (uptr classId = 0; classId < NumberOfSizeClasses; ++classId) {
            u8* const flags = Flags(classId);
            for (uptr index = 0, count = NumberOfFrames(stackSizeLog, classId); index < count; ++index) {
                if (flags[index] == 0) continue;
                if (reinterpret_cast<FakeFrame*>(Frame(classId, index))->realStack < realStack) flags[index] = 0;
            }
        }
        needsGc = false;
    }

private:
    uptr hintPosition[NumberOfSizeClasses];
    uptr stackSizeLog;
    bool needsGc;
};

static_assert(sizeof(FakeStack) <= FlagsOffset);

void SetShadow(uptr pointer, uptr size, uptr classId, std::uint64_t magic) {
    if (classId <= 6) {
        RequireShadow(pointer, FakeStack::BytesInSizeClass(classId));
        auto* const shadow = reinterpret_cast<std::uint64_t*>(ShadowOf(pointer));
        for (uptr index = 0; index < (uptr{1} << classId); ++index) shadow[index] = magic;
    } else {
        PoisonShadow(pointer, size, static_cast<u8>(magic));
    }
}

struct ThreadState {
    bool initialized = false;
    uptr stackBottom = 0;
    uptr stackTop = 0;
    uptr nextStackBottom = 0;
    uptr nextStackTop = 0;
    std::atomic<bool> stackSwitching{false};
    FakeStack* fakeStack = nullptr;
    bool fakeStackClaimed = false;

    struct Bounds {
        uptr bottom;
        uptr top;
    };

    Bounds StackBounds() const {
        if (!stackSwitching.load(std::memory_order_acquire)) {
            if (stackBottom >= stackTop) return {0, 0};
            return {stackBottom, stackTop};
        }
        volatile char local = 0;
        const uptr current = reinterpret_cast<uptr>(&local);
        if (current >= nextStackBottom && current < nextStackTop) return {nextStackBottom, nextStackTop};
        return {stackBottom, stackTop};
    }

    bool AddrIsInStack(uptr address) const {
        const Bounds bounds = StackBounds();
        return address >= bounds.bottom && address < bounds.top;
    }

    ~ThreadState();
};

std::mutex threadsMutex;
std::vector<ThreadState*> threads;

ThreadState::~ThreadState() {
    {
        std::lock_guard lock(threadsMutex);
        threads.erase(std::remove(threads.begin(), threads.end(), this), threads.end());
    }
    if (fakeStack != nullptr && ShadowReady()) fakeStack->Destroy();
}

bool GuestStackOfCurrentThread(uptr* bottom, uptr* top) {
    const Pthread self = scePthreadSelf();
    if (self == nullptr) return false;
    PthreadAttr attribute = nullptr;
    if (scePthreadAttrInit(&attribute) != 0) return false;
    void* address = nullptr;
    std::size_t bytes = 0;
    const bool known = scePthreadAttrGet(self, &attribute) == 0 && scePthreadAttrGetstack(&attribute, &address, &bytes) == 0;
    scePthreadAttrDestroy(&attribute);
    if (!known || address == nullptr || bytes == 0) return false;
    *bottom = reinterpret_cast<uptr>(address);
    *top = *bottom + bytes;
    return true;
}

thread_local ThreadState currentThread;

ThreadState& CurrentThread() {
    if (!currentThread.initialized) {
        currentThread.initialized = true;
        uptr bottom = 0;
        uptr top = 0;
        if (GuestStackOfCurrentThread(&bottom, &top)) {
            currentThread.stackBottom = bottom;
            currentThread.stackTop = top;
        }
        std::lock_guard lock(threadsMutex);
        threads.push_back(&currentThread);
    }
    return currentThread;
}

uptr Log2RoundUp(uptr value) {
    uptr log = 0;
    while ((uptr{1} << log) < value) ++log;
    return log;
}

FakeStack* LazyInitFakeStack(ThreadState& thread) {
    if (thread.fakeStack != nullptr) return thread.fakeStack;
    if (thread.fakeStackClaimed) return nullptr;
    const ThreadState::Bounds bounds = thread.StackBounds();
    const uptr stackSize = bounds.top - bounds.bottom;
    if (stackSize == 0) return nullptr;
    thread.fakeStackClaimed = true;
    uptr stackSizeLog = Log2RoundUp(stackSize);
    stackSizeLog = std::min(stackSizeLog, static_cast<uptr>(GetFlags().maxUarStackSizeLog));
    stackSizeLog = std::max(stackSizeLog, static_cast<uptr>(GetFlags().minUarStackSizeLog));
    thread.fakeStack = FakeStack::Create(stackSizeLog);
    return thread.fakeStack;
}

FakeStack* FakeStackFast() {
    ThreadState& thread = CurrentThread();
    if (thread.fakeStack != nullptr) return thread.fakeStack;
    if (!GetFlags().detectStackUseAfterReturn) return nullptr;
    return LazyInitFakeStack(thread);
}

FakeStack* FakeStackFastAlways() {
    ThreadState& thread = CurrentThread();
    if (thread.fakeStack != nullptr) return thread.fakeStack;
    return LazyInitFakeStack(thread);
}

uptr OnMalloc(FakeStack* stack, uptr classId, uptr size) {
    if (stack == nullptr) return 0;
    volatile uptr localStack = 0;
    FakeFrame* const frame = stack->Allocate(classId, reinterpret_cast<uptr>(&localStack));
    if (frame == nullptr) return 0;
    const uptr pointer = reinterpret_cast<uptr>(frame);
    SetShadow(pointer, size, classId, 0);
    return pointer;
}

void OnFree(uptr pointer, uptr classId, uptr size) {
    Require(pointer != 0, "guest AddressSanitizer: __asan_stack_free of a null frame");
    u8* flag = nullptr;
    if (!GuestCopyIn_nid_no_patch(&flag, FakeStack::SavedFlagPtr(pointer, classId), sizeof(flag)) || flag == nullptr)
        throw std::invalid_argument("guest AddressSanitizer: fake frame has no saved flag pointer");
    *flag = 0;
    SetShadow(pointer, size, classId, Magic8);
}

struct StackAccess {
    ThreadState* thread = nullptr;
    bool hasFrame = false;
    uptr offset = 0;
    uptr framePc = 0;
    uptr frameDescription = 0;
};

ThreadState& CurrentThread();

ThreadState* FindThreadByStackAddress(uptr address) {
    CurrentThread();
    std::lock_guard lock(threadsMutex);
    for (ThreadState* thread : threads) {
        if (thread->AddrIsInStack(address)) return thread;
        uptr begin = 0;
        uptr end = 0;
        if (thread->fakeStack != nullptr && thread->fakeStack->AddrIsInFakeStack(address, &begin, &end)) return thread;
    }
    return nullptr;
}

bool ReadWord(uptr address, uptr* value) {
    return GuestCopyIn_nid_no_patch(value, reinterpret_cast<const void*>(address), sizeof(*value));
}

bool StackFrameAccessByAddr(ThreadState& thread, uptr address, StackAccess* access) {
    const ThreadState::Bounds bounds = thread.StackBounds();
    if (bounds.top == bounds.bottom && thread.fakeStack == nullptr) return false;
    uptr bottom = 0;
    if (thread.AddrIsInStack(address)) {
        bottom = bounds.bottom;
    } else if (thread.fakeStack != nullptr) {
        uptr frameBegin = 0;
        uptr frameEnd = 0;
        bottom = thread.fakeStack->AddrIsInFakeStack(address, &frameBegin, &frameEnd);
        if (bottom == 0) return false;
        access->offset = address - bottom;
        return ReadWord(bottom + 2 * sizeof(uptr), &access->framePc) && ReadWord(bottom + sizeof(uptr), &access->frameDescription);
    } else {
        return false;
    }
    const uptr aligned = RoundDown(address, sizeof(uptr));
    uptr memory = RoundDown(aligned, Granularity);
    sptr shadow = static_cast<sptr>(reinterpret_cast<uptr>(ShadowOf(aligned)));
    const sptr shadowBottom = static_cast<sptr>(reinterpret_cast<uptr>(ShadowOf(bottom)));
    while (shadow >= shadowBottom && *reinterpret_cast<const u8*>(shadow) != StackLeftRedzoneMagic) {
        --shadow;
        memory -= Granularity;
    }
    while (shadow >= shadowBottom && *reinterpret_cast<const u8*>(shadow) == StackLeftRedzoneMagic) {
        --shadow;
        memory -= Granularity;
    }
    if (shadow < shadowBottom) return false;
    const uptr frame = memory + Granularity;
    uptr magic = 0;
    if (!ReadWord(frame, &magic) || magic != CurrentStackFrameMagic) return false;
    access->offset = address - frame;
    return ReadWord(frame + 2 * sizeof(uptr), &access->framePc) && ReadWord(frame + sizeof(uptr), &access->frameDescription);
}

struct StackVariable {
    uptr begin;
    uptr size;
    std::string name;
    uptr line;
};

bool ParseNumber(const std::string& text, std::size_t* position, uptr* value) {
    while (*position < text.size() && (text[*position] == ' ' || text[*position] == '\t' || text[*position] == '\n')) ++*position;
    bool negative = false;
    if (*position < text.size() && (text[*position] == '-' || text[*position] == '+')) negative = text[(*position)++] == '-';
    uptr result = 0;
    while (*position < text.size() && text[*position] >= '0' && text[*position] <= '9') result = result * 10 + static_cast<uptr>(text[(*position)++] - '0');
    *value = negative ? static_cast<uptr>(-static_cast<sptr>(result)) : result;
    return true;
}

bool ParseFrameDescription(const std::string& text, std::vector<StackVariable>* variables) {
    std::size_t position = 0;
    uptr count = 0;
    ParseNumber(text, &position, &count);
    if (count == 0) return false;
    for (uptr index = 0; index < count; ++index) {
        uptr begin = 0;
        uptr size = 0;
        uptr length = 0;
        ParseNumber(text, &position, &begin);
        ParseNumber(text, &position, &size);
        ParseNumber(text, &position, &length);
        if (begin == 0 || size == 0 || position >= text.size() || text[position] != ' ') return false;
        ++position;
        if (length > text.size() - position) return false;
        std::string name = text.substr(position, length);
        uptr line = 0;
        const std::size_t colon = name.find(':');
        if (colon != std::string::npos) {
            std::size_t linePosition = colon + 1;
            ParseNumber(name, &linePosition, &line);
            name.resize(colon);
        }
        variables->push_back(StackVariable{begin, size, name, line});
        position += length;
    }
    return true;
}

std::string AccessAndVariableIntersection(const StackVariable& variable, uptr address, uptr accessSize, uptr previousEnd, uptr nextBegin) {
    const uptr variableEnd = variable.begin + variable.size;
    const uptr addressEnd = address + accessSize;
    const char* position = nullptr;
    if (address >= variable.begin) {
        if (addressEnd <= variableEnd) position = "is inside";
        else if (address < variableEnd) position = "partially overflows";
        else if (addressEnd <= nextBegin && nextBegin - addressEnd >= address - variableEnd) position = "overflows";
    } else {
        if (addressEnd > variable.begin) position = "partially underflows";
        else if (address >= previousEnd && address - previousEnd >= variable.begin - addressEnd) position = "underflows";
    }
    std::string text = Format("    [%zd, %zd) '%s'", static_cast<std::ptrdiff_t>(variable.begin), static_cast<std::ptrdiff_t>(variableEnd), variable.name.c_str());
    if (variable.line > 0) text += Format(" (line %zd)", static_cast<std::ptrdiff_t>(variable.line));
    if (position != nullptr) text += Format(" <== Memory access at offset %zd %s this variable\n", static_cast<std::ptrdiff_t>(address), position);
    else text += "\n";
    return text;
}

bool StackAccessForAddress(uptr address, StackAccess* access) {
    ThreadState* const thread = FindThreadByStackAddress(address);
    if (thread == nullptr) return false;
    access->thread = thread;
    access->hasFrame = StackFrameAccessByAddr(*thread, address, access);
    return true;
}

}

bool CurrentStackBounds(uptr* bottom, uptr* top) {
    const ThreadState::Bounds bounds = CurrentThread().StackBounds();
    if (bounds.top == bounds.bottom) return false;
    *bottom = bounds.bottom;
    *top = bounds.top;
    return true;
}

void FakeStackHandleNoReturn() {
    if (FakeStack* const stack = CurrentThread().fakeStack) stack->HandleNoReturn();
}

bool DescribeStackAddress(uptr address, uptr accessSize, std::string* text) {
    StackAccess access;
    if (!StackAccessForAddress(address, &access)) return false;
    *text = Format("Address %s is located in stack of thread T-1", Pointer(address).c_str());
    if (!access.hasFrame) {
        *text += "\n";
        return true;
    }
    *text += Format(" at offset %zu in frame\n", static_cast<std::size_t>(access.offset));
    *text += StackLine(access.framePc);
    const std::string description = GuestString(access.frameDescription);
    std::vector<StackVariable> variables;
    if (!ParseFrameDescription(description, &variables)) {
        *text += Format("AddressSanitizer can't parse the stack frame descriptor: |%s|\n", description.c_str());
        return true;
    }
    *text += Format("  This frame has %zu object(s):\n", variables.size());
    for (std::size_t index = 0; index < variables.size(); ++index) {
        const uptr previousEnd = index ? variables[index - 1].begin + variables[index - 1].size : 0;
        const uptr nextBegin = index + 1 < variables.size() ? variables[index + 1].begin : ~uptr{0};
        *text += AccessAndVariableIntersection(variables[index], access.offset, accessSize, previousEnd, nextBegin);
    }
    *text += "HINT: this may be a false positive if your program uses some custom stack unwind mechanism, swapcontext or vfork\n"
             "      (longjmp and C++ exceptions *are* supported)\n";
    return true;
}

bool StackRegionForAddress(uptr address, char* name, uptr nameSize, uptr* begin, uptr* size) {
    StackAccess access;
    if (!StackAccessForAddress(address, &access)) return false;
    *begin = 0;
    *size = 0;
    if (!access.hasFrame) return true;
    std::vector<StackVariable> variables;
    if (!ParseFrameDescription(GuestString(access.frameDescription), &variables)) return true;
    for (const auto& variable : variables) {
        if (access.offset <= variable.begin + variable.size) {
            if (name != nullptr && nameSize > 0) {
                const std::size_t count = std::min<std::size_t>(variable.name.size(), nameSize - 1);
                std::string copy = variable.name.substr(0, count);
                copy.push_back('\0');
                if (!GuestCopyOut_nid_no_patch(name, copy.data(), copy.size())) throw std::invalid_argument("guest AddressSanitizer: name buffer is not writable");
            }
            *begin = address - (access.offset - variable.begin);
            *size = variable.size;
            return true;
        }
    }
    return true;
}

}

using namespace Asan::Runtime;

#define APS5_ASAN_FAKE_STACK_CLASS(classId)                                                         \
    std::uintptr_t APS5_VABI __asan_stack_malloc_##classId(std::uintptr_t size) {                  \
        EnsureShadow();                                                                             \
        return OnMalloc(FakeStackFast(), classId, size);                                            \
    }                                                                                               \
    std::uintptr_t APS5_VABI __asan_stack_malloc_always_##classId(std::uintptr_t size) {           \
        EnsureShadow();                                                                             \
        return OnMalloc(FakeStackFastAlways(), classId, size);                                      \
    }                                                                                               \
    void APS5_VABI __asan_stack_free_##classId(std::uintptr_t pointer, std::uintptr_t size) {      \
        EnsureShadow();                                                                             \
        OnFree(pointer, classId, size);                                                             \
    }

extern "C" {

APS5_ASAN_FAKE_STACK_CLASS(0)
APS5_ASAN_FAKE_STACK_CLASS(1)
APS5_ASAN_FAKE_STACK_CLASS(2)
APS5_ASAN_FAKE_STACK_CLASS(3)
APS5_ASAN_FAKE_STACK_CLASS(4)
APS5_ASAN_FAKE_STACK_CLASS(5)
APS5_ASAN_FAKE_STACK_CLASS(6)
APS5_ASAN_FAKE_STACK_CLASS(7)
APS5_ASAN_FAKE_STACK_CLASS(8)
APS5_ASAN_FAKE_STACK_CLASS(9)
APS5_ASAN_FAKE_STACK_CLASS(10)

void* APS5_VABI __asan_get_current_fake_stack() {
    EnsureShadow();
    return FakeStackFast();
}

void* APS5_VABI __asan_addr_is_in_fake_stack(void* fakeStack, void* address, void** begin, void** end) {
    if (fakeStack == nullptr) return nullptr;
    auto* const stack = static_cast<FakeStack*>(fakeStack);
    uptr frameBegin = 0;
    uptr frameEnd = 0;
    const uptr frame = stack->AddrIsInFakeStack(reinterpret_cast<uptr>(address), &frameBegin, &frameEnd);
    if (frame == 0) return nullptr;
    FakeFrame header{};
    if (!GuestCopyIn_nid_no_patch(&header, reinterpret_cast<const void*>(frame), sizeof(header))) throw std::invalid_argument("guest AddressSanitizer: fake frame is not readable");
    if (header.magic != CurrentStackFrameMagic) return nullptr;
    if (begin != nullptr) {
        void* const value = reinterpret_cast<void*>(frameBegin);
        if (!GuestCopyOut_nid_no_patch(begin, &value, sizeof(value))) throw std::invalid_argument("guest AddressSanitizer: frame begin output is not writable");
    }
    if (end != nullptr) {
        void* const value = reinterpret_cast<void*>(frameEnd);
        if (!GuestCopyOut_nid_no_patch(end, &value, sizeof(value))) throw std::invalid_argument("guest AddressSanitizer: frame end output is not writable");
    }
    return reinterpret_cast<void*>(header.realStack);
}

void APS5_VABI __sanitizer_start_switch_fiber(void** fakeStackSave, const void* bottom, std::size_t size) {
    EnsureShadow();
    ThreadState& thread = CurrentThread();
    if (thread.stackSwitching.load(std::memory_order_relaxed)) {
        Emit(ReportLine("ERROR: starting fiber switch while in fiber switch\n"));
        Die();
    }
    thread.nextStackBottom = reinterpret_cast<uptr>(bottom);
    thread.nextStackTop = reinterpret_cast<uptr>(bottom) + size;
    thread.stackSwitching.store(true, std::memory_order_release);
    FakeStack* const current = thread.fakeStack;
    if (fakeStackSave != nullptr && !GuestCopyOut_nid_no_patch(fakeStackSave, &current, sizeof(current)))
        throw std::invalid_argument("guest AddressSanitizer: fake stack save slot is not writable");
    thread.fakeStack = nullptr;
    thread.fakeStackClaimed = false;
    if (fakeStackSave == nullptr && current != nullptr) current->Destroy();
}

void APS5_VABI __sanitizer_finish_switch_fiber(void* fakeStackSave, const void** bottomOld, std::size_t* sizeOld) {
    EnsureShadow();
    ThreadState& thread = CurrentThread();
    if (!thread.stackSwitching.load(std::memory_order_relaxed)) {
        Emit(ReportLine("ERROR: finishing a fiber switch that has not started\n"));
        Die();
    }
    if (fakeStackSave != nullptr) {
        thread.fakeStack = static_cast<FakeStack*>(fakeStackSave);
        thread.fakeStackClaimed = true;
    }
    if (bottomOld != nullptr) {
        const void* const value = reinterpret_cast<const void*>(thread.stackBottom);
        if (!GuestCopyOut_nid_no_patch(bottomOld, &value, sizeof(value))) throw std::invalid_argument("guest AddressSanitizer: old stack bottom output is not writable");
    }
    if (sizeOld != nullptr) {
        const std::size_t value = thread.stackTop - thread.stackBottom;
        if (!GuestCopyOut_nid_no_patch(sizeOld, &value, sizeof(value))) throw std::invalid_argument("guest AddressSanitizer: old stack size output is not writable");
    }
    thread.stackBottom = thread.nextStackBottom;
    thread.stackTop = thread.nextStackTop;
    thread.stackSwitching.store(false, std::memory_order_release);
    thread.nextStackTop = 0;
    thread.nextStackBottom = 0;
}

}
