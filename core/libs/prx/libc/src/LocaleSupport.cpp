#include "prx/libc/include/Api.hpp"
#include <climits>
#include <cmath>
#include <cstdlib>
#include <clocale>
#include <cstddef>
#include <cstdio>
#include <cstdint>
#include <cwchar>
#include <cerrno>
#include <ios>
#include <locale>
#include <mutex>
#include <new>
#include <atomic>
#include <cstring>
#include <vector>
#include <algorithm>
#include <array>
#include <limits>
#include <stdexcept>
#include <string>

#include "prx/libc/include/General.hpp"
#include "prx/libc/include/SonyCrt.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/GuestLocale.hpp"
#include "SceTypes.hpp"

extern "C" {
extern std::uint64_t _ZNSt5ctypeIcE2idE_nid_postfix;
extern std::uint64_t _ZNSt8numpunctIcE2idE_nid_postfix;
extern std::int32_t _ZNSt6locale2id7_Id_cntE_nid_postfix;
}

namespace {

std::mutex g_localeInitMutex;
bool g_localeInitialized = false;
std::vector<GuestLocale::Facet*> g_registeredFacets;

constexpr std::int64_t StandardStreamSlots = 8;
std::mutex g_standardStreamMutex;
GuestLocale::IosBase* g_standardStreams[StandardStreamSlots + 1] = {};
int g_standardStreamOpens[StandardStreamSlots + 1] = {};

void APS5_VABI DestroyClassicLocale(GuestLocale::Facet*) {
    throw std::runtime_error("Cannot destroy the classic locale");
}

void APS5_VABI RetainLocale(GuestLocale::Facet* self) {
    if (self == nullptr) throw std::invalid_argument("locale retain: null facet");
    std::atomic_ref<std::uint32_t> references(self->references);
    auto count = references.load();
    do {
        if (count == 0 || count == UINT32_MAX) throw std::runtime_error("locale retain: invalid reference count");
    } while (!references.compare_exchange_weak(count, count + 1));
}

GuestLocale::Facet* APS5_VABI ReleaseLocale(GuestLocale::Facet* self) {
    if (self == nullptr) throw std::invalid_argument("locale release: null facet");
    std::atomic_ref<std::uint32_t> references(self->references);
    auto count = references.load();
    // The classic locale is immortal: a release that would drop it to zero keeps its last reference.
    do {
        if (count <= 1) return nullptr;
    } while (!references.compare_exchange_weak(count, count - 1));
    return nullptr;
}

// Copies of the classic locale (see _Locimp::_Locimp(const _Locimp&)) count down to zero but are never
// freed either, since their facets are shared with the classic locale.
void APS5_VABI DestroyCopiedLocale(GuestLocale::Facet*) {}

void APS5_VABI RetainCopiedLocale(GuestLocale::Facet* self) {
    if (self == nullptr) throw std::invalid_argument("locale retain: null facet");
    std::atomic_ref<std::uint32_t>(self->references).fetch_add(1);
}

GuestLocale::Facet* APS5_VABI ReleaseCopiedLocale(GuestLocale::Facet* self) {
    if (self == nullptr) throw std::invalid_argument("locale release: null facet");
    std::atomic_ref<std::uint32_t> references(self->references);
    auto count = references.load();
    while (count != 0 && !references.compare_exchange_weak(count, count - 1)) {}
    return nullptr;
}

const GuestLocale::FacetVtable g_localeVtable{DestroyClassicLocale, DestroyClassicLocale, RetainLocale, ReleaseLocale};
const GuestLocale::FacetVtable g_copiedLocaleVtable{DestroyCopiedLocale, DestroyCopiedLocale, RetainCopiedLocale, ReleaseCopiedLocale};
GuestLocale::Facet* g_classicFacets[1]{};
GuestLocale::Implementation g_classicLocale{{&g_localeVtable, 1, 0}, g_classicFacets, 1, 0, false, "C"};

constexpr std::size_t GuestStringInlineCapacity = 15;
constexpr std::size_t GuestStringCapacityMask = 15;
constexpr std::size_t GuestStringLargeAllocation = 0x1000;
constexpr std::size_t GuestLargeAllocationPadding = 0x27;
constexpr std::uintptr_t GuestLargeAllocationAlignment = 0x20;

void ValidateCharacterRange(const char* first, const char* last, const char* function) {
    if ((first == nullptr) != (last == nullptr) || last < first) throw std::invalid_argument(std::string(function) + ": invalid character range");
}

void APS5_VABI DestroyCollate(GuestLocale::Facet*) {}

void APS5_VABI DeleteCollate(GuestLocale::Facet* self) {
    if (self == nullptr) throw std::invalid_argument("collate delete: null facet");
    _ZdlPv_nid_postfix(self);

}

void APS5_VABI RetainFacet(GuestLocale::Facet* self) {
    if (self == nullptr) throw std::invalid_argument("facet retain: null facet");
    std::atomic_ref<std::uint32_t>(self->references).fetch_add(1);
}

GuestLocale::Facet* APS5_VABI ReleaseFacet(GuestLocale::Facet* self) {
    if (self == nullptr) throw std::invalid_argument("facet release: null facet");
    return std::atomic_ref<std::uint32_t>(self->references).fetch_sub(1) == 1 ? self : nullptr;
}

int APS5_VABI CollateCompare(const GuestLocale::CollateFacet*, const char* first1, const char* last1, const char* first2, const char* last2) {
    ValidateCharacterRange(first1, last1, "collate::do_compare");
    ValidateCharacterRange(first2, last2, "collate::do_compare");
    const auto length1 = static_cast<std::size_t>(last1 - first1);
    const auto length2 = static_cast<std::size_t>(last2 - first2);
    const auto common = std::min(length1, length2);
    const int order = common == 0 ? 0 : std::memcmp(first1, first2, common);
    if (order != 0) return order < 0 ? -1 : 1;
    if (length1 == length2) return 0;
    return length1 < length2 ? -1 : 1;
}

GuestLocale::String* APS5_VABI CollateTransform(GuestLocale::String* result, const GuestLocale::CollateFacet*, const char* first, const char* last) {
    if (result == nullptr) throw std::invalid_argument("collate::do_transform: null result");
    ValidateCharacterRange(first, last, "collate::do_transform");
    const auto length = static_cast<std::size_t>(last - first);
    if (length != 0 && std::memchr(first, 0, length) != nullptr) throw std::runtime_error("collate::do_transform: embedded null characters are not supported");
    char* data = result->buffer;
    std::size_t capacity = GuestStringInlineCapacity;
    if (length > GuestStringInlineCapacity) {
        capacity = length | GuestStringCapacityMask;
        data = static_cast<char*>(GuestLocale::AllocateStringStorage(capacity + 1));
    }
    if (length != 0) std::memcpy(data, first, length);
    data[length] = 0;
    if (data != result->buffer) result->pointer = data;
    result->size = length;
    result->capacity = capacity;
    return result;
}

std::int64_t APS5_VABI CollateHash(const GuestLocale::CollateFacet*, const char* first, const char* last) {
    ValidateCharacterRange(first, last, "collate::do_hash");
    std::uint64_t hash = 0xcbf29ce484222325;
    for (; first != last; ++first) hash = (hash ^ static_cast<unsigned char>(*first)) * 0x100000001b3;
    return static_cast<std::int64_t>(hash);
}

const GuestLocale::CollateVtable g_collateVtable{{DestroyCollate, DeleteCollate, RetainFacet, ReleaseFacet}, CollateCompare, CollateTransform, CollateHash};

constexpr std::size_t GuestWideCharacterSize = 2;
constexpr std::size_t GuestWideInlineCapacity = sizeof(GuestLocale::String::buffer) / GuestWideCharacterSize - 1;

void ValidateWideCharacterRange(const std::uint16_t* first, const std::uint16_t* last, const char* function) {
    if ((first == nullptr) != (last == nullptr) || last < first) throw std::invalid_argument(std::string(function) + ": invalid character range");
}

int APS5_VABI WideCollateCompare(const GuestLocale::CollateFacet*, const char* first1, const char* last1, const char* first2, const char* last2) {
    const auto* characters1 = reinterpret_cast<const std::uint16_t*>(first1);
    const auto* end1 = reinterpret_cast<const std::uint16_t*>(last1);
    const auto* characters2 = reinterpret_cast<const std::uint16_t*>(first2);
    const auto* end2 = reinterpret_cast<const std::uint16_t*>(last2);
    ValidateWideCharacterRange(characters1, end1, "collate::do_compare");
    ValidateWideCharacterRange(characters2, end2, "collate::do_compare");
    const auto length1 = static_cast<std::size_t>(end1 - characters1);
    const auto length2 = static_cast<std::size_t>(end2 - characters2);
    const auto common = std::min(length1, length2);
    for (std::size_t index = 0; index < common; ++index) {
        if (characters1[index] != characters2[index]) return characters1[index] < characters2[index] ? -1 : 1;
    }
    if (length1 == length2) return 0;
    return length1 < length2 ? -1 : 1;
}

GuestLocale::String* APS5_VABI WideCollateTransform(GuestLocale::String* result, const GuestLocale::CollateFacet*, const char* first, const char* last) {
    if (result == nullptr) throw std::invalid_argument("collate::do_transform: null result");
    const auto* characters = reinterpret_cast<const std::uint16_t*>(first);
    const auto* end = reinterpret_cast<const std::uint16_t*>(last);
    ValidateWideCharacterRange(characters, end, "collate::do_transform");
    const auto length = static_cast<std::size_t>(end - characters);
    if (length != 0 && std::find(characters, end, static_cast<std::uint16_t>(0)) != end) throw std::runtime_error("collate::do_transform: embedded null characters are not supported");
    const auto bytes = length * GuestWideCharacterSize;
    char* data = result->buffer;
    std::size_t capacity = GuestWideInlineCapacity;
    if (length > GuestWideInlineCapacity) {
        capacity = length | GuestWideInlineCapacity;
        data = static_cast<char*>(GuestLocale::AllocateStringStorage((capacity + 1) * GuestWideCharacterSize));
    }
    if (length != 0) std::memcpy(data, characters, bytes);
    data[bytes] = 0;
    data[bytes + 1] = 0;
    if (data != result->buffer) result->pointer = data;
    result->size = length;
    result->capacity = capacity;
    return result;
}

std::int64_t APS5_VABI WideCollateHash(const GuestLocale::CollateFacet*, const char* first, const char* last) {
    const auto* characters = reinterpret_cast<const std::uint16_t*>(first);
    const auto* end = reinterpret_cast<const std::uint16_t*>(last);
    ValidateWideCharacterRange(characters, end, "collate::do_hash");
    std::uint64_t hash = 0xcbf29ce484222325;
    for (; characters != end; ++characters) hash = (hash ^ static_cast<std::uint64_t>(*characters)) * 0x100000001b3;
    return static_cast<std::int64_t>(hash);
}

const GuestLocale::CollateVtable g_wideCollateVtable{{DestroyCollate, DeleteCollate, RetainFacet, ReleaseFacet}, WideCollateCompare, WideCollateTransform, WideCollateHash};

bool APS5_VABI CodecvtAlwaysNoconv(const GuestLocale::Facet*) {
    return true;
}

int APS5_VABI CodecvtConstantOne(const GuestLocale::Facet*) {
    return 1;
}

int APS5_VABI CodecvtNoConversion(GuestLocale::Facet*, std::uint64_t*, const char* first1, const char*, const char** next1, char* first3, char*, char** next3) {
    *next1 = first1;
    *next3 = first3;
    return 3;
}

int APS5_VABI CodecvtUnshift(GuestLocale::Facet*, std::uint64_t*, char* first3, char*, char** next3) {
    *next3 = first3;
    return 0;
}

std::size_t APS5_VABI CodecvtLength(GuestLocale::Facet*, std::uint64_t*, const char* first1, const char* last1, std::size_t count) {
    return std::min(static_cast<std::size_t>(last1 - first1), count);
}

const GuestLocale::CodecvtVtable g_codecvtVtable{{DestroyCollate, DeleteCollate, RetainFacet, ReleaseFacet}, CodecvtAlwaysNoconv, CodecvtConstantOne, CodecvtConstantOne, CodecvtNoConversion, CodecvtNoConversion, CodecvtUnshift, CodecvtLength};

GuestLocale::String* APS5_VABI AssignCString(GuestLocale::String* result, const char* source) {
    if (result == nullptr || source == nullptr) throw std::invalid_argument("numpunct string: null argument");
    const std::size_t length = std::strlen(source);
    char* data = result->buffer;
    std::size_t capacity = GuestStringInlineCapacity;
    if (length > GuestStringInlineCapacity) {
        capacity = length | GuestStringCapacityMask;
        data = static_cast<char*>(GuestLocale::AllocateStringStorage(capacity + 1));
    }
    if (length != 0) std::memcpy(data, source, length);
    data[length] = 0;
    if (data != result->buffer) result->pointer = data;
    result->size = length;
    result->capacity = capacity;
    return result;
}

char APS5_VABI NumpunctDecimalPoint(const GuestLocale::NumpunctFacet* self) {
    return self->decimalPoint;
}

char APS5_VABI NumpunctThousandsSep(const GuestLocale::NumpunctFacet* self) {
    return self->thousandsSep;
}

GuestLocale::String* APS5_VABI NumpunctGrouping(GuestLocale::String* result, const GuestLocale::NumpunctFacet* self) {
    return AssignCString(result, self->grouping);
}

GuestLocale::String* APS5_VABI NumpunctFalsename(GuestLocale::String* result, const GuestLocale::NumpunctFacet* self) {
    return AssignCString(result, self->falsename);
}

GuestLocale::String* APS5_VABI NumpunctTruename(GuestLocale::String* result, const GuestLocale::NumpunctFacet* self) {
    return AssignCString(result, self->truename);
}

const GuestLocale::NumpunctVtable g_numpunctVtable{{DestroyCollate, DeleteCollate, RetainFacet, ReleaseFacet}, NumpunctDecimalPoint, NumpunctThousandsSep, NumpunctGrouping, NumpunctFalsename, NumpunctTruename};

void PutCharacter(GuestLocale::OutputIterator& out, char character) {
    GuestLocale::Streambuf* buffer = out.buffer;
    if (buffer == nullptr) {
        out.failed = true;
        return;
    }
    char* next = *buffer->putNext;
    if (next != nullptr && static_cast<std::int32_t>(static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(*buffer->putEnd) - reinterpret_cast<std::uintptr_t>(next))) > 0) {
        *buffer->putNext = next + 1;
        *next = character;
        return;
    }
    if (buffer->vtable->overflow(buffer, static_cast<unsigned char>(character)) == -1) out.failed = true;
}

