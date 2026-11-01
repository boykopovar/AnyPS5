#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <vector>
#include <algorithm>
#include <charconv>
#include <cstring>
#include <limits>
#include <new>
#include <stdexcept>
#include <string>

#include "WtfStringAbi.hpp"

namespace {

class StringHasher {
public:
    static constexpr unsigned stringHashingStartValue = 0x9E3779B9U;
    static constexpr unsigned maskHash = 0x00FFFFFFU;

    void addCharacter(std::uint16_t character) {
        if (m_hasPendingCharacter) {
            m_hasPendingCharacter = false;
            addPair(m_pendingCharacter, character);
            return;
        }
        m_pendingCharacter = character;
        m_hasPendingCharacter = true;
    }

    void addCharacters(std::uint16_t a, std::uint16_t b) {
        if (m_hasPendingCharacter) {
            m_hasPendingCharacter = false;
            addPair(m_pendingCharacter, a);
            m_pendingCharacter = b;
            m_hasPendingCharacter = true;
            return;
        }
        addPair(a, b);
    }

    unsigned hashWithTop8BitsMasked() const {
        return hash(true);
    }

    unsigned hash(bool masked) const {
        unsigned result = m_hash;
        if (m_hasPendingCharacter) {
            result += m_pendingCharacter;
            result ^= result << 11;
            result += result >> 17;
        }
        result ^= result << 3;
        result += result >> 5;
        result ^= result << 2;
        result += result >> 15;
        result ^= result << 10;
        if (masked) result &= maskHash;
        if (!result) {
            return 0x00800000U;
        }
        return result;
    }

private:
    void addPair(std::uint16_t first, std::uint16_t second) {
        unsigned result = m_hash + first;
        result = (result << 16) ^ ((static_cast<unsigned>(second) << 11) ^ result);
        result += result >> 11;
        m_hash = result;
    }

    unsigned m_hash{stringHashingStartValue};
    std::uint16_t m_pendingCharacter{0};
    bool m_hasPendingCharacter{false};
};

}  // namespace

extern "C" {
unsigned int APS5_VABI _ZN3WTF7Unicode51calculateStringHashAndLengthFromUTF8MaskingTop8BitsEPKcS2_RjS3_(
    const char* data, const char* dataEnd, unsigned int& dataLength, unsigned int& utf16Length) {
    if (!data || !dataEnd || reinterpret_cast<std::uintptr_t>(dataEnd) < reinterpret_cast<std::uintptr_t>(data))
        throw std::invalid_argument("invalid WTF UTF-8 hash range");
    StringHasher hasher;
    utf16Length = 0;
    const auto* cur = reinterpret_cast<const std::uint8_t*>(data);
    const auto* end = reinterpret_cast<const std::uint8_t*>(dataEnd);

    while (cur < end) {
        std::uint8_t lead = *cur++;
        std::uint32_t cp = 0;
        int extraBytes = 0;

        if (lead < 0x80) {
            cp = lead;
        } else if (lead >= 0xC2 && lead <= 0xDF) {
            cp = lead & 0x1F;
            extraBytes = 1;
        } else if ((lead & 0xF0) == 0xE0) {
            cp = lead & 0x0F;
            extraBytes = 2;
        } else if (lead >= 0xF0 && lead <= 0xF4) {
            cp = lead & 0x07;
            extraBytes = 3;
        } else {
            return 0;
        }

        if (static_cast<std::size_t>(end - cur) < static_cast<unsigned>(extraBytes)) {
            return 0;
        }

        for (int i = 0; i < extraBytes; ++i) {
            std::uint8_t b = *cur++;
            if ((b & 0xC0) != 0x80) {
                return 0;
            }
            cp = (cp << 6) | (b & 0x3F);
        }

        if ((extraBytes == 1 && cp < 0x80) || (extraBytes == 2 && cp < 0x800) || (extraBytes == 3 && cp < 0x10000)) return 0;

        if (cp <= 0xFFFF) {
            if (cp >= 0xD800 && cp <= 0xDFFF) {
                return 0;
            }
            hasher.addCharacter(static_cast<std::uint16_t>(cp));
            utf16Length++;
        } else if (cp <= 0x10FFFF) {
            std::uint32_t sub = cp - 0x10000;
            auto leadSurrogate = static_cast<std::uint16_t>((sub >> 10) + 0xD800);
            auto trailSurrogate = static_cast<std::uint16_t>((sub & 0x3FF) + 0xDC00);
            hasher.addCharacters(leadSurrogate, trailSurrogate);
            utf16Length += 2;
        } else {
            return 0;
        }
    }

    dataLength = static_cast<unsigned int>(reinterpret_cast<const char*>(cur) - data);
    return hasher.hashWithTop8BitsMasked();
}

double APS5_VABI _ZN3WTF8Internal25parseDoubleFromLongStringEPKDsmRm(
    const char16_t* string, std::size_t length, std::size_t& parsedLength) {
    std::vector<char> buffer(length + 1);
    for (std::size_t i = 0; i < length; ++i) {
        char16_t ch = string[i];
        buffer[i] = (ch < 0x80) ? static_cast<char>(ch) : 0;
    }
    buffer[length] = '\0';

    char* endPtr = nullptr;
    double val = std::strtod(buffer.data(), &endPtr);
    parsedLength = static_cast<std::size_t>(endPtr - buffer.data());
    return val;
}

}

namespace NKWebKit {
void* Allocate(std::size_t size, std::size_t alignment = 16);
void Deallocate(void* ptr);
}  // namespace NKWebKit

