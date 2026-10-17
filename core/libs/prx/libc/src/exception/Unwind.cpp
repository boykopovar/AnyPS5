#include "prx/libc/include/exceptions/Unwind.hpp"
#include "prx/libc/include/exceptions/Runtime.hpp"
#include "prx/libc/include/specifics/linux/ElfTypes.hpp"
#include "prx/libc/src/specifics/x86_64/RegisterContext.cpp"
#include <cstdio>

#if defined(__linux__)
#include <dlfcn.h>
#endif
#ifdef _WIN32
#include <windows.h>
#include <algorithm>
#include <array>
#include <map>
#include <mutex>
#include <utility>
#include <vector>
#endif
#ifdef __APPLE__
#include <dlfcn.h>
#include <mach-o/getsect.h>
#include <mach-o/loader.h>
#endif

#if defined(__linux__) || defined(_WIN32) || defined(__APPLE__)

extern "C" _Unwind_Reason_Code __gxx_personality_v0(int, _Unwind_Action, std::uint64_t, _Unwind_Exception*, _Unwind_Context*);
extern "C" _Unwind_Reason_Code __gcc_personality_v0(int, _Unwind_Action, std::uint64_t, _Unwind_Exception*, _Unwind_Context*);

namespace LibcUnwind {
_Unwind_Reason_Code CallPersonality(Word personality, _Unwind_Action actions, _Unwind_Exception* exception, _Unwind_Context* context) {
#ifdef _WIN32
    const auto* code = reinterpret_cast<const Byte*>(personality);
    if (code[0] == 0xff && code[1] == 0x25) {
        std::int32_t displacement;
        std::memcpy(&displacement, code + 2, 4);
        std::memcpy(&personality, code + 6 + displacement, sizeof(personality));
    }
#endif
    if (personality == reinterpret_cast<Word>(__gxx_personality_v0_nid_postfix) || personality == reinterpret_cast<Word>(__gxx_personality_v0))
        return __gxx_personality_v0_nid_postfix(1, actions, exception->exception_class, exception, context);
    if (personality == reinterpret_cast<Word>(__gcc_personality_v0_nid_postfix) || personality == reinterpret_cast<Word>(__gcc_personality_v0))
        return __gcc_personality_v0_nid_postfix(1, actions, exception->exception_class, exception, context);
    return (actions & _UA_SEARCH_PHASE) ? _URC_FATAL_PHASE1_ERROR : _URC_FATAL_PHASE2_ERROR;
}
#if defined(__linux__) || defined(__APPLE__)
int FindFrame(dl_phdr_info* info, std::size_t, void* argument) {
    auto& query = *static_cast<Lookup*>(argument);
    const Byte* header = nullptr;
    bool contains = false;
    for (unsigned i = 0; i < info->dlpi_phnum; ++i) {
        const auto& ph = info->dlpi_phdr[i];
        Word start = info->dlpi_addr + ph.p_vaddr;
        if (ph.p_type == PT_LOAD && query.pc >= start && query.pc - start < ph.p_memsz) {
            contains = true;
            query.segment = start;
        }
        if (ph.p_type == PT_GNU_EH_FRAME) header = reinterpret_cast<const Byte*>(start);
        if (ph.p_type == PT_LOAD && (ph.p_flags & PF_X)) query.text = start;
        if (ph.p_type == PT_LOAD && (ph.p_flags & PF_W)) query.data = start;
    }
    if (!contains || !header || header[0] != 1) return 0;
    const Byte* p = header + 4;
    Encoded(p, header[1], Word(header));
    Word count = Encoded(p, header[2]);
    if (header[3] == 255) return 1;
    unsigned width = EncodingSize(header[3]);
    const Byte* table = p;
    Word lo = 0, hi = count;
    while (lo < hi) {
        Word mid = lo + (hi - lo) / 2;
        p = table + mid * width * 2;
        Word begin = Encoded(p, header[3], Word(header));
        if (begin <= query.pc) lo = mid + 1; else hi = mid;
    }
    if (lo) {
        p = table + (lo - 1) * width * 2 + width;
        query.fde = reinterpret_cast<const Byte*>(Encoded(p, header[3], Word(header)));
    }
    return 1;
}

#endif

bool DecodeCandidate(_Unwind_Context& context, Frame& frame, const Lookup& query) {
    const Byte* p;
    const char* augmentation;
    Byte lsdaEncoding;
    if (!DecodeRange(frame, query, p, augmentation, lsdaEncoding)) return false;
    if (query.pc < frame.start || query.pc - frame.start >= frame.length) return false;
    if (*augmentation == 'z') {
        Word size = Uleb(p);
        const Byte* end = p + size;
        if (lsdaEncoding != 255) frame.lsda = Encoded(p, lsdaEncoding, query.data, frame.start, query.text);
        p = end;
    }
    frame.begin = p;
    context.region = frame.start;
    context.lsda = frame.lsda;
    context.personality = frame.personality;
    context.textBase = query.text;
    context.dataBase = query.data;
    context.regionEnd = frame.start + frame.length;
    context.frameEntry = Word(query.fde);
    context.frameEntrySize = Word(frame.end - query.fde);
    context.segment = query.segment;
    return true;
}

#ifdef _WIN32
using FrameIndex = std::vector<std::pair<Word, const Byte*>>;
using ImageKey = std::array<Word, 6>;

const FrameIndex& IndexFrames(const ImageKey& key, const Byte* begin, const Byte* end) {
    static std::mutex mutex;
    static std::map<ImageKey, FrameIndex> indexes;
    std::lock_guard lock(mutex);
    const auto [entry, inserted] = indexes.try_emplace(key);
    auto& index = entry->second;
    if (!inserted) return index;
    const Byte* p = begin;
    while (end - p >= 8) {
        const Byte* record = p;
        const auto length = Read<std::uint32_t>(p);
        if (!length) continue;
        if (length == 0xffffffff || length < 4 || Word(end - p) < length) break;
        const Byte* next = p + length;
        if (Read<std::uint32_t>(p)) {
            Frame candidate {};
            const Byte* cursor;
            const char* augmentation;
            Byte lsdaEncoding;
            if (DecodeRange(candidate, Lookup {0, record}, cursor, augmentation, lsdaEncoding))
                index.emplace_back(candidate.start, record);
        }
        p = next;
    }
    std::sort(index.begin(), index.end());
    return index;
}

const IMAGE_NT_HEADERS64* ImageHeaders(Word pc, const Byte*& base) {
    void* image = nullptr;
    if (!RtlPcToFileHeader(reinterpret_cast<void*>(pc), &image) || !image) return nullptr;
    base = static_cast<const Byte*>(image);
    const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) return nullptr;
    const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(base + dos->e_lfanew);
    return nt->Signature == IMAGE_NT_SIGNATURE ? nt : nullptr;
}