GuestLocale::OutputIterator APS5_VABI TimePutDoPut(const GuestLocale::Facet*, GuestLocale::OutputIterator out, GuestLocale::IosBase*, char, const GuestTm* time, char format, char modifier) {
    const char pattern[] = {'!', '%', modifier != 0 ? modifier : format, modifier != 0 ? format : '\0', '\0'};
    std::string text;
    std::size_t length = 0;
    for (std::size_t size = 16; length == 0; size *= 2) {
        text.resize(size);
        length = strftime_nid_postfix(text.data(), size, pattern, time);
    }
    for (std::size_t index = 1; index < length; ++index) PutCharacter(out, text[index]);
    return out;
}

constexpr std::int32_t IosLeft = 0x2;
constexpr std::int32_t IosInternal = 0x8;
constexpr std::int32_t IosAdjustField = 0xe;
constexpr std::int32_t IosOct = 0x20;
constexpr std::int32_t IosHex = 0x40;
constexpr std::int32_t IosBaseField = 0x70;
constexpr std::int32_t IosShowBase = 0x80;
constexpr std::int32_t IosShowPoint = 0x100;
constexpr std::int32_t IosUppercase = 0x200;
constexpr std::int32_t IosShowPos = 0x400;
constexpr std::int32_t IosScientific = 0x800;
constexpr std::int32_t IosFixed = 0x1000;
constexpr std::int32_t IosHexFloat = 0x1800;
constexpr std::int32_t IosBoolAlpha = 0x8000;

