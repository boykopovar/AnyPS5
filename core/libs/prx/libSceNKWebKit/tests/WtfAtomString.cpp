#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/include/WtfAtomStringExports.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

using NKWebKit::WtfAbi::AtomString;
using NKWebKit::WtfAbi::String;
using NKWebKit::WtfAbi::StringImpl;

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "WtfAtomString check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

template <typename Exception, typename Function>
static bool Throws(Function function) {
    try {
        function();
    } catch (const Exception&) {
        return true;
    }
    return false;
}

namespace AtomDriver {

struct Hash {
    std::uint64_t value = 1469598103934665603ull;
    void Mix(std::uint64_t v) {
        for (int i = 0; i < 8; ++i) {
            value ^= (v >> (8 * i)) & 0xff;
            value *= 1099511628211ull;
        }
    }
};

struct Random {
    std::uint32_t state;
    std::uint32_t operator()(std::uint32_t bound) {
        state ^= state << 13;
        state ^= state >> 17;
        state ^= state << 5;
        return state % bound;
    }
};

struct RawImpl {
    unsigned refCount;
    unsigned length;
    const void* data;
    unsigned hashAndFlags;
};

inline const char16_t Alphabet16[] = {u'a', u'b', u'A', u'B', u'z', u'Z', u'0', u'-', 0xE9, 0xC9, 0xFF, 0x100, 0x4E2D, 0xD83D, 0xDE00, 0};
inline const unsigned char Alphabet8[] = {'a', 'b', 'A', 'B', 'z', 'Z', '0', '-', 0xE9, 0xC9, 0xFF, 0x80, ' ', 0};

struct Text {
    std::vector<unsigned char> latin1;
    std::u16string utf16;
};

inline std::vector<unsigned char> Latin1(Random& random, unsigned maxLength) {
    unsigned length = random(maxLength + 1);
    std::vector<unsigned char> result;
    for (unsigned i = 0; i < length; ++i) {
        unsigned pick = random(sizeof(Alphabet8));
        if (pick >= 8) pick = random(sizeof(Alphabet8));
        result.push_back(Alphabet8[pick]);
    }
    return result;
}

inline std::u16string Utf16(Random& random, unsigned maxLength) {
    unsigned length = random(maxLength + 1);
    std::u16string result;
    bool narrow = random(3) == 0;
    for (unsigned i = 0; i < length; ++i) {
        unsigned pick = random(narrow ? 11 : sizeof(Alphabet16) / sizeof(Alphabet16[0]));
        if (pick >= 8) pick = random(narrow ? 11 : sizeof(Alphabet16) / sizeof(Alphabet16[0]));
        result.push_back(Alphabet16[pick]);
    }
    return result;
}

inline std::string Utf8(Random& random) {
    static const char* const pieces[] = {"a", "b", "A", "Z", "-", "\xC3\xA9", "\xC3\x89", "\xE4\xB8\xAD", "\xF0\x9F\x98\x80", "\xC3", "\x80", "\xED\xA0\x80", "\xF4\x90\x80\x80", "\xC0\xAF", "\xE0\x80\xAF", "0"};
    unsigned count = 1 + random(6);
    std::string result;
    for (unsigned i = 0; i < count; ++i) {
        unsigned pick = random(16);
        if (pick >= 9) pick = random(16);
        result += pieces[pick];
    }
    return result;
}

template<typename Adapter> struct Runner {
    Adapter& adapter;
    Random random;
    Hash hash;
    std::vector<void*> held;
    std::deque<RawImpl> statics;
    std::deque<std::vector<unsigned char>> staticText;

    void Describe(void* input, void* result) {
        hash.Mix(0xABCD);
        if (!result) {
            hash.Mix(0);
            return;
        }
        hash.Mix(1);
        hash.Mix(result == input);
        std::uint64_t same = held.size();
        for (std::size_t i = 0; i < held.size(); ++i) {
            if (held[i] == result) {
                same = i;
                break;
            }
        }
        hash.Mix(same == held.size() ? ~0ull : same);
        const auto* raw = static_cast<const RawImpl*>(result);
        hash.Mix(raw->length);
        hash.Mix(raw->hashAndFlags & 0x37);
        hash.Mix(raw->hashAndFlags >> 8);
        hash.Mix(raw->refCount & 1 ? 1 : raw->refCount);
        for (unsigned i = 0; i < raw->length; ++i)
            hash.Mix(raw->hashAndFlags & 4 ? static_cast<const unsigned char*>(raw->data)[i] : static_cast<const char16_t*>(raw->data)[i]);
        held.push_back(result);
    }

    void* Pick() {
        if (held.empty()) return nullptr;
        unsigned index = random(static_cast<std::uint32_t>(held.size()));
        return held[index];
    }

    void* PickNonAtom() {
        for (int attempt = 0; attempt < 4; ++attempt) {
            void* candidate = Pick();
            if (candidate && !(static_cast<RawImpl*>(candidate)->hashAndFlags & 16)) return candidate;
        }
        return nullptr;
    }

    void Step() {
        switch (random(17)) {
        case 0: {
            auto text = Latin1(random, 8);
            Describe(nullptr, adapter.AddLatin1(text.empty() && random(4) == 0 ? nullptr : text.data(), static_cast<unsigned>(text.size())));
            break;
        }
        case 1: {
            auto text = Utf16(random, 8);
            Describe(nullptr, adapter.AddUtf16(text.empty() && random(4) == 0 ? nullptr : text.data(), static_cast<unsigned>(text.size())));
            break;
        }
        case 2: {
            auto text = Latin1(random, 8);
            Describe(nullptr, adapter.LookUpLatin1(text.data(), static_cast<unsigned>(text.size())));
            break;
        }
        case 3: {
            auto text = Utf16(random, 8);
            Describe(nullptr, adapter.LookUpUtf16(text.data(), static_cast<unsigned>(text.size())));
            break;
        }
        case 4: {
            auto text = Latin1(random, 6);
            if (text.empty()) text.push_back('q');
            staticText.push_back(text);
            Describe(nullptr, adapter.AddLiteral(reinterpret_cast<const char*>(staticText.back().data()), static_cast<unsigned>(staticText.back().size())));
            break;
        }
        case 5: {
            auto text = Latin1(random, 6);
            if (text.empty()) text.push_back('s');
            staticText.push_back(text);
            statics.push_back({1, static_cast<unsigned>(text.size()), staticText.back().data(), 4 | 8});
            adapter.SetStaticHash(statics.back());
            Describe(nullptr, adapter.AddStatic(&statics.back()));
            break;
        }
        case 6:
        case 7: {
            unsigned kind = random(3);
            void* created;
            if (kind == 0) {
                auto text = Latin1(random, 8);
                text.push_back('x');
                created = adapter.NewLatin1(text.data(), static_cast<unsigned>(text.size()));
            } else if (kind == 1) {
                auto text = Utf16(random, 8);
                text.push_back(0x101);
                created = adapter.NewUtf16(text.data(), static_cast<unsigned>(text.size()));
            } else {
                auto text = Latin1(random, 40);
                text.push_back('y');
                unsigned start = random(static_cast<std::uint32_t>(text.size()));
                unsigned length = 1 + random(static_cast<std::uint32_t>(text.size() - start));
                created = adapter.NewSubstring(text.data(), static_cast<unsigned>(text.size()), start, length);
            }
            Describe(nullptr, created);
            break;
        }
        case 8: {
            void* string = PickNonAtom();
            if (!string) break;
            bool withTable = random(2);
            Describe(string, adapter.AddSlowCase(string, withTable));
            break;
        }
        case 9: {
            void* string = PickNonAtom();
            if (!string) break;
            Describe(string, adapter.LookUpSlowCase(string));
            break;
        }
        case 10: {
            void* string = Pick();
            if (!string) break;
            unsigned length = static_cast<RawImpl*>(string)->length;
            unsigned start = random(length + 2);
            unsigned count = random(length + 2);
            Describe(string, adapter.AddSubstring(string, start, count));
            break;
        }
        case 11: {
            std::string text = Utf8(random);
            bool terminated = random(2);
            Describe(nullptr, adapter.FromUtf8(text.c_str(), terminated ? nullptr : text.c_str() + text.size()));
            break;
        }
        case 12: {
            unsigned kind = random(6);
            std::uint64_t bits = random(0x10000);
            bits = (bits << 16) | random(0x10000);
            bits = (bits << 16) | random(0x10000);
            bits = (bits << 16) | random(0x10000);
            Describe(nullptr, adapter.Number(kind, bits));
            break;
        }
        case 13:
        case 14: {
            void* string = Pick();
            if (!string || !(static_cast<RawImpl*>(string)->hashAndFlags & 16)) break;
            Describe(string, adapter.ConvertCase(string, random(2)));
            break;
        }
        default: {
            if (held.empty()) break;
            unsigned index = random(static_cast<std::uint32_t>(held.size()));
            adapter.Release(held[index]);
            held.erase(held.begin() + index);
            hash.Mix(0xDEAD);
            hash.Mix(held.size());
            break;
        }
        }
    }

    std::uint64_t Run(int count) {
        for (int i = 0; i < count; ++i) Step();
        for (void* string : held) {
            hash.Mix(static_cast<RawImpl*>(string)->refCount & 1 ? 1 : static_cast<RawImpl*>(string)->refCount);
            hash.Mix(static_cast<RawImpl*>(string)->hashAndFlags & 0x37);
        }
        for (void* string : held) adapter.Release(string);
        held.clear();
        return hash.value;
    }
};

template<typename Adapter> std::uint64_t Run(Adapter& adapter, std::uint32_t seed, int count) {
    Runner<Adapter> runner{adapter, Random{seed ? seed : 1}};
    return runner.Run(count);
}

inline double NumberDouble(std::uint64_t bits) {
    switch (bits % 7) {
    case 0: return 0.0;
    case 1: return -0.0;
    case 2: return static_cast<double>(static_cast<std::int64_t>(bits >> 20) % 100000) / 64.0;
    case 3: return 1e21 * static_cast<double>(bits % 13);
    case 4: return 1e-7 * static_cast<double>(bits % 1000);
    case 5: {
        double value;
        std::uint64_t pattern = bits & 0x7FEFFFFFFFFFFFFFull;
        std::memcpy(&value, &pattern, sizeof(value));
        return bits & 8 ? -value : value;
    }
    default: return (bits & 1) ? std::numeric_limits<double>::infinity() : -std::numeric_limits<double>::infinity();
    }
}

}  // namespace AtomDriver