namespace NKWebKit::WtfAbi {

void Release(StringImpl* impl) {
    if (!impl) return;
    if (impl->refCount != 2) {
        impl->refCount -= 2;
        return;
    }
    if (impl->hashAndFlags & 48 || (impl->hashAndFlags & 3) == 3) std::terminate();
    switch (impl->hashAndFlags & 3) {
    case 1:
        Deallocate(const_cast<void*>(impl->data));
        break;
    case 2:
        Release(*reinterpret_cast<StringImpl**>(reinterpret_cast<unsigned char*>(impl) + 24));
        break;
    }
    Deallocate(impl);
}

void Release(CStringBuffer* buffer) {
    if (buffer && !--buffer->refCount) Deallocate(buffer);
}

}  // namespace NKWebKit::WtfAbi

namespace {

using NKWebKit::WtfAbi::StringImpl;
using NKWebKit::WtfAbi::StringView;
using NKWebKit::WtfAbi::String;
using NKWebKit::WtfAbi::CString;
using NKWebKit::WtfAbi::CStringBuffer;

constexpr unsigned MaxStringLength = 0x7FFFFFFF;
constexpr std::size_t NotFound = std::numeric_limits<std::size_t>::max();

template<typename T> T& Checked(T* value) {
    if (!value) throw std::invalid_argument("null WTF object");
    return *value;
}

void CheckBuffer(const void* value, std::size_t length) {
    if (!value && length) throw std::invalid_argument("null WTF character buffer");
}

StringView View(const StringImpl* impl) {
    if (!impl) return {nullptr, 0, true};
    return {impl->data, impl->length, !!(impl->hashAndFlags & 4)};
}

char16_t Unit(StringView view, unsigned index) {
    CheckBuffer(view.data, view.length);
    return view.is8Bit ? static_cast<const unsigned char*>(view.data)[index] : static_cast<const char16_t*>(view.data)[index];
}

unsigned Hash(StringView view, bool masked = true) {
    StringHasher hasher;
    for (unsigned i = 0; i < view.length; ++i) hasher.addCharacter(Unit(view, i));
    return hasher.hash(masked);
}

StringImpl* EmptyImpl() {
    static const unsigned char empty[] = "";
    static StringImpl impl{1, 0, empty, (Hash({empty, 0, true}) << 8) | 28};
    impl.refCount += 2;
    return &impl;
}

StringImpl* Retain(StringImpl* impl) {
    if (impl) impl->refCount += 2;
    return impl;
}

StringImpl* NewImpl(unsigned length, bool is8Bit, void*& characters) {
    if (!length) {
        characters = nullptr;
        return EmptyImpl();
    }
    const std::size_t width = is8Bit ? 1 : 2;
    if (length > std::min<std::size_t>(MaxStringLength, (0xFFFFFFFFULL - sizeof(StringImpl)) / width))
        throw std::length_error("WTF string exceeds maximum length");
    auto* storage = static_cast<unsigned char*>(NKWebKit::Allocate(std::max<std::size_t>(sizeof(StringImpl), 20 + width * length)));
    characters = storage + 20;
    return new (storage) StringImpl{2, length, characters, is8Bit ? 4U : 0U};
}

StringImpl* CopyImpl(StringView view, bool narrow = false) {
    if (!view.data || !view.length) return EmptyImpl();
    bool is8Bit = view.is8Bit;
    if (narrow && !is8Bit) {
        is8Bit = true;
        for (unsigned i = 0; i < view.length; ++i) is8Bit &= Unit(view, i) <= 255;
    }
    void* data;
    auto* impl = NewImpl(view.length, is8Bit, data);
    for (unsigned i = 0; i < view.length; ++i) {
        if (is8Bit) static_cast<unsigned char*>(data)[i] = static_cast<unsigned char>(Unit(view, i));
        else static_cast<char16_t*>(data)[i] = Unit(view, i);
    }
    return impl;
}

char16_t AsciiCase(char16_t value, bool lower) {
    if (lower && value >= 'A' && value <= 'Z') return value + 32;
    if (!lower && value >= 'a' && value <= 'z') return value - 32;
    return value;
}

StringImpl* ConvertCase(StringView view, bool lower, StringImpl* original = nullptr) {
    if (!view.data) return nullptr;
    if (original) {
        bool changed = false;
        for (unsigned i = 0; i < view.length; ++i) changed |= AsciiCase(Unit(view, i), lower) != Unit(view, i);
        if (!changed) return Retain(original);
    }
    void* data;
    auto* impl = NewImpl(view.length, view.is8Bit, data);
    for (unsigned i = 0; i < view.length; ++i) {
        auto value = AsciiCase(Unit(view, i), lower);
        if (view.is8Bit) static_cast<unsigned char*>(data)[i] = static_cast<unsigned char>(value);
        else static_cast<char16_t*>(data)[i] = value;
    }
    return impl;
}

bool EqualViews(StringView a, StringView b, bool ignoreCase = false) {
    if (a.length != b.length) return false;
    for (unsigned i = 0; i < a.length; ++i) {
        auto x = Unit(a, i);
        auto y = Unit(b, i);
        if (ignoreCase) { x = AsciiCase(x, true); y = AsciiCase(y, true); }
        if (x != y) return false;
    }
    return true;
}

bool EqualImpls(const StringImpl* a, const StringImpl* b, bool ignoreCase = false) {
    if (a == b) return true;
    return a && b && EqualViews(View(a), View(b), ignoreCase);
}

bool EqualBuffer(const StringImpl* impl, const void* buffer, unsigned length, bool is8Bit) {
    if (!impl) return !buffer;
    return buffer && EqualViews(View(impl), {buffer, length, is8Bit});
}

bool Affix(const StringImpl* impl, const char* buffer, unsigned length, bool suffix) {
    auto view = View(&Checked(impl));
    if (length > view.length) return false;
    CheckBuffer(buffer, length);
    unsigned start = suffix ? view.length - length : 0;
    for (unsigned i = 0; i < length; ++i) {
        if (Unit(view, start + i) != static_cast<unsigned char>(buffer[i])) return false;
    }
    return true;
}

std::int32_t CharacterAt(StringView view, unsigned index) {
    if (index >= view.length) throw std::out_of_range("WTF character index");
    auto first = Unit(view, index);
    if (view.is8Bit || first < 0xD800 || first > 0xDFFF) return first;
    if (first <= 0xDBFF && index + 1 < view.length) {
        auto second = Unit(view, index + 1);
        if (second >= 0xDC00 && second <= 0xDFFF) return 0x10000 + ((first - 0xD800) << 10) + second - 0xDC00;
    }
    return 0;
}

CString NewCString(std::size_t length, char*& data) {
    if (length > std::numeric_limits<std::size_t>::max() - sizeof(CStringBuffer) - 1)
        throw std::length_error("WTF CString allocation overflow");
    auto* buffer = new (NKWebKit::Allocate(sizeof(CStringBuffer) + length + 1)) CStringBuffer{1, length};
    data = reinterpret_cast<char*>(buffer + 1);
    data[length] = 0;
    return CString(buffer);
}

const char* Bytes(const CString& value) {
    return value.buffer ? reinterpret_cast<const char*>(value.buffer + 1) : nullptr;
}

CString CopyCString(const char* data, std::size_t length) {
    if (!data) {
        CheckBuffer(data, length);
        return CString();
    }
    char* output;
    auto result = NewCString(length, output);
    if (length) std::memcpy(output, data, length);
    return result;
}

String FromUTF8(const unsigned char* input, std::size_t length) {
    if (length > MaxStringLength) throw std::length_error("WTF UTF-8 input too long");
    if (!input) return String();
    bool ascii = true;
    for (std::size_t i = 0; i < length; ++i) ascii &= input[i] < 128;
    if (ascii) return String(CopyImpl({input, static_cast<unsigned>(length), true}));
    std::u16string output;
    for (std::size_t i = 0; i < length;) {
        unsigned first = input[i++];
        unsigned codePoint;
        unsigned count;
        unsigned minimum;
        if (first < 128) { output.push_back(static_cast<char16_t>(first)); continue; }
        if (first >= 0xC2 && first <= 0xDF) { codePoint = first & 31; count = 1; minimum = 0x80; }
        else if (first >= 0xE0 && first <= 0xEF) { codePoint = first & 15; count = 2; minimum = 0x800; }
        else if (first >= 0xF0 && first <= 0xF4) { codePoint = first & 7; count = 3; minimum = 0x10000; }
        else return String();
        if (count > length - i) return String();
        for (unsigned j = 0; j < count; ++j) {
            unsigned next = input[i++];
            if ((next & 0xC0) != 0x80) return String();
            codePoint = (codePoint << 6) | (next & 63);
        }
        if (codePoint < minimum || codePoint > 0x10FFFF || (codePoint >= 0xD800 && codePoint <= 0xDFFF)) return String();
        if (codePoint < 0x10000) output.push_back(static_cast<char16_t>(codePoint));
        else {
            output.push_back(static_cast<char16_t>(0xD800 + ((codePoint - 0x10000) >> 10)));
            output.push_back(static_cast<char16_t>(0xDC00 + ((codePoint - 0x10000) & 1023)));
        }
    }
    return String(CopyImpl({output.data(), static_cast<unsigned>(output.size()), false}));
}

CString ToUTF8(StringView view, unsigned mode) {
    if (mode > 2 && !view.is8Bit) throw std::invalid_argument("invalid WTF UTF-8 conversion mode");
    std::string bytes;
    for (unsigned i = 0; i < view.length; ++i) {
        unsigned cp = Unit(view, i);
        if (!view.is8Bit && cp >= 0xD800 && cp <= 0xDFFF) {
            if (cp <= 0xDBFF && i + 1 < view.length && Unit(view, i + 1) >= 0xDC00 && Unit(view, i + 1) <= 0xDFFF) {
                cp = 0x10000 + ((cp - 0xD800) << 10) + Unit(view, i + 1) - 0xDC00;
                ++i;
            } else if (mode == 1) throw std::invalid_argument("unpaired WTF UTF-16 surrogate");
            else if (mode == 2) cp = 0xFFFD;
        }
        if (cp < 0x80) bytes.push_back(static_cast<char>(cp));
        else if (cp < 0x800) {
            bytes.push_back(static_cast<char>(0xC0 | (cp >> 6)));
            bytes.push_back(static_cast<char>(0x80 | (cp & 63)));
        } else if (cp < 0x10000) {
            bytes.push_back(static_cast<char>(0xE0 | (cp >> 12)));
            bytes.push_back(static_cast<char>(0x80 | ((cp >> 6) & 63)));
            bytes.push_back(static_cast<char>(0x80 | (cp & 63)));
        } else {
            bytes.push_back(static_cast<char>(0xF0 | (cp >> 18)));
            bytes.push_back(static_cast<char>(0x80 | ((cp >> 12) & 63)));
            bytes.push_back(static_cast<char>(0x80 | ((cp >> 6) & 63)));
            bytes.push_back(static_cast<char>(0x80 | (cp & 63)));
        }
    }
    return CopyCString(bytes.data(), bytes.size());
}

String Substring(const StringImpl* original, unsigned start, unsigned length) {
    const auto& impl = Checked(original);
    if (start >= impl.length) return String(EmptyImpl());
    if (length >= impl.length - start) {
        if (!start) return String(Retain(const_cast<StringImpl*>(original)));
        length = impl.length - start;
    }
    const auto* data = static_cast<const unsigned char*>(impl.data) + (impl.hashAndFlags & 4 ? start : 2ULL * start);
    return String(CopyImpl({data, length, !!(impl.hashAndFlags & 4)}));
}

String SharedSubstring(const String* string, unsigned start, unsigned length) {
    const auto& value = Checked(string);
    unsigned size = value.impl ? value.impl->length : 0;
    start = std::min(start, size);
    length = std::min(length, size - start);
    if (!start && length == size) return String(Retain(value.impl));
    if (!length) return String(EmptyImpl());
    const auto& impl = Checked(value.impl);
    bool is8Bit = impl.hashAndFlags & 4;
    if (32 >= 20 + std::size_t(length) * (is8Bit ? 1 : 2)) return Substring(&impl, start, length);
    auto* owner = value.impl;
    if ((impl.hashAndFlags & 3) == 2) owner = *reinterpret_cast<StringImpl* const*>(reinterpret_cast<const unsigned char*>(&impl) + 24);
    auto* memory = static_cast<unsigned char*>(NKWebKit::Allocate(32));
    auto* result = new (memory) StringImpl{2, length, static_cast<const unsigned char*>(impl.data) + std::size_t(start) * (is8Bit ? 1 : 2), (is8Bit ? 4U : 0U) | 2U};
    *reinterpret_cast<StringImpl**>(memory + 24) = Retain(owner);
    return String(result);
}

template<typename T> String IntegerString(T value) {
    char buffer[32];
    auto result = std::to_chars(buffer, buffer + sizeof(buffer), value);
    if (result.ec != std::errc{}) throw std::runtime_error("WTF integer formatting failed");
    return String(CopyImpl({buffer, static_cast<unsigned>(result.ptr - buffer), true}));
}

}  // namespace