const GuestLocale::Facet* FindFacet(const GuestLocale::IosBase* stream, std::uint64_t& id) {
    std::atomic_ref<std::uint64_t> slot(id);
    if (slot.load() == 0) {
        _Locksyslock_nid_postfix();
        if (slot.load() == 0) slot.store(static_cast<std::uint64_t>(++_ZNSt6locale2id7_Id_cntE_nid_postfix));
        _Unlocksyslock_nid_postfix();
    }
    const std::uint64_t index = slot.load();
    const GuestLocale::Implementation* locale = *stream->locale;
    const GuestLocale::Facet* facet = index < locale->facetCount ? locale->facets[index] : nullptr;
    if (facet != nullptr || !locale->transparent) return facet;
    const GuestLocale::Implementation* global = _ZNSt6locale16_GetgloballocaleEv_nid_postfix();
    return index < global->facetCount ? global->facets[index] : nullptr;
}

const GuestLocale::Facet* StreamNumpunct(const GuestLocale::IosBase* stream) {
    if (const GuestLocale::Facet* facet = FindFacet(stream, _ZNSt8numpunctIcE2idE_nid_postfix)) return facet;
    static GuestLocale::Facet* const fallback = [] {
        GuestLocale::Facet* created = nullptr;
        _ZNSt8numpunctIcE7_GetcatEPPKNSt6locale5facetEPKS1__nid_postfix(&created, nullptr);
        return created;
    }();
    return fallback;
}

const GuestLocale::NumpunctVtable& Punctuation(const GuestLocale::Facet* facet) {
    return *reinterpret_cast<const GuestLocale::NumpunctVtable*>(facet->vtable);
}

const GuestLocale::NumpunctFacet* AsNumpunct(const GuestLocale::Facet* facet) {
    return reinterpret_cast<const GuestLocale::NumpunctFacet*>(facet);
}

std::string TakeString(GuestLocale::String* (APS5_VABI* method)(GuestLocale::String*, const GuestLocale::NumpunctFacet*), const GuestLocale::Facet* facet) {
    GuestLocale::String value{};
    method(&value, AsNumpunct(facet));
    const bool onHeap = value.capacity > GuestStringInlineCapacity;
    std::string text(onHeap ? value.pointer : value.buffer, value.size);
    if (onHeap) _ZdlPv_nid_postfix(value.capacity + 1 < GuestStringLargeAllocation ? static_cast<void*>(value.pointer) : reinterpret_cast<void**>(value.pointer)[-1]);
    return text;
}

std::string Widen(const GuestLocale::IosBase* stream, const char* text, std::size_t count) {
    std::string result(text, count);
    if (const GuestLocale::Facet* ctype = FindFacet(stream, _ZNSt5ctypeIcE2idE_nid_postfix)) reinterpret_cast<const GuestLocale::CtypeVtable*>(ctype->vtable)->widenRange(ctype, text, text + count, result.data());
    return result;
}

bool HexPrefix(const char* text, std::size_t prefix, std::size_t count) {
    return prefix + 2 <= count && text[prefix] == '0' && (text[prefix + 1] == 'x' || text[prefix + 1] == 'X');
}

std::size_t SignPrefix(const char* text, std::size_t count) {
    return count > 0 && (text[0] == '+' || text[0] == '-') ? 1 : 0;
}

