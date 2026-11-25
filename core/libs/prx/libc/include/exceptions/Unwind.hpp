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
    std::uintptr_t regionEnd {};
    std::uintptr_t frameEntry {};
    std::uintptr_t frameEntrySize {};
    std::uintptr_t argumentsSize {};
    std::uintptr_t segment {};
    bool infoMissing {};
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

template<class T> inline bool TryRead(const Byte*& p, const Byte* end, T& value) {
    if (!p || !end || end < p || static_cast<std::size_t>(end - p) < sizeof(T)) return false;
    std::memcpy(&value, p, sizeof(value));
    p += sizeof(value);
    return true;
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

inline bool TryUleb(const Byte*& p, const Byte* end, Word& result) {
    result = 0;
    if (!p || !end) return false;
    for (unsigned shift = 0; shift < sizeof(Word) * 8; shift += 7) {
        if (p >= end) return false;
        Byte b = *p++;
        result |= Word(b & 127) << shift;
        if (!(b & 128)) return true;
    }
    return false;
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

inline bool TrySleb(const Byte*& p, const Byte* end, std::intptr_t& out) {
    Word result = 0;
    unsigned shift = 0;
    Byte b = 0;
    if (!p || !end) return false;
    constexpr unsigned bits = sizeof(Word) * 8;
    do {
        if (shift >= bits || p >= end) return false;
        b = *p++;
        result |= Word(b & 127) << shift;
        shift += 7;
    } while (b & 128);
    if (shift < bits && (b & 64)) result |= (~Word(0)) << shift;
    out = static_cast<std::intptr_t>(result);
    return true;
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

inline bool TryEncoded(const Byte*& p, const Byte* end, Byte encoding, Word& out, Word data = 0, Word function = 0, Word text = 0) {
    out = 0;
    if (encoding == 255) return true;
    if (!p || !end || end < p) return false;
    if ((encoding & 112) == 80) {
        const Byte* aligned = reinterpret_cast<const Byte*>((Word(p) + 7) & ~Word(7));
        if (aligned > end) return false;
        p = aligned;
    }
    Word location = Word(p), value = 0;
    switch (encoding & 15) {
    case 0: if (!TryRead(p, end, value)) return false; break;
    case 1: if (!TryUleb(p, end, value)) return false; break;
    case 2: { std::uint16_t v {}; if (!TryRead(p, end, v)) return false; value = v; break; }
    case 3: { std::uint32_t v {}; if (!TryRead(p, end, v)) return false; value = v; break; }
    case 4: { std::uint64_t v {}; if (!TryRead(p, end, v)) return false; value = static_cast<Word>(v); break; }
    case 9: { std::intptr_t v {}; if (!TrySleb(p, end, v)) return false; value = static_cast<Word>(v); break; }
    case 10: { std::int16_t v {}; if (!TryRead(p, end, v)) return false; value = static_cast<Word>(static_cast<std::intptr_t>(v)); break; }
    case 11: { std::int32_t v {}; if (!TryRead(p, end, v)) return false; value = static_cast<Word>(static_cast<std::intptr_t>(v)); break; }
    case 12: { std::int64_t v {}; if (!TryRead(p, end, v)) return false; value = static_cast<Word>(static_cast<std::intptr_t>(v)); break; }
    default: return false;
    }
    if (!value) {
        out = 0;
        return true;
    }
    switch (encoding & 112) {
    case 0: case 80: break;
    case 16: value += location; break;
    case 32: value += text; break;
    case 48: value += data; break;
    case 64: value += function; break;
    default: return false;
    }
    if (encoding & 128) std::memcpy(&value, reinterpret_cast<void*>(value), sizeof(value));
    out = value;
    return true;
}

inline unsigned EncodingSize(Byte encoding) {
    switch (encoding & 15) {
    case 0: case 4: case 12: return 8;
    case 2: case 10: return 2;
    case 3: case 11: return 4;
    default: std::abort();
    }
}

inline unsigned TryEncodingSize(Byte encoding) {
    switch (encoding & 15) {
    case 0: case 4: case 12: return 8;
    case 2: case 10: return 2;
    case 3: case 11: return 4;
    default: return 0;
    }
}

struct Lookup { Word pc; const Byte* fde {}; Word text {}; Word data {}; Word segment {}; const Byte* fdeEnd {}; const Byte* fdeBegin {}; };

inline bool SearchEhFrameHeader(const Byte* header, const Byte* headerEnd, Lookup& query) {
    if (!header || !headerEnd || headerEnd < header || static_cast<std::size_t>(headerEnd - header) < 4) return false;
    if (header[0] != 1 || header[3] == 255) return false;
    const Byte* p = header + 4;
    Word ehFramePtr {};
    if (!TryEncoded(p, headerEnd, header[1], ehFramePtr, Word(header))) return false;
    Word count {};
    if (!TryEncoded(p, headerEnd, header[2], count)) return false;
    const unsigned width = TryEncodingSize(header[3]);
    if (!width) return false;
    const std::size_t entrySize = static_cast<std::size_t>(width) * 2;
    if (count > static_cast<std::size_t>(headerEnd - p) / entrySize) return false;
    const Byte* table = p;
    Word lo = 0, hi = count;
    while (lo < hi) {
        const Word mid = lo + (hi - lo) / 2;
        p = table + mid * entrySize;
        Word begin {};
        if (!TryEncoded(p, headerEnd, header[3], begin, Word(header))) return false;
        if (begin <= query.pc) lo = mid + 1; else hi = mid;
    }
    if (!lo) return false;
    p = table + (lo - 1) * entrySize + width;
    Word fdeAddr {};
    if (!TryEncoded(p, headerEnd, header[3], fdeAddr, Word(header))) return false;
    query.fde = reinterpret_cast<const Byte*>(fdeAddr);
    return true;
}

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
    if (query.fdeBegin && p < query.fdeBegin) return false;
    std::uint32_t length {};
    if (query.fdeEnd) {
        if (query.fdeEnd < p || !TryRead(p, query.fdeEnd, length)) return false;
    } else {
        length = Read<std::uint32_t>(p);
    }
    if (length < 4 || length == 0xffffffff) return false;
    if (Word(p) > ~Word(0) - length) return false;
    if (query.fdeEnd && static_cast<std::size_t>(query.fdeEnd - p) < length) return false;
    frame.end = p + length;
    const Byte* ciePointer = p;
    std::uint32_t cieOffset {};
    if (!TryRead(p, frame.end, cieOffset)) return false;
    if (!cieOffset || Word(ciePointer) < Word(cieOffset)) return false;
    const Byte* cie = ciePointer - cieOffset;
    if (query.fdeBegin && cie < query.fdeBegin) return false;
    const Byte* c = cie;
    std::uint32_t cieLength {};
    if (query.fdeEnd) {
        if (c >= query.fdeEnd || !TryRead(c, query.fdeEnd, cieLength)) return false;
    } else {
        cieLength = Read<std::uint32_t>(c);
    }
    if (cieLength < 4 || cieLength == 0xffffffff) return false;
    if (Word(c) > ~Word(0) - cieLength) return false;
    if (query.fdeEnd && static_cast<std::size_t>(query.fdeEnd - c) < cieLength) return false;
    frame.cieEnd = c + cieLength;
    std::uint32_t cieId {};
    if (!TryRead(c, frame.cieEnd, cieId) || cieId != 0) return false;
    Byte version {};
    if (!TryRead(c, frame.cieEnd, version)) return false;
    if (version != 1 && version != 3 && version != 4) return false;
    augmentation = reinterpret_cast<const char*>(c);
    while (true) {
        if (c >= frame.cieEnd) return false;
        if (!*c++) break;
    }
    if (version == 4) {
        Byte addressSize {}, segmentSize {};
        if (!TryRead(c, frame.cieEnd, addressSize) || !TryRead(c, frame.cieEnd, segmentSize)) return false;
        if (addressSize != sizeof(Word) || segmentSize != 0) return false;
    }
    if (!TryUleb(c, frame.cieEnd, frame.codeAlign)) return false;
    if (!TrySleb(c, frame.cieEnd, frame.dataAlign)) return false;
    Word returnReg {};
    if (version == 1) {
        Byte reg {};
        if (!TryRead(c, frame.cieEnd, reg)) return false;
        returnReg = reg;
    } else {
        if (!TryUleb(c, frame.cieEnd, returnReg)) return false;
    }
#ifdef _WIN32
    if (returnReg >= 17 && returnReg != 32) return false;
#else
    if (returnReg >= 17) return false;
#endif
    frame.returnRegister = static_cast<unsigned>(returnReg);
    Byte pointerEncoding = 0;
    lsdaEncoding = 255;
    if (*augmentation == 'z') {
        Word size {};
        if (!TryUleb(c, frame.cieEnd, size)) return false;
        if (size > static_cast<Word>(frame.cieEnd - c)) return false;
        const Byte* end = c + size;
        for (const char* a = augmentation + 1; *a; ++a) {
            switch (*a) {
            case 'R': if (!TryRead(c, end, pointerEncoding)) return false; break;
            case 'L': if (!TryRead(c, end, lsdaEncoding)) return false; break;
            case 'P': {
                Byte encoding {};
                if (!TryRead(c, end, encoding)) return false;
                if (!TryEncoded(c, end, encoding, frame.personality, query.data, 0, query.text)) return false;
                break;
            }
            case 'S': frame.signal = true; break;
            default: return false;
            }
        }
        c = end;
    } else if (*augmentation) return false;
    frame.cieBegin = c;
    if (!TryEncoded(p, frame.end, pointerEncoding, frame.start, query.data, 0, query.text)) return false;
    if (!TryEncoded(p, frame.end, pointerEncoding & 15, frame.length)) return false;
    return true;
}

inline bool DecodeFdeAugmentation(Frame& frame, const Lookup& query, const Byte*& p, const char* augmentation, Byte lsdaEncoding) {
    if (!augmentation || *augmentation != 'z') return true;
    Word size {};
    if (!TryUleb(p, frame.end, size)) return false;
    if (size > static_cast<Word>(frame.end - p)) return false;
    const Byte* end = p + size;
    if (lsdaEncoding != 255 && !TryEncoded(p, end, lsdaEncoding, frame.lsda, query.data, frame.start, query.text)) return false;
    p = end;
    return true;
}

struct Rule { unsigned kind {}; std::intptr_t value {}; const Byte* expression {}; };
#ifdef _WIN32
constexpr unsigned RuleCount = 33;
#else
constexpr unsigned RuleCount = 17;
#endif

struct Rules { Rule registers[RuleCount] {}; unsigned cfaRegister {7}; std::intptr_t cfaOffset {}; const Byte* cfaExpression {}; Word argumentsSize {}; };

inline bool Instructions(const Byte* p, const Byte* end, const Frame& frame, Word target, Rules& state, const Rules& initial) {
    Word location = frame.start;
    Rules saved[16];
    unsigned depth = 0;
    while (p < end && location <= target) {
        Byte opcode = *p++;
        if ((opcode & 192) == 64) { location += (opcode & 63) * frame.codeAlign; continue; }
        if ((opcode & 192) == 128) {
            unsigned reg = opcode & 63;
            Word rawOffset {};
            if (!TryUleb(p, end, rawOffset)) return false;
            auto offset = static_cast<std::intptr_t>(rawOffset * static_cast<Word>(frame.dataAlign));
            if (reg < RuleCount) state.registers[reg] = {1, offset};
            continue;
        }
        if ((opcode & 192) == 192) {
            unsigned reg = opcode & 63;
            if (reg < RuleCount) state.registers[reg] = initial.registers[reg];
            continue;
        }
        switch (opcode) {
        case 0: break;
        case 1: {
            Word newLocation {};
            if (!TryRead(p, end, newLocation)) return false;
            location = newLocation;
            break;
        }
        case 2: {
            Byte delta {};
            if (!TryRead(p, end, delta)) return false;
            location += delta * frame.codeAlign;
            break;
        }
        case 3: {
            std::uint16_t delta {};
            if (!TryRead(p, end, delta)) return false;
            location += delta * frame.codeAlign;
            break;
        }
        case 4: {
            std::uint32_t delta {};
            if (!TryRead(p, end, delta)) return false;
            location += delta * frame.codeAlign;
            break;
        }
        case 5: case 17: case 20: case 21: {
            Word reg {};
            if (!TryUleb(p, end, reg)) return false;
            Word rawOffset {};
            if (opcode == 17 || opcode == 21) {
                std::intptr_t s {};
                if (!TrySleb(p, end, s)) return false;
                rawOffset = static_cast<Word>(s);
            } else {
                if (!TryUleb(p, end, rawOffset)) return false;
            }
            auto offset = static_cast<std::intptr_t>(rawOffset * static_cast<Word>(frame.dataAlign));
            if (reg < RuleCount) state.registers[reg] = {unsigned(opcode >= 20 ? 4 : 1), offset};
            break;
        }
        case 6: {
            Word reg {};
            if (!TryUleb(p, end, reg)) return false;
            if (reg < RuleCount) state.registers[reg] = initial.registers[reg];
            break;
        }
        case 7: case 8: {
            Word reg {};
            if (!TryUleb(p, end, reg)) return false;
            if (reg < RuleCount) state.registers[reg] = {unsigned(opcode == 7 ? 5 : 0)};
            break;
        }
        case 9: {
            Word reg {}, other {};
            if (!TryUleb(p, end, reg) || !TryUleb(p, end, other)) return false;
            if (reg < RuleCount) state.registers[reg] = {2, static_cast<std::intptr_t>(other)};
            break;
        }
        case 10: if (depth == 16) return false; saved[depth++] = state; break;
        case 11: if (!depth) return false; state = saved[--depth]; break;
        case 12: case 18: {
            Word reg {};
            if (!TryUleb(p, end, reg)) return false;
            Word cfaOffset {};
            if (opcode == 18) {
                std::intptr_t s {};
                if (!TrySleb(p, end, s)) return false;
                cfaOffset = static_cast<Word>(s) * static_cast<Word>(frame.dataAlign);
            } else {
                if (!TryUleb(p, end, cfaOffset)) return false;
            }
            state.cfaExpression = nullptr;
            state.cfaRegister = static_cast<unsigned>(reg);
            state.cfaOffset = static_cast<std::intptr_t>(cfaOffset);
            break;
        }
        case 13: {
            Word reg {};
            if (!TryUleb(p, end, reg)) return false;
            state.cfaRegister = static_cast<unsigned>(reg);
            break;
        }
        case 14: case 19: {
            Word cfaOffset {};
            if (opcode == 19) {
                std::intptr_t s {};
                if (!TrySleb(p, end, s)) return false;
                cfaOffset = static_cast<Word>(s) * static_cast<Word>(frame.dataAlign);
            } else {
                if (!TryUleb(p, end, cfaOffset)) return false;
            }
            state.cfaOffset = static_cast<std::intptr_t>(cfaOffset);
            break;
        }
        case 15: {
            const Byte* expr = p;
            Word size {};
            if (!TryUleb(p, end, size) || size > static_cast<Word>(end - p)) return false;
            p += size;
            state.cfaExpression = expr;
            break;
        }
        case 16: case 22: {
            Word reg {};
            if (!TryUleb(p, end, reg)) return false;
            const Byte* expr = p;
            Word size {};
            if (!TryUleb(p, end, size) || size > static_cast<Word>(end - p)) return false;
            p += size;
            if (reg < RuleCount) state.registers[reg] = {unsigned(opcode == 16 ? 3 : 6), 0, expr};
            break;
        }
        case 46: {
            Word argsSize {};
            if (!TryUleb(p, end, argsSize)) return false;
            state.argumentsSize = argsSize;
            break;
        }
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
