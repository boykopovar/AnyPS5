#pragma once

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <unwind.h>

struct _Unwind_Context {
    std::uintptr_t registers[17] {};
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
}

extern "C" {
_Unwind_Reason_Code __gxx_personality_v0_nid_postfix(int, _Unwind_Action, std::uint64_t, _Unwind_Exception*, _Unwind_Context*);
_Unwind_Reason_Code _Unwind_RaiseException_nid_postfix(_Unwind_Exception*);
[[noreturn]] void _Unwind_Resume_nid_postfix(_Unwind_Exception*);
_Unwind_Reason_Code _Unwind_Resume_or_Rethrow_nid_postfix(_Unwind_Exception*);
void _Unwind_DeleteException_nid_postfix(_Unwind_Exception*);
}