bool NearestExport(Word pc, const char*& name, Word& address) {
    const Byte* base = nullptr;
    const auto* nt = ImageHeaders(pc, base);
    if (!nt) return false;
    const auto& directory = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
    if (!directory.VirtualAddress || !directory.Size) return false;
    const auto* exports = reinterpret_cast<const IMAGE_EXPORT_DIRECTORY*>(base + directory.VirtualAddress);
    const auto* functions = reinterpret_cast<const DWORD*>(base + exports->AddressOfFunctions);
    const auto* names = reinterpret_cast<const DWORD*>(base + exports->AddressOfNames);
    const auto* ordinals = reinterpret_cast<const WORD*>(base + exports->AddressOfNameOrdinals);
    const Word rva = pc - Word(base);
    name = nullptr;
    DWORD nearest = 0;
    for (DWORD i = 0; i < exports->NumberOfNames; ++i) {
        if (ordinals[i] >= exports->NumberOfFunctions) continue;
        const DWORD target = functions[ordinals[i]];
        if (target - directory.VirtualAddress < directory.Size || target > rva || (name && target <= nearest)) continue;
        name = reinterpret_cast<const char*>(base + names[i]);
        nearest = target;
    }
    address = Word(base) + nearest;
    return name != nullptr;
}

bool VirtualUnwind(const _Unwind_Context& context, _Unwind_Context& caller) {
    const Word pc = context.registers[16];
    const Byte* base = nullptr;
    const auto* nt = ImageHeaders(pc, base);
    if (!nt) return false;
    const auto* sections = IMAGE_FIRST_SECTION(nt);
    for (unsigned i = 0; i < nt->FileHeader.NumberOfSections; ++i)
        if (std::memcmp(sections[i].Name, ".ehmeta", 8) == 0 || std::memcmp(sections[i].Name, ".ehfram", 8) == 0) return false;
    DWORD64 imageBase = 0;
    auto* function = RtlLookupFunctionEntry(pc, &imageBase, nullptr);
    if (!function) return false;
    CONTEXT state {};
    DWORD64* const registers[17] {&state.Rax, &state.Rdx, &state.Rcx, &state.Rbx, &state.Rsi, &state.Rdi, &state.Rbp, &state.Rsp,
        &state.R8, &state.R9, &state.R10, &state.R11, &state.R12, &state.R13, &state.R14, &state.R15, &state.Rip};
    for (unsigned i = 0; i < 17; ++i) *registers[i] = context.registers[i];
    for (unsigned i = 0; i < 16; ++i) std::memcpy(&state.FltSave.XmmRegisters[i], context.vectorRegisters[i], 16);
    void* handlerData = nullptr;
    DWORD64 establisher = 0;
    if (RtlVirtualUnwind(UNW_FLAG_EHANDLER | UNW_FLAG_UHANDLER, imageBase, pc, function, &state, &handlerData, &establisher, nullptr)) return false;
    caller = context;
    for (unsigned i = 0; i < 17; ++i) caller.registers[i] = *registers[i];
    for (unsigned i = 0; i < 16; ++i) std::memcpy(caller.vectorRegisters[i], &state.FltSave.XmmRegisters[i], 16);
    caller.signalFrame = false;
    return true;
}
#endif