extern "C" {
String APS5_VABI _ZN3WTF10StringImpl6createEPKDsj(const char16_t* data, unsigned length) {
    return String(CopyImpl({data, length, false}));
}

String APS5_VABI _ZN3WTF10StringImpl19createUninitializedEjRPDs(unsigned length, char16_t*& data) {
    void* output;
    auto* impl = NewImpl(length, false, output);
    data = static_cast<char16_t*>(output);
    return String(impl);
}

String APS5_VABI _ZN3WTF10StringImpl6createEPKhj(const unsigned char* data, unsigned length) {
    return String(CopyImpl({data, length, true}));
}

String APS5_VABI _ZN3WTF10StringImpl19createUninitializedEjRPh(unsigned length, unsigned char*& data) {
    void* output;
    auto* impl = NewImpl(length, true, output);
    data = static_cast<unsigned char*>(output);
    return String(impl);
}

String APS5_VABI _ZN3WTF10StringImpl20create8BitIfPossibleEPKDsj(const char16_t* data, unsigned length) {
    return String(CopyImpl({data, length, false}, true));
}

void APS5_VABI _ZN3WTF6StringC1EPKDsj(String* self, const char16_t* data, unsigned length) {
    Checked(self);
    Checked(self).impl = data ? CopyImpl({data, length, false}) : nullptr;
}

void APS5_VABI _ZN3WTF6StringC2EPKDsj(String* self, const char16_t* data, unsigned length) {
    Checked(self);
    Checked(self).impl = data ? CopyImpl({data, length, false}) : nullptr;
}

void APS5_VABI _ZN3WTF6StringC1EPKc(String* self, const char* data) {
    Checked(self);
    Checked(self).impl = data ? CopyImpl({data, static_cast<unsigned>(std::strlen(data)), true}) : nullptr;
}

void APS5_VABI _ZN3WTF6StringC2EPKc(String* self, const char* data) {
    Checked(self);
    Checked(self).impl = data ? CopyImpl({data, static_cast<unsigned>(std::strlen(data)), true}) : nullptr;
}

void APS5_VABI _ZN3WTF6StringC1EPKcj(String* self, const char* data, unsigned length) {
    Checked(self);
    Checked(self).impl = data ? CopyImpl({data, length, true}) : nullptr;
}

void APS5_VABI _ZN3WTF6StringC2EPKcj(String* self, const char* data, unsigned length) {
    Checked(self);
    Checked(self).impl = data ? CopyImpl({data, length, true}) : nullptr;
}

void APS5_VABI _ZN3WTF6StringC1EPKhj(String* self, const unsigned char* data, unsigned length) {
    Checked(self);
    Checked(self).impl = data ? CopyImpl({data, length, true}) : nullptr;
}

void APS5_VABI _ZN3WTF6StringC2EPKhj(String* self, const unsigned char* data, unsigned length) {
    Checked(self);
    Checked(self).impl = data ? CopyImpl({data, length, true}) : nullptr;
}

String APS5_VABI _ZN3WTF6String6numberEi(int value) {
    return IntegerString(value);
}

String APS5_VABI _ZN3WTF6String6numberEj(unsigned value) {
    return IntegerString(value);
}

String APS5_VABI _ZN3WTF6String6numberEl(std::int64_t value) {
    return IntegerString(value);
}

String APS5_VABI _ZN3WTF6String6numberEm(std::uint64_t value) {
    return IntegerString(value);
}

String APS5_VABI _ZN3WTF6String6numberEx(long long value) {
    return IntegerString(value);
}

String APS5_VABI _ZN3WTF6String6numberEy(unsigned long long value) {
    return IntegerString(value);
}

String APS5_VABI _ZN3WTF6String13fromCodePointEi(std::int32_t cp) {
    if (cp < 0 || cp > 0x10FFFF) return String();
    char16_t data[2];
    unsigned length = 1;
    if (cp <= 0xFFFF) data[0] = static_cast<char16_t>(cp);
    else { data[0] = static_cast<char16_t>(0xD800 + ((cp - 0x10000) >> 10)); data[1] = static_cast<char16_t>(0xDC00 + ((cp - 0x10000) & 1023)); length = 2; }
    return String(CopyImpl({data, length, false}));
}

String APS5_VABI _ZN3WTF6String8fromUTF8EPKhm(const unsigned char* data, std::size_t length) {
    return FromUTF8(data, length);
}

String APS5_VABI _ZN3WTF6String8fromUTF8EPKh(const unsigned char* data) {
    return FromUTF8(data, data ? std::strlen(reinterpret_cast<const char*>(data)) : 0);
}

String APS5_VABI _ZN3WTF6String8fromUTF8ERKNS_7CStringE(const CString& value) {
    const char* data = Bytes(value);
    return FromUTF8(reinterpret_cast<const unsigned char*>(data), data ? std::strlen(data) : 0);
}

String APS5_VABI _ZN3WTF6String26fromUTF8WithLatin1FallbackEPKhm(const unsigned char* data, std::size_t length) {
    auto result = FromUTF8(data, length);
    if (result.impl) return result;
    return String(data ? CopyImpl({data, static_cast<unsigned>(length), true}) : nullptr);
}

String APS5_VABI _ZN3WTF10StringImpl23convertToASCIILowercaseEv(const StringImpl* self) {
    return String(ConvertCase(View(&Checked(self)), true, const_cast<StringImpl*>(self)));
}

String APS5_VABI _ZN3WTF10StringImpl23convertToASCIIUppercaseEv(const StringImpl* self) {
    return String(ConvertCase(View(&Checked(self)), false, const_cast<StringImpl*>(self)));
}

CString APS5_VABI _ZNK3WTF10StringImpl4utf8ENS_14ConversionModeE(const StringImpl* self, unsigned mode) {
    return ToUTF8(View(&Checked(self)), mode);
}

String APS5_VABI _ZNK3WTF6String23convertToASCIILowercaseEv(const String* self) {
    return String(ConvertCase(View(Checked(self).impl), true, Checked(self).impl));
}

String APS5_VABI _ZNK3WTF6String23convertToASCIIUppercaseEv(const String* self) {
    return String(ConvertCase(View(Checked(self).impl), false, Checked(self).impl));
}

CString APS5_VABI _ZNK3WTF6String4utf8ENS_14ConversionModeE(const String* self, unsigned mode) {
    return ToUTF8(View(Checked(self).impl), mode);
}

String APS5_VABI _ZNK3WTF10StringView23convertToASCIILowercaseEv(const StringView* self) {
    return String(ConvertCase(Checked(self), true));
}

String APS5_VABI _ZNK3WTF10StringView23convertToASCIIUppercaseEv(const StringView* self) {
    return String(ConvertCase(Checked(self), false));
}

CString APS5_VABI _ZNK3WTF10StringView4utf8ENS_14ConversionModeE(const StringView* self, unsigned mode) {
    return ToUTF8(Checked(self), mode);
}

bool APS5_VABI _ZNK3WTF10StringImpl10startsWithEDs(const StringImpl* self, char16_t value) {
    auto view = View(&Checked(self));
    return view.length && Unit(view, 0) == value;
}

bool APS5_VABI _ZNK3WTF10StringImpl8endsWithEDs(const StringImpl* self, char16_t value) {
    auto view = View(&Checked(self));
    return view.length && Unit(view, view.length - 1) == value;
}

bool APS5_VABI _ZNK3WTF10StringView10startsWithEDs(const StringView* self, char16_t value) {
    auto view = Checked(self);
    return view.length && Unit(view, 0) == value;
}

bool APS5_VABI _ZNK3WTF10StringView8endsWithEDs(const StringView* self, char16_t value) {
    auto view = Checked(self);
    return view.length && Unit(view, view.length - 1) == value;
}

bool APS5_VABI _ZNK3WTF10StringImpl10startsWithEPKcj(const StringImpl* self, const char* data, unsigned length) {
    return Affix(self, data, length, false);
}

bool APS5_VABI _ZNK3WTF10StringImpl8endsWithEPKcj(const StringImpl* self, const char* data, unsigned length) {
    return Affix(self, data, length, true);
}

std::size_t APS5_VABI _ZNK3WTF10StringImpl11sizeInBytesEv(const StringImpl* self) {
    const auto& impl = Checked(self);
    return sizeof(StringImpl) + std::size_t(impl.length) * (impl.hashAndFlags & 4 ? 1 : 2);
}

unsigned APS5_VABI _ZNK3WTF10StringImpl14concurrentHashEv(const StringImpl* self) {
    return Hash(View(&Checked(self)));
}

unsigned APS5_VABI _ZNK3WTF10StringImpl12hashSlowCaseEv(const StringImpl* self) {
    auto& impl = Checked(self);
    unsigned hash = Hash(View(&impl));
    impl.hashAndFlags = (impl.hashAndFlags & 255) | (hash << 8);
    return hash;
}

std::int32_t APS5_VABI _ZN3WTF10StringImpl19characterStartingAtEj(const StringImpl* self, unsigned index) {
    return CharacterAt(View(&Checked(self)), index);
}

std::int32_t APS5_VABI _ZNK3WTF6String19characterStartingAtEj(const String* self, unsigned index) {
    auto view = View(Checked(self).impl);
    return index < view.length ? CharacterAt(view, index) : 0;
}

std::size_t APS5_VABI _ZN3WTF10StringImpl11reverseFindEDsj(const StringImpl* self, char16_t value, unsigned start) {
    auto view = View(&Checked(self));
    if (!view.length) return NotFound;
    std::size_t i = std::min(start, view.length - 1);
    do { if (Unit(view, static_cast<unsigned>(i)) == value) return i; } while (i--);
    return NotFound;
}

String APS5_VABI _ZN3WTF10StringImpl9substringEjj(const StringImpl* self, unsigned start, unsigned length) {
    return Substring(self, start, length);
}

String APS5_VABI _ZNK3WTF6String20substringSharingImplEjj(const String* self, unsigned start, unsigned length) {
    return SharedSubstring(self, start, length);
}

bool APS5_VABI _ZNK3WTF6String27isSafeToSendToAnotherThreadEv(const String* self) {
    const auto* impl = Checked(self).impl;
    return !impl || !impl->length || (impl->refCount == 2 && !(impl->hashAndFlags & 16));
}

CString APS5_VABI _ZNK3WTF6String5asciiEv(const String* self) {
    auto view = View(Checked(self).impl);
    char* output;
    auto result = NewCString(view.length, output);
    for (unsigned i = 0; i < view.length; ++i) {
        auto value = Unit(view, i);
        output[i] = value && (value < 32 || value > 127) ? '?' : static_cast<char>(value);
    }
    return result;
}

CString APS5_VABI _ZNK3WTF6String6latin1Ev(const String* self) {
    auto view = View(Checked(self).impl);
    char* output;
    auto result = NewCString(view.length, output);
    for (unsigned i = 0; i < view.length; ++i) {
        auto value = Unit(view, i);
        output[i] = value > 255 ? '?' : static_cast<char>(value);
    }
    return result;
}

std::size_t APS5_VABI _ZNK3WTF10StringView4findES0_j(const StringView* self, StringView match, unsigned start) {
    auto view = Checked(self);
    if (start > view.length || match.length > view.length - start) return NotFound;
    if (!match.length) return start;
    for (unsigned i = start; i <= view.length - match.length; ++i) {
        unsigned j = 0;
        while (j < match.length && Unit(view, i + j) == Unit(match, j)) ++j;
        if (j == match.length) return i;
    }
    return NotFound;
}

void APS5_VABI _ZNK3WTF10StringView26getCharactersWithASCIICaseENS0_15CaseConvertTypeEPDs(const StringView* self, unsigned type, char16_t* output) {
    auto view = Checked(self);
    CheckBuffer(output, view.length);
    for (unsigned i = 0; i < view.length; ++i) output[i] = static_cast<char16_t>(AsciiCase(Unit(view, i), type == 1));
}

void APS5_VABI _ZNK3WTF10StringView26getCharactersWithASCIICaseENS0_15CaseConvertTypeEPh(const StringView* self, unsigned type, unsigned char* output) {
    auto view = Checked(self);
    CheckBuffer(output, view.length);
    if (!view.is8Bit) throw std::invalid_argument("cannot narrow WTF StringView case output");
    for (unsigned i = 0; i < view.length; ++i) output[i] = static_cast<unsigned char>(AsciiCase(Unit(view, i), type == 1));
}

bool APS5_VABI _ZN3WTF22equalRespectingNullityENS_10StringViewES0_(StringView a, StringView b) {
    if (a.data == b.data) return a.length == b.length;
    if (!a.length && !b.length) return !a.data == !b.data;
    return EqualViews(a, b);
}

bool APS5_VABI _ZN3WTF5equalEPKNS_10StringImplES2_(const StringImpl* a, const StringImpl* b) {
    return EqualImpls(a, b);
}

bool APS5_VABI _ZN3WTF5equalERKNS_10StringImplES2_(const StringImpl& a, const StringImpl& b) {
    return EqualImpls(&a, &b);
}

bool APS5_VABI _ZN3WTF5equalEPKNS_10StringImplEPKDsj(const StringImpl* impl, const char16_t* data, unsigned length) {
    return EqualBuffer(impl, data, length, false);
}

bool APS5_VABI _ZN3WTF5equalEPKNS_10StringImplEPKhj(const StringImpl* impl, const unsigned char* data, unsigned length) {
    return EqualBuffer(impl, data, length, true);
}

bool APS5_VABI _ZN3WTF5equalEPKNS_10StringImplEPKh(const StringImpl* impl, const unsigned char* data) {
    if (!impl || !data) return !impl && !data;
    return EqualBuffer(impl, data, static_cast<unsigned>(std::strlen(reinterpret_cast<const char*>(data))), true);
}

bool APS5_VABI _ZN3WTF20equalIgnoringNullityEPNS_10StringImplES1_(StringImpl* a, StringImpl* b) {
    return EqualViews(View(a), View(b));
}

bool APS5_VABI _ZN3WTF20equalIgnoringNullityEPKDsmPNS_10StringImplE(const char16_t* data, std::size_t length, StringImpl* impl) {
    if (!impl) return !length;
    if (length != impl->length) return false;
    return EqualViews({data, static_cast<unsigned>(length), false}, View(impl));
}

bool APS5_VABI _ZN3WTF22equalIgnoringASCIICaseEPKNS_10StringImplES2_(const StringImpl* a, const StringImpl* b) {
    return EqualImpls(a, b, true);
}

bool APS5_VABI _ZN3WTF29equalIgnoringASCIICaseNonNullEPKNS_10StringImplES2_(const StringImpl* a, const StringImpl* b) {
    Checked(a);
    Checked(b);
    return EqualImpls(a, b, true);
}

void APS5_VABI _ZN3WTF7CStringC1EPKc(CString* self, const char* data) {
    auto value = CopyCString(data, (data ? std::strlen(data) : 0));
    Checked(self).buffer = std::exchange(value.buffer, nullptr);
}

void APS5_VABI _ZN3WTF7CStringC2EPKc(CString* self, const char* data) {
    auto value = CopyCString(data, (data ? std::strlen(data) : 0));
    Checked(self).buffer = std::exchange(value.buffer, nullptr);
}

void APS5_VABI _ZN3WTF7CStringC1EPKcm(CString* self, const char* data, std::size_t length) {
    auto value = CopyCString(data, length);
    Checked(self).buffer = std::exchange(value.buffer, nullptr);
}

void APS5_VABI _ZN3WTF7CStringC2EPKcm(CString* self, const char* data, std::size_t length) {
    auto value = CopyCString(data, length);
    Checked(self).buffer = std::exchange(value.buffer, nullptr);
}

CString APS5_VABI _ZN3WTF7CString16newUninitializedEmRPc(std::size_t length, char*& output) {
    return NewCString(length, output);
}

char* APS5_VABI _ZN3WTF7CString11mutableDataEv(CString* self) {
    auto& value = Checked(self);
    if (!value.buffer) return nullptr;
    if (value.buffer->refCount != 1) {
        auto copy = CopyCString(Bytes(value), value.buffer->length);
        NKWebKit::WtfAbi::Release(value.buffer);
        value.buffer = std::exchange(copy.buffer, nullptr);
    }
    return const_cast<char*>(Bytes(value));
}

void APS5_VABI _ZN3WTF7CString4growEm(CString* self, std::size_t length) {
    auto& value = Checked(self);
    if (!value.buffer || length <= value.buffer->length) throw std::invalid_argument("WTF CString must grow");
    char* output;
    auto copy = NewCString(length, output);
    std::memcpy(output, Bytes(value), value.buffer->length + 1);
    NKWebKit::WtfAbi::Release(value.buffer);
    value.buffer = std::exchange(copy.buffer, nullptr);
}

unsigned APS5_VABI _ZNK3WTF7CString4hashEv(const CString* self) {
    const char* data = Bytes(Checked(self));
    if (!data) return 0;
    StringHasher hasher;
    while (*data) hasher.addCharacter(static_cast<char16_t>(*data++));
    return hasher.hash(false);
}

bool APS5_VABI _ZN3WTFeqERKNS_7CStringES2_(const CString& a, const CString& b) {
    if (!a.buffer || !b.buffer) return !a.buffer && !b.buffer;
    return a.buffer->length == b.buffer->length && !std::memcmp(Bytes(a), Bytes(b), a.buffer->length);
}

bool APS5_VABI _ZN3WTFeqERKNS_7CStringEPKc(const CString& a, const char* b) {
    if (!a.buffer || !b) return !a.buffer && !b;
    return !std::strcmp(Bytes(a), b);
}

bool APS5_VABI _ZN3WTFltERKNS_7CStringES2_(const CString& a, const CString& b) {
    if (!a.buffer) return !!b.buffer;
    return b.buffer && std::strcmp(Bytes(a), Bytes(b)) < 0;
}

bool APS5_VABI _ZN3WTF11CStringHash5equalERKNS_7CStringES3_(const CString& a, const CString& b) {
    auto* deleted = reinterpret_cast<CStringBuffer*>(static_cast<std::uintptr_t>(-1));
    if (a.buffer == deleted) return b.buffer == deleted;
    if (b.buffer == deleted) return false;
    if (!a.buffer || !b.buffer) return !a.buffer && !b.buffer;
    return a.buffer->length == b.buffer->length && !std::memcmp(Bytes(a), Bytes(b), a.buffer->length);
}

}

