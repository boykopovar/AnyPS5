#ifndef CORE_LIBS_PRX_LIBC_INCLUDE_EXCEPTIONS_UNWIND_HPP
#define CORE_LIBS_PRX_LIBC_INCLUDE_EXCEPTIONS_UNWIND_HPP

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>

#include "prx/libc/include/General.hpp"
#include "prx/libc/include/specifics/itanium/UnwindAbi.hpp"

extern "C" const std::uintptr_t* UnwindCursorSlots_nid_no_patch();

struct _Unwind_Context {
    const std::uintptr_t* cursor = UnwindCursorSlots_nid_no_patch();
    std::uintptr_t registers[17] {};
#ifdef _WIN32
    std::uintptr_t vectorRegisters[16][2] {};
#endif
    std::uintptr_t cfa {};
    std::uintptr_t region {};
    std::uintptr_t lsda {};
    std::uintptr_t textBase {};
    std::uintptr_t dataBase {};
    std::uintptr_t personality {};
    bool signalFrame {};
};

namespace LibcUnwind {
using Byte = unsigned char;
using Word = std::uintptr_t;

template<class T> T Read(const Byte*& p) {
    T value;
    std::memcpy(&value, p, sizeof(value));
    p += sizeof(value);
    return value;
}

inline Word Uleb(const Byte*& p) {
    Word result = 0;
    for (unsigned shift = 0; shift < 64; shift += 7) {
        Byte b = *p++;
        result |= Word(b & 127) << shift;
        if (!(b & 128)) return result;
    }
    std::abort();
}

inline std::intptr_t Sleb(const Byte*& p) {
    Word result = 0;
    unsigned shift = 0;
    Byte b;
    do {
        if (shift >= 64) std::abort();
        b = *p++;
        result |= Word(b & 127) << shift;
        shift += 7;
    } while (b & 128);
    if (shift < 64 && (b & 64)) result |= (~Word(0)) << shift;
    return static_cast<std::intptr_t>(result);
}

inline Word Encoded(const Byte*& p, Byte encoding, Word data = 0, Word function = 0, Word text = 0) {
    if (encoding == 255) return 0;
    if ((encoding & 112) == 80) p = reinterpret_cast<const Byte*>((Word(p) + 7) & ~Word(7));
    Word location = Word(p), value;
    switch (encoding & 15) {
    case 0: value = Read<Word>(p); break;
    case 1: value = Uleb(p); break;
    case 2: value = Read<std::uint16_t>(p); break;
    case 3: value = Read<std::uint32_t>(p); break;
    case 4: value = Read<std::uint64_t>(p); break;
    case 9: value = Sleb(p); break;
    case 10: value = Read<std::int16_t>(p); break;
    case 11: value = Read<std::int32_t>(p); break;
    case 12: value = Read<std::int64_t>(p); break;
    default: std::abort();
    }
    if (!value) return 0;
    switch (encoding & 112) {
    case 0: case 80: break;
    case 16: value += location; break;
    case 32: value += text; break;
    case 48: value += data; break;
    case 64: value += function; break;
    default: std::abort();
    }
    if (encoding & 128) std::memcpy(&value, reinterpret_cast<void*>(value), sizeof(value));
    return value;
}

inline unsigned EncodingSize(Byte encoding) {
    switch (encoding & 15) {
    case 0: case 4: case 12: return 8;
    case 2: case 10: return 2;
    case 3: case 11: return 4;
    default: std::abort();
    }
}

struct Lookup { Word pc; const Byte* fde {}; Word text {}; Word data {}; };

struct Frame {
    const Byte* cieBegin {};
    const Byte* cieEnd {};
    const Byte* begin {};
    const Byte* end {};
    Word start {}, length {}, personality {}, lsda {};
    Word codeAlign {};
    std::intptr_t dataAlign {};
    unsigned returnRegister {};
    bool signal {};
#ifdef __APPLE__
    bool useCompact {};
    std::uint32_t compactEncoding {};
    Word functionStart {};
    Word functionEnd {};
#endif
#ifdef _WIN32
    bool foreign {};
    _Unwind_Context caller {};
#endif
};

inline bool DecodeRange(Frame& frame, const Lookup& query, const Byte*& p, const char*& augmentation, Byte& lsdaEncoding) {
    if (!query.fde) return false;
    p = query.fde;
    auto length = Read<std::uint32_t>(p);
    if (!length || length == 0xffffffff) return false;
    frame.end = p + length;
    const Byte* ciePointer = p;
    auto cieOffset = Read<std::uint32_t>(p);
    const Byte* cie = ciePointer - cieOffset;
    const Byte* c = cie;
    auto cieLength = Read<std::uint32_t>(c);
    frame.cieEnd = c + cieLength;
    if (Read<std::uint32_t>(c) != 0) return false;
    Byte version = *c++;
    if (version != 1 && version != 3 && version != 4) return false;
    augmentation = reinterpret_cast<const char*>(c);
    while (*c++) {}
    if (version == 4 && (*c++ != sizeof(Word) || *c++ != 0)) return false;
    frame.codeAlign = Uleb(c);
    frame.dataAlign = Sleb(c);
    frame.returnRegister = version == 1 ? *c++ : Uleb(c);
#ifdef _WIN32
    if (frame.returnRegister >= 17 && frame.returnRegister != 32) return false;
#else
    if (frame.returnRegister >= 17) return false;
#endif
    Byte pointerEncoding = 0;
    lsdaEncoding = 255;
    if (*augmentation == 'z') {
        Word size = Uleb(c);
        const Byte* end = c + size;
        for (const char* a = augmentation + 1; *a; ++a) {
            switch (*a) {
            case 'R': pointerEncoding = *c++; break;
            case 'L': lsdaEncoding = *c++; break;
            case 'P': { Byte encoding = *c++; frame.personality = Encoded(c, encoding, query.data, 0, query.text); break; }
            case 'S': frame.signal = true; break;
            default: return false;
            }
        }
        c = end;
    } else if (*augmentation) return false;
    frame.cieBegin = c;
    frame.start = Encoded(p, pointerEncoding, query.data, 0, query.text);
    frame.length = Encoded(p, pointerEncoding & 15);
    return true;
}

struct Rule { unsigned kind {}; std::intptr_t value {}; const Byte* expression {}; };
#ifdef _WIN32
constexpr unsigned RuleCount = 33;
#else
constexpr unsigned RuleCount = 17;
#endif

struct Rules { Rule registers[RuleCount] {}; unsigned cfaRegister {7}; std::intptr_t cfaOffset {}; const Byte* cfaExpression {}; };

inline bool Instructions(const Byte* p, const Byte* end, const Frame& frame, Word target, Rules& state, const Rules& initial) {
    Word location = frame.start;
    Rules saved[16];
    unsigned depth = 0;
    while (p < end && location <= target) {
        Byte opcode = *p++;
        unsigned reg;
        if ((opcode & 192) == 64) { location += (opcode & 63) * frame.codeAlign; continue; }
        if ((opcode & 192) == 128) {
            reg = opcode & 63;
            auto offset = Uleb(p) * frame.dataAlign;
            if (reg < RuleCount) state.registers[reg] = {1, static_cast<std::intptr_t>(offset)};
            continue;
        }
        if ((opcode & 192) == 192) {
            reg = opcode & 63;
            if (reg < RuleCount) state.registers[reg] = initial.registers[reg];
            continue;
        }
        switch (opcode) {
        case 0: break;
        case 1: location = Read<Word>(p); break;
        case 2: location += Read<Byte>(p) * frame.codeAlign; break;
        case 3: location += Read<std::uint16_t>(p) * frame.codeAlign; break;
        case 4: location += Read<std::uint32_t>(p) * frame.codeAlign; break;
        case 5: case 17: case 20: case 21: {
            reg = Uleb(p);
            auto offset = (opcode == 17 || opcode == 21 ? Sleb(p) : std::intptr_t(Uleb(p))) * frame.dataAlign;
            if (reg < RuleCount) state.registers[reg] = {unsigned(opcode >= 20 ? 4 : 1), offset};
            break;
        }
        case 6: reg = Uleb(p); if (reg < RuleCount) state.registers[reg] = initial.registers[reg]; break;
        case 7: case 8: reg = Uleb(p); if (reg < RuleCount) state.registers[reg] = {unsigned(opcode == 7 ? 5 : 0)}; break;
        case 9: { reg = Uleb(p); auto other = Uleb(p); if (reg < RuleCount) state.registers[reg] = {2, std::intptr_t(other)}; break; }
        case 10: if (depth == 16) return false; saved[depth++] = state; break;
        case 11: if (!depth) return false; state = saved[--depth]; break;
        case 12: case 18:
            state.cfaExpression = nullptr; state.cfaRegister = Uleb(p);
            state.cfaOffset = opcode == 18 ? Sleb(p) * frame.dataAlign : Uleb(p); break;
        case 13: state.cfaRegister = Uleb(p); break;
        case 14: case 19: state.cfaOffset = opcode == 19 ? Sleb(p) * frame.dataAlign : Uleb(p); break;
        case 15: { state.cfaExpression = p; auto size = Uleb(p); p += size; break; }
        case 16: case 22: {
            reg = Uleb(p); const Byte* expr = p; auto size = Uleb(p); p += size;
            if (reg < RuleCount) state.registers[reg] = {unsigned(opcode == 16 ? 3 : 6), 0, expr};
            break;
        }
        case 46: Uleb(p); break;
        default: return false;
        }
    }
    return true;
}
}

extern "C" {
    _Unwind_Reason_Code APS5_VABI __gxx_personality_v0_nid_postfix(int, _Unwind_Action, std::uint64_t, _Unwind_Exception*, _Unwind_Context*);
    _Unwind_Reason_Code APS5_VABI __gcc_personality_v0_nid_postfix(int, _Unwind_Action, std::uint64_t, _Unwind_Exception*, _Unwind_Context*);
    _Unwind_Reason_Code APS5_VABI _Unwind_RaiseException_nid_postfix(_Unwind_Exception*);
    [[noreturn]] void APS5_VABI _Unwind_Resume_nid_postfix(_Unwind_Exception*);
    _Unwind_Reason_Code APS5_VABI _Unwind_Resume_or_Rethrow_nid_postfix(_Unwind_Exception*);
    void APS5_VABI _Unwind_DeleteException_nid_postfix(_Unwind_Exception*);
}

#endif