bool DecodeFrame(_Unwind_Context& context, Frame& frame) {
    Lookup query {context.registers[16] - !context.signalFrame};
#if defined(__linux__) || defined(__APPLE__)
    dl_iterate_phdr(FindFrame, &query);
#ifdef __APPLE__
    if (!query.fde) {
        Dl_info image {};
        if (!dladdr(reinterpret_cast<void*>(query.pc), &image) || !image.dli_fbase) return false;
        query.segment = Word(image.dli_fbase);
        unsigned long size = 0;
        const Byte* p = getsectiondata(static_cast<const mach_header_64*>(image.dli_fbase), "__TEXT", "__eh_frame", &size);
        if (!p) return false;
        const Byte* end = p + size;
        while (end - p >= 8) {
            const Byte* record = p;
            const auto length = Read<std::uint32_t>(p);
            if (length == 0) return false;
            if (length == 0xffffffff || length < 4 || Word(end - p) < length) return false;
            const Byte* next = p + length;
            if (Read<std::uint32_t>(p)) {
                query.fde = record;
                frame = {};
                if (DecodeCandidate(context, frame, query)) return true;
            }
            p = next;
        }
        return false;
    }
#endif
    return DecodeCandidate(context, frame, query);
#else
    const Byte* base = nullptr;
    const auto* nt = ImageHeaders(query.pc, base);
    if (!nt) return false;
    const auto* sections = IMAGE_FIRST_SECTION(nt);
    for (unsigned i = 0; i < nt->FileHeader.NumberOfSections; ++i) {
        const Word start = Word(base) + sections[i].VirtualAddress;
        if (query.pc >= start && query.pc - start < sections[i].Misc.VirtualSize) query.segment = start;
    }
    for (unsigned i = 0; i < nt->FileHeader.NumberOfSections; ++i) {
        const auto& section = sections[i];
        if (std::memcmp(section.Name, ".ehmeta", 8) == 0) {
            const Byte* metadata = base + section.VirtualAddress;
            const Byte* header = base + Read<std::uint32_t>(metadata);
            if (header[0] != 1 || header[3] == 255) return false;
            const Byte* p = header + 4;
            Encoded(p, header[1], Word(header));
            const Word count = Encoded(p, header[2]);
            const auto width = EncodingSize(header[3]);
            const Byte* table = p;
            Word lo = 0, hi = count;
            while (lo < hi) {
                const Word mid = lo + (hi - lo) / 2;
                p = table + mid * width * 2;
                if (Encoded(p, header[3], Word(header)) <= query.pc) lo = mid + 1;
                else hi = mid;
            }
            if (!lo) return false;
            p = table + (lo - 1) * width * 2 + width;
            query.fde = reinterpret_cast<const Byte*>(Encoded(p, header[3], Word(header)));
            return DecodeCandidate(context, frame, query);
        }
        if (std::memcmp(section.Name, ".ehfram", 8) != 0) continue;
        const Byte* frames = base + section.VirtualAddress;
        const ImageKey key {Word(base), nt->FileHeader.TimeDateStamp, nt->OptionalHeader.CheckSum, nt->OptionalHeader.SizeOfImage, section.VirtualAddress, section.Misc.VirtualSize};
        const auto& index = IndexFrames(key, frames, frames + section.Misc.VirtualSize);
        auto found = std::upper_bound(index.begin(), index.end(), query.pc, [](Word pc, const std::pair<Word, const Byte*>& entry) { return pc < entry.first; });
        if (found == index.begin()) continue;
        query.fde = std::prev(found)->second;
        frame = {};
        if (DecodeCandidate(context, frame, query)) return true;
    }
    return false;
#endif
}