namespace {

using NKWebKit::WtfAbi::URL;

StringView Slice(StringView view, unsigned start, unsigned length = MaxStringLength) {
    if (start >= view.length) return {"", 0, true};
    length = std::min(length, view.length - start);
    return {static_cast<const unsigned char*>(view.data) + std::size_t(start) * (view.is8Bit ? 1 : 2), length, view.is8Bit};
}

unsigned HostStart(const URL& url) {
    return url.passwordEnd == url.userStart ? url.passwordEnd : url.passwordEnd + 1;
}

unsigned PathStart(const URL& url) {
    unsigned start = url.hostEnd + ((url.bits >> 3) & 7);
    auto view = View(url.impl);
    if (start == (url.bits >> 6) + 1 && start < view.length && view.length - start > 1 && Unit(view, start) == '/' && Unit(view, start + 1) == '.') start += 2;
    return start;
}

bool URLProtocol(const URL& url, StringView protocol) {
    if (!(url.bits & 1) || (url.bits >> 6) != protocol.length) return false;
    auto view = View(url.impl);
    for (unsigned i = 0; i < protocol.length; ++i) {
        if (i >= view.length) throw std::out_of_range("invalid WTF URL scheme boundary");
        if ((Unit(view, i) | 32) != Unit(protocol, i)) return false;
    }
    return true;
}

bool JavaScriptProtocol(StringView view) {
    constexpr char protocol[] = "javascript";
    unsigned index = 0;
    bool leading = true;
    for (unsigned i = 0; i < view.length; ++i) {
        auto value = Unit(view, i);
        if (leading) {
            if (value <= 32) continue;
            leading = false;
        } else if (value == '\t' || value == '\r' || value == '\n') continue;
        if (index == 10) return value == ':';
        if ((value | 32) != protocol[index++]) return false;
    }
    return false;
}

StringView ViewWithoutFragment(const URL& url) {
    auto view = View(url.impl);
    if (!(url.bits & 1)) return view;
    return Slice(view, 0, std::min(url.queryEnd, view.length));
}

}  // namespace

