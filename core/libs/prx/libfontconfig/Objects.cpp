#include "prx/libfontconfig/include/InventoryExports.hpp"
#include "prx/libc/include/General.hpp"
#include "FontconfigData.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <iterator>
#include <limits>
#include <list>
#include <map>
#include <memory>
#include <new>
#include <set>
#include <string>
#include <string_view>

namespace {

unsigned char Lower(unsigned char value) {
    return value >= 'A' && value <= 'Z' ? value + ('a' - 'A') : value;
}

class CaseWalker {
public:
    explicit CaseWalker(std::string_view value) : source(value) {}

    unsigned char Next() {
        if (read < end) return buffer[read++];
        if (source.empty()) return 0;
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

struct PatternValue {
    FcType type = FcTypeVoid;
    int integer = 0;
    double real = 0;
    std::string text;
    FcMatrix matrix{};

    FcValue View() const {
        FcValue value{};
        value.type = type;
        switch (type) {
        case FcTypeInteger: value.u.i = integer; break;
        case FcTypeBool: value.u.b = integer; break;
        case FcTypeDouble: value.u.d = real; break;
        case FcTypeString: value.u.s = reinterpret_cast<const FcChar8*>(text.c_str()); break;
        case FcTypeMatrix: value.u.m = &matrix; break;
        default: NotImplemented_nid_no_patch(__func__);
        }
        return value;
    }
};

bool ValidType(std::string_view object, FcType type) {
    for (const auto& entry : fontconfig_data::Objects) {
        if (entry.name != object) continue;
        if (entry.type == FcTypeInteger || entry.type == FcTypeDouble || entry.type == FcTypeRange)
            return type == FcTypeInteger || type == FcTypeDouble;
        if (entry.type == FcTypeLangSet) return type == FcTypeString;
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
    case FcTypeMatrix: return first.matrix.xx == second.matrix.xx && first.matrix.xy == second.matrix.xy
        && first.matrix.yx == second.matrix.yx && first.matrix.yy == second.matrix.yy;
    default: NotImplemented_nid_no_patch(__func__);
    }
}

}

struct FcLangSet {
    std::array<bool, std::size(fontconfig_data::Languages)> known{};
    std::set<std::string> extra;
};

struct FcPattern {
    std::atomic<unsigned> references{1};
    std::map<std::string, std::list<PatternValue>, std::less<>> objects;
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
        set->extra.emplace(name);
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

void APS5_VABI FcPatternDestroy(FcPattern* pattern) {
    if (pattern && pattern->references.fetch_sub(1, std::memory_order_acq_rel) == 1) delete pattern;
}

void APS5_VABI FcPatternReference(FcPattern* pattern) {
    if (!pattern) NotImplemented_nid_no_patch(__func__);
    pattern->references.fetch_add(1, std::memory_order_relaxed);
}

FcPattern* APS5_VABI FcPatternDuplicate(const FcPattern* pattern) {
    if (!pattern) return nullptr;
    try {
        auto copy = std::make_unique<FcPattern>();
        copy->objects = pattern->objects;
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

FcBool APS5_VABI FcPatternDel(FcPattern* pattern, const char* object) {
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

FcResult APS5_VABI FcPatternGet(const FcPattern* pattern, const char* object, int id, FcValue* value) {
    if (!object) NotImplemented_nid_no_patch(__func__);
    if (!pattern) return FcResultNoMatch;
    const auto entry = pattern->objects.find(object);
    if (entry == pattern->objects.end()) return FcResultNoMatch;
    if (id < 0 || static_cast<std::size_t>(id) >= entry->second.size()) return FcResultNoId;
    if (!value) NotImplemented_nid_no_patch(__func__);
    *value = std::next(entry->second.begin(), id)->View();
    return FcResultMatch;
}

FcResult APS5_VABI FcPatternGetInteger(const FcPattern* pattern, const char* object, int id, int* value) {
    FcValue found{};
    const auto result = FcPatternGet(pattern, object, id, &found);
    if (result != FcResultMatch) return result;
    if (found.type != FcTypeInteger && found.type != FcTypeDouble) return FcResultTypeMismatch;
    if (!value) NotImplemented_nid_no_patch(__func__);
    if (found.type == FcTypeDouble) {
        const double truncated = std::trunc(found.u.d);
        if (!std::isfinite(truncated) || truncated < std::numeric_limits<int>::min() || truncated > std::numeric_limits<int>::max())
            NotImplemented_nid_no_patch(__func__);
        *value = static_cast<int>(truncated);
    } else {
        *value = found.u.i;
    }
    return FcResultMatch;
}

FcResult APS5_VABI FcPatternGetBool(const FcPattern* pattern, const char* object, int id, FcBool* value) {
    FcValue found{};
    const auto result = FcPatternGet(pattern, object, id, &found);
    if (result != FcResultMatch) return result;
    if (found.type != FcTypeBool) return FcResultTypeMismatch;
    if (!value) NotImplemented_nid_no_patch(__func__);
    *value = found.u.b;
    return FcResultMatch;
}

FcResult APS5_VABI FcPatternGetString(const FcPattern* pattern, const char* object, int id, FcChar8** value) {
    FcValue found{};
    const auto result = FcPatternGet(pattern, object, id, &found);
    if (result != FcResultMatch) return result;
    if (found.type != FcTypeString) return FcResultTypeMismatch;
    if (!value) NotImplemented_nid_no_patch(__func__);
    *value = const_cast<FcChar8*>(found.u.s);
    return FcResultMatch;
}

FcResult APS5_VABI FcPatternGetMatrix(const FcPattern* pattern, const char* object, int id, FcMatrix** value) {
    FcValue found{};
    const auto result = FcPatternGet(pattern, object, id, &found);
    if (result != FcResultMatch) return result;
    if (found.type != FcTypeMatrix) return FcResultTypeMismatch;
    if (!value) NotImplemented_nid_no_patch(__func__);
    *value = const_cast<FcMatrix*>(found.u.m);
    return FcResultMatch;
}

FcBool APS5_VABI FcPatternAddInteger(FcPattern* pattern, const char* object, int value) {
    try {
        PatternValue entry;
        entry.type = FcTypeInteger;
        entry.integer = value;
        return AddValue(pattern, object, std::move(entry));
    } catch (const std::bad_alloc&) { return 0; }
}

FcBool APS5_VABI FcPatternAddBool(FcPattern* pattern, const char* object, FcBool value) {
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

FcBool APS5_VABI FcPatternAddMatrix(FcPattern* pattern, const char* object, const FcMatrix* value) {
    if (!pattern || !object) NotImplemented_nid_no_patch(__func__);
    if (!value) return 0;
    try {
        PatternValue entry;
        entry.type = FcTypeMatrix;
        entry.matrix = *value;
        return AddValue(pattern, object, std::move(entry));
    } catch (const std::bad_alloc&) { return 0; }
}

}