bool Expression(const Byte* p, const _Unwind_Context& context, Word cfa, Word& result) {
    Word length = Uleb(p);
    const Byte* end = p + length;
    Word stack[64]; unsigned size = 0;
    unsigned steps = 0;
    while (p < end && ++steps < 4096) {
        Byte op = *p++;
        if (size >= 62) return false;
        if (op >= 48 && op <= 79) { stack[size++] = op - 48; continue; }
        if (op >= 80 && op <= 96) { stack[size++] = context.registers[op - 80]; continue; }
        if (op >= 112 && op <= 128) { stack[size++] = context.registers[op - 112] + Sleb(p); continue; }
        switch (op) {
        case 3: stack[size++] = Read<Word>(p); break;
        case 6: if (!size) return false; std::memcpy(&stack[size-1], reinterpret_cast<void*>(stack[size-1]), 8); break;
        case 8: stack[size++] = Read<Byte>(p); break;
        case 9: stack[size++] = Read<std::int8_t>(p); break;
        case 10: stack[size++] = Read<std::uint16_t>(p); break;
        case 11: stack[size++] = Read<std::int16_t>(p); break;
        case 12: stack[size++] = Read<std::uint32_t>(p); break;
        case 13: stack[size++] = Read<std::int32_t>(p); break;
        case 14: case 15: stack[size++] = Read<Word>(p); break;
        case 16: stack[size++] = Uleb(p); break;
        case 17: stack[size++] = Sleb(p); break;
        case 18: if (!size) return false; stack[size] = stack[size-1]; ++size; break;
        case 19: if (!size) return false; --size; break;
        case 20: if (size < 2) return false; stack[size] = stack[size-2]; ++size; break;
        case 22: if (size < 2) return false; { Word x=stack[size-1]; stack[size-1]=stack[size-2]; stack[size-2]=x; } break;
        case 26: case 28: case 30: case 33: case 34: case 36: case 37: case 39: case 41: case 42: case 43: case 44: case 45: case 46: {
            if (size < 2) return false;
            Word b = stack[--size], &a = stack[size-1];
            switch (op) {
            case 26: a &= b; break; case 28: a -= b; break; case 30: a *= b; break;
            case 33: a |= b; break; case 34: a += b; break; case 36: a = b < 64 ? a << b : 0; break;
            case 37: a = b < 64 ? a >> b : 0; break; case 39: a ^= b; break;
            case 41: a = a == b; break; case 42: a = std::intptr_t(a) >= std::intptr_t(b); break;
            case 43: a = std::intptr_t(a) > std::intptr_t(b); break; case 44: a = std::intptr_t(a) <= std::intptr_t(b); break;
            case 45: a = std::intptr_t(a) < std::intptr_t(b); break; case 46: a = a != b; break;
            }
            break;
        }
        case 35: if (!size) return false; stack[size-1] += Uleb(p); break;
        case 146: { auto reg = Uleb(p); if (reg >= 17) return false; stack[size++] = context.registers[reg] + Sleb(p); break; }
        case 150: break;
        case 156: stack[size++] = cfa; break;
        case 159: break;
        default: return false;
        }
    }
    if (p != end || size != 1) return false;
    result = stack[0]; return true;
}

void ClearInfo(_Unwind_Context& context) {
    context.region = context.lsda = context.personality = context.textBase = context.dataBase = 0;
    context.regionEnd = context.frameEntry = context.frameEntrySize = context.argumentsSize = context.segment = 0;
}

bool FrameRules(_Unwind_Context& context, Frame& frame, Rules& rules) {
    if (!DecodeFrame(context, frame)) {
#ifdef _WIN32
        frame = {};
        if (!VirtualUnwind(context, frame.caller)) return false;
        frame.foreign = true;
        context.cfa = frame.caller.registers[7];
        return true;
#else
        return false;
#endif
    }
    Rules initial;
    if (!Instructions(frame.cieBegin, frame.cieEnd, frame, ~Word(0), initial, {})) return false;
#ifdef _WIN32
    if (frame.returnRegister == 32) {
        initial.registers[16] = initial.registers[32];
        initial.registers[32] = {};
        frame.returnRegister = 16;
    }
#endif
    rules = initial;
    if (!Instructions(frame.begin, frame.end, frame, context.registers[16] - !context.signalFrame, rules, initial)) return false;
    context.argumentsSize = rules.argumentsSize;
    if (rules.cfaExpression) return Expression(rules.cfaExpression, context, 0, context.cfa);
    if (rules.cfaRegister >= 17) return false;
    context.cfa = context.registers[rules.cfaRegister] + rules.cfaOffset;
    return true;
}

bool GetRules(_Unwind_Context& context, Frame& frame, Rules& rules) {
    ClearInfo(context);
    context.infoMissing = !FrameRules(context, frame, rules);
    if (context.infoMissing) ClearInfo(context);
    return !context.infoMissing;
}