struct ExportAdapter {
    static void* Leak(String&& value) { return std::exchange(value.impl, nullptr); }
    static void* Leak(AtomString&& value) { return std::exchange(value.string.impl, nullptr); }

    void* AddLatin1(const unsigned char* data, unsigned length) { return Leak(_ZN3WTF14AtomStringImpl3addEPKhj(data, length)); }
    void* AddUtf16(const char16_t* data, unsigned length) { return Leak(_ZN3WTF14AtomStringImpl3addEPKDsj(data, length)); }
    void* LookUpLatin1(const unsigned char* data, unsigned length) { return Leak(_ZN3WTF14AtomStringImpl6lookUpEPKhj(data, length)); }
    void* LookUpUtf16(const char16_t* data, unsigned length) { return Leak(_ZN3WTF14AtomStringImpl6lookUpEPKDsj(data, length)); }
    void* AddLiteral(const char* data, unsigned length) { return Leak(_ZN3WTF14AtomStringImpl10addLiteralEPKcj(data, length)); }
    void SetStaticHash(AtomDriver::RawImpl& raw) {
        String copy = _ZN3WTF10StringImpl6createEPKhj(static_cast<const unsigned char*>(raw.data), raw.length);
        raw.hashAndFlags |= _ZNK3WTF10StringImpl12hashSlowCaseEv(copy.impl) << 8;
    }
    void* AddStatic(AtomDriver::RawImpl* raw) { return Leak(_ZN3WTF14AtomStringImpl3addEPKNS_10StringImpl16StaticStringImplE(reinterpret_cast<const StringImpl*>(raw))); }
    void* NewLatin1(const unsigned char* data, unsigned length) { return Leak(_ZN3WTF10StringImpl6createEPKhj(data, length)); }
    void* NewUtf16(const char16_t* data, unsigned length) { return Leak(_ZN3WTF10StringImpl6createEPKDsj(data, length)); }
    void* NewSubstring(const unsigned char* data, unsigned size, unsigned start, unsigned length) {
        String whole = _ZN3WTF10StringImpl6createEPKhj(data, size);
        return Leak(NKWebKit::WtfAbi::SubstringSharingImpl(whole, start, length));
    }
    void* AddSlowCase(void* string, bool withTable) {
        static int table;
        auto* impl = static_cast<StringImpl*>(string);
        if (withTable) return Leak(_ZN3WTF14AtomStringImpl11addSlowCaseERNS_15AtomStringTableERNS_10StringImplE(&table, impl));
        return Leak(_ZN3WTF14AtomStringImpl11addSlowCaseERNS_10StringImplE(impl));
    }
    void* LookUpSlowCase(void* string) { return Leak(_ZN3WTF14AtomStringImpl14lookUpSlowCaseERNS_10StringImplE(static_cast<StringImpl*>(string))); }
    void* AddSubstring(void* string, unsigned start, unsigned length) { return Leak(_ZN3WTF14AtomStringImpl3addEPNS_10StringImplEjj(static_cast<StringImpl*>(string), start, length)); }
    void* FromUtf8(const char* start, const char* end) { return Leak(_ZN3WTF10AtomString16fromUTF8InternalEPKcS2_(start, end)); }
    void* Number(unsigned kind, std::uint64_t bits) {
        switch (kind) {
        case 0: return Leak(_ZN3WTF10AtomString6numberEi(static_cast<int>(bits)));
        case 1: return Leak(_ZN3WTF10AtomString6numberEj(static_cast<unsigned>(bits)));
        case 2: return Leak(_ZN3WTF10AtomString6numberEm(bits));
        case 3: return Leak(_ZN3WTF10AtomString6numberEy(static_cast<unsigned long long>(bits >> (bits % 64))));
        case 4: return Leak(_ZN3WTF10AtomString6numberEf(static_cast<float>(AtomDriver::NumberDouble(bits))));
        default: return Leak(_ZN3WTF10AtomString6numberEd(AtomDriver::NumberDouble(bits)));
        }
    }
    void* ConvertCase(void* string, bool lower) {
        auto* impl = static_cast<StringImpl*>(string);
        impl->refCount += 2;
        const AtomString atom{String(impl)};
        return Leak(lower ? _ZNK3WTF10AtomString23convertToASCIILowercaseEv(&atom) : _ZNK3WTF10AtomString23convertToASCIIUppercaseEv(&atom));
    }
    void Release(void* string) {
        auto* impl = static_cast<StringImpl*>(string);
        if (impl->refCount == 2) {
            _ZN3WTF10StringImpl7destroyEPS0_(impl);
            return;
        }
        impl->refCount -= 2;
    }
};

