/*
 * Derived from src/fclang.c, src/fcmatch.c and src/fcstr.c (Fontconfig 72b9a48f57de6204d99ce1c217b5609ee92ece9b)
 *
 * Copyright © 2000 Keith Packard
 * Copyright © 2002 Keith Packard
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of the author(s) not be used in
 * advertising or publicity pertaining to distribution of the software without
 * specific, written prior permission.  The authors make no
 * representations about the suitability of this software for any purpose.  It
 * is provided "as is" without express or implied warranty.
 *
 * THE AUTHOR(S) DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL THE AUTHOR(S) BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

#include "prx/libfontconfig/include/Config.hpp"
#include "prx/libfontconfig/include/InventoryExports.hpp"
#include "prx/libfontconfig/include/Sets.hpp"
#include "prx/libc/include/General.hpp"
#include "FontconfigData.hpp"
#include "prx/libfontconfig/include/FormatAccess.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <iterator>
#include <list>
#include <map>
#include <memory>
#include <new>
#include <set>
#include <string>
#include <string_view>

struct FcLangSet {
    std::array<bool, std::size(fontconfig_data::Languages)> known{};
    std::set<std::string> extra;
    std::vector<std::string> extraOrder;
};

namespace {

unsigned char Lower(unsigned char value) {
    return value >= 'A' && value <= 'Z' ? value + ('a' - 'A') : value;
}

class CaseWalker {
public:
    explicit CaseWalker(std::string_view value, std::string_view skip = {}) : source(value), skip(skip), size(value.size()) {}

    unsigned char Next() {
        if (read < end) return buffer[read++];
        while (!source.empty() && skip.find(source.front()) != std::string_view::npos) source.remove_prefix(1);
        if (source.empty()) {
            finished = true;
            return 0;
        }
        const auto first = static_cast<unsigned char>(source.front());
        source.remove_prefix(1);
        if (first < 0xc0) return Lower(first);
        unsigned length = 2;
        while (length < 7 && (first & (0x80 >> length))) ++length;
        if (length > 6 || source.size() < length - 1) return first;
        std::uint32_t codepoint = first & (0x7f >> length);
        for (unsigned i = 0; i < length - 1; ++i) {
            const auto next = static_cast<unsigned char>(source[i]);
            if ((next & 0xc0) != 0x80) return first;
            codepoint = (codepoint << 6) | (next & 0x3f);
        }
        const auto fold = std::lower_bound(std::begin(fontconfig_data::Folds), std::end(fontconfig_data::Folds), codepoint,
            [](const auto& entry, std::uint32_t point) { return entry.upper < point; });
        if (fold == std::end(fontconfig_data::Folds) || fold->upper != codepoint) return first;
        source.remove_prefix(length - 1);
        read = end = 0;
        for (unsigned i = 0; i < fold->count; ++i) Encode(fold->lower[i]);
        if (!end) return 0;
        return buffer[read++];
    }

    std::size_t Consumed() const { return size - source.size() + (finished ? 1 : 0); }

private:
    void Encode(std::uint32_t value) {
        if (value >= 0x80000000u) return;
        if (value < 0x80) {
            buffer[end++] = static_cast<unsigned char>(value);
        } else if (value < 0x800) {
            buffer[end++] = 0xc0 | (value >> 6);
            buffer[end++] = 0x80 | (value & 0x3f);
        } else if (value < 0x10000) {
            buffer[end++] = 0xe0 | (value >> 12);
            buffer[end++] = 0x80 | ((value >> 6) & 0x3f);
            buffer[end++] = 0x80 | (value & 0x3f);
        } else {
            buffer[end++] = 0xf0 | (value >> 18);
            buffer[end++] = 0x80 | ((value >> 12) & 0x3f);
            buffer[end++] = 0x80 | ((value >> 6) & 0x3f);
            buffer[end++] = 0x80 | (value & 0x3f);
        }
    }

    std::string_view source;
    std::string_view skip;
    std::size_t size = 0;
    bool finished = false;
    std::array<unsigned char, 12> buffer{};
    unsigned read = 0;
    unsigned end = 0;
};

int CompareIgnoringCase(std::string_view first, std::string_view second) {
    CaseWalker left(first), right(second);
    for (;;) {
        const auto a = left.Next();
        const auto b = right.Next();
        if (!a || a != b) return static_cast<int>(a) - b;
    }
}

int CompareIgnoringBlanks(std::string_view first, std::string_view second) {
    CaseWalker left(first, " "), right(second, " ");
    for (;;) {
        const auto a = left.Next();
        const auto b = right.Next();
        if (!a || a != b) return static_cast<int>(a) - b;
    }
}

std::size_t MatchedPrefix(std::string_view first, std::string_view second, std::string_view delimiters) {
    CaseWalker left(first, delimiters), right(second, delimiters);
    for (;;) {
        const auto a = left.Next();
        const auto b = right.Next();
        if (!a || a != b) return left.Consumed() - 1;
    }
}

bool GlobMatch(std::string_view glob, std::string_view text) {
    while (!glob.empty()) {
        const char c = glob.front();
        glob.remove_prefix(1);
        if (c == '*') {
            if (glob.empty()) return true;
            if (glob.find('*') == std::string_view::npos) {
                if (text.size() < glob.size()) return false;
                text.remove_prefix(text.size() - glob.size());
            }
            for (; !text.empty(); text.remove_prefix(1))
                if (GlobMatch(glob, text)) return true;
            return false;
        }
        if (text.empty() || (c != '?' && text.front() != c)) return false;
        text.remove_prefix(1);
    }
    return text.empty();
}

FcLangResult CompareLanguages(std::string_view first, std::string_view second) {
    const auto at = [](std::string_view value, std::size_t index) {
        return index < value.size() ? Lower(static_cast<unsigned char>(value[index])) : 0;
    };
    const auto isEnd = [](unsigned char value) { return value == '-' || value == 0; };
    bool undefined = at(first, 0) == 'u' && at(first, 1) == 'n' && at(first, 2) == 'd' && isEnd(at(first, 3));
    auto result = FcLangDifferentLang;
    for (std::size_t i = 0;; ++i) {
        const auto a = at(first, i), b = at(second, i);
        if (a != b) return !undefined && isEnd(a) && isEnd(b) ? FcLangDifferentTerritory : result;
        if (!a) return undefined ? result : FcLangEqual;
        if (a == '-' && !undefined) result = FcLangDifferentTerritory;
        if (undefined && i == 3) undefined = false;
    }
}

int CompareLanguageIndex(std::string_view known, std::string_view language) {
    for (unsigned i = 0; i < 2; ++i) {
        const auto a = i < known.size() ? Lower(static_cast<unsigned char>(known[i])) : 0;
        const auto b = i < language.size() ? Lower(static_cast<unsigned char>(language[i])) : 0;
        if (a != b) return a - b;
        if (!a) return 0;
    }
    return CompareIgnoringCase(known.substr(2), language.substr(2));
}

struct ReleaseCharSet {
    void operator()(FcCharSet* set) const { FcCharSetDestroy(set); }
};

using CharSetValue = std::unique_ptr<FcCharSet, ReleaseCharSet>;

CharSetValue RetainCharSet(const FcCharSet* source) {
    auto* set = const_cast<FcCharSet*>(source);
    if (set) set->references.fetch_add(1, std::memory_order_relaxed);
    return CharSetValue(set);
}

struct PatternValue {
    FcType type = FcTypeVoid;
    int integer = 0;
    double real = 0;
    std::string text;
    FcMatrix matrix{};
    CharSetValue charset;
    std::unique_ptr<FcLangSet> langset;

    PatternValue() = default;
    PatternValue(PatternValue&&) noexcept = default;
    PatternValue& operator=(PatternValue&&) noexcept = default;
    PatternValue(const PatternValue& other)
        : type(other.type), integer(other.integer), real(other.real), text(other.text), matrix(other.matrix),
          charset(RetainCharSet(other.charset.get())),
          langset(other.langset ? std::make_unique<FcLangSet>(*other.langset) : nullptr) {}

    FcValue View() const {
        FcValue value{};
        value.type = type;
        switch (type) {
        case FcTypeInteger: value.u.i = integer; break;
        case FcTypeBool: value.u.b = integer; break;
        case FcTypeDouble: value.u.d = real; break;
        case FcTypeString: value.u.s = reinterpret_cast<const FcChar8*>(text.c_str()); break;
        case FcTypeMatrix: value.u.m = &matrix; break;
        case FcTypeCharSet: value.u.c = charset.get(); break;
        case FcTypeLangSet: value.u.l = langset.get(); break;
        default: NotImplemented_nid_no_patch(__func__);
        }
        return value;
    }
};

template <typename T>
void ReadGuestValue(T& destination, const T* source, const char* caller) {
    const int savedErrno = errno;
    const bool ok = GuestCopyIn_nid_no_patch(&destination, source, sizeof(destination));
    errno = savedErrno;
    if (!ok) NotImplemented_nid_no_patch(caller);
}

template <typename T>
void WriteGuestValue(T* destination, const T& value, const char* caller) {
    const int savedErrno = errno;
    const bool ok = GuestCopyOut_nid_no_patch(destination, &value, sizeof(value));
    errno = savedErrno;
    if (!ok) NotImplemented_nid_no_patch(caller);
}

template <typename T, typename TExtract>
FcResult GetPatternTyped(const FcPattern* pattern, const char* object, int id, T* destination, FcType expectedType,
                         TExtract extract, const char* caller) {
    FcValue found{};
    const auto result = FcPatternGet_nid_postfix(pattern, object, id, &found);
    if (result != FcResultMatch) return result;
    if (found.type != expectedType) return FcResultTypeMismatch;
    if (!destination) NotImplemented_nid_no_patch(caller);
    WriteGuestValue(destination, extract(found), caller);
    return FcResultMatch;
}

bool ValidType(std::string_view object, FcType type) {
    for (const auto& entry : fontconfig_data::Objects) {
        if (entry.name != object) continue;
        if (entry.type == FcTypeInteger || entry.type == FcTypeDouble || entry.type == FcTypeRange)
            return type == FcTypeInteger || type == FcTypeDouble;
        if (entry.type == FcTypeLangSet) return type == FcTypeString || type == FcTypeLangSet;
        return entry.type == type;
    }
    return true;
}

bool ValuesEqual(const PatternValue& first, const PatternValue& second) {
    if (first.type != second.type) {
        if (first.type == FcTypeInteger && second.type == FcTypeDouble) return first.integer == second.real;
        if (first.type == FcTypeDouble && second.type == FcTypeInteger) return first.real == second.integer;
        return false;
    }
    switch (first.type) {
    case FcTypeInteger:
    case FcTypeBool: return first.integer == second.integer;
    case FcTypeDouble: return first.real == second.real;
    case FcTypeString: return CompareIgnoringCase(first.text, second.text) == 0;
    case FcTypeCharSet: return first.charset->leaves == second.charset->leaves;
    case FcTypeLangSet: return first.langset->known == second.langset->known && first.langset->extra == second.langset->extra;
    case FcTypeMatrix: return first.matrix.xx == second.matrix.xx && first.matrix.xy == second.matrix.xy
        && first.matrix.yx == second.matrix.yx && first.matrix.yy == second.matrix.yy;
    default: NotImplemented_nid_no_patch(__func__);
    }
}

}

struct FcPattern {
    std::atomic<unsigned> references{1};
    std::map<std::string, std::list<PatternValue>, std::less<>> objects;

    FcPattern() = default;
    FcPattern(const FcPattern& other) : objects(other.objects) {}
};

namespace {

FcBool AddValue(FcPattern* pattern, const char* object, PatternValue value) {
    if (!pattern || !object) NotImplemented_nid_no_patch(__func__);
    if (!ValidType(object, value.type)) return 0;
    const auto found = pattern->objects.find(object);
    if (found != pattern->objects.end()) {
        found->second.push_back(std::move(value));
    } else {
        std::list<PatternValue> values;
        values.push_back(std::move(value));
        pattern->objects.emplace(object, std::move(values));
    }
    return 1;
}

enum class Matcher { None, Family, String, Number, Bool, Filename, CharSet, Lang, PostScript, Range, Size };

struct MatcherEntry {
    std::string_view object;
    Matcher matcher;
};

constexpr MatcherEntry Matchers[] = {
    {"family", Matcher::Family}, {"style", Matcher::String}, {"slant", Matcher::Number}, {"weight", Matcher::Range},
    {"width", Matcher::Range}, {"size", Matcher::Size}, {"pixelsize", Matcher::Number}, {"spacing", Matcher::Number},
    {"foundry", Matcher::String}, {"antialias", Matcher::Bool}, {"file", Matcher::Filename},
    {"rasterizer", Matcher::String}, {"outline", Matcher::Bool}, {"scalable", Matcher::Bool},
    {"charset", Matcher::CharSet}, {"lang", Matcher::Lang}, {"fontversion", Matcher::Number},
    {"fontformat", Matcher::String}, {"decorative", Matcher::Bool}, {"postscriptname", Matcher::PostScript},
    {"color", Matcher::Bool}, {"symbol", Matcher::Bool}, {"variable", Matcher::Bool}, {"fonthashint", Matcher::Bool},
    {"order", Matcher::Number}, {"namedinstance", Matcher::Bool}, {"fontwrapper", Matcher::String},
};

constexpr bool KnownObjects() {
    for (const auto& entry : Matchers) {
        bool found = false;
        for (const auto& object : fontconfig_data::Objects) found = found || object.name == entry.object;
        if (!found) return false;
    }
    return true;
}

static_assert(KnownObjects());

Matcher MatcherFor(std::string_view object) {
    for (const auto& entry : Matchers)
        if (entry.object == object) return entry.matcher;
    return Matcher::None;
}

bool Number(const PatternValue& value, double& number) {
    if (value.type == FcTypeInteger) number = value.integer;
    else if (value.type == FcTypeDouble) number = value.real;
    else return false;
    return true;
}

PatternValue Real(double number) {
    PatternValue value;
    value.type = FcTypeDouble;
    value.real = number;
    return value;
}

bool SameTerritoryFamily(std::string_view first, std::string_view second) {
    const auto dash = first.find('-');
    return dash != std::string_view::npos && second.find('-') == dash && first.substr(0, dash) == second.substr(0, dash);
}

FcLangResult CompareLanguageSet(const FcLangSet& set, const std::set<std::string>& languages) {
    auto best = FcLangDifferentLang;
    for (auto entry = languages.begin(); best > FcLangEqual && entry != languages.end(); ++entry)
        best = std::min(best, FcLangSetHasLang(&set, reinterpret_cast<const FcChar8*>(entry->c_str())));
    return best;
}

FcLangResult CompareLanguageSets(const FcLangSet& first, const FcLangSet& second) {
    for (std::size_t i = 0; i < first.known.size(); ++i)
        if (first.known[i] && second.known[i]) return FcLangEqual;
    auto best = FcLangDifferentLang;
    for (std::size_t i = 0; i < first.known.size() && best == FcLangDifferentLang; ++i) {
        if (!first.known[i]) continue;
        for (std::size_t j = 0; j < second.known.size(); ++j) {
            if (second.known[j] && SameTerritoryFamily(fontconfig_data::Languages[i], fontconfig_data::Languages[j])) {
                best = FcLangDifferentTerritory;
                break;
            }
        }
    }
    if (!first.extra.empty()) best = std::min(best, CompareLanguageSet(second, first.extra));
    if (best > FcLangEqual && !second.extra.empty()) best = std::min(best, CompareLanguageSet(first, second.extra));
    return best;
}

double CompareLanguage(const PatternValue& first, const PatternValue& second) {
    FcLangResult result;
    const auto text = [](const PatternValue& value) { return reinterpret_cast<const FcChar8*>(value.text.c_str()); };
    if (first.type == FcTypeLangSet && second.type == FcTypeLangSet) result = CompareLanguageSets(*first.langset, *second.langset);
    else if (first.type == FcTypeLangSet && second.type == FcTypeString) result = FcLangSetHasLang(first.langset.get(), text(second));
    else if (first.type == FcTypeString && second.type == FcTypeLangSet) result = FcLangSetHasLang(second.langset.get(), text(first));
    else if (first.type == FcTypeString && second.type == FcTypeString) result = CompareLanguages(first.text, second.text);
    else return -1;
    return result == FcLangEqual ? 0 : result == FcLangDifferentTerritory ? 1 : 2;
}

bool DifferentInitials(std::string_view first, std::string_view second) {
    const unsigned char a = first.empty() ? 0 : first.front();
    const unsigned char b = second.empty() ? 0 : second.front();
    return Lower(a) != Lower(b) && a != ' ' && b != ' ';
}

double CompareRange(const PatternValue& first, const PatternValue& second, PatternValue& best, bool size) {
    double b1 = 0, b2 = 0;
    if (!Number(first, b1) || !Number(second, b2)) return -1;
    const double e1 = b1, e2 = b2;
    if (size) best = Real((b1 + e1) * .5);
    else best = Real(e1 < b2 ? b2 : e2 < b1 ? e2 : ((b1 > b2 ? b1 : b2) + (e1 < e2 ? e1 : e2)) * .5);
    if (e1 < b2 || e2 < b1) {
        const double low = std::fabs(b2 - e1), high = std::fabs(b1 - e2);
        return low < high ? low : high;
    }
    return 0.0;
}

double Compare(Matcher matcher, const PatternValue& first, const PatternValue& second, PatternValue& best) {
    switch (matcher) {
    case Matcher::Number: {
        double a = 0, b = 0;
        if (!Number(first, a) || !Number(second, b)) return -1;
        best = PatternValue(second);
        const double distance = b - a;
        return distance < 0 ? -distance : distance;
    }
    case Matcher::String:
        best = PatternValue(second);
        return CompareIgnoringCase(first.text, second.text) != 0;
    case Matcher::Family:
        best = PatternValue(second);
        if (DifferentInitials(first.text, second.text)) return 1.0;
        return CompareIgnoringBlanks(first.text, second.text) != 0;
    case Matcher::PostScript: {
        best = PatternValue(second);
        if (DifferentInitials(first.text, second.text)) return 1.0;
        const auto matched = MatchedPrefix(first.text, second.text, " -");
        const auto longest = std::max(first.text.size(), second.text.size());
        return static_cast<double>(longest - matched) / static_cast<double>(longest);
    }
    case Matcher::Lang:
        best = PatternValue(second);
        return CompareLanguage(first, second);
    case Matcher::Bool:
        if (first.type != FcTypeBool || second.type != FcTypeBool) return -1;
        best = PatternValue{};
        best.type = FcTypeBool;
        best.integer = second.integer != 2 ? second.integer : first.integer;
        return (second.integer ^ first.integer) == 1;
    case Matcher::CharSet:
        best = PatternValue(second);
        return FcCharSetSubtractCount(first.charset.get(), second.charset.get());
    case Matcher::Range: return CompareRange(first, second, best, false);
    case Matcher::Size: return CompareRange(first, second, best, true);
    case Matcher::Filename:
        best = PatternValue(second);
        if (first.text == second.text) return 0.0;
        if (CompareIgnoringCase(first.text, second.text) == 0) return 1.0;
        return GlobMatch(first.text, second.text) ? 2.0 : 3.0;
    case Matcher::None: break;
    }
    NotImplemented_nid_no_patch(__func__);
}

bool CompareLists(Matcher matcher, const std::list<PatternValue>& pattern, const std::list<PatternValue>& font,
    PatternValue* bestValue, std::size_t* position) {
    if (matcher == Matcher::None) {
        if (bestValue) *bestValue = PatternValue(font.front());
        if (position) *position = 0;
        return true;
    }
    const bool single = matcher != Matcher::Family && matcher != Matcher::PostScript;
    double best = 1e99;
    std::size_t found = 0;
    bool scored = false;
    std::size_t j = 0;
    for (const auto& first : pattern) {
        std::size_t k = 0;
        for (const auto& second : font) {
            PatternValue match;
            double score = Compare(matcher, first, second, match);
            if (score < 0) return false;
            score = score * 1000 + j * 100 + (second.type == FcTypeString ? k : 0);
            if (score < best) {
                if (bestValue) *bestValue = std::move(match);
                best = score;
                found = k;
                scored = true;
            }
            if (single && best < 1000) {
                if (position) *position = found;
                return true;
            }
            ++k;
        }
        ++j;
    }
    if (bestValue && !scored) *bestValue = PatternValue(font.front());
    if (position) *position = found;
    return true;
}

bool IsNameLanguage(std::string_view object) {
    return object == "familylang" || object == "stylelang" || object == "fullnamelang";
}

}

extern "C" {

FcLangSet* APS5_VABI FcLangSetCreate() {
    return new (std::nothrow) FcLangSet;
}

void APS5_VABI FcLangSetDestroy(FcLangSet* set) {
    delete set;
}

FcBool APS5_VABI FcLangSetAdd(FcLangSet* set, const FcChar8* language) {
    if (!set || !language) NotImplemented_nid_no_patch(__func__);
    const std::string_view name(reinterpret_cast<const char*>(language));
    const auto entry = std::lower_bound(std::begin(fontconfig_data::Languages), std::end(fontconfig_data::Languages), name,
        [](auto known, auto candidate) { return CompareLanguageIndex(known, candidate) < 0; });
    if (entry != std::end(fontconfig_data::Languages) && CompareLanguageIndex(*entry, name) == 0) {
        set->known[entry - std::begin(fontconfig_data::Languages)] = true;
        return 1;
    }
    try {
        const auto [entry, inserted] = set->extra.emplace(name);
        if (inserted) {
            try {
                set->extraOrder.emplace_back(name);
            } catch (...) {
                set->extra.erase(entry);
                throw;
            }
        }
        return 1;
    } catch (const std::bad_alloc&) {
        return 0;
    }
}

FcLangResult APS5_VABI FcLangSetHasLang(const FcLangSet* set, const FcChar8* language) {
    if (!set || !language) NotImplemented_nid_no_patch(__func__);
    const std::string_view name(reinterpret_cast<const char*>(language));
    const auto entry = std::lower_bound(std::begin(fontconfig_data::Languages), std::end(fontconfig_data::Languages), name,
        [](auto known, auto candidate) { return CompareLanguageIndex(known, candidate) < 0; });
    const auto index = static_cast<int>(entry - std::begin(fontconfig_data::Languages));
    if (entry != std::end(fontconfig_data::Languages) && CompareLanguageIndex(*entry, name) == 0 && set->known[index])
        return FcLangEqual;
    auto best = FcLangDifferentLang;
    for (int i = index - 1; i >= 0; --i) {
        const auto result = CompareLanguages(name, fontconfig_data::Languages[i]);
        if (result == FcLangDifferentLang) break;
        if (set->known[i]) best = std::min(best, result);
    }
    for (auto i = static_cast<std::size_t>(index); i < std::size(fontconfig_data::Languages); ++i) {
        const auto result = CompareLanguages(name, fontconfig_data::Languages[i]);
        if (result == FcLangDifferentLang) break;
        if (set->known[i]) best = std::min(best, result);
    }
    for (const auto& extra : set->extra) best = std::min(best, CompareLanguages(name, extra));
    return best;
}

FcPattern* APS5_VABI FcPatternCreate() {
    return new (std::nothrow) FcPattern;
}

void APS5_VABI FcPatternDestroy_nid_postfix(FcPattern* pattern) {
    if (pattern && pattern->references.fetch_sub(1, std::memory_order_acq_rel) == 1) delete pattern;
}

void APS5_VABI FcPatternReference(FcPattern* pattern) {
    if (!pattern) NotImplemented_nid_no_patch(__func__);
    pattern->references.fetch_add(1, std::memory_order_relaxed);
}

FcPattern* APS5_VABI FcPatternDuplicate_nid_postfix(const FcPattern* pattern) {
    if (!pattern) return nullptr;
    try {
        return new FcPattern(*pattern);
    } catch (const std::bad_alloc&) {
        return nullptr;
    }
}

FcPattern* APS5_VABI FcPatternFilter(FcPattern* pattern, const FcObjectSet* objects) {
    if (!objects) return FcPatternDuplicate_nid_postfix(pattern);
    if (!pattern) NotImplemented_nid_no_patch(__func__);
    try {
        auto copy = std::make_unique<FcPattern>();
        for (int i = 0; i < objects->nobject; ++i) {
            const auto entry = pattern->objects.find(objects->objects[i]);
            if (entry == pattern->objects.end()) continue;
            auto& values = copy->objects[entry->first];
            for (const auto& value : entry->second) values.push_back(value);
        }
        return copy.release();
    } catch (const std::bad_alloc&) {
        return nullptr;
    }
}

FcBool APS5_VABI FcPatternEqual(const FcPattern* first, const FcPattern* second) {
    if (first == second) return 1;
    if (!first || !second || first->objects.empty() || first->objects.size() != second->objects.size()) return 0;
    auto right = second->objects.begin();
    for (const auto& [object, values] : first->objects) {
        if (object != right->first || values.size() != right->second.size()) return 0;
        if (!std::equal(values.begin(), values.end(), right->second.begin(), ValuesEqual)) return 0;
        ++right;
    }
    return 1;
}

FcBool APS5_VABI FcPatternDel_nid_postfix(FcPattern* pattern, const char* object) {
    if (!pattern || !object) NotImplemented_nid_no_patch(__func__);
    const auto entry = pattern->objects.find(object);
    if (entry == pattern->objects.end()) return 0;
    pattern->objects.erase(entry);
    return 1;
}

FcBool APS5_VABI FcPatternRemove(FcPattern* pattern, const char* object, int id) {
    if (!pattern || !object) NotImplemented_nid_no_patch(__func__);
    const auto entry = pattern->objects.find(object);
    if (entry == pattern->objects.end() || id < 0 || static_cast<std::size_t>(id) >= entry->second.size()) return 0;
    entry->second.erase(std::next(entry->second.begin(), id));
    if (entry->second.empty()) pattern->objects.erase(entry);
    return 1;
}

FcResult APS5_VABI FcPatternGet_nid_postfix(const FcPattern* pattern, const char* object, int id, FcValue* value) {
    if (!object) NotImplemented_nid_no_patch(__func__);
    if (!pattern) return FcResultNoMatch;
    const auto entry = pattern->objects.find(object);
    if (entry == pattern->objects.end()) return FcResultNoMatch;
    if (id < 0 || static_cast<std::size_t>(id) >= entry->second.size()) return FcResultNoId;
    if (!value) NotImplemented_nid_no_patch(__func__);
    WriteGuestValue(value, std::next(entry->second.begin(), id)->View(), __func__);
    return FcResultMatch;
}

FcResult APS5_VABI FcPatternGetInteger_nid_postfix(const FcPattern* pattern, const char* object, int id, int* value) {
    return GetPatternTyped(pattern, object, id, value, FcTypeInteger, [](const FcValue& found) { return found.u.i; }, __func__);
}

FcResult APS5_VABI FcPatternGetBool_nid_postfix(const FcPattern* pattern, const char* object, int id, FcBool* value) {
    return GetPatternTyped(pattern, object, id, value, FcTypeBool, [](const FcValue& found) { return found.u.b; }, __func__);
}

FcResult APS5_VABI FcPatternGetString_nid_postfix(const FcPattern* pattern, const char* object, int id, FcChar8** value) {
    return GetPatternTyped(pattern, object, id, value, FcTypeString,
                           [](const FcValue& found) { return const_cast<FcChar8*>(found.u.s); }, __func__);
}

FcResult APS5_VABI FcPatternGetFTFace_nid_postfix(const FcPattern* pattern, const char* object, int id, void** face) {
    return GetPatternTyped(pattern, object, id, face, FcTypeFTFace, [](const FcValue& found) { return found.u.f; }, __func__);
}

FcResult APS5_VABI FcPatternGetMatrix(const FcPattern* pattern, const char* object, int id, FcMatrix** value) {
    return GetPatternTyped(pattern, object, id, value, FcTypeMatrix,
                           [](const FcValue& found) { return const_cast<FcMatrix*>(found.u.m); }, __func__);
}

FcResult APS5_VABI FcPatternGetCharSet(const FcPattern* pattern, const char* object, int id, FcCharSet** value) {
    return GetPatternTyped(pattern, object, id, value, FcTypeCharSet,
                           [](const FcValue& found) { return const_cast<FcCharSet*>(found.u.c); }, __func__);
}

FcResult APS5_VABI FcPatternGetLangSet(const FcPattern* pattern, const char* object, int id, FcLangSet** value) {
    return GetPatternTyped(pattern, object, id, value, FcTypeLangSet,
                           [](const FcValue& found) { return const_cast<FcLangSet*>(found.u.l); }, __func__);
}

FcBool APS5_VABI FcPatternAddInteger_nid_postfix(FcPattern* pattern, const char* object, int value) {
    try {
        PatternValue entry;
        entry.type = FcTypeInteger;
        entry.integer = value;
        return AddValue(pattern, object, std::move(entry));
    } catch (const std::bad_alloc&) { return 0; }
}

FcBool APS5_VABI FcPatternAddBool_nid_postfix(FcPattern* pattern, const char* object, FcBool value) {
    try {
        PatternValue entry;
        entry.type = FcTypeBool;
        entry.integer = value;
        return AddValue(pattern, object, std::move(entry));
    } catch (const std::bad_alloc&) { return 0; }
}

FcBool APS5_VABI FcPatternAddDouble(FcPattern* pattern, const char* object, double value) {
    try {
        PatternValue entry;
        entry.type = FcTypeDouble;
        entry.real = value;
        return AddValue(pattern, object, std::move(entry));
    } catch (const std::bad_alloc&) { return 0; }
}

FcBool APS5_VABI FcPatternAddString(FcPattern* pattern, const char* object, const FcChar8* value) {
    if (!pattern || !object) NotImplemented_nid_no_patch(__func__);
    if (!value) return 0;
    try {
        PatternValue entry;
        entry.type = FcTypeString;
        entry.text = reinterpret_cast<const char*>(value);
        return AddValue(pattern, object, std::move(entry));
    } catch (const std::bad_alloc&) { return 0; }
}

FcBool APS5_VABI FcPatternAddCharSet(FcPattern* pattern, const char* object, const FcCharSet* value) {
    if (!pattern || !object) NotImplemented_nid_no_patch(__func__);
    if (!value) return 0;
    try {
        PatternValue entry;
        entry.type = FcTypeCharSet;
        entry.charset = RetainCharSet(value);
        return AddValue(pattern, object, std::move(entry));
    } catch (const std::bad_alloc&) { return 0; }
}

FcBool APS5_VABI FcPatternAddLangSet(FcPattern* pattern, const char* object, const FcLangSet* value) {
    if (!pattern || !object) NotImplemented_nid_no_patch(__func__);
    if (!value) return 0;
    try {
        PatternValue entry;
        entry.type = FcTypeLangSet;
        entry.langset = std::make_unique<FcLangSet>(*value);
        return AddValue(pattern, object, std::move(entry));
    } catch (const std::bad_alloc&) { return 0; }
}

FcBool APS5_VABI FcPatternAddMatrix(FcPattern* pattern, const char* object, const FcMatrix* value) {
    if (!pattern || !object) NotImplemented_nid_no_patch(__func__);
    if (!value) return 0;
    try {
        PatternValue entry;
        entry.type = FcTypeMatrix;
        ReadGuestValue(entry.matrix, value, __func__);
        return AddValue(pattern, object, std::move(entry));
    } catch (const std::bad_alloc&) { return 0; }
}


FcPattern* APS5_VABI FcFontRenderPrepare(FcConfig* config, FcPattern* pattern, FcPattern* font) {
    if (!pattern || !font) NotImplemented_nid_no_patch(__func__);
    try {
        auto prepared = std::make_unique<FcPattern>();
        for (const auto& [object, values] : font->objects) {
            if (IsNameLanguage(object)) continue;
            if (object == "family" || object == "style" || object == "fullname") {
                const auto language = object + "lang";
                const auto fontLanguages = font->objects.find(language);
                const auto patternLanguages = pattern->objects.find(language);
                if (fontLanguages != font->objects.end() && patternLanguages != pattern->objects.end()) {
                    std::size_t chosen = 1;
                    if (!CompareLists(Matcher::Lang, patternLanguages->second, fontLanguages->second, nullptr, &chosen))
                        return nullptr;
                    std::list<PatternValue> names, languages;
                    auto name = values.begin();
                    auto tag = fontLanguages->second.begin();
                    for (std::size_t j = 0; name != values.end() || tag != fontLanguages->second.end(); ++j) {
                        if (name != values.end()) {
                            if (j == chosen) names.push_front(*name);
                            else names.push_back(*name);
                            ++name;
                        }
                        if (tag != fontLanguages->second.end()) {
                            if (j == chosen) languages.push_front(*tag);
                            else languages.push_back(*tag);
                            ++tag;
                        }
                    }
                    prepared->objects.emplace(object, std::move(names));
                    prepared->objects.emplace(language, std::move(languages));
                    continue;
                }
                if (fontLanguages != font->objects.end()) {
                    prepared->objects.emplace(object, values);
                    prepared->objects.emplace(language, fontLanguages->second);
                    continue;
                }
            }
            const auto requested = pattern->objects.find(object);
            if (requested == pattern->objects.end()) {
                prepared->objects.emplace(object, values);
                continue;
            }
            PatternValue best;
            if (!CompareLists(MatcherFor(object), requested->second, values, &best, nullptr)) return nullptr;
            std::list<PatternValue> chosen;
            chosen.push_back(std::move(best));
            prepared->objects.emplace(object, std::move(chosen));
        }
        for (const auto& [object, values] : pattern->objects)
            if (!font->objects.contains(object) && !IsNameLanguage(object)) prepared->objects.emplace(object, values);
        if (FcConfigReference(config)) NotImplemented_nid_no_patch(__func__);
        return prepared.release();
    } catch (const std::bad_alloc&) {
        return nullptr;
    }
}

}

std::vector<std::string> FontconfigLanguages_nid_no_patch(const FcLangSet* set, bool enumerate) {
    std::vector<std::string> result;
    const auto* order = enumerate ? fontconfig_data::Languages : fontconfig_data::FormatLanguages;
    for (std::size_t i = 0; i < std::size(fontconfig_data::Languages); ++i) {
        const auto language = order[i];
        const auto found = std::lower_bound(std::begin(fontconfig_data::Languages), std::end(fontconfig_data::Languages), language);
        if (found != std::end(fontconfig_data::Languages) && *found == language &&
            set->known[found - std::begin(fontconfig_data::Languages)]) result.emplace_back(language);
    }
    for (const auto& extra : set->extraOrder) result.push_back(extra);
    return result;
}

std::string FontconfigDowncase_nid_no_patch(std::string_view text) {
    CaseWalker walker(text);
    std::string result;
    while (const auto c = walker.Next()) result.push_back(static_cast<char>(c));
    return result;
}