void InsertSeparators(std::string& text, const std::string& grouping, char separator, std::size_t offset, std::size_t prefix) {
    const char* group = grouping.c_str();
    while (static_cast<signed char>(*group) != CHAR_MAX && static_cast<signed char>(*group) > 0 && static_cast<std::size_t>(*group) < offset - prefix) {
        offset -= static_cast<std::size_t>(*group);
        text.insert(offset, 1, separator);
        if (static_cast<signed char>(group[1]) > 0) ++group;
    }
}

GuestLocale::OutputIterator PutField(GuestLocale::OutputIterator out, GuestLocale::IosBase* stream, char fill, const std::string& text, std::size_t prefix) {
    const std::int32_t width = stream->width;
    std::size_t fillCount = width <= 0 || static_cast<std::size_t>(width) <= text.size() ? 0 : static_cast<std::size_t>(width) - text.size();
    const std::int32_t adjust = stream->flags & IosAdjustField;
    if (adjust != IosLeft && adjust != IosInternal) {
        for (; fillCount != 0; --fillCount) PutCharacter(out, fill);
    }
    for (std::size_t index = 0; index < prefix; ++index) PutCharacter(out, text[index]);
    if (adjust == IosInternal) {
        for (; fillCount != 0; --fillCount) PutCharacter(out, fill);
    }
    for (std::size_t index = prefix; index < text.size(); ++index) PutCharacter(out, text[index]);
    stream->width = 0;
    for (; fillCount != 0; --fillCount) PutCharacter(out, fill);
    return out;
}

GuestLocale::OutputIterator PutInteger(GuestLocale::OutputIterator out, GuestLocale::IosBase* stream, char fill, const char* digits, std::size_t count) {
    std::size_t prefix = SignPrefix(digits, count);
    if ((stream->flags & IosBaseField) == IosHex && HexPrefix(digits, prefix, count)) prefix += 2;
    std::string text = Widen(stream, digits, count);
    const GuestLocale::Facet* punct = StreamNumpunct(stream);
    const std::string grouping = TakeString(Punctuation(punct).grouping, punct);
    if (static_cast<signed char>(grouping[0]) != CHAR_MAX && static_cast<signed char>(grouping[0]) > 0) InsertSeparators(text, grouping, Punctuation(punct).thousandsSep(AsNumpunct(punct)), count, prefix);
    return PutField(out, stream, fill, text, prefix);
}

GuestLocale::OutputIterator PutFloating(GuestLocale::OutputIterator out, GuestLocale::IosBase* stream, char fill, const char* digits, std::size_t count) {
    const std::size_t sign = SignPrefix(digits, count);
    const std::size_t prefix = sign != 0 ? sign : HexPrefix(digits, 0, count) ? 2 : 0;
    const bool hex = (stream->flags & IosHexFloat) == IosHexFloat;
    const std::size_t exponent = std::strcspn(digits, hex ? "pP" : "eE");
    const char point[2] = {localeconv_nid_postfix()->decimal_point[0], '\0'};
    const std::size_t pointOffset = std::strcspn(digits, point);
    std::string text = Widen(stream, digits, count);
    const GuestLocale::Facet* punct = StreamNumpunct(stream);
    const std::string grouping = TakeString(Punctuation(punct).grouping, punct);
    const char separator = Punctuation(punct).thousandsSep(AsNumpunct(punct));
    std::size_t offset = exponent;
    if (pointOffset != count) {
        offset = pointOffset;
        text[pointOffset] = Punctuation(punct).decimalPoint(AsNumpunct(punct));
    }
    const char lead = digits[prefix];
    if (lead != 'I' && lead != 'N' && lead != 'i' && lead != 'n') InsertSeparators(text, grouping, separator, offset, prefix);
    return PutField(out, stream, fill, text, prefix);
}

template<typename Value> GuestLocale::OutputIterator PutFormattedInteger(GuestLocale::OutputIterator out, GuestLocale::IosBase* stream, char fill, const char* qualifier, char decimal, Value value) {
    const std::int32_t flags = stream->flags;
    const std::int32_t base = flags & IosBaseField;
    const char conversion = base == IosOct ? 'o' : base != IosHex ? decimal : (flags & IosUppercase) != 0 ? 'X' : 'x';
    const std::string format = std::string("%") + ((flags & IosShowPos) != 0 ? "+" : "") + ((flags & IosShowBase) != 0 ? "#" : "") + qualifier + conversion;
    char digits[64];
    const int count = snprintf_nid_postfix(digits, sizeof(digits), format.c_str(), value);
    return PutInteger(out, stream, fill, digits, static_cast<std::size_t>(count));
}

template<typename Value> GuestLocale::OutputIterator PutFormattedFloating(GuestLocale::OutputIterator out, GuestLocale::IosBase* stream, char fill, const char* qualifier, Value value) {
    const std::int32_t flags = stream->flags;
    const std::int32_t field = flags & IosHexFloat;
    const std::int32_t precision = field == IosHexFloat ? 6 : stream->precision;
    std::int64_t size = precision;
    if (field == IosFixed && std::fabs(value) > Value(1e10)) {
        int exponent = 0;
        std::frexp(value, &exponent);
        size += static_cast<std::int64_t>(std::abs(exponent)) * 30103 / 100000;
    }
    size += 50;
    const char conversion = field == IosFixed ? 'f' : field == IosHexFloat ? 'a' : field == IosScientific ? 'e' : 'g';
    const bool upper = (flags & IosUppercase) != 0 && field != IosFixed;
    const std::string format = std::string("%") + ((flags & IosShowPos) != 0 ? "+" : "") + ((flags & IosShowPoint) != 0 ? "#" : "") + (field != IosHexFloat ? ".*" : "") + qualifier + static_cast<char>(upper ? conversion - 'a' + 'A' : conversion);
    if (size <= 0) throw std::invalid_argument("num_put::do_put: the precision leaves no room for the converted value");
    std::string digits(static_cast<std::size_t>(size), '\0');
    const int count = snprintf_nid_postfix(digits.data(), digits.size(), format.c_str(), precision, value);
    if (count < 0 || count >= size) throw std::invalid_argument("num_put::do_put: the precision leaves no room for the converted value");
    return PutFloating(out, stream, fill, digits.c_str(), static_cast<std::size_t>(count));
}