static const std::uint64_t ExpectedHashes[] = {
    0x0798a59f83acf97bULL,
    0x69914ba89b6c05dcULL,
    0xb0fbbc10137e2e66ULL,
    0x4a99c24c4729bf49ULL,
    0x207f2b6297d9f71eULL,
    0xe46ace8c6bb60400ULL,
    0x530b8f2f44ab653dULL,
    0x2e75d991d16023fbULL,
    0xf9431df89e7f6abeULL,
    0x06fc1d23562d6ba9ULL,
    0xa34bfd34fffb1d4aULL,
    0xf97675d296a133a3ULL,
    0xc56b56d896c8cf06ULL,
    0xbb0ca56ee55c7902ULL,
    0x98e5392a64a4e4d1ULL,
    0x258c914d64185c04ULL,
};

static std::string Text(const StringImpl* string) {
    std::string result;
    for (unsigned i = 0; i < string->length; ++i)
        result += static_cast<char>(string->hashAndFlags & 4 ? static_cast<const unsigned char*>(string->data)[i] : static_cast<const char16_t*>(string->data)[i]);
    return result;
}

static const unsigned char* Bytes(const char* text) {
    return reinterpret_cast<const unsigned char*>(text);
}

static void TestTranscripts() {
    for (std::uint32_t seed = 1; seed <= 16; ++seed) {
        ExportAdapter adapter;
        Require(AtomDriver::Run(adapter, seed * 2654435761u, 1000) == ExpectedHashes[seed - 1]);
    }
}