bool Caller(const _Unwind_Context& context, const Frame& frame, const Rules& rules, _Unwind_Context& next) {
#ifdef _WIN32
    if (frame.foreign) {
        next = frame.caller;
        return true;
    }
#endif
    next = context;
    for (unsigned i = 0; i < 17; ++i) {
        const auto& rule = rules.registers[i];
        Word value;
        switch (rule.kind) {
        case 0: break;
        case 1: std::memcpy(&next.registers[i], reinterpret_cast<void*>(context.cfa + rule.value), 8); break;
        case 2: if (rule.value < 0 || rule.value >= 17) return false; next.registers[i] = context.registers[rule.value]; break;
        case 3: case 6:
            if (!Expression(rule.expression, context, context.cfa, value)) return false;
            if (rule.kind == 3) std::memcpy(&value, reinterpret_cast<void*>(value), 8);
            next.registers[i] = value; break;
        case 4: next.registers[i] = context.cfa + rule.value; break;
        case 5: next.registers[i] = 0; break;
        }
    }
#ifdef _WIN32
    for (unsigned i = 17; i < RuleCount; ++i) {
        const auto& rule = rules.registers[i];
        auto* destination = next.vectorRegisters[i - 17];
        if (rule.kind == 1) std::memcpy(destination, reinterpret_cast<void*>(context.cfa + rule.value), 16);
        else if (rule.kind == 2 && rule.value >= 17 && rule.value < RuleCount)
            std::memcpy(destination, context.vectorRegisters[rule.value - 17], 16);
        else if (rule.kind == 3) {
            Word address;
            if (!Expression(rule.expression, context, context.cfa, address)) return false;
            std::memcpy(destination, reinterpret_cast<const void*>(address), 16);
        }
        else if (rule.kind == 5) std::memset(destination, 0, 16);
        else if (rule.kind != 0) return false;
    }
#endif
    next.registers[7] = context.cfa;
    next.registers[16] = next.registers[frame.returnRegister];
    next.signalFrame = frame.signal;
    return true;
}

bool Moved(const _Unwind_Context& context, const _Unwind_Context& next) {
    return next.registers[7] != context.registers[7] || next.registers[16] != context.registers[16];
}

bool Step(_Unwind_Context& context) {
    Frame frame; Rules rules;
    _Unwind_Context next;
    if (!GetRules(context, frame, rules) || !Caller(context, frame, rules, next)) return false;
    if (!next.registers[16] || !Moved(context, next)) return false;
    context = next;
    return true;
}

_Unwind_Reason_Code PhaseTwo(_Unwind_Context context, _Unwind_Exception* exception) {
    for (unsigned depth = 0; depth < 65536; ++depth) {
        Frame frame; Rules rules;
        if (!GetRules(context, frame, rules)) {
            if (exception->private_1) {
                auto stop = reinterpret_cast<_Unwind_Stop_Fn>(exception->private_1);
                return stop(1, _Unwind_Action(_UA_FORCE_UNWIND | _UA_CLEANUP_PHASE | _UA_END_OF_STACK), exception->exception_class, exception, &context, reinterpret_cast<void*>(exception->private_2));
            }
            return _URC_END_OF_STACK;
        }
        {
            auto actions = _UA_CLEANUP_PHASE;
            if (exception->private_1) {
                actions = _Unwind_Action(actions | _UA_FORCE_UNWIND);
                auto stop = reinterpret_cast<_Unwind_Stop_Fn>(exception->private_1);
                auto result = stop(1, actions, exception->exception_class, exception, &context, reinterpret_cast<void*>(exception->private_2));
                if (result != _URC_NO_REASON) return result;
            } else if (context.cfa == exception->private_2) actions = _Unwind_Action(actions | _UA_HANDLER_FRAME);
            if (frame.personality) {
                auto result = CallPersonality(frame.personality, actions, exception, &context);
                if (result == _URC_INSTALL_CONTEXT) LibcRestoreRegisters(context.registers);
                if (result != _URC_CONTINUE_UNWIND) return _URC_FATAL_PHASE2_ERROR;
            }
        }
        if (!Step(context)) {
            if (exception->private_1) {
                auto stop = reinterpret_cast<_Unwind_Stop_Fn>(exception->private_1);
                return stop(1, _Unwind_Action(_UA_FORCE_UNWIND | _UA_CLEANUP_PHASE | _UA_END_OF_STACK), exception->exception_class, exception, &context, reinterpret_cast<void*>(exception->private_2));
            }
            return _URC_FATAL_PHASE2_ERROR;
        }
    }
    return _URC_FATAL_PHASE2_ERROR;
}
}

