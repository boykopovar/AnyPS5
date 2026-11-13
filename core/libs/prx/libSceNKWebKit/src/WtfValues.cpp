#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/include/WtfValueExports.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"
#include "prx/libSceNKWebKit/include/InventoryExports.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <new>
#include <stdexcept>
#include <string>
#include <utility>

namespace NKWebKit {
void* Allocate(std::size_t size, std::size_t alignment = 16);
void Deallocate(void* ptr);
}  // namespace NKWebKit

namespace {

using NKWebKit::WtfAbi::StringImpl;
using NKWebKit::WtfAbi::StringView;
using NKWebKit::WtfAbi::String;
using NKWebKit::WtfAbi::CString;
using NKWebKit::WtfAbi::URL;
using NKWebKit::WtfAbi::SplitResult;
using NKWebKit::WtfAbi::SplitResultIterator;
using NKWebKit::WtfAbi::Vector;
using NKWebKit::WtfAbi::SHA1;
using NKWebKit::WtfAbi::Encoder;
using NKWebKit::WtfAbi::Decoder;
using NKWebKit::WtfAbi::AtomString;

StringImpl* Retain(StringImpl* impl) {
    if (impl) impl->refCount += 2;
    return impl;
}

char16_t Unit(StringView view, unsigned index) {
    return view.is8Bit ? static_cast<const unsigned char*>(view.data)[index] : static_cast<const char16_t*>(view.data)[index];
}

unsigned HostStart(const URL& url) {
    return url.passwordEnd == url.userStart ? url.passwordEnd : url.passwordEnd + 1;
}

unsigned PathStart(const URL& url) {
    unsigned start = url.hostEnd + ((url.bits >> 3) & 7);
    if (!url.impl) return start;
    StringView view{url.impl->data, url.impl->length, !!(url.impl->hashAndFlags & 4)};
    if (start == (url.bits >> 6) + 1 && start < view.length && view.length - start > 1 && Unit(view, start) == '/' && Unit(view, start + 1) == '.') start += 2;
    return start;
}

String Substring(const StringImpl* impl, unsigned start, unsigned length) {
    if (!impl || start >= impl->length || !length) {
        return _ZN3WTF10StringImpl6createEPKhj(reinterpret_cast<const unsigned char*>(""), 0);
    }
    length = std::min(length, impl->length - start);
    return _ZN3WTF10StringImpl9substringEjj(impl, start, length);
}

String ConcatStrings(const String& a, const String& b) {
    unsigned lenA = a.impl ? a.impl->length : 0;
    unsigned lenB = b.impl ? b.impl->length : 0;
    unsigned total = lenA + lenB;
    if (!total) return _ZN3WTF10StringImpl6createEPKhj(reinterpret_cast<const unsigned char*>(""), 0);
    bool is8Bit = (!a.impl || (a.impl->hashAndFlags & 4)) && (!b.impl || (b.impl->hashAndFlags & 4));
    if (is8Bit) {
        unsigned char* out = nullptr;
        auto res = _ZN3WTF10StringImpl19createUninitializedEjRPh(total, out);
        if (lenA) std::memcpy(out, a.impl->data, lenA);
        if (lenB) std::memcpy(out + lenA, b.impl->data, lenB);
        return res;
    }
    char16_t* out = nullptr;
    auto res = _ZN3WTF10StringImpl19createUninitializedEjRPDs(total, out);
    if (lenA) {
        bool a8 = !!(a.impl->hashAndFlags & 4);
        for (unsigned i = 0; i < lenA; ++i) {
            out[i] = a8 ? static_cast<const unsigned char*>(a.impl->data)[i] : static_cast<const char16_t*>(a.impl->data)[i];
        }
    }
    if (lenB) {
        bool b8 = !!(b.impl->hashAndFlags & 4);
        for (unsigned i = 0; i < lenB; ++i) {
            out[lenA + i] = b8 ? static_cast<const unsigned char*>(b.impl->data)[i] : static_cast<const char16_t*>(b.impl->data)[i];
        }
    }
    return res;
}

int HexDigitValue(char16_t ch) {
    if (ch >= '0' && ch <= '9') return ch - '0';
    if (ch >= 'a' && ch <= 'f') return ch - 'a' + 10;
    if (ch >= 'A' && ch <= 'F') return ch - 'A' + 10;
    return -1;
}

String DecodeEscapeSequences(StringView input) {
    if (!input.data) return String();
    if (!input.length) return _ZN3WTF10StringImpl6createEPKhj(reinterpret_cast<const unsigned char*>(""), 0);
    bool hasPercent = false;
    for (unsigned i = 0; i < input.length; ++i) {
        if (Unit(input, i) == '%') {
            hasPercent = true;
            break;
        }
    }
    if (!hasPercent || input.length < 3) {
        if (input.is8Bit) return _ZN3WTF10StringImpl6createEPKhj(static_cast<const unsigned char*>(input.data), input.length);
        return _ZN3WTF10StringImpl6createEPKDsj(static_cast<const char16_t*>(input.data), input.length);
    }
    std::string bytes;
    bytes.reserve(input.length);
    for (unsigned i = 0; i < input.length;) {
        char16_t ch = Unit(input, i);
        if (ch == '%' && i + 2 < input.length) {
            int d1 = HexDigitValue(Unit(input, i + 1));
            int d2 = HexDigitValue(Unit(input, i + 2));
            if (d1 >= 0 && d2 >= 0) {
                bytes.push_back(static_cast<char>((d1 << 4) | d2));
                i += 3;
                continue;
            }
        }
        bytes.push_back(static_cast<char>(ch & 0xFF));
        ++i;
    }
    return _ZN3WTF6String8fromUTF8EPKhm(reinterpret_cast<const unsigned char*>(bytes.data()), bytes.size());
}

bool IsIPv4(StringView string) {
    unsigned count = 0;
    unsigned start = 0;
    for (unsigned i = 0; i <= string.length; ++i) {
        if (i == string.length || Unit(string, i) == '.') {
            if (count >= 4) return false;
            unsigned segLen = i - start;
            if (!segLen || segLen > 3) return false;
            unsigned val = 0;
            for (unsigned j = 0; j < segLen; ++j) {
                char16_t ch = Unit(string, start + j);
                if (ch < '0' || ch > '9') return false;
                if (!j && segLen > 1 && ch == '0') return false;
                val = val * 10 + (ch - '0');
            }
            if (val > 255) return false;
            ++count;
            start = i + 1;
        }
    }
    return count == 4;
}

bool IsIPv6(StringView string) {
    enum SkipState { None, WillSkip, Skipping, Skipped, Final };
    SkipState skipState = None;
    unsigned count = 0;
    unsigned start = 0;
    for (unsigned i = 0; i <= string.length; ++i) {
        if (i != string.length && Unit(string, i) != ':') continue;
        unsigned length = i - start;
        StringView hextet{static_cast<const unsigned char*>(string.data) + (string.is8Bit ? start : start * 2), length, string.is8Bit};
        start = i + 1;
        if (count >= 8 || skipState == Final) return false;
        if (!length) {
            if (skipState == Skipped) return false;
            if (skipState == None) skipState = !count ? WillSkip : Skipping;
            else if (skipState == WillSkip) skipState = Skipping;
            else skipState = Final;
            continue;
        }
        if (skipState == WillSkip) return false;
        if (skipState == Skipping) skipState = Skipped;
        if (length > 4) {
            if ((skipState == None && count != 6) || (skipState == Skipped && count >= 6) || !IsIPv4(hextet)) return false;
            skipState = Final;
            continue;
        }
        for (unsigned j = 0; j < length; ++j) {
            if (HexDigitValue(Unit(hextet, j)) < 0) return false;
        }
        ++count;
    }
    return (count == 8 && skipState == None) || skipState == Skipped || skipState == Final;
}

uint32_t Sha1Rotate(int n, uint32_t x) {
    return (x << n) | (x >> (32 - n));
}

uint32_t Sha1F(int t, uint32_t b, uint32_t c, uint32_t d) {
    if (t < 20) return (b & c) | ((~b) & d);
    if (t < 40) return b ^ c ^ d;
    if (t < 60) return (b & c) | (b & d) | (c & d);
    return b ^ c ^ d;
}

uint32_t Sha1K(int t) {
    if (t < 20) return 0x5a827999U;
    if (t < 40) return 0x6ed9eba1U;
    if (t < 60) return 0x8f1bbcdcU;
    return 0xca62c1d6U;
}

void Sha1Process(SHA1* sha1) {
    uint32_t w[80];
    for (int t = 0; t < 16; ++t) {
        w[t] = (static_cast<uint32_t>(sha1->buffer[t * 4]) << 24) |
               (static_cast<uint32_t>(sha1->buffer[t * 4 + 1]) << 16) |
               (static_cast<uint32_t>(sha1->buffer[t * 4 + 2]) << 8) |
               static_cast<uint32_t>(sha1->buffer[t * 4 + 3]);
    }
    for (int t = 16; t < 80; ++t) {
        w[t] = Sha1Rotate(1, w[t - 3] ^ w[t - 8] ^ w[t - 14] ^ w[t - 16]);
    }
    uint32_t a = sha1->hash[0];
    uint32_t b = sha1->hash[1];
    uint32_t c = sha1->hash[2];
    uint32_t d = sha1->hash[3];
    uint32_t e = sha1->hash[4];
    for (int t = 0; t < 80; ++t) {
        uint32_t temp = Sha1Rotate(5, a) + Sha1F(t, b, c, d) + e + w[t] + Sha1K(t);
        e = d;
        d = c;
        c = Sha1Rotate(30, b);
        b = a;
        a = temp;
    }
    sha1->hash[0] += a;
    sha1->hash[1] += b;
    sha1->hash[2] += c;
    sha1->hash[3] += d;
    sha1->hash[4] += e;
    sha1->cursor = 0;
}

void Sha1Add(SHA1* sha1, const uint8_t* data, std::size_t length) {
    for (std::size_t i = 0; i < length; ++i) {
        sha1->buffer[sha1->cursor++] = data[i];
        ++sha1->totalBytes;
        if (sha1->cursor == 64) Sha1Process(sha1);
    }
}

void Sha1Final(SHA1* sha1, uint8_t* digest) {
    sha1->buffer[sha1->cursor++] = 0x80;
    if (sha1->cursor > 56) {
        while (sha1->cursor < 64) sha1->buffer[sha1->cursor++] = 0x00;
        Sha1Process(sha1);
    }
    for (std::size_t i = sha1->cursor; i < 56; ++i) sha1->buffer[i] = 0x00;
    uint64_t bits = sha1->totalBytes * 8;
    for (int i = 0; i < 8; ++i) sha1->buffer[56 + (7 - i)] = static_cast<uint8_t>((bits >> (i * 8)) & 0xFF);
    sha1->cursor = 64;
    Sha1Process(sha1);
    for (std::size_t i = 0; i < 5; ++i) {
        uint32_t h = sha1->hash[i];
        for (int j = 0; j < 4; ++j) {
            digest[4 * i + (3 - j)] = static_cast<uint8_t>(h & 0xFF);
            h >>= 8;
        }
    }
    sha1->cursor = 0;
    sha1->totalBytes = 0;
    sha1->hash[0] = 0x67452301;
    sha1->hash[1] = 0xefcdab89;
    sha1->hash[2] = 0x98badcfe;
    sha1->hash[3] = 0x10325476;
    sha1->hash[4] = 0xc3d2e1f0;
    std::memset(sha1->buffer, 0, sizeof(sha1->buffer));
}

void EncoderEnsure(Encoder* enc, std::size_t additional) {
    std::size_t req = static_cast<std::size_t>(enc->size) + additional;
    if (req > enc->capacity) {
        std::size_t next = std::max(static_cast<std::size_t>(enc->capacity) * 2, req);
        auto* nextBuf = static_cast<uint8_t*>(_ZN3WTF10fastMallocEm(next));
        std::memcpy(nextBuf, enc->buffer, enc->size);
        if (enc->buffer != enc->inlineBuffer) _ZN3WTF8fastFreeEPv(enc->buffer);
        enc->buffer = nextBuf;
        enc->capacity = static_cast<unsigned>(next);
    }
}

void EncoderAppendBytes(Encoder* enc, const uint8_t* data, std::size_t length) {
    unsigned salt = 101;
    Sha1Add(&enc->sha1, reinterpret_cast<const uint8_t*>(&salt), sizeof(salt));
    Sha1Add(&enc->sha1, data, length);
    EncoderEnsure(enc, length);
    std::memcpy(enc->buffer + enc->size, data, length);
    enc->size += static_cast<unsigned>(length);
}

template<typename T>
void EncoderAppendNum(Encoder* enc, unsigned salt, T val) {
    Sha1Add(&enc->sha1, reinterpret_cast<const uint8_t*>(&salt), sizeof(salt));
    Sha1Add(&enc->sha1, reinterpret_cast<const uint8_t*>(&val), sizeof(val));
    EncoderEnsure(enc, sizeof(T));
    std::memcpy(enc->buffer + enc->size, &val, sizeof(T));
    enc->size += static_cast<unsigned>(sizeof(T));
}

template<typename T>
void VectorAppend(Vector<T>& vec, T&& item) {
    if (vec.size >= vec.capacity) {
        unsigned newCap = vec.capacity ? vec.capacity * 2 : 16;
        auto* newBuf = static_cast<T*>(_ZN3WTF10fastMallocEm(newCap * sizeof(T)));
        for (unsigned i = 0; i < vec.size; ++i) {
            new (newBuf + i) T(std::move(vec.buffer[i]));
            if constexpr (!std::is_trivially_destructible_v<T>) vec.buffer[i].~T();
        }
        if (vec.buffer) _ZN3WTF8fastFreeEPv(vec.buffer);
        vec.buffer = newBuf;
        vec.capacity = newCap;
    }
    new (vec.buffer + vec.size++) T(std::move(item));
}

}  // namespace