extern "C" {
bool APS5_VABI _ZN3WTF31equalIgnoringFragmentIdentifierERKNS_3URLES2_(const URL* a, const URL* b) {
    const auto& first = Checked(a);
    const auto& second = Checked(b);
    return EqualViews(ViewWithoutFragment(first), ViewWithoutFragment(second));
}

void APS5_VABI _ZN3WTF3URL10invalidateEv(URL* self) {
    auto& url = Checked(self);
    url.bits = 0;
    url.userStart = 0;
    url.userEnd = 0;
    url.passwordEnd = 0;
    url.hostEnd = 0;
    url.pathAfterLastSlash = 0;
    url.pathEnd = 0;
    url.queryEnd = 0;
}

StringView APS5_VABI _ZNK3WTF3URL8protocolEv(const URL* self) {
    const auto& url = Checked(self);
    return url.bits & 1 ? Slice(View(url.impl), 0, url.bits >> 6) : StringView{nullptr, 0, true};
}

StringView APS5_VABI _ZNK3WTF3URL4hostEv(const URL* self) {
    const auto& url = Checked(self);
    unsigned start = HostStart(url);
    return url.bits & 1 ? Slice(View(url.impl), start, url.hostEnd - start) : StringView{nullptr, 0, true};
}

StringView APS5_VABI _ZNK3WTF3URL4pathEv(const URL* self) {
    const auto& url = Checked(self);
    if (!(url.bits & 1)) return {nullptr, 0, true};
    unsigned start = PathStart(url);
    return Slice(View(url.impl), start, url.pathEnd - start);
}

StringView APS5_VABI _ZNK3WTF3URL11encodedUserEv(const URL* self) {
    const auto& url = Checked(self);
    return Slice(View(url.impl), url.userStart, url.userEnd - url.userStart);
}

StringView APS5_VABI _ZNK3WTF3URL15encodedPasswordEv(const URL* self) {
    const auto& url = Checked(self);
    return url.passwordEnd != url.userEnd ? Slice(View(url.impl), url.userEnd + 1, url.passwordEnd - url.userEnd - 1) : StringView{nullptr, 0, true};
}

StringView APS5_VABI _ZNK3WTF3URL18fragmentIdentifierEv(const URL* self) {
    const auto& url = Checked(self);
    auto view = View(url.impl);
    return (url.bits & 1) && view.length > url.queryEnd ? Slice(view, url.queryEnd + 1) : StringView{nullptr, 0, true};
}

StringView APS5_VABI _ZNK3WTF3URL39fragmentIdentifierWithLeadingNumberSignEv(const URL* self) {
    const auto& url = Checked(self);
    auto view = View(url.impl);
    return (url.bits & 1) && view.length > url.queryEnd ? Slice(view, url.queryEnd) : StringView{nullptr, 0, true};
}

StringView APS5_VABI _ZNK3WTF3URL28queryWithLeadingQuestionMarkEv(const URL* self) {
    const auto& url = Checked(self);
    return url.queryEnd > url.pathEnd ? Slice(View(url.impl), url.pathEnd, url.queryEnd - url.pathEnd) : StringView{nullptr, 0, true};
}

bool APS5_VABI _ZNK3WTF3URL10protocolIsENS_10StringViewE(const URL* self, StringView protocol) {
    return URLProtocol(Checked(self), protocol);
}

bool APS5_VABI _ZNK3WTF3URL14isHierarchicalEv(const URL* self) {
    const auto& url = Checked(self);
    if (!(url.bits & 1)) return false;
    auto view = View(url.impl);
    unsigned index = (url.bits >> 6) + 1;
    return index < view.length && Unit(view, index) == '/';
}

bool APS5_VABI _ZNK3WTF3URL12isAboutBlankEv(const URL* self) {
    const auto& url = Checked(self);
    if (!URLProtocol(url, {"about", 5, true})) return false;
    unsigned start = PathStart(url);
    return EqualViews(Slice(View(url.impl), start, url.pathEnd - start), {"blank", 5, true});
}

bool APS5_VABI _ZNK3WTF3URL13isAboutSrcDocEv(const URL* self) {
    const auto& url = Checked(self);
    if (!URLProtocol(url, {"about", 5, true})) return false;
    unsigned start = PathStart(url);
    return EqualViews(Slice(View(url.impl), start, url.pathEnd - start), {"srcdoc", 6, true});
}

bool APS5_VABI _ZNK3WTF3URL20protocolIsJavaScriptEv(const URL* self) {
    return JavaScriptProtocol(View(Checked(self).impl));
}

bool APS5_VABI _ZN3WTF20protocolIsJavaScriptENS_10StringViewE(StringView view) {
    return JavaScriptProtocol(view);
}

bool APS5_VABI _ZN3WTF22protocolIsInHTTPFamilyENS_10StringViewE(StringView view) {
    if (view.length < 5) return false;
    if ((Unit(view, 0) | 32) != 'h' || (Unit(view, 1) | 32) != 't' || (Unit(view, 2) | 32) != 't' || (Unit(view, 3) | 32) != 'p') return false;
    return Unit(view, 4) == ':' || ((Unit(view, 4) | 32) == 's' && view.length >= 6 && Unit(view, 5) == ':');
}

}