namespace {
constexpr int CursorInstructionRegister = -1;
constexpr int CursorStackRegister = -2;
constexpr int CursorRegisterCount = 17;
constexpr int CursorStepSuccess = 1;
constexpr int CursorStepEnd = 0;
constexpr int CursorBadFrame = -6546;
constexpr std::uint32_t CursorDwarfFormat = 0x04000000;

struct CursorProcedureInfo {
    std::uintptr_t start;
    std::uintptr_t end;
    std::uintptr_t lsda;
    std::uintptr_t handler;
    std::uintptr_t gp;
    std::uintptr_t flags;
    std::uint32_t format;
    std::uint32_t unwindInfoSize;
    std::uintptr_t unwindInfo;
    std::uintptr_t extra;
};

static_assert(sizeof(CursorProcedureInfo) == 72);

[[noreturn]] void CursorAbort(const char* function, const char* message) {
    std::fprintf(stderr, "libunwind: %s - %s\n", function, message);
    std::fflush(stderr);
    std::abort();
}

std::uintptr_t* CursorRegister(_Unwind_Context* context, int number) {
    if (number == CursorInstructionRegister) return &context->registers[16];
    if (number == CursorStackRegister) return &context->registers[7];
    if (number >= 0 && number < CursorRegisterCount) return &context->registers[number];
    return nullptr;
}

void CursorDescribe(_Unwind_Context& context) {
    LibcUnwind::Frame frame;
    LibcUnwind::Rules rules;
    if (context.registers[16]) {
        LibcUnwind::GetRules(context, frame, rules);
        return;
    }
    LibcUnwind::ClearInfo(context);
    context.infoMissing = true;
}

void APS5_VABI CursorDestroy(_Unwind_Context*) {}

bool APS5_VABI CursorValidRegister(_Unwind_Context* context, int number) {
    return CursorRegister(context, number) != nullptr;
}

std::uintptr_t APS5_VABI CursorGetRegister(_Unwind_Context* context, int number) {
    auto* value = CursorRegister(context, number);
    if (!value) CursorAbort("getRegister", "unsupported x86_64 register");
    return *value;
}

void APS5_VABI CursorSetRegister(_Unwind_Context* context, int number, std::uintptr_t value) {
    auto* slot = CursorRegister(context, number);
    if (!slot) CursorAbort("setRegister", "unsupported x86_64 register");
    *slot = value;
}

bool APS5_VABI CursorValidFloatRegister(_Unwind_Context*, int) {
    return false;
}

double APS5_VABI CursorGetFloatRegister(_Unwind_Context*, int) {
    CursorAbort("getFloatRegister", "no x86_64 float registers");
}

void APS5_VABI CursorSetFloatRegister(_Unwind_Context*, int, double) {
    CursorAbort("setFloatRegister", "no x86_64 float registers");
}

int APS5_VABI CursorStep(_Unwind_Context* context, bool) {
    if (context->infoMissing) return CursorStepEnd;
    LibcUnwind::Frame frame;
    LibcUnwind::Rules rules;
    _Unwind_Context current = *context;
    _Unwind_Context caller;
    if (!LibcUnwind::GetRules(current, frame, rules) || !LibcUnwind::Caller(current, frame, rules, caller)) return CursorBadFrame;
    if (caller.registers[16] && !LibcUnwind::Moved(current, caller)) return CursorStepEnd;
    CursorDescribe(caller);
    *context = caller;
    return context->infoMissing ? CursorStepEnd : CursorStepSuccess;
}

void APS5_VABI CursorGetInfo(_Unwind_Context* context, CursorProcedureInfo* info) {
    *info = {};
    if (context->infoMissing) return;
    info->start = context->region;
    info->end = context->regionEnd;
    info->lsda = context->lsda;
    info->handler = context->personality;
    info->gp = context->argumentsSize;
    info->format = context->frameEntry ? CursorDwarfFormat : 0;
    info->unwindInfoSize = static_cast<std::uint32_t>(context->frameEntrySize);
    info->unwindInfo = context->frameEntry;
    info->extra = context->segment;
}

[[noreturn]] void APS5_VABI CursorJumpTo(_Unwind_Context* context) {
    LibcRestoreRegisters(context->registers);
}

bool APS5_VABI CursorIsSignalFrame(_Unwind_Context* context) {
    return context->signalFrame;
}

bool APS5_VABI CursorGetFunctionName(_Unwind_Context* context, char* buffer, std::size_t length, std::uintptr_t* offset) {
    const auto pc = context->registers[16];
#ifdef _WIN32
    const char* name = nullptr;
    LibcUnwind::Word address = 0;
    if (!LibcUnwind::NearestExport(pc, name, address)) return false;
#else
    Dl_info image {};
    if (!dladdr(reinterpret_cast<void*>(pc), &image) || !image.dli_sname) return false;
    const char* name = image.dli_sname;
    const auto address = reinterpret_cast<std::uintptr_t>(image.dli_saddr);
#endif
    std::snprintf(buffer, length, "%s", name);
    *offset = pc - address;
    return true;
}

void APS5_VABI CursorSetInfoBasedOnIPRegister(_Unwind_Context* context, bool returnAddress) {
    _Unwind_Context described = *context;
    described.signalFrame = !returnAddress;
    CursorDescribe(described);
    described.signalFrame = context->signalFrame;
    described.cfa = context->cfa;
    *context = described;
}

const char* APS5_VABI CursorGetRegisterName(_Unwind_Context*, int number) {
    static constexpr const char* names[] {"rax", "rdx", "rcx", "rbx", "rsi", "rdi", "rbp", "rsp", "r8", "r9", "r10", "r11", "r12", "r13", "r14", "r15", "rip",
        "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6", "xmm7", "xmm8", "xmm9", "xmm10", "xmm11", "xmm12", "xmm13", "xmm14", "xmm15"};
    if (number == CursorInstructionRegister) return "rip";
    if (number == CursorStackRegister) return "rsp";
    if (number >= 0 && number < static_cast<int>(sizeof(names) / sizeof(names[0]))) return names[number];
    return "unknown register";
}

template<class Function> std::uintptr_t Slot(Function function) {
    return reinterpret_cast<std::uintptr_t>(function);
}

const std::uintptr_t CursorSlots[] = {
    Slot(&CursorDestroy),
    Slot(&CursorDestroy),
    Slot(&CursorValidRegister),
    Slot(&CursorGetRegister),
    Slot(&CursorSetRegister),
    Slot(&CursorValidFloatRegister),
    Slot(&CursorGetFloatRegister),
    Slot(&CursorSetFloatRegister),
    Slot(&CursorStep),
    Slot(&CursorGetInfo),
    Slot(&CursorJumpTo),
    Slot(&CursorIsSignalFrame),
    Slot(&CursorGetFunctionName),
    Slot(&CursorSetInfoBasedOnIPRegister),
    Slot(&CursorGetRegisterName),
};
}