GuestLocale::OutputIterator APS5_VABI NumPutBool(const GuestLocale::Facet* self, GuestLocale::OutputIterator out, GuestLocale::IosBase* stream, char fill, bool value) {
    if ((stream->flags & IosBoolAlpha) == 0) return reinterpret_cast<const GuestLocale::NumPutVtable*>(self->vtable)->putLong(self, out, stream, fill, value ? 1 : 0);
    const GuestLocale::Facet* punct = StreamNumpunct(stream);
    return PutField(out, stream, fill, TakeString(value ? Punctuation(punct).truename : Punctuation(punct).falsename, punct), 0);
}

GuestLocale::OutputIterator APS5_VABI NumPutLong(const GuestLocale::Facet*, GuestLocale::OutputIterator out, GuestLocale::IosBase* stream, char fill, std::int64_t value) {
    return PutFormattedInteger(out, stream, fill, "l", 'd', value);
}

GuestLocale::OutputIterator APS5_VABI NumPutUnsignedLong(const GuestLocale::Facet*, GuestLocale::OutputIterator out, GuestLocale::IosBase* stream, char fill, std::uint64_t value) {
    return PutFormattedInteger(out, stream, fill, "l", 'u', value);
}

GuestLocale::OutputIterator APS5_VABI NumPutLongLong(const GuestLocale::Facet*, GuestLocale::OutputIterator out, GuestLocale::IosBase* stream, char fill, std::int64_t value) {
    return PutFormattedInteger(out, stream, fill, "ll", 'd', value);
}

GuestLocale::OutputIterator APS5_VABI NumPutUnsignedLongLong(const GuestLocale::Facet*, GuestLocale::OutputIterator out, GuestLocale::IosBase* stream, char fill, std::uint64_t value) {
    return PutFormattedInteger(out, stream, fill, "ll", 'u', value);
}

GuestLocale::OutputIterator APS5_VABI NumPutDouble(const GuestLocale::Facet*, GuestLocale::OutputIterator out, GuestLocale::IosBase* stream, char fill, double value) {
    return PutFormattedFloating(out, stream, fill, "", value);
}

GuestLocale::OutputIterator APS5_VABI NumPutLongDouble(const GuestLocale::Facet*, GuestLocale::OutputIterator out, GuestLocale::IosBase* stream, char fill, long double value) {
    return PutFormattedFloating(out, stream, fill, "L", value);
}

GuestLocale::OutputIterator APS5_VABI NumPutPointer(const GuestLocale::Facet*, GuestLocale::OutputIterator out, GuestLocale::IosBase* stream, char fill, const void* value) {
    char digits[64];
    const int count = snprintf_nid_postfix(digits, sizeof(digits), "%p", value);
    return PutInteger(out, stream, fill, digits, static_cast<std::size_t>(count));
}

const GuestLocale::NumPutVtable g_numPutVtable{{DestroyCollate, DeleteCollate, RetainFacet, ReleaseFacet}, NumPutBool, NumPutLong, NumPutUnsignedLong, NumPutLongLong, NumPutUnsignedLongLong, NumPutDouble, NumPutLongDouble, NumPutPointer};
const GuestLocale::TimePutVtable g_timePutVtable{{DestroyCollate, DeleteCollate, RetainFacet, ReleaseFacet}, TimePutDoPut};

constexpr std::size_t GuestTimePutFacetSize = 0xb8;

constexpr std::array<short, 257> MakeClassificationTable() {
    std::array<short, 257> table{};
    for (unsigned int value = 0; value < 256; ++value) {
        short mask = 0;
        if (value < 32 || value == 127) mask |= 0x80;
        if (value == ' ') mask |= 0x04;
        if (value >= '\t' && value <= '\r') mask |= 0x40;
        if (value == '\t') mask |= 0x400;
        if (value >= 'A' && value <= 'Z') mask |= 0x02;
        if (value >= 'a' && value <= 'z') mask |= 0x10;
        if (value >= '0' && value <= '9') mask |= 0x20;
        if ((value >= '0' && value <= '9') || (value >= 'A' && value <= 'F') || (value >= 'a' && value <= 'f')) mask |= 0x01;
        if (value >= 33 && value <= 126 && (mask & 0x232) == 0) mask |= 0x08;
        table[value + 1] = mask;
    }
    return table;
}

constexpr std::array<short, 257> MakeCaseTable(bool upper) {
    std::array<short, 257> table{};
    table[0] = -1;
    for (unsigned int value = 0; value < 256; ++value) {
        auto converted = value;
        if (upper && value >= 'a' && value <= 'z') converted -= 'a' - 'A';
        if (!upper && value >= 'A' && value <= 'Z') converted += 'a' - 'A';
        table[value + 1] = static_cast<short>(converted);
    }
    return table;
}

constexpr auto g_classificationTable = MakeClassificationTable();
constexpr auto g_lowerTable = MakeCaseTable(false);
constexpr auto g_upperTable = MakeCaseTable(true);

void ValidateCharacter(int value) {
    if (value < SCHAR_MIN || value > UCHAR_MAX) throw std::invalid_argument("Invalid character value");
}

int ClassifyCharacter(int value, short mask) {
    ValidateCharacter(value);
    if (value == EOF) return 0;
    return g_classificationTable[static_cast<std::size_t>(value < 0 ? value + UCHAR_MAX + 1 : value) + 1] & mask;
}

int ConvertCharacter(int value, bool upper) {
    ValidateCharacter(value);
    if (value < 0) return value;
    const auto& facet = std::use_facet<std::ctype<char>>(std::locale::classic());
    const auto character = static_cast<char>(value);
    return static_cast<unsigned char>(upper ? facet.toupper(character) : facet.tolower(character));
}

}

void* GuestLocale::AllocateStringStorage(std::size_t bytes) {
    if (bytes < GuestStringLargeAllocation) return _Znwm_nid_postfix(bytes);
    if (bytes > std::numeric_limits<std::size_t>::max() - GuestLargeAllocationPadding) throw std::bad_alloc();
    auto* raw = static_cast<unsigned char*>(_Znwm_nid_postfix(bytes + GuestLargeAllocationPadding));
    const auto aligned = (reinterpret_cast<std::uintptr_t>(raw) + GuestLargeAllocationPadding) & ~(GuestLargeAllocationAlignment - 1);
    reinterpret_cast<void**>(aligned)[-1] = raw;
    return reinterpret_cast<void*>(aligned);
}

void GuestLocale::ReleaseStringStorage(String& string) {
    if (string.capacity > GuestStringInlineCapacity) {
        void* storage = string.pointer;
        if (string.capacity + 1 >= GuestStringLargeAllocation) storage = static_cast<void**>(storage)[-1];
        _ZdlPv_nid_postfix(storage);
    }
    string = {};
    string.capacity = GuestStringInlineCapacity;
}