extern "C" {

NKWebKit::WtfAbi::String APS5_VABI _ZN3WTF10StringImpl7replaceEDsDs(const StringImpl* self, char16_t pattern, char16_t replacement) {
    if (!self) throw std::invalid_argument("StringImpl::replace: null this");
    if (pattern == replacement || !self->length) {
        return String(Retain(const_cast<StringImpl*>(self)));
    }
    bool is8Bit = !!(self->hashAndFlags & 4);
    if (is8Bit && pattern > 255) {
        return String(Retain(const_cast<StringImpl*>(self)));
    }
    bool found = false;
    for (unsigned i = 0; i < self->length; ++i) {
        char16_t ch = is8Bit ? static_cast<const unsigned char*>(self->data)[i] : static_cast<const char16_t*>(self->data)[i];
        if (ch == pattern) {
            found = true;
            break;
        }
    }
    if (!found) {
        return String(Retain(const_cast<StringImpl*>(self)));
    }
    if (is8Bit && replacement <= 255) {
        unsigned char* out = nullptr;
        auto res = _ZN3WTF10StringImpl19createUninitializedEjRPh(self->length, out);
        for (unsigned i = 0; i < self->length; ++i) {
            unsigned char ch = static_cast<const unsigned char*>(self->data)[i];
            out[i] = (ch == pattern) ? static_cast<unsigned char>(replacement) : ch;
        }
        return res;
    }
    char16_t* out = nullptr;
    auto res = _ZN3WTF10StringImpl19createUninitializedEjRPDs(self->length, out);
    for (unsigned i = 0; i < self->length; ++i) {
        char16_t ch = is8Bit ? static_cast<const unsigned char*>(self->data)[i] : static_cast<const char16_t*>(self->data)[i];
        out[i] = (ch == pattern) ? replacement : ch;
    }
    return res;
}

NKWebKit::WtfAbi::String APS5_VABI _ZN3WTF10StringImpl7replaceEDsPKhj(const StringImpl* self, char16_t pattern, const unsigned char* replacement, unsigned repStrLength) {
    if (!self || !replacement) throw std::invalid_argument("StringImpl::replace: null argument");
    if (!self->length) return String(Retain(const_cast<StringImpl*>(self)));
    bool is8Bit = !!(self->hashAndFlags & 4);
    if (is8Bit && pattern > 255) {
        return String(Retain(const_cast<StringImpl*>(self)));
    }
    unsigned matchCount = 0;
    for (unsigned i = 0; i < self->length; ++i) {
        char16_t ch = is8Bit ? static_cast<const unsigned char*>(self->data)[i] : static_cast<const char16_t*>(self->data)[i];
        if (ch == pattern) ++matchCount;
    }
    if (!matchCount) {
        return String(Retain(const_cast<StringImpl*>(self)));
    }
    std::size_t newLength = static_cast<std::size_t>(self->length) - matchCount + static_cast<std::size_t>(matchCount) * repStrLength;
    if (newLength > 0x7FFFFFFF) throw std::length_error("string length overflow");
    if (is8Bit) {
        unsigned char* out = nullptr;
        auto res = _ZN3WTF10StringImpl19createUninitializedEjRPh(static_cast<unsigned>(newLength), out);
        unsigned dst = 0;
        for (unsigned i = 0; i < self->length; ++i) {
            unsigned char ch = static_cast<const unsigned char*>(self->data)[i];
            if (ch == pattern) {
                if (repStrLength) {
                    std::memcpy(out + dst, replacement, repStrLength);
                    dst += repStrLength;
                }
            } else {
                out[dst++] = ch;
            }
        }
        return res;
    }
    char16_t* out = nullptr;
    auto res = _ZN3WTF10StringImpl19createUninitializedEjRPDs(static_cast<unsigned>(newLength), out);
    unsigned dst = 0;
    for (unsigned i = 0; i < self->length; ++i) {
        char16_t ch = static_cast<const char16_t*>(self->data)[i];
        if (ch == pattern) {
            for (unsigned j = 0; j < repStrLength; ++j) {
                out[dst++] = replacement[j];
            }
        } else {
            out[dst++] = ch;
        }
    }
    return res;
}

void APS5_VABI _ZN3WTF10StringView11SplitResult8Iterator17findNextSubstringEv(SplitResultIterator* self) {
    if (!self || !self->result) throw std::invalid_argument("SplitResult::Iterator: null this");
    const auto& res = *self->result;
    const auto& str = res.string;
    while (self->position <= str.length) {
        unsigned sepPos = str.length;
        for (unsigned i = self->position; i < str.length; ++i) {
            char16_t ch = str.is8Bit ? static_cast<const unsigned char*>(str.data)[i] : static_cast<const char16_t*>(str.data)[i];
            if (ch == res.separator) {
                sepPos = i;
                break;
            }
        }
        if (sepPos != str.length) {
            if (res.allowEmptyEntries || sepPos > self->position) {
                self->length = sepPos - self->position;
                return;
            }
            ++self->position;
        } else {
            break;
        }
    }
    self->length = str.length - self->position;
    if (!self->length && !res.allowEmptyEntries) {
        self->isDone = true;
    }
}

NKWebKit::WtfAbi::SplitResultIterator& APS5_VABI _ZN3WTF10StringView11SplitResult8IteratorppEv(SplitResultIterator* self) {
    if (!self || !self->result) throw std::invalid_argument("SplitResult::Iterator: null this");
    const auto& str = self->result->string;
    self->position += self->length;
    if (self->position < str.length) {
        ++self->position;
        _ZN3WTF10StringView11SplitResult8Iterator17findNextSubstringEv(self);
    } else if (!self->isDone) {
        self->isDone = true;
    }
    return *self;
}

void APS5_VABI _ZN3WTF11Persistence7EncoderC1Ev(Encoder* self) {
    if (!self) throw std::invalid_argument("Encoder::Encoder: null this");
    self->buffer = self->inlineBuffer;
    self->capacity = 4096;
    self->size = 0;
    std::memset(&self->sha1, 0, sizeof(self->sha1));
    self->sha1.hash[0] = 0x67452301;
    self->sha1.hash[1] = 0xefcdab89;
    self->sha1.hash[2] = 0x98badcfe;
    self->sha1.hash[3] = 0x10325476;
    self->sha1.hash[4] = 0xc3d2e1f0;
}

void APS5_VABI _ZN3WTF11Persistence7EncoderC2Ev(Encoder* self) {
    _ZN3WTF11Persistence7EncoderC1Ev(self);
}

void APS5_VABI _ZN3WTF11Persistence7EncoderD1Ev(Encoder* self) {
    if (!self) return;
    if (self->buffer && self->buffer != self->inlineBuffer) {
        _ZN3WTF8fastFreeEPv(self->buffer);
        self->buffer = nullptr;
    }
}

void APS5_VABI _ZN3WTF11Persistence7EncoderD2Ev(Encoder* self) {
    _ZN3WTF11Persistence7EncoderD1Ev(self);
}

void APS5_VABI _ZN3WTF11Persistence7Encoder14encodeChecksumEv(Encoder* self) {
    if (!self) throw std::invalid_argument("Encoder::encodeChecksum: null this");
    uint8_t digest[20];
    Sha1Final(&self->sha1, digest);
    EncoderAppendBytes(self, digest, 20);
}

NKWebKit::WtfAbi::Encoder& APS5_VABI _ZN3WTF11Persistence7EncoderlsEb(Encoder* self, bool value) {
    if (!self) throw std::invalid_argument("Encoder::operator<<: null this");
    EncoderAppendNum(self, 3, value);
    return *self;
}

NKWebKit::WtfAbi::Encoder& APS5_VABI _ZN3WTF11Persistence7EncoderlsEh(Encoder* self, std::uint8_t value) {
    if (!self) throw std::invalid_argument("Encoder::operator<<: null this");
    EncoderAppendNum(self, 5, value);
    return *self;
}

NKWebKit::WtfAbi::Encoder& APS5_VABI _ZN3WTF11Persistence7EncoderlsEs(Encoder* self, std::int16_t value) {
    if (!self) throw std::invalid_argument("Encoder::operator<<: null this");
    EncoderAppendNum(self, 103, value);
    return *self;
}

NKWebKit::WtfAbi::Encoder& APS5_VABI _ZN3WTF11Persistence7EncoderlsEt(Encoder* self, std::uint16_t value) {
    if (!self) throw std::invalid_argument("Encoder::operator<<: null this");
    EncoderAppendNum(self, 7, value);
    return *self;
}

NKWebKit::WtfAbi::Encoder& APS5_VABI _ZN3WTF11Persistence7EncoderlsEi(Encoder* self, std::int32_t value) {
    if (!self) throw std::invalid_argument("Encoder::operator<<: null this");
    EncoderAppendNum(self, 17, value);
    return *self;
}

NKWebKit::WtfAbi::Encoder& APS5_VABI _ZN3WTF11Persistence7EncoderlsEj(Encoder* self, std::uint32_t value) {
    if (!self) throw std::invalid_argument("Encoder::operator<<: null this");
    EncoderAppendNum(self, 11, value);
    return *self;
}

NKWebKit::WtfAbi::Encoder& APS5_VABI _ZN3WTF11Persistence7EncoderlsEl(Encoder* self, std::int64_t value) {
    if (!self) throw std::invalid_argument("Encoder::operator<<: null this");
    EncoderAppendNum(self, 19, value);
    return *self;
}

NKWebKit::WtfAbi::Encoder& APS5_VABI _ZN3WTF11Persistence7EncoderlsEm(Encoder* self, std::uint64_t value) {
    if (!self) throw std::invalid_argument("Encoder::operator<<: null this");
    EncoderAppendNum(self, 13, value);
    return *self;
}

NKWebKit::WtfAbi::Encoder& APS5_VABI _ZN3WTF11Persistence7EncoderlsEf(Encoder* self, float value) {
    if (!self) throw std::invalid_argument("Encoder::operator<<: null this");
    EncoderAppendNum(self, 23, value);
    return *self;
}

NKWebKit::WtfAbi::Encoder& APS5_VABI _ZN3WTF11Persistence7EncoderlsEd(Encoder* self, double value) {
    if (!self) throw std::invalid_argument("Encoder::operator<<: null this");
    EncoderAppendNum(self, 29, value);
    return *self;
}

bool APS5_VABI _ZNK3WTF11Persistence7Decoder28bufferIsLargeEnoughToContainEm(const Decoder* self, unsigned long size) {
    if (!self) throw std::invalid_argument("Decoder::bufferIsLargeEnoughToContain: null this");
    if (!self->bufferData || !self->bufferPosition) return false;
    std::size_t remaining = (self->bufferData + self->bufferSize >= self->bufferPosition) ? static_cast<std::size_t>((self->bufferData + self->bufferSize) - self->bufferPosition) : 0;
    return size <= remaining;
}

bool APS5_VABI _ZN3WTF11Persistence7Decoder14verifyChecksumEv(Decoder* self) {
    if (!self) throw std::invalid_argument("Decoder::verifyChecksum: null this");
    if (!_ZNK3WTF11Persistence7Decoder28bufferIsLargeEnoughToContainEm(self, 20)) return false;
    uint8_t computed[20];
    Sha1Final(&self->sha1, computed);
    const uint8_t* saved = self->bufferPosition;
    self->bufferPosition += 20;
    unsigned salt = 101;
    Sha1Add(&self->sha1, reinterpret_cast<const uint8_t*>(&salt), sizeof(salt));
    Sha1Add(&self->sha1, saved, 20);
    return std::memcmp(computed, saved, 20) == 0;
}

void APS5_VABI _ZN3WTF11Persistence5CoderINS_10AtomStringEE6encodeERNS0_7EncoderERKS2_(Encoder& encoder, const AtomString& string) {
    _ZN3WTF11Persistence5CoderINS_6StringEE6encodeERNS0_7EncoderERKS2_(encoder, string.string);
}

void APS5_VABI _ZN3WTF11Persistence5CoderINS_6StringEE6encodeERNS0_7EncoderERKS2_(Encoder& encoder, const String& string) {
    if (!string.impl) {
        _ZN3WTF11Persistence7EncoderlsEj(&encoder, 0xFFFFFFFFU);
        return;
    }
    _ZN3WTF11Persistence7EncoderlsEj(&encoder, string.impl->length);
    bool is8Bit = !!(string.impl->hashAndFlags & 4);
    _ZN3WTF11Persistence7EncoderlsEb(&encoder, is8Bit);
    std::size_t byteLen = string.impl->length * (is8Bit ? 1 : 2);
    EncoderAppendBytes(&encoder, static_cast<const uint8_t*>(string.impl->data), byteLen);
}

void APS5_VABI _ZN3WTF11Persistence5CoderINS_7CStringEE6encodeERNS0_7EncoderERKS2_(Encoder& encoder, const CString& string) {
    if (!string.buffer) {
        _ZN3WTF11Persistence7EncoderlsEj(&encoder, 0xFFFFFFFFU);
        return;
    }
    _ZN3WTF11Persistence7EncoderlsEj(&encoder, static_cast<uint32_t>(string.buffer->length));
    const auto* data = reinterpret_cast<const uint8_t*>(string.buffer + 1);
    EncoderAppendBytes(&encoder, data, string.buffer->length);
}

bool APS5_VABI _ZNK3WTF3URL10protocolIsEPKc(const URL* self, const char* protocol) {
    if (!self || !protocol) throw std::invalid_argument("URL::protocolIs: null argument");
    return _ZNK3WTF3URL10protocolIsENS_10StringViewE(self, {protocol, static_cast<unsigned>(std::strlen(protocol)), true});
}

bool APS5_VABI _ZN3WTF3URL15hostIsIPAddressENS_10StringViewE(StringView host) {
    if (!host.data || !host.length) return false;
    bool hasColon = false;
    for (unsigned i = 0; i < host.length; ++i) {
        if (Unit(host, i) == ':') {
            hasColon = true;
            break;
        }
    }
    return hasColon ? IsIPv6(host) : IsIPv4(host);
}

void APS5_VABI _ZN3WTF3URL32removeQueryAndFragmentIdentifierEv(URL* self) {
    if (!self) throw std::invalid_argument("URL::removeQueryAndFragmentIdentifier: null this");
    if (!(self->bits & 1) || !self->impl) return;
    auto truncated = Substring(self->impl, 0, self->pathEnd);
    NKWebKit::WtfAbi::Release(self->impl);
    self->impl = truncated.impl;
    truncated.impl = nullptr;
    self->queryEnd = self->pathEnd;
}

NKWebKit::WtfAbi::String APS5_VABI _ZNK3WTF3URL11hostAndPortEv(const URL* self) {
    if (!self) throw std::invalid_argument("URL::hostAndPort: null this");
    if (!(self->bits & 1) || !self->impl) return String();
    unsigned portLen = (self->bits >> 3) & 7;
    auto hostView = _ZNK3WTF3URL4hostEv(self);
    if (!portLen) {
        if (hostView.is8Bit) return _ZN3WTF10StringImpl6createEPKhj(static_cast<const unsigned char*>(hostView.data), hostView.length);
        return _ZN3WTF10StringImpl6createEPKDsj(static_cast<const char16_t*>(hostView.data), hostView.length);
    }
    unsigned start = HostStart(*self);
    unsigned totalLen = (self->hostEnd + portLen) - start;
    return Substring(self->impl, start, totalLen);
}

NKWebKit::WtfAbi::String APS5_VABI _ZNK3WTF3URL14fileSystemPathEv(const URL* self) {
    if (!self) throw std::invalid_argument("URL::fileSystemPath: null this");
    if (!_ZNK3WTF3URL10protocolIsENS_10StringViewE(self, {"file", 4, true})) return String();
    auto pathView = _ZNK3WTF3URL4pathEv(self);
    return DecodeEscapeSequences(pathView);
}

bool APS5_VABI _ZNK3WTF3URL16isMatchingDomainENS_10StringViewE(const URL* self, StringView domain) {
    if (!self) throw std::invalid_argument("URL::isMatchingDomain: null this");
    if (!self->impl) return false;
    if (!domain.length) return true;
    if (!(self->bits & 2)) return false;
    auto hostView = _ZNK3WTF3URL4hostEv(self);
    if (hostView.length < domain.length) return false;
    unsigned offset = hostView.length - domain.length;
    for (unsigned i = 0; i < domain.length; ++i) {
        if (Unit(hostView, offset + i) != Unit(domain, i)) return false;
    }
    return offset == 0 || Unit(hostView, offset - 1) == '.';
}

NKWebKit::WtfAbi::String APS5_VABI _ZNK3WTF3URL19protocolHostAndPortEv(const URL* self) {
    if (!self) throw std::invalid_argument("URL::protocolHostAndPort: null this");
    if (!(self->bits & 1) || !self->impl) return String();
    unsigned pathStart = PathStart(*self);
    bool hasCredentials = self->passwordEnd > self->userStart;
    if (!hasCredentials) {
        return Substring(self->impl, 0, pathStart);
    }
    unsigned hostStart = HostStart(*self);
    auto leftPart = Substring(self->impl, 0, self->userStart);
    auto rightPart = Substring(self->impl, hostStart, pathStart - hostStart);
    return ConcatStrings(leftPart, rightPart);
}

NKWebKit::WtfAbi::String APS5_VABI _ZNK3WTF3URL4userEv(const URL* self) {
    if (!self) throw std::invalid_argument("URL::user: null this");
    if (!(self->bits & 1) || !self->impl) return String();
    auto view = _ZNK3WTF3URL11encodedUserEv(self);
    return DecodeEscapeSequences(view);
}

NKWebKit::WtfAbi::String APS5_VABI _ZNK3WTF3URL8passwordEv(const URL* self) {
    if (!self) throw std::invalid_argument("URL::password: null this");
    if (!(self->bits & 1) || !self->impl) return String();
    auto view = _ZNK3WTF3URL15encodedPasswordEv(self);
    return DecodeEscapeSequences(view);
}

NKWebKit::WtfAbi::Vector<NKWebKit::WtfAbi::String> APS5_VABI _ZNK3WTF6String25splitAllowingEmptyEntriesEDs(const String* self, char16_t separator) {
    if (!self) throw std::invalid_argument("String::splitAllowingEmptyEntries: null this");
    Vector<String> result;
    if (!self->impl) {
        VectorAppend(result, String());
        return result;
    }
    bool is8Bit = !!(self->impl->hashAndFlags & 4);
    unsigned start = 0;
    for (unsigned i = 0; i <= self->impl->length; ++i) {
        if (i == self->impl->length || (is8Bit ? static_cast<const unsigned char*>(self->impl->data)[i] : static_cast<const char16_t*>(self->impl->data)[i]) == separator) {
            VectorAppend(result, Substring(self->impl, start, i - start));
            start = i + 1;
        }
    }
    return result;
}

NKWebKit::WtfAbi::Vector<char16_t> APS5_VABI _ZNK3WTF6String29charactersWithNullTerminationEv(const String* self) {
    if (!self) throw std::invalid_argument("String::charactersWithNullTermination: null this");
    Vector<char16_t> result;
    if (!self->impl || !self->impl->length) {
        VectorAppend(result, char16_t{0});
        return result;
    }
    bool is8Bit = !!(self->impl->hashAndFlags & 4);
    for (unsigned i = 0; i < self->impl->length; ++i) {
        char16_t ch = is8Bit ? static_cast<const unsigned char*>(self->impl->data)[i] : static_cast<const char16_t*>(self->impl->data)[i];
        VectorAppend(result, std::move(ch));
    }
    VectorAppend(result, char16_t{0});
    return result;
}

NKWebKit::WtfAbi::Vector<NKWebKit::WtfAbi::String> APS5_VABI _ZNK3WTF6String5splitEDs(const String* self, char16_t separator) {
    if (!self) throw std::invalid_argument("String::split: null this");
    Vector<String> result;
    if (!self->impl) return result;
    bool is8Bit = !!(self->impl->hashAndFlags & 4);
    unsigned start = 0;
    for (unsigned i = 0; i <= self->impl->length; ++i) {
        if (i == self->impl->length || (is8Bit ? static_cast<const unsigned char*>(self->impl->data)[i] : static_cast<const char16_t*>(self->impl->data)[i]) == separator) {
            if (i > start) {
                VectorAppend(result, Substring(self->impl, start, i - start));
            }
            start = i + 1;
        }
    }
    return result;
}

float APS5_VABI _ZNK3WTF6String7toFloatEPb(const String* self, bool* ok) {
    if (!self) throw std::invalid_argument("String::toFloat: null this");
    if (!self->impl) {
        if (ok) *ok = false;
        return 0.0f;
    }
    if (self->impl->hashAndFlags & 4) {
        return _ZN3WTF17charactersToFloatEPKhmPb(static_cast<const unsigned char*>(self->impl->data), self->impl->length, ok);
    }
    return _ZN3WTF17charactersToFloatEPKDsmPb(static_cast<const char16_t*>(self->impl->data), self->impl->length, ok);
}

double APS5_VABI _ZNK3WTF6String8toDoubleEPb(const String* self, bool* ok) {
    if (!self) throw std::invalid_argument("String::toDouble: null this");
    if (!self->impl) {
        if (ok) *ok = false;
        return 0.0;
    }
    if (self->impl->hashAndFlags & 4) {
        return _ZN3WTF18charactersToDoubleEPKhmPb(static_cast<const unsigned char*>(self->impl->data), self->impl->length, ok);
    }
    return _ZN3WTF18charactersToDoubleEPKDsmPb(static_cast<const char16_t*>(self->impl->data), self->impl->length, ok);
}

NKWebKit::WtfAbi::String APS5_VABI _ZNKR3WTF6String12isolatedCopyEv(const String* self) {
    if (!self) throw std::invalid_argument("String::isolatedCopy: null this");
    if (!self->impl) return String();
    if (self->impl->hashAndFlags & 4) {
        return _ZN3WTF10StringImpl6createEPKhj(static_cast<const unsigned char*>(self->impl->data), self->impl->length);
    }
    return _ZN3WTF10StringImpl6createEPKDsj(static_cast<const char16_t*>(self->impl->data), self->impl->length);
}

NKWebKit::WtfAbi::String APS5_VABI _ZNO3WTF6String12isolatedCopyEv(String* self) {
    if (!self) throw std::invalid_argument("String::isolatedCopy: null this");
    if (!self->impl) return String();
    if (_ZNK3WTF6String27isSafeToSendToAnotherThreadEv(self)) {
        return String(std::exchange(self->impl, nullptr));
    }
    return _ZNKR3WTF6String12isolatedCopyEv(self);
}

}