static void TestTableLifetime() {
    StringImpl* first;
    {
        String a = _ZN3WTF14AtomStringImpl3addEPKhj(Bytes("table"), 5);
        String b = _ZN3WTF14AtomStringImpl3addEPKDsj(u"table", 5);
        Require(a.impl == b.impl && (a.impl->hashAndFlags & 16) && a.impl->refCount == 4);
        String found = _ZN3WTF14AtomStringImpl6lookUpEPKhj(Bytes("table"), 5);
        Require(found.impl == a.impl);
        first = a.impl;
    }
    String missing = _ZN3WTF14AtomStringImpl6lookUpEPKDsj(u"table", 5);
    Require(!missing.impl);
    String again = _ZN3WTF14AtomStringImpl3addEPKhj(Bytes("table"), 5);
    Require(again.impl && Text(again.impl) == "table");
    static_cast<void>(first);

    String plain = _ZN3WTF10StringImpl6createEPKhj(Bytes("table"), 5);
    Require(!(plain.impl->hashAndFlags & 16));
    String atom = _ZN3WTF14AtomStringImpl11addSlowCaseERNS_10StringImplE(plain.impl);
    Require(atom.impl == again.impl && !(plain.impl->hashAndFlags & 16));
    String looked = _ZN3WTF14AtomStringImpl14lookUpSlowCaseERNS_10StringImplE(plain.impl);
    Require(looked.impl == again.impl);

    String fresh = _ZN3WTF10StringImpl6createEPKhj(Bytes("fresh"), 5);
    String adopted = _ZN3WTF14AtomStringImpl11addSlowCaseERNS_10StringImplE(fresh.impl);
    Require(adopted.impl == fresh.impl && (fresh.impl->hashAndFlags & 16) && fresh.impl->refCount == 4);
    Require((fresh.impl->hashAndFlags >> 8) != 0);
}