const std::uintptr_t* UnwindCursorSlots_nid_no_patch() {
    return CursorSlots;
}

extern "C" {
_Unwind_Reason_Code APS5_VABI _Unwind_RaiseException_nid_postfix(_Unwind_Exception* exception) {
    _Unwind_Context start;
    LibcCaptureRegisters(start.registers);
    if (!LibcUnwind::Step(start)) return _URC_FATAL_PHASE1_ERROR;
    auto context = start;
    exception->private_1 = 0;
    for (unsigned depth = 0; depth < 65536; ++depth) {
        LibcUnwind::Frame frame; LibcUnwind::Rules rules;
        if (!LibcUnwind::GetRules(context, frame, rules)) return _URC_END_OF_STACK;
        if (frame.personality) {
            auto result = LibcUnwind::CallPersonality(frame.personality, _UA_SEARCH_PHASE, exception, &context);
            if (result == _URC_HANDLER_FOUND) {
                exception->private_2 = context.cfa;
                return LibcUnwind::PhaseTwo(start, exception);
            }
            if (result != _URC_CONTINUE_UNWIND) return _URC_FATAL_PHASE1_ERROR;
        }
        if (!LibcUnwind::Step(context)) return _URC_END_OF_STACK;
    }
    return _URC_FATAL_PHASE1_ERROR;
}

[[noreturn]] void APS5_VABI _Unwind_Resume_nid_postfix(_Unwind_Exception* exception) {
#if defined(__linux__)
    if (!LibcException::Native(exception->exception_class) && !exception->private_1) {
        using Resume = void (*)(_Unwind_Exception*);
        const auto resume = reinterpret_cast<Resume>(dlsym(RTLD_NEXT, "_Unwind_Resume"));
        if (!resume) std::abort();
        resume(exception);
    }
#endif
    _Unwind_Context context;
    LibcCaptureRegisters(context.registers);
    if (!LibcUnwind::Step(context)) std::abort();
    LibcUnwind::PhaseTwo(context, exception);
    std::abort();
}

_Unwind_Reason_Code APS5_VABI _Unwind_Resume_or_Rethrow_nid_postfix(_Unwind_Exception* exception) {
    if (exception->private_1) _Unwind_Resume_nid_postfix(exception);
    return _Unwind_RaiseException_nid_postfix(exception);
}

void APS5_VABI _Unwind_DeleteException_nid_postfix(_Unwind_Exception* exception) {
    if (exception && exception->exception_cleanup) exception->exception_cleanup(_URC_FOREIGN_EXCEPTION_CAUGHT, exception);
}

_Unwind_Word APS5_VABI _Unwind_GetGR_nid_postfix(_Unwind_Context* context, int index) {
    if (index < 0 || index >= 17) std::abort();
    return context->registers[index];
}
void APS5_VABI _Unwind_SetGR_nid_postfix(_Unwind_Context* context, int index, _Unwind_Word value) {
    if (index < 0 || index >= 17) std::abort();
    context->registers[index] = value;
}
_Unwind_Ptr APS5_VABI _Unwind_GetIP_nid_postfix(_Unwind_Context* context) { return context->registers[16]; }
void APS5_VABI _Unwind_SetIP_nid_postfix(_Unwind_Context* context, _Unwind_Ptr value) { context->registers[16] = value; }
_Unwind_Ptr APS5_VABI _Unwind_GetIPInfo_nid_postfix(_Unwind_Context* context, int* before) { if (before) *before = context->signalFrame; return context->registers[16]; }
_Unwind_Word APS5_VABI _Unwind_GetCFA_nid_postfix(_Unwind_Context* context) { return context->cfa; }
_Unwind_Ptr APS5_VABI _Unwind_GetLanguageSpecificData_nid_postfix(_Unwind_Context* context) { return context->lsda; }
_Unwind_Ptr APS5_VABI _Unwind_GetRegionStart_nid_postfix(_Unwind_Context* context) { return context->region; }
_Unwind_Ptr APS5_VABI _Unwind_GetDataRelBase_nid_postfix(_Unwind_Context* context) { return context->dataBase; }
_Unwind_Ptr APS5_VABI _Unwind_GetTextRelBase_nid_postfix(_Unwind_Context* context) { return context->textBase; }

_Unwind_Reason_Code APS5_VABI _Unwind_ForcedUnwind_nid_postfix(_Unwind_Exception* exception, _Unwind_Stop_Fn stop, void* argument) {
    if (!stop) return _URC_FATAL_PHASE2_ERROR;
    _Unwind_Context context;
    LibcCaptureRegisters(context.registers);
    if (!LibcUnwind::Step(context)) return _URC_FATAL_PHASE2_ERROR;
    exception->private_1 = reinterpret_cast<std::uintptr_t>(stop);
    exception->private_2 = reinterpret_cast<std::uintptr_t>(argument);
    return LibcUnwind::PhaseTwo(context, exception);
}

_Unwind_Reason_Code APS5_VABI _Unwind_Backtrace_nid_postfix(_Unwind_Trace_Fn trace, void* argument) {
    _Unwind_Context context;
    LibcCaptureRegisters(context.registers);
    if (!LibcUnwind::Step(context)) return _URC_END_OF_STACK;
    for (unsigned depth = 0; depth < 65536; ++depth) {
        LibcUnwind::Frame frame; LibcUnwind::Rules rules;
        if (!LibcUnwind::GetRules(context, frame, rules)) return _URC_END_OF_STACK;
        auto result = trace(&context, argument);
        if (result != _URC_NO_REASON) return result;
        if (!LibcUnwind::Step(context)) return _URC_END_OF_STACK;
    }
    return _URC_FATAL_PHASE1_ERROR;
}
}