extern "C" {

int APS5_VABI isupper_nid_postfix(int c) { return ClassifyCharacter(c, 0x02); }
int APS5_VABI islower_nid_postfix(int c) { return ClassifyCharacter(c, 0x10); }
int APS5_VABI isalpha_nid_postfix(int c) { return ClassifyCharacter(c, 0x12); }
int APS5_VABI isdigit_nid_postfix(int c) { return ClassifyCharacter(c, 0x20); }
int APS5_VABI isalnum_nid_postfix(int c) { return ClassifyCharacter(c, 0x32); }
int APS5_VABI isspace_nid_postfix(int c) { return ClassifyCharacter(c, 0x44); }
int APS5_VABI isblank_nid_postfix(int c) { return ClassifyCharacter(c, 0x404); }
int APS5_VABI iscntrl_nid_postfix(int c) { return ClassifyCharacter(c, 0x80); }
int APS5_VABI isprint_nid_postfix(int c) { return ClassifyCharacter(c, 0x3e); }
int APS5_VABI isgraph_nid_postfix(int c) { return ClassifyCharacter(c, 0x3a); }
int APS5_VABI ispunct_nid_postfix(int c) { return ClassifyCharacter(c, 0x08); }
int APS5_VABI isxdigit_nid_postfix(int c) { return ClassifyCharacter(c, 0x01); }
int APS5_VABI isascii_nid_postfix(int c) { return (c & ~0x7F) == 0; }
int APS5_VABI toascii_nid_postfix(int c) { return c & 0x7F; }
int APS5_VABI digittoint_nid_postfix(int c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return 0;
}
int APS5_VABI isnumber_nid_postfix(int c) { return isdigit_nid_postfix(c); }
int APS5_VABI ishexnumber_nid_postfix(int c) { return isxdigit_nid_postfix(c); }
int APS5_VABI toupper_nid_postfix(int c) { return ConvertCharacter(c, true); }
int APS5_VABI tolower_nid_postfix(int c) { return ConvertCharacter(c, false); }

std::uint64_t _ZNSt5ctypeIcE2idE_nid_postfix = 0;
std::uint64_t _ZNSt5ctypeIwE2idE_nid_postfix = 0;
std::uintptr_t _ZTVSt5ctypeIcE_nid_postfix[14] {};
std::uint64_t _ZNSt7collateIwE2idE_nid_postfix = 0;
std::uint64_t _ZNSt7collateIcE2idE_nid_postfix = 0;
std::uintptr_t _ZTVSt7collateIcE_nid_postfix[9] {
    0,
    0,
    reinterpret_cast<std::uintptr_t>(&DestroyCollate),
    reinterpret_cast<std::uintptr_t>(&DeleteCollate),
    reinterpret_cast<std::uintptr_t>(&RetainFacet),
    reinterpret_cast<std::uintptr_t>(&ReleaseFacet),
    reinterpret_cast<std::uintptr_t>(&CollateCompare),
    reinterpret_cast<std::uintptr_t>(&CollateTransform),
    reinterpret_cast<std::uintptr_t>(&CollateHash),
};
std::uint64_t _ZNSt7codecvtIcc9_MbstatetE2idE_nid_postfix = 0;
std::uintptr_t _ZTVSt7codecvtIcc9_MbstatetE_nid_postfix[16] {};
std::uintptr_t _ZTVSt7codecvtIwc9_MbstatetE_nid_postfix[13] {};
std::uint64_t _ZNSt7num_putIcSt19ostreambuf_iteratorIcSt11char_traitsIcEEE2idE_nid_postfix = 0;
std::uintptr_t _ZTVSt7num_putIcSt19ostreambuf_iteratorIcSt11char_traitsIcEEE_nid_postfix[kNumPutCharVtableSlots] {};
std::uint64_t _ZNSt7num_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE2idE_nid_postfix = 0;
std::uintptr_t _ZTVSt7num_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE_nid_postfix[17] {};
std::uint64_t _ZNSt8time_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE2idE_nid_postfix = 0;
std::uint64_t _ZNSt7num_putIwSt19ostreambuf_iteratorIwSt11char_traitsIwEEE2idE_nid_postfix = 0;
std::uintptr_t _ZTVSt7num_putIwSt19ostreambuf_iteratorIwSt11char_traitsIwEEE_nid_postfix[14] {};
std::uint64_t _ZNSt8numpunctIcE2idE_nid_postfix = 0;
std::uint64_t _ZNSt8time_putIcSt19ostreambuf_iteratorIcSt11char_traitsIcEEE2idE_nid_postfix = 0;
std::uintptr_t _ZTVSt8time_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE_nid_postfix[13] {};
std::uintptr_t _ZTVSt8time_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE_nid_postfix[13] {};
std::uintptr_t _ZTVSt7num_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE_nid_postfix[17] {};

std::streamoff _ZSt7_BADOFF_nid_postfix = -1;
std::fpos_t _ZSt4_Fpz_nid_postfix {};
std::int32_t _ZNSt6locale2id7_Id_cntE_nid_postfix = 0;

GuestLocale::Implementation* _ZSt21_sceLibcClassicLocale_nid_postfix = &g_classicLocale;

void APS5_VABI _ZNSt8ios_baseD2Ev_nid_postfix(GuestLocale::IosBase* self) {
    if (self == nullptr) throw std::invalid_argument("ios_base destructor: null object");
    std::lock_guard lock(g_standardStreamMutex);
    if (self->standardStream > 0) {
        if (self->standardStream > StandardStreamSlots) throw std::runtime_error("ios_base destructor: unknown standard stream slot");
        if (0 < --g_standardStreamOpens[self->standardStream]) return;
    }
    if (self->storage != nullptr || self->callbacks != nullptr) throw std::runtime_error("ios_base destructor: unsupported stream storage or callbacks");
    if (self->locale != &_ZSt21_sceLibcClassicLocale_nid_postfix) throw std::runtime_error("ios_base destructor: unsupported locale ownership");
    self->locale = nullptr;
}

GuestLocale::IosBase* APS5_VABI SetStreamWidth(GuestLocale::IosBase* stream, std::int32_t width) {
    if (stream == nullptr) throw std::invalid_argument("setw: null stream");
    stream->width = width;
    return stream;
}

GuestLocale::IosBase* APS5_VABI SetStreamPrecision(GuestLocale::IosBase* stream, std::int32_t precision) {
    if (stream == nullptr) throw std::invalid_argument("setprecision: null stream");
    stream->precision = precision;
    return stream;
}

static_assert(sizeof(StreamManipulator) == 0x10);

StreamManipulator APS5_VABI _ZSt4setwi_nid_postfix(std::int32_t width) {
    return {&SetStreamWidth, width};
}

StreamManipulator APS5_VABI _ZSt12setprecisioni_nid_postfix(std::int32_t precision) {
    return {&SetStreamPrecision, precision};
}

void APS5_VABI _ZNSt8ios_base7_AddstdEPS__nid_postfix(GuestLocale::IosBase* self) {
    if (self == nullptr) throw std::invalid_argument("ios_base _Addstd: null stream");
    std::lock_guard lock(g_standardStreamMutex);
    for (self->standardStream = 0; ++self->standardStream < StandardStreamSlots; ) {
        if (g_standardStreams[self->standardStream] == nullptr || g_standardStreams[self->standardStream] == self) break;
    }
    g_standardStreams[self->standardStream] = self;
    ++g_standardStreamOpens[self->standardStream];
}

// Dinkumware's static locale::_Init(bool) returns the global _Locimp, which the caller copies.
GuestLocale::Implementation* APS5_VABI _ZNSt6locale5_InitEv_nid_postfix() {
    std::lock_guard<std::mutex> lock(g_localeInitMutex);
    if (!g_localeInitialized) {
        g_localeInitialized = true;
    }
    return &g_classicLocale;
}

void APS5_VABI _ZNSt6locale5facet9_RegisterEv_nid_postfix(GuestLocale::Facet* self) {
    if (self == nullptr || self->vtable == nullptr) throw std::invalid_argument("locale register: invalid facet");
    std::lock_guard<std::mutex> lock(g_localeInitMutex);
    for (const auto* facet : g_registeredFacets) {
        if (facet == self) throw std::runtime_error("locale register: duplicate facet");
    }
    g_registeredFacets.push_back(self);
}

GuestLocale::Implementation* APS5_VABI _ZNSt6locale16_GetgloballocaleEv_nid_postfix() {
    return &g_classicLocale;
}

namespace {

GuestLocaleFacade g_classicLocaleFacade{&g_classicLocale};
}

GuestLocaleFacade* APS5_VABI _ZNSt6locale7classicEv_nid_postfix() {
    return &g_classicLocaleFacade;
}

std::size_t APS5_VABI _ZNSt7collateIwE7_GetcatEPPKNSt6locale5facetEPKS1__nid_postfix(GuestLocale::Facet** facet, const GuestLocale::Implementation* const* locale) {
    if (facet != nullptr && *facet == nullptr) {
        if (locale == nullptr || *locale == nullptr || (*locale)->name == nullptr || std::strcmp((*locale)->name, "C") != 0) throw std::invalid_argument("collate::_Getcat: only the C locale is supported");
        auto* collate = static_cast<GuestLocale::CollateFacet*>(_Znwm_nid_postfix(sizeof(GuestLocale::CollateFacet)));
        *collate = {{&g_wideCollateVtable.facet, 0, 0}, nullptr, nullptr};
        *facet = &collate->base;
    }
    return 1;
}

std::size_t APS5_VABI _ZNSt7collateIcE7_GetcatEPPKNSt6locale5facetEPKS1__nid_postfix(GuestLocale::Facet** facet, const GuestLocale::Implementation* const* locale) {
    if (facet != nullptr && *facet == nullptr) {
        if (locale == nullptr || *locale == nullptr || (*locale)->name == nullptr || std::strcmp((*locale)->name, "C") != 0) throw std::invalid_argument("collate::_Getcat: only the C locale is supported");
        auto* collate = static_cast<GuestLocale::CollateFacet*>(_Znwm_nid_postfix(sizeof(GuestLocale::CollateFacet)));
        *collate = {{&g_collateVtable.facet, 0, 0}, nullptr, nullptr};
        *facet = &collate->base;
    }
    return 1;
}

std::size_t APS5_VABI _ZNSt7codecvtIcc9_MbstatetE7_GetcatEPPKNSt6locale5facetEPKS2__nid_postfix(GuestLocale::Facet** facet, const GuestLocale::Implementation* const*) {
    if (facet != nullptr && *facet == nullptr) {
        auto* object = static_cast<GuestLocale::Facet*>(_Znwm_nid_postfix(sizeof(GuestLocale::Facet)));
        *object = {&g_codecvtVtable.facet, 0, 0};
        *facet = object;
    }
    return 2;
}

std::size_t APS5_VABI _ZNSt8numpunctIcE7_GetcatEPPKNSt6locale5facetEPKS1__nid_postfix(GuestLocale::Facet** facet, const GuestLocale::Implementation* const*) {
    if (facet != nullptr && *facet == nullptr) {
        auto* punct = static_cast<GuestLocale::NumpunctFacet*>(_Znwm_nid_postfix(sizeof(GuestLocale::NumpunctFacet)));
        *punct = {{&g_numpunctVtable.facet, 0, 0}, "", '.', ',', "false", "true"};
        *facet = &punct->base;
    }
    return 4;
}

std::size_t APS5_VABI _ZNSt7num_putIcSt19ostreambuf_iteratorIcSt11char_traitsIcEEE7_GetcatEPPKNSt6locale5facetEPKS5__nid_postfix(GuestLocale::Facet** facet, const GuestLocale::Implementation* const*) {
    if (facet != nullptr && *facet == nullptr) {
        auto* object = static_cast<GuestLocale::Facet*>(_Znwm_nid_postfix(sizeof(GuestLocale::Facet)));
        *object = {&g_numPutVtable.facet, 0, 0};
        *facet = object;
    }
    return 4;
}

std::size_t APS5_VABI _ZNSt8time_putIcSt19ostreambuf_iteratorIcSt11char_traitsIcEEE7_GetcatEPPKNSt6locale5facetEPKS5__nid_postfix(GuestLocale::Facet** facet, const GuestLocale::Implementation* const*) {
    if (facet != nullptr && *facet == nullptr) {
        auto* object = static_cast<GuestLocale::Facet*>(_Znwm_nid_postfix(GuestTimePutFacetSize));
        std::memset(object, 0, GuestTimePutFacetSize);
        object->vtable = &g_timePutVtable.facet;
        *facet = object;
    }
    return 5;
}

void APS5_VABI _ZNSt8_LocinfoC1EPKc_nid_postfix(GuestLocale::LocinfoStorage* self, const char* localeName) {
    if (self == nullptr || localeName == nullptr || std::strcmp(localeName, "C") != 0) throw std::invalid_argument("_Locinfo: only the C locale is supported");
    new (self) GuestLocale::LocinfoStorage{};
}

void APS5_VABI _ZNSt8_LocinfoD1Ev_nid_postfix(GuestLocale::LocinfoStorage* self) {
    if (self == nullptr) throw std::invalid_argument("_Locinfo destructor: null object");
}

int APS5_VABI _Mbtowcx_nid_postfix(std::uint16_t* dst, const char* src, std::size_t count, mbstate_t* st) {
    if (dst == nullptr || src == nullptr || st == nullptr || count == 0) throw std::invalid_argument("_Mbtowcx: invalid conversion arguments");
    wchar_t converted{};
    const auto result = std::mbrtowc(&converted, src, count, st);
    if (result == static_cast<std::size_t>(-1)) throw std::runtime_error("_Mbtowcx: invalid multibyte character");
    if (result == static_cast<std::size_t>(-2)) throw std::runtime_error("_Mbtowcx: incomplete multibyte character");
    if (result > static_cast<std::size_t>(std::numeric_limits<int>::max()) || static_cast<std::uint32_t>(converted) > 0xffff) throw std::runtime_error("_Mbtowcx: conversion exceeds guest character limits");
    *dst = static_cast<std::uint16_t>(converted);
    return static_cast<int>(result);
}

int APS5_VABI _Wctombx_nid_postfix(char* dst, std::uint16_t src, mbstate_t* st) {
    if (dst == nullptr || st == nullptr) throw std::invalid_argument("_Wctombx: invalid conversion arguments");
    const auto result = std::wcrtomb(dst, static_cast<wchar_t>(src), st);
    if (result == static_cast<std::size_t>(-1)) throw std::runtime_error("_Wctombx: invalid wide character");
    if (result > static_cast<std::size_t>(std::numeric_limits<int>::max())) throw std::runtime_error("_Wctombx: conversion size exceeds guest limits");
    return static_cast<int>(result);
}

const short* APS5_VABI _Getpctype_nid_postfix() {
    return g_classificationTable.data() + 1;
}

const short* APS5_VABI _Getptolower_nid_postfix() {
    return g_lowerTable.data() + 1;
}

const short* APS5_VABI _Getptoupper_nid_postfix() {
    return g_upperTable.data() + 1;
}

mbstate_t* APS5_VABI _Getpmbstate_nid_postfix() {
    thread_local mbstate_t state {};
    return &state;
}

mbstate_t* APS5_VABI _Getpwcstate_nid_postfix() {
    thread_local mbstate_t state {};
    return &state;
}

std::uint32_t APS5_VABI _Towctrans_nid_postfix(std::uint32_t c, std::uint64_t desc) {
    constexpr std::uint64_t ToLower = 1;
    constexpr std::uint64_t ToUpper = 2;
    if (desc != ToLower && desc != ToUpper) {
        errno = 22;
        return c;
    }
    if (c > UCHAR_MAX) return c;
    return static_cast<std::uint32_t>((desc == ToLower ? g_lowerTable : g_upperTable)[c + 1]);
}

// Locale ids and stream objects the title imports besides the ones above; the streams are zeroed
// storage (the guest constructs Dinkumware streams itself) so address-taking code links and only a
// use of them would misbehave.
std::uint64_t _ZNSt7codecvtIwc9_MbstatetE2idE_nid_postfix = 0;
alignas(16) unsigned char _ZSt4cout_nid_postfix[0x400] {};
alignas(16) unsigned char _ZSt4cerr_nid_postfix[0x400] {};
alignas(16) unsigned char _ZSt4clog_nid_postfix[0x400] {};
alignas(16) unsigned char _ZSt3cin_nid_postfix[0x400] {};
alignas(16) unsigned char _ZSt5wcout_nid_postfix[0x400] {};
alignas(16) unsigned char _ZSt5wcerr_nid_postfix[0x400] {};
alignas(16) unsigned char _ZSt5wclog_nid_postfix[0x400] {};
alignas(16) unsigned char _ZSt4wcin_nid_postfix[0x400] {};

// Facet vectors are read by inlined guest code, so they come from the guest application heap.
GuestLocale::Facet** AllocateFacetVector(std::size_t count) {
    auto* vector = static_cast<GuestLocale::Facet**>(ApplicationHeapAllocate_nid_no_patch(count * sizeof(GuestLocale::Facet*)));
    if (vector == nullptr) throw std::runtime_error("locale: facet vector allocation failed");
    std::memset(vector, 0, count * sizeof(GuestLocale::Facet*));
    return vector;
}

// _Locimp::_Locimp(const _Locimp&): the copy shares the source's facets; guest locales are never freed.
void APS5_VABI _ZNSt6locale7_LocimpC1ERKS0__nid_postfix(GuestLocale::Implementation* self, const GuestLocale::Implementation* source) {
    if (self == nullptr || source == nullptr) throw std::invalid_argument("_Locimp copy: null object");
    std::lock_guard<std::mutex> lock(g_localeInitMutex);
    *self = *source;
    self->base.vtable = &g_copiedLocaleVtable;
    self->base.references = 1;
    if (source->facetCount != 0) {
        self->facets = AllocateFacetVector(source->facetCount);
        std::memcpy(self->facets, source->facets, source->facetCount * sizeof(GuestLocale::Facet*));
    }
}

// _Locimp::_Addfac(facet*, size_t id): installs a facet at its id, growing the vector as needed.
void APS5_VABI _ZNSt6locale7_Locimp7_AddfacEPNS_5facetEm_nid_postfix(GuestLocale::Implementation* self, GuestLocale::Facet* facet, std::size_t id) {
    if (self == nullptr) throw std::invalid_argument("_Locimp::_Addfac: null object");
    std::lock_guard<std::mutex> lock(g_localeInitMutex);
    if (id >= self->facetCount) {
        const std::size_t count = std::max<std::size_t>(id + 1, 40);
        auto* grown = AllocateFacetVector(count);
        if (self->facetCount != 0) std::memcpy(grown, self->facets, self->facetCount * sizeof(GuestLocale::Facet*));
        self->facets = grown;
        self->facetCount = count;
    }
    self->facets[id] = facet;
}

void APS5_VABI _init_env_nid_postfix() {
    ApplicationHeapInitialize_nid_no_patch(ApplicationProcessParameters_nid_no_patch());
}

void APS5_VABI init_env_nid_postfix(const InitEnvParams* params) {
    (void)params;
    _init_env_nid_postfix();

}

SonyLconv* APS5_VABI localeconv_nid_postfix(void) {
    return SonyLocaleconv();
}

}