static void TestThreadTables() {
    String mine = _ZN3WTF14AtomStringImpl3addEPKhj(Bytes("thread"), 6);
    String theirs;
    std::thread worker([&] {
        String other = _ZN3WTF14AtomStringImpl3addEPKhj(Bytes("thread"), 6);
        Require(other.impl != mine.impl && (other.impl->hashAndFlags & 16));
        String none = _ZN3WTF14AtomStringImpl6lookUpEPKhj(Bytes("only-main"), 9);
        Require(!none.impl);
        theirs = other;
    });
    worker.join();
    Require(theirs.impl && !(theirs.impl->hashAndFlags & 16) && Text(theirs.impl) == "thread");
    theirs = String();
    String still = _ZN3WTF14AtomStringImpl6lookUpEPKhj(Bytes("thread"), 6);
    Require(still.impl == mine.impl);
}

static void TestEdges() {
    Require(!_ZN3WTF14AtomStringImpl3addEPKhj(nullptr, 3).impl);
    String empty = _ZN3WTF14AtomStringImpl3addEPKDsj(u"", 0);
    Require(empty.impl && !empty.impl->length && (empty.impl->hashAndFlags & 16) && (empty.impl->refCount & 1));
    String base = _ZN3WTF10StringImpl6createEPKhj(Bytes("prefix-body-suffix"), 18);
    String body = _ZN3WTF14AtomStringImpl3addEPNS_10StringImplEjj(base.impl, 7, 4);
    Require(Text(body.impl) == "body" && (body.impl->hashAndFlags & 16));
    String tail = _ZN3WTF14AtomStringImpl3addEPNS_10StringImplEjj(base.impl, 12, 100);
    Require(Text(tail.impl) == "suffix");
    String past = _ZN3WTF14AtomStringImpl3addEPNS_10StringImplEjj(base.impl, 18, 1);
    Require(past.impl && !past.impl->length);
    String whole = _ZN3WTF14AtomStringImpl3addEPNS_10StringImplEjj(base.impl, 0, 18);
    Require(whole.impl == base.impl && (base.impl->hashAndFlags & 16));

    String literal = _ZN3WTF14AtomStringImpl10addLiteralEPKcj("literal", 7);
    Require(literal.impl->data != nullptr && Text(literal.impl) == "literal" && (literal.impl->hashAndFlags & 3) == 0);

    const AtomString number = _ZN3WTF10AtomString6numberEd(1.5);
    Require(Text(number.string.impl) == "1.5");
    Require(Text(_ZN3WTF10AtomString6numberEd(1e21).string.impl) == "1e+21");
    Require(Text(_ZN3WTF10AtomString6numberEd(-0.0).string.impl) == "0");
    Require(Text(_ZN3WTF10AtomString6numberEd(std::numeric_limits<double>::quiet_NaN()).string.impl) == "NaN");
    Require(Text(_ZN3WTF10AtomString6numberEf(0.1f).string.impl) == "0.1");
    Require(Text(_ZN3WTF10AtomString6numberEi(-42).string.impl) == "-42");
    Require(Text(_ZN3WTF10AtomString6numberEy(18446744073709551615ULL).string.impl) == "18446744073709551615");
    const AtomString same = _ZN3WTF10AtomString6numberEj(7);
    Require(same.string.impl == _ZN3WTF10AtomString6numberEm(7).string.impl);

    const AtomString mixed{_ZN3WTF14AtomStringImpl3addEPKhj(Bytes("MiXeD"), 5)};
    const AtomString lower = _ZNK3WTF10AtomString23convertToASCIILowercaseEv(&mixed);
    Require(Text(lower.string.impl) == "mixed" && (lower.string.impl->hashAndFlags & 16));
    const AtomString lowerAgain = _ZNK3WTF10AtomString23convertToASCIILowercaseEv(&lower);
    Require(lowerAgain.string.impl == lower.string.impl);
    const AtomString null{};
    Require(!_ZNK3WTF10AtomString23convertToASCIIUppercaseEv(&null).string.impl);

    Require(!_ZN3WTF10AtomString16fromUTF8InternalEPKcS2_("\xC3", nullptr).string.impl);
    const AtomString utf8 = _ZN3WTF10AtomString16fromUTF8InternalEPKcS2_("caf\xC3\xA9", nullptr);
    Require(utf8.string.impl->length == 4 && !(utf8.string.impl->hashAndFlags & 4));

    static const unsigned char symbolText[] = "symbol-description-text";
    StringImpl symbol{4, 23, symbolText, 4 | 32};
    String fromSymbol = _ZN3WTF14AtomStringImpl11addSlowCaseERNS_10StringImplE(&symbol);
    Require(fromSymbol.impl != &symbol && !(symbol.hashAndFlags & 16) && symbol.refCount == 6);
    Require((fromSymbol.impl->hashAndFlags & 3) == 2 && fromSymbol.impl->data == symbolText && (fromSymbol.impl->hashAndFlags & 16));
    Require(Text(fromSymbol.impl) == "symbol-description-text");
    fromSymbol = String();
    Require(symbol.refCount == 4);
    StringImpl shortSymbol{4, 6, symbolText, 4 | 32};
    String fromShort = _ZN3WTF14AtomStringImpl11addSlowCaseERNS_10StringImplE(&shortSymbol);
    Require((fromShort.impl->hashAndFlags & 3) == 0 && fromShort.impl->data != symbolText && Text(fromShort.impl) == "symbol");
    Require(Throws<std::logic_error>([&] { _ZN3WTF14AtomStringImpl11addSlowCaseERNS_10StringImplE(body.impl); }));
    Require(Throws<std::logic_error>([&] { _ZN3WTF14AtomStringImpl14lookUpSlowCaseERNS_10StringImplE(body.impl); }));
    Require(Throws<std::invalid_argument>([] { _ZN3WTF14AtomStringImpl11addSlowCaseERNS_10StringImplE(nullptr); }));
    Require(Throws<std::invalid_argument>([] { _ZN3WTF14AtomStringImpl10addLiteralEPKcj(nullptr, 1); }));
    Require(Throws<std::invalid_argument>([&] { _ZN3WTF14AtomStringImpl3addEPKNS_10StringImpl16StaticStringImplE(base.impl); }));
    Require(Throws<std::invalid_argument>([] { _ZN3WTF10AtomString16fromUTF8InternalEPKcS2_(nullptr, nullptr); }));
    Require(base.impl->refCount == 4 && Throws<std::logic_error>([&] { _ZN3WTF10StringImpl7destroyEPS0_(base.impl); }));
    Require(Throws<std::invalid_argument>([] { _ZN3WTF10StringImpl7destroyEPS0_(nullptr); }));
}

int main() {
    TestTranscripts();
    TestTableLifetime();
    TestThreadTables();
    TestEdges();
    return 0;
}