#else

const std::uintptr_t* UnwindCursorSlots_nid_no_patch() {
    return nullptr;
}

extern "C" {
_Unwind_Reason_Code APS5_VABI _Unwind_RaiseException_nid_postfix(_Unwind_Exception*) { std::abort(); }
[[noreturn]] void APS5_VABI _Unwind_Resume_nid_postfix(_Unwind_Exception*) { std::abort(); }
_Unwind_Reason_Code APS5_VABI _Unwind_Resume_or_Rethrow_nid_postfix(_Unwind_Exception*) { std::abort(); }
void APS5_VABI _Unwind_DeleteException_nid_postfix(_Unwind_Exception* e) { if (e && e->exception_cleanup) e->exception_cleanup(_URC_FOREIGN_EXCEPTION_CAUGHT, e); }
_Unwind_Word APS5_VABI _Unwind_GetGR_nid_postfix(_Unwind_Context*, int) { std::abort(); }
void APS5_VABI _Unwind_SetGR_nid_postfix(_Unwind_Context*, int, _Unwind_Word) { std::abort(); }
_Unwind_Ptr APS5_VABI _Unwind_GetIP_nid_postfix(_Unwind_Context*) { std::abort(); }
void APS5_VABI _Unwind_SetIP_nid_postfix(_Unwind_Context*, _Unwind_Ptr) { std::abort(); }
_Unwind_Ptr APS5_VABI _Unwind_GetIPInfo_nid_postfix(_Unwind_Context*, int*) { std::abort(); }
_Unwind_Word APS5_VABI _Unwind_GetCFA_nid_postfix(_Unwind_Context*) { std::abort(); }
_Unwind_Ptr APS5_VABI _Unwind_GetLanguageSpecificData_nid_postfix(_Unwind_Context*) { std::abort(); }
_Unwind_Ptr APS5_VABI _Unwind_GetRegionStart_nid_postfix(_Unwind_Context*) { std::abort(); }
_Unwind_Ptr APS5_VABI _Unwind_GetDataRelBase_nid_postfix(_Unwind_Context*) { std::abort(); }
_Unwind_Ptr APS5_VABI _Unwind_GetTextRelBase_nid_postfix(_Unwind_Context*) { std::abort(); }
_Unwind_Reason_Code APS5_VABI _Unwind_ForcedUnwind_nid_postfix(_Unwind_Exception*, _Unwind_Stop_Fn, void*) { std::abort(); }
_Unwind_Reason_Code APS5_VABI _Unwind_Backtrace_nid_postfix(_Unwind_Trace_Fn, void*) { std::abort(); }
}

#endif
