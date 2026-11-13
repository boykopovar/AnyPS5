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
#include <type_traits>

#include "prx/libc/include/General.hpp"
#include "prx/libc/include/SonyCrt.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/GuestLocale.hpp"
#include "prx/libc/include/GuestTime.hpp"
#include "prx/libc/GuestStringStorage.hpp"
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
    std::size_t capacity = GuestLocale::StringInlineCapacity;
    if (length > GuestLocale::StringInlineCapacity) {
        capacity = length | GuestLocale::StringCapacityMask;
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

GuestLocale::String* APS5_VABI AssignCString(GuestLocale::String* result, const char* source) {
    if (result == nullptr || source == nullptr) throw std::invalid_argument("numpunct string: null argument");
    const std::size_t length = std::strlen(source);
    char* data = result->buffer;
    std::size_t capacity = GuestLocale::StringInlineCapacity;
    if (length > GuestLocale::StringInlineCapacity) {
        capacity = length | GuestLocale::StringCapacityMask;
        data = static_cast<char*>(GuestLocale::AllocateStringStorage(capacity + 1));
    }
    if (length != 0) std::memcpy(data, source, length);
    data[length] = 0;
    if (data != result->buffer) result->pointer = data;
    result->size = length;
    result->capacity = capacity;
    return result;
}

const char g_wideNumpunctGrouping[] = "";
const std::uint16_t g_wideNumpunctFalseName[] = {'f', 'a', 'l', 's', 'e', 0};
const std::uint16_t g_wideNumpunctTrueName[] = {'t', 'r', 'u', 'e', 0};

GuestLocale::String* APS5_VABI AssignWideCString(GuestLocale::String* result, const std::uint16_t* source) {
    if (result == nullptr || source == nullptr) throw std::invalid_argument("numpunct string: null argument");
    std::size_t length = 0;
    while (source[length] != 0) ++length;
    const auto bytes = length * GuestWideCharacterSize;
    char* data = result->buffer;
    std::size_t capacity = GuestWideInlineCapacity;
    if (length > GuestWideInlineCapacity) {
        capacity = length | GuestWideInlineCapacity;
        data = static_cast<char*>(GuestLocale::AllocateStringStorage((capacity + 1) * GuestWideCharacterSize));
    }
    if (bytes != 0) std::memcpy(data, source, bytes);
    data[bytes] = 0;
    data[bytes + 1] = 0;
    if (data != result->buffer) result->pointer = data;
    result->size = length;
    result->capacity = capacity;
    return result;
}

std::uint16_t APS5_VABI WideNumpunctDecimalPoint(const GuestLocale::WideNumpunctFacet* self) {
    return self->decimalPoint;
}

std::uint16_t APS5_VABI WideNumpunctThousandsSep(const GuestLocale::WideNumpunctFacet* self) {
    return self->thousandsSep;
}

GuestLocale::String* APS5_VABI WideNumpunctGrouping(GuestLocale::String* result, const GuestLocale::WideNumpunctFacet* self) {
    return AssignCString(result, self->grouping);
}

GuestLocale::String* APS5_VABI WideNumpunctFalsename(GuestLocale::String* result, const GuestLocale::WideNumpunctFacet* self) {
    return AssignWideCString(result, self->falsename);
}

GuestLocale::String* APS5_VABI WideNumpunctTruename(GuestLocale::String* result, const GuestLocale::WideNumpunctFacet* self) {
    return AssignWideCString(result, self->truename);
}

const GuestLocale::WideNumpunctVtable g_wideNumpunctVtable{{DestroyCollate, DeleteCollate, RetainFacet, ReleaseFacet}, WideNumpunctDecimalPoint, WideNumpunctThousandsSep, WideNumpunctGrouping, WideNumpunctFalsename, WideNumpunctTruename};

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

const GuestLocale::NumpunctVtable g_numpunctVtable{{DestroyCollate, DeleteCollate, RetainFacet, ReleaseFacet}, _ZNKSt8numpunctIcE16do_decimal_pointEv_nid_postfix, _ZNKSt8numpunctIcE16do_thousands_sepEv_nid_postfix, _ZNKSt8numpunctIcE11do_groupingEv_nid_postfix, _ZNKSt8numpunctIcE12do_falsenameEv_nid_postfix, _ZNKSt8numpunctIcE11do_truenameEv_nid_postfix};

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

const GuestLocale::NumpunctFacet* RequireNumpunct(const GuestLocale::NumpunctFacet* self, const char* function) {
    if (self == nullptr || self->base.vtable == nullptr) throw std::invalid_argument(std::string(function) + ": null facet");
    return self;
}

std::string TakeString(GuestLocale::String* (APS5_VABI* method)(GuestLocale::String*, const GuestLocale::NumpunctFacet*), const GuestLocale::Facet* facet) {
    GuestLocale::String value{};
    method(&value, AsNumpunct(facet));
    const bool onHeap = value.capacity > GuestLocale::StringInlineCapacity;
    std::string text(onHeap ? value.pointer : value.buffer, value.size);
    if (onHeap) GuestStringStorage::Deallocate(value.pointer, value.capacity + 1);
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

template<typename Text> void InsertSeparators(std::basic_string<Text>& text, const std::string& grouping, Text separator, std::size_t offset, std::size_t prefix) {
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

struct SceV2Cvtvec {
    std::uint32_t page;
    std::uint32_t maxLength;
    std::uint32_t reserved[8];
};

inline constexpr SceV2Cvtvec SceV2ClassicCvtvec{1, 1, {}};

struct SceV2CtypeFacet {
    GuestLocale::Facet base;
    const short* table;
    std::int32_t deleteMode;
};

struct SceV2WideCtypeFacet {
    GuestLocale::Facet base;
    const short* table;
    std::int32_t deleteMode;
    std::uint32_t reserved;
    SceV2Cvtvec conversion;
};

struct SceV2WideCodecvtFacet {
    const GuestLocale::FacetVtable* vtable;
    std::uint32_t references;
    SceV2Cvtvec conversion;
};

template<typename Text> struct SceV2MoneypunctFacet {
    GuestLocale::Facet base;
    const char* grouping;
    Text decimalPoint;
    Text thousandsSep;
    const Text* currency;
    const Text* positiveSign;
    const Text* negativeSign;
    std::int32_t fractionDigits;
    char positivePattern[4];
    char negativePattern[4];
    bool international;
    SceV2Cvtvec conversion;
};

template<typename Text> struct SceV2TimeGetFacet {
    GuestLocale::Facet base;
    const Text* days;
    const Text* months;
    const Text* amPm;
    std::int32_t dateOrder;
    SceV2Cvtvec conversion;
};

struct SceV2TimePutFacet {
    GuestLocale::Facet base;
    void* names;
};

static_assert(sizeof(SceV2Cvtvec) == 0x28);
static_assert(sizeof(SceV2CtypeFacet) == 0x20);
static_assert(offsetof(SceV2CtypeFacet, deleteMode) == 0x18);
static_assert(sizeof(SceV2WideCtypeFacet) == 0x48);
static_assert(offsetof(SceV2WideCtypeFacet, conversion) == 0x20);
static_assert(sizeof(SceV2WideCodecvtFacet) == 0x38);
static_assert(offsetof(SceV2WideCodecvtFacet, conversion) == 0xc);
static_assert(sizeof(SceV2MoneypunctFacet<char>) == 0x70);
static_assert(sizeof(SceV2MoneypunctFacet<std::uint16_t>) == 0x70);
static_assert(offsetof(SceV2MoneypunctFacet<char>, thousandsSep) == 0x19);
static_assert(offsetof(SceV2MoneypunctFacet<std::uint16_t>, thousandsSep) == 0x1a);
static_assert(offsetof(SceV2MoneypunctFacet<char>, fractionDigits) == 0x38);
static_assert(offsetof(SceV2MoneypunctFacet<char>, international) == 0x44);
static_assert(offsetof(SceV2MoneypunctFacet<char>, conversion) == 0x48);
static_assert(sizeof(SceV2TimeGetFacet<char>) == 0x58);
static_assert(offsetof(SceV2TimeGetFacet<char>, dateOrder) == 0x28);
static_assert(offsetof(SceV2TimeGetFacet<char>, conversion) == 0x2c);
static_assert(sizeof(SceV2TimePutFacet) == 0x18);

template<std::size_t Virtuals> struct SceV2FacetVtable {
    GuestLocale::FacetVtable facet;
    void (APS5_VABI *virtuals[Virtuals])();
};

template<const char* Family> void APS5_VABI SceV2FacetVirtual_nid_no_patch() {
    NotImplemented_nid_no_patch(Family);
}

using SceV2FacetDestroy = void (APS5_VABI *)(GuestLocale::Facet*);

template<SceV2FacetDestroy Destroy> void APS5_VABI DeleteSceV2Facet_nid_no_patch(GuestLocale::Facet* self) {
    Destroy(self);
    _ZdlPv_nid_postfix(self);
}

void APS5_VABI DestroySceV2PlainFacet_nid_no_patch(GuestLocale::Facet*) {}

void APS5_VABI DestroySceV2CtypeFacet_nid_no_patch(GuestLocale::Facet* self) {
    auto* facet = reinterpret_cast<SceV2CtypeFacet*>(self);
    if (facet->deleteMode > 0) free_nid_postfix(const_cast<short*>(facet->table));
    else if (facet->deleteMode < 0 && facet->table != nullptr) _ZdaPv_nid_postfix(const_cast<short*>(facet->table));
}

void APS5_VABI DestroySceV2WideCtypeFacet_nid_no_patch(GuestLocale::Facet* self) {
    auto* facet = reinterpret_cast<SceV2WideCtypeFacet*>(self);
    if (facet->deleteMode != 0) free_nid_postfix(const_cast<short*>(facet->table));
}

void APS5_VABI DestroySceV2TimePutFacet_nid_no_patch(GuestLocale::Facet* self) {
    free_nid_postfix(reinterpret_cast<SceV2TimePutFacet*>(self)->names);
}

void APS5_VABI DestroySceV2UshortNumpunctFacet_nid_no_patch(GuestLocale::Facet* self) {
    auto* facet = reinterpret_cast<GuestLocale::WideNumpunctFacet*>(self);
    free_nid_postfix(const_cast<char*>(facet->grouping));
    free_nid_postfix(const_cast<std::uint16_t*>(facet->falsename));
    free_nid_postfix(const_cast<std::uint16_t*>(facet->truename));
}

void APS5_VABI DestroySceV2UshortMoneypunctFacet_nid_no_patch(GuestLocale::Facet* self) {
    auto* facet = reinterpret_cast<SceV2MoneypunctFacet<std::uint16_t>*>(self);
    free_nid_postfix(const_cast<char*>(facet->grouping));
    free_nid_postfix(const_cast<std::uint16_t*>(facet->currency));
    free_nid_postfix(const_cast<std::uint16_t*>(facet->positiveSign));
    free_nid_postfix(const_cast<std::uint16_t*>(facet->negativeSign));
}

void APS5_VABI DestroySceV2UshortTimeGetFacet_nid_no_patch(GuestLocale::Facet* self) {
    auto* facet = reinterpret_cast<SceV2TimeGetFacet<std::uint16_t>*>(self);
    free_nid_postfix(const_cast<std::uint16_t*>(facet->days));
    free_nid_postfix(const_cast<std::uint16_t*>(facet->months));
    free_nid_postfix(const_cast<std::uint16_t*>(facet->amPm));
}

template<std::size_t Virtuals, const char* Family, SceV2FacetDestroy Destroy = DestroySceV2PlainFacet_nid_no_patch>
constexpr SceV2FacetVtable<Virtuals> MakeSceV2FacetVtable() {
    SceV2FacetVtable<Virtuals> vtable{{Destroy, DeleteSceV2Facet_nid_no_patch<Destroy>, RetainFacet, ReleaseFacet}, {}};
    for (auto& slot : vtable.virtuals) slot = SceV2FacetVirtual_nid_no_patch<Family>;
    return vtable;
}

template<std::size_t Size> constexpr std::array<std::uint16_t, Size> WidenSceV2Text(const char (&text)[Size]) {
    std::array<std::uint16_t, Size> wide{};
    for (std::size_t index = 0; index < Size; ++index) wide[index] = static_cast<unsigned char>(text[index]);
    return wide;
}

constexpr char g_sceV2CtypeFamily[] = "std::__sce_v2::ctype virtual";
constexpr char g_sceV2NumGetFamily[] = "std::__sce_v2::num_get virtual";
constexpr char g_sceV2NumpunctFamily[] = "std::__sce_v2::numpunct virtual";
constexpr char g_sceV2NumPutFamily[] = "std::__sce_v2::num_put virtual";
constexpr char g_sceV2CodecvtFamily[] = "std::__sce_v2::codecvt virtual";
constexpr char g_sceV2CollateFamily[] = "std::__sce_v2::collate virtual";
constexpr char g_sceV2MessagesFamily[] = "std::__sce_v2::messages virtual";
constexpr char g_sceV2MoneyGetFamily[] = "std::__sce_v2::money_get virtual";
constexpr char g_sceV2MoneypunctFamily[] = "std::__sce_v2::moneypunct virtual";
constexpr char g_sceV2MoneyPutFamily[] = "std::__sce_v2::money_put virtual";
constexpr char g_sceV2TimeGetFamily[] = "std::__sce_v2::time_get virtual";
constexpr char g_sceV2TimePutFamily[] = "std::__sce_v2::time_put virtual";

template<typename Text> struct SceV2BasicString {
    static constexpr std::size_t InlineCapacity = 16 / sizeof(Text) - 1;
    union {
        Text buffer[InlineCapacity + 1];
        Text* pointer;
    };
    std::size_t size;
    std::size_t capacity;
};

using SceV2String = SceV2BasicString<char>;
using SceV2WideString = SceV2BasicString<char16_t>;

static_assert(sizeof(SceV2String) == 0x20);
static_assert(sizeof(SceV2WideString) == 0x20);

template<typename Text> Text* ReserveSceV2String(SceV2BasicString<Text>* result, std::size_t length) {
    std::memset(result, 0, sizeof(SceV2BasicString<Text>));
    Text* data = result->buffer;
    std::size_t capacity = SceV2BasicString<Text>::InlineCapacity;
    if (length > SceV2BasicString<Text>::InlineCapacity) {
        capacity = length | SceV2BasicString<Text>::InlineCapacity;
        data = static_cast<Text*>(GuestLocale::AllocateStringStorage((capacity + 1) * sizeof(Text)));
        result->pointer = data;
    }
    data[length] = 0;
    result->size = length;
    result->capacity = capacity;
    return data;
}

SceV2String* AssignSceV2String(SceV2String* result, const char* text) {
    const std::size_t length = std::strlen(text);
    std::memcpy(ReserveSceV2String(result, length), text, length);
    return result;
}

SceV2WideString* AssignSceV2WideString(SceV2WideString* result, const std::uint16_t* text) {
    const auto* source = reinterpret_cast<const char16_t*>(text);
    const std::size_t length = std::char_traits<char16_t>::length(source);
    std::memcpy(ReserveSceV2String(result, length), source, length * sizeof(char16_t));
    return result;
}

struct SceV2CtypeCharVtable {
    GuestLocale::FacetVtable facet;
    char (APS5_VABI *toLower)(const GuestLocale::Facet* self, char character);
    const char* (APS5_VABI *toLowerRange)(const GuestLocale::Facet* self, char* first, const char* last);
    char (APS5_VABI *toUpper)(const GuestLocale::Facet* self, char character);
    const char* (APS5_VABI *toUpperRange)(const GuestLocale::Facet* self, char* first, const char* last);
    char (APS5_VABI *widen)(const GuestLocale::Facet* self, char character);
    const char* (APS5_VABI *widenRange)(const GuestLocale::Facet* self, const char* first, const char* last, char* destination);
    char (APS5_VABI *narrow)(const GuestLocale::Facet* self, char character, char fallback);
    const char* (APS5_VABI *narrowRange)(const GuestLocale::Facet* self, const char* first, const char* last, char fallback, char* destination);
};

char APS5_VABI SceV2CtypeToLower_nid_no_patch(const GuestLocale::Facet*, char character) {
    return static_cast<char>(_Tolower_nid_postfix(static_cast<unsigned char>(character), nullptr));
}

const char* APS5_VABI SceV2CtypeToLowerRange_nid_no_patch(const GuestLocale::Facet*, char* first, const char* last) {
    for (; first != last; ++first) *first = static_cast<char>(_Tolower_nid_postfix(static_cast<unsigned char>(*first), nullptr));
    return first;
}

char APS5_VABI SceV2CtypeToUpper_nid_no_patch(const GuestLocale::Facet*, char character) {
    return static_cast<char>(_Toupper_nid_postfix(static_cast<unsigned char>(character), nullptr));
}

const char* APS5_VABI SceV2CtypeToUpperRange_nid_no_patch(const GuestLocale::Facet*, char* first, const char* last) {
    for (; first != last; ++first) *first = static_cast<char>(_Toupper_nid_postfix(static_cast<unsigned char>(*first), nullptr));
    return first;
}

char APS5_VABI SceV2CtypeWiden_nid_no_patch(const GuestLocale::Facet*, char character) {
    return character;
}

const char* APS5_VABI SceV2CtypeWidenRange_nid_no_patch(const GuestLocale::Facet*, const char* first, const char* last, char* destination) {
    if (first != last) std::memcpy(destination, first, static_cast<std::size_t>(last - first));
    return last;
}

char APS5_VABI SceV2CtypeNarrow_nid_no_patch(const GuestLocale::Facet*, char character, char) {
    return character;
}

const char* APS5_VABI SceV2CtypeNarrowRange_nid_no_patch(const GuestLocale::Facet*, const char* first, const char* last, char, char* destination) {
    if (first != last) std::memcpy(destination, first, static_cast<std::size_t>(last - first));
    return last;
}

struct SceV2NumpunctCharVtable {
    GuestLocale::FacetVtable facet;
    char (APS5_VABI *decimalPoint)(const GuestLocale::NumpunctFacet* self);
    char (APS5_VABI *thousandsSep)(const GuestLocale::NumpunctFacet* self);
    SceV2String* (APS5_VABI *grouping)(SceV2String* result, const GuestLocale::NumpunctFacet* self);
    SceV2String* (APS5_VABI *falsename)(SceV2String* result, const GuestLocale::NumpunctFacet* self);
    SceV2String* (APS5_VABI *truename)(SceV2String* result, const GuestLocale::NumpunctFacet* self);
};

char APS5_VABI SceV2NumpunctDecimalPoint_nid_no_patch(const GuestLocale::NumpunctFacet* self) {
    return self->decimalPoint;
}

char APS5_VABI SceV2NumpunctThousandsSep_nid_no_patch(const GuestLocale::NumpunctFacet* self) {
    return self->thousandsSep;
}

SceV2String* APS5_VABI SceV2NumpunctGrouping_nid_no_patch(SceV2String* result, const GuestLocale::NumpunctFacet* self) {
    return AssignSceV2String(result, self->grouping);
}

SceV2String* APS5_VABI SceV2NumpunctFalsename_nid_no_patch(SceV2String* result, const GuestLocale::NumpunctFacet* self) {
    return AssignSceV2String(result, self->falsename);
}

SceV2String* APS5_VABI SceV2NumpunctTruename_nid_no_patch(SceV2String* result, const GuestLocale::NumpunctFacet* self) {
    return AssignSceV2String(result, self->truename);
}

struct SceV2CtypeWideVtable {
    GuestLocale::FacetVtable facet;
    bool (APS5_VABI *is)(const GuestLocale::Facet* self, short mask, char16_t character);
    const char16_t* (APS5_VABI *isRange)(const GuestLocale::Facet* self, const char16_t* first, const char16_t* last, short* destination);
    const char16_t* (APS5_VABI *scanIs)(const GuestLocale::Facet* self, short mask, const char16_t* first, const char16_t* last);
    const char16_t* (APS5_VABI *scanNot)(const GuestLocale::Facet* self, short mask, const char16_t* first, const char16_t* last);
    char16_t (APS5_VABI *toLower)(const GuestLocale::Facet* self, char16_t character);
    const char16_t* (APS5_VABI *toLowerRange)(const GuestLocale::Facet* self, char16_t* first, const char16_t* last);
    char16_t (APS5_VABI *toUpper)(const GuestLocale::Facet* self, char16_t character);
    const char16_t* (APS5_VABI *toUpperRange)(const GuestLocale::Facet* self, char16_t* first, const char16_t* last);
    char16_t (APS5_VABI *widen)(const GuestLocale::Facet* self, char character);
    const char* (APS5_VABI *widenRange)(const GuestLocale::Facet* self, const char* first, const char* last, char16_t* destination);
    char (APS5_VABI *narrow)(const GuestLocale::Facet* self, char16_t character, char fallback);
    const char16_t* (APS5_VABI *narrowRange)(const GuestLocale::Facet* self, const char16_t* first, const char16_t* last, char fallback, char* destination);
};

const short* SceV2WideCtypeTable(const GuestLocale::Facet* self) {
    return reinterpret_cast<const SceV2WideCtypeFacet*>(self)->table;
}

const void* SceV2WideCtypeInfo(const GuestLocale::Facet* self) {
    return &reinterpret_cast<const SceV2WideCtypeFacet*>(self)->table;
}

const SceV2CtypeWideVtable& SceV2WideCtypeVirtuals(const GuestLocale::Facet* self) {
    return *reinterpret_cast<const SceV2CtypeWideVtable*>(self->vtable);
}

short SceV2WideCtypeMask(const GuestLocale::Facet* self, char16_t character) {
    const short* table = SceV2WideCtypeTable(self);
    return table[static_cast<unsigned char>(SceV2WideCtypeVirtuals(self).narrow(self, character, 0))];
}

bool APS5_VABI SceV2WideCtypeIs_nid_no_patch(const GuestLocale::Facet* self, short mask, char16_t character) {
    return (SceV2WideCtypeMask(self, character) & mask) != 0;
}

const char16_t* APS5_VABI SceV2WideCtypeIsRange_nid_no_patch(const GuestLocale::Facet* self, const char16_t* first, const char16_t* last, short* destination) {
    for (; first != last; ++first, ++destination) *destination = SceV2WideCtypeMask(self, *first);
    return first;
}

const char16_t* APS5_VABI SceV2WideCtypeScanIs_nid_no_patch(const GuestLocale::Facet* self, short mask, const char16_t* first, const char16_t* last) {
    while (first != last && !SceV2WideCtypeVirtuals(self).is(self, mask, *first)) ++first;
    return first;
}

const char16_t* APS5_VABI SceV2WideCtypeScanNot_nid_no_patch(const GuestLocale::Facet* self, short mask, const char16_t* first, const char16_t* last) {
    while (first != last && SceV2WideCtypeVirtuals(self).is(self, mask, *first)) ++first;
    return first;
}

char16_t APS5_VABI SceV2WideCtypeToLower_nid_no_patch(const GuestLocale::Facet* self, char16_t character) {
    return static_cast<char16_t>(_Towlower_nid_postfix(character, SceV2WideCtypeInfo(self)));
}

const char16_t* APS5_VABI SceV2WideCtypeToLowerRange_nid_no_patch(const GuestLocale::Facet* self, char16_t* first, const char16_t* last) {
    for (; first != last; ++first) *first = static_cast<char16_t>(_Towlower_nid_postfix(*first, SceV2WideCtypeInfo(self)));
    return first;
}

char16_t APS5_VABI SceV2WideCtypeToUpper_nid_no_patch(const GuestLocale::Facet* self, char16_t character) {
    return static_cast<char16_t>(_Towupper_nid_postfix(character, SceV2WideCtypeInfo(self)));
}

const char16_t* APS5_VABI SceV2WideCtypeToUpperRange_nid_no_patch(const GuestLocale::Facet* self, char16_t* first, const char16_t* last) {
    for (; first != last; ++first) *first = static_cast<char16_t>(_Towupper_nid_postfix(*first, SceV2WideCtypeInfo(self)));
    return first;
}

char16_t WidenSceV2Character(char character) {
    std::uint64_t state[2]{};
    std::uint16_t wide = 0;
    return static_cast<int>(mbrtowc_nid_postfix(&wide, &character, 1, state)) < 0 ? char16_t{0xffff} : static_cast<char16_t>(wide);
}

char NarrowSceV2Character(char16_t character, char fallback) {
    std::uint64_t state[2]{};
    char bytes[8]{};
    return static_cast<int>(wcrtomb_nid_postfix(bytes, character, state)) == 1 ? bytes[0] : fallback;
}

char16_t APS5_VABI SceV2WideCtypeWiden_nid_no_patch(const GuestLocale::Facet*, char character) {
    return WidenSceV2Character(character);
}

const char* APS5_VABI SceV2WideCtypeWidenRange_nid_no_patch(const GuestLocale::Facet*, const char* first, const char* last, char16_t* destination) {
    for (; first != last; ++first, ++destination) *destination = WidenSceV2Character(*first);
    return first;
}

char APS5_VABI SceV2WideCtypeNarrow_nid_no_patch(const GuestLocale::Facet*, char16_t character, char fallback) {
    return NarrowSceV2Character(character, fallback);
}

const char16_t* APS5_VABI SceV2WideCtypeNarrowRange_nid_no_patch(const GuestLocale::Facet*, const char16_t* first, const char16_t* last, char fallback, char* destination) {
    for (; first != last; ++first, ++destination) *destination = NarrowSceV2Character(*first, fallback);
    return first;
}

struct SceV2NumpunctWideVtable {
    GuestLocale::FacetVtable facet;
    char16_t (APS5_VABI *decimalPoint)(const GuestLocale::WideNumpunctFacet* self);
    char16_t (APS5_VABI *thousandsSep)(const GuestLocale::WideNumpunctFacet* self);
    SceV2String* (APS5_VABI *grouping)(SceV2String* result, const GuestLocale::WideNumpunctFacet* self);
    SceV2WideString* (APS5_VABI *falsename)(SceV2WideString* result, const GuestLocale::WideNumpunctFacet* self);
    SceV2WideString* (APS5_VABI *truename)(SceV2WideString* result, const GuestLocale::WideNumpunctFacet* self);
};

char16_t APS5_VABI SceV2WideNumpunctDecimalPoint_nid_no_patch(const GuestLocale::WideNumpunctFacet* self) {
    return self->decimalPoint;
}

char16_t APS5_VABI SceV2WideNumpunctThousandsSep_nid_no_patch(const GuestLocale::WideNumpunctFacet* self) {
    return self->thousandsSep;
}

SceV2String* APS5_VABI SceV2WideNumpunctGrouping_nid_no_patch(SceV2String* result, const GuestLocale::WideNumpunctFacet* self) {
    return AssignSceV2String(result, self->grouping);
}

SceV2WideString* APS5_VABI SceV2WideNumpunctFalsename_nid_no_patch(SceV2WideString* result, const GuestLocale::WideNumpunctFacet* self) {
    return AssignSceV2WideString(result, self->falsename);
}

SceV2WideString* APS5_VABI SceV2WideNumpunctTruename_nid_no_patch(SceV2WideString* result, const GuestLocale::WideNumpunctFacet* self) {
    return AssignSceV2WideString(result, self->truename);
}

bool APS5_VABI SceV2CodecvtAlwaysNoconv_nid_no_patch(const GuestLocale::Facet*) {
    return true;
}

int APS5_VABI SceV2CodecvtOne_nid_no_patch(const GuestLocale::Facet*) {
    return 1;
}

int APS5_VABI SceV2CodecvtConvert_nid_no_patch(GuestLocale::Facet*, std::uint64_t*, const char* first1, const char*, const char** next1, char* first3, char*, char** next3) {
    *next1 = first1;
    *next3 = first3;
    return 3;
}

int APS5_VABI SceV2CodecvtUnshift_nid_no_patch(GuestLocale::Facet*, std::uint64_t*, char* first3, char*, char** next3) {
    *next3 = first3;
    return 3;
}

std::size_t APS5_VABI SceV2CodecvtLength_nid_no_patch(GuestLocale::Facet*, std::uint64_t*, const char* first1, const char* last1, std::size_t count) {
    constexpr std::int64_t Limit = std::numeric_limits<std::int32_t>::max();
    const std::int64_t available = reinterpret_cast<std::intptr_t>(last1) - reinterpret_cast<std::intptr_t>(first1);
    const auto limited = static_cast<std::size_t>(available < Limit ? available : Limit);
    return limited >= count ? count : limited;
}

constexpr int SceV2CodecvtOk = 0;
constexpr int SceV2CodecvtPartial = 1;
constexpr int SceV2CodecvtError = 2;
constexpr std::size_t SceV2MbstateBytes = 12;
constexpr std::size_t SceV2MultibyteRoom = 6;

const SceV2Cvtvec& SceV2WideConversion(const GuestLocale::Facet* self) {
    return reinterpret_cast<const SceV2WideCodecvtFacet*>(self)->conversion;
}

bool APS5_VABI SceV2WideCodecvtAlwaysNoconv_nid_no_patch(const GuestLocale::Facet*) {
    return false;
}

int APS5_VABI SceV2WideCodecvtMaxLength_nid_no_patch(const GuestLocale::Facet* self) {
    return static_cast<int>(SceV2WideConversion(self).page);
}

int APS5_VABI SceV2WideCodecvtEncoding_nid_no_patch(const GuestLocale::Facet* self) {
    return SceV2WideConversion(self).page == 1 ? 1 : 0;
}

int APS5_VABI SceV2WideCodecvtIn_nid_no_patch(GuestLocale::Facet*, void*, const char* first1, const char* last1, const char** next1, char16_t* first2, char16_t* last2, char16_t** next2) {
    std::uint64_t state[2]{};
    *next1 = first1;
    *next2 = first2;
    while (*next1 != last1) {
        if (*next2 == last2) return SceV2CodecvtPartial;
        const auto converted = static_cast<int>(mbrtowc_nid_postfix(reinterpret_cast<std::uint16_t*>(*next2), *next1, static_cast<std::size_t>(last1 - *next1), state));
        if (converted == -2) return SceV2CodecvtPartial;
        if (converted == -1) return SceV2CodecvtError;
        *next1 += converted == 0 ? 1 : converted;
        ++*next2;
    }
    return SceV2CodecvtOk;
}

int APS5_VABI SceV2WideCodecvtOut_nid_no_patch(GuestLocale::Facet*, void* state, const char16_t* first1, const char16_t* last1, const char16_t** next1, char* first2, char* last2, char** next2) {
    *next1 = first1;
    *next2 = first2;
    while (*next1 != last1) {
        if (*next2 == last2) break;
        if (last2 - *next2 >= static_cast<std::ptrdiff_t>(SceV2MultibyteRoom)) {
            const auto written = static_cast<int>(wcrtomb_nid_postfix(*next2, **next1, state));
            if (written < 0) return SceV2CodecvtError;
            ++*next1;
            *next2 += written;
            continue;
        }
        unsigned char saved[SceV2MbstateBytes];
        std::memcpy(saved, state, sizeof(saved));
        char bytes[SceV2MultibyteRoom + 2];
        const auto written = static_cast<int>(wcrtomb_nid_postfix(bytes, **next1, state));
        if (written < 0) return SceV2CodecvtError;
        if (last2 - *next2 < written) {
            std::memcpy(state, saved, sizeof(saved));
            break;
        }
        std::memcpy(*next2, bytes, static_cast<std::size_t>(written));
        ++*next1;
        *next2 += written;
    }
    return *next1 != last1 ? SceV2CodecvtPartial : SceV2CodecvtOk;
}

int APS5_VABI SceV2WideCodecvtUnshift_nid_no_patch(GuestLocale::Facet*, void* state, char* first2, char* last2, char** next2) {
    *next2 = first2;
    unsigned char saved[SceV2MbstateBytes];
    std::memcpy(saved, state, sizeof(saved));
    char bytes[SceV2MultibyteRoom + 2];
    const auto written = static_cast<int>(wcrtomb_nid_postfix(bytes, 0, state));
    if (written <= 0) return SceV2CodecvtError;
    const std::ptrdiff_t shift = written - 1;
    if (last2 - *next2 < shift) {
        std::memcpy(state, saved, sizeof(saved));
        return SceV2CodecvtPartial;
    }
    if (written != 1) {
        std::memcpy(*next2, bytes, static_cast<std::size_t>(shift));
        *next2 += shift;
    }
    return SceV2CodecvtOk;
}

int APS5_VABI SceV2WideCodecvtLength_nid_no_patch(GuestLocale::Facet*, void* state, const char* first1, const char* last1, std::size_t count) {
    const char* next = first1;
    for (; count != 0 && next != last1; --count) {
        std::uint16_t wide = 0;
        const auto converted = static_cast<int>(mbrtowc_nid_postfix(&wide, next, static_cast<std::size_t>(last1 - next), state));
        if (converted < 0) break;
        next += converted == 0 ? 1 : converted;
    }
    const std::ptrdiff_t length = next - first1;
    return length < std::numeric_limits<std::int32_t>::max() ? static_cast<int>(length) : std::numeric_limits<std::int32_t>::max();
}

struct SceV2CodecvtWideVtable {
    GuestLocale::FacetVtable facet;
    bool (APS5_VABI *alwaysNoconv)(const GuestLocale::Facet* self);
    int (APS5_VABI *maxLength)(const GuestLocale::Facet* self);
    int (APS5_VABI *encoding)(const GuestLocale::Facet* self);
    int (APS5_VABI *in)(GuestLocale::Facet* self, void* state, const char* first1, const char* last1, const char** next1, char16_t* first2, char16_t* last2, char16_t** next2);
    int (APS5_VABI *out)(GuestLocale::Facet* self, void* state, const char16_t* first1, const char16_t* last1, const char16_t** next1, char* first2, char* last2, char** next2);
    int (APS5_VABI *unshift)(GuestLocale::Facet* self, void* state, char* first2, char* last2, char** next2);
    int (APS5_VABI *length)(GuestLocale::Facet* self, void* state, const char* first1, const char* last1, std::size_t count);
};

const SceV2CtypeCharVtable g_sceV2CtypeCharVtable{
    {DestroySceV2CtypeFacet_nid_no_patch, DeleteSceV2Facet_nid_no_patch<DestroySceV2CtypeFacet_nid_no_patch>, RetainFacet, ReleaseFacet},
    SceV2CtypeToLower_nid_no_patch, SceV2CtypeToLowerRange_nid_no_patch, SceV2CtypeToUpper_nid_no_patch, SceV2CtypeToUpperRange_nid_no_patch,
    SceV2CtypeWiden_nid_no_patch, SceV2CtypeWidenRange_nid_no_patch, SceV2CtypeNarrow_nid_no_patch, SceV2CtypeNarrowRange_nid_no_patch};
const SceV2NumpunctCharVtable g_sceV2NumpunctCharVtable{
    {DestroySceV2PlainFacet_nid_no_patch, DeleteSceV2Facet_nid_no_patch<DestroySceV2PlainFacet_nid_no_patch>, RetainFacet, ReleaseFacet},
    SceV2NumpunctDecimalPoint_nid_no_patch, SceV2NumpunctThousandsSep_nid_no_patch, SceV2NumpunctGrouping_nid_no_patch,
    SceV2NumpunctFalsename_nid_no_patch, SceV2NumpunctTruename_nid_no_patch};
const GuestLocale::CodecvtVtable g_sceV2CodecvtCharVtable{
    {DestroySceV2PlainFacet_nid_no_patch, DeleteSceV2Facet_nid_no_patch<DestroySceV2PlainFacet_nid_no_patch>, RetainFacet, ReleaseFacet},
    SceV2CodecvtAlwaysNoconv_nid_no_patch, SceV2CodecvtOne_nid_no_patch, SceV2CodecvtOne_nid_no_patch, SceV2CodecvtConvert_nid_no_patch,
    SceV2CodecvtConvert_nid_no_patch, SceV2CodecvtUnshift_nid_no_patch, SceV2CodecvtLength_nid_no_patch};
const SceV2CtypeWideVtable g_sceV2CtypeWideVtable{
    {DestroySceV2WideCtypeFacet_nid_no_patch, DeleteSceV2Facet_nid_no_patch<DestroySceV2WideCtypeFacet_nid_no_patch>, RetainFacet, ReleaseFacet},
    SceV2WideCtypeIs_nid_no_patch, SceV2WideCtypeIsRange_nid_no_patch, SceV2WideCtypeScanIs_nid_no_patch, SceV2WideCtypeScanNot_nid_no_patch,
    SceV2WideCtypeToLower_nid_no_patch, SceV2WideCtypeToLowerRange_nid_no_patch, SceV2WideCtypeToUpper_nid_no_patch, SceV2WideCtypeToUpperRange_nid_no_patch,
    SceV2WideCtypeWiden_nid_no_patch, SceV2WideCtypeWidenRange_nid_no_patch, SceV2WideCtypeNarrow_nid_no_patch, SceV2WideCtypeNarrowRange_nid_no_patch};
const SceV2CtypeWideVtable g_sceV2CtypeUshortVtable = g_sceV2CtypeWideVtable;
template<typename Text> struct SceV2InputIterator {
    GuestLocale::Streambuf* buffer;
    bool got;
    Text value;
};

static_assert(sizeof(SceV2InputIterator<char>) == 0x10);
static_assert(offsetof(SceV2InputIterator<char16_t>, value) == 0xa && sizeof(SceV2InputIterator<char16_t>) == 0x10);

template<typename Text> struct SceV2NumGetVtable {
    GuestLocale::FacetVtable facet;
    SceV2InputIterator<Text> (APS5_VABI *getBool)(const GuestLocale::Facet* self, SceV2InputIterator<Text> first, SceV2InputIterator<Text> last, GuestLocale::SceV2IosBase* stream, std::int32_t* state, bool* value);
    SceV2InputIterator<Text> (APS5_VABI *getUnsignedShort)(const GuestLocale::Facet* self, SceV2InputIterator<Text> first, SceV2InputIterator<Text> last, GuestLocale::SceV2IosBase* stream, std::int32_t* state, std::uint16_t* value);
    SceV2InputIterator<Text> (APS5_VABI *getUnsignedInt)(const GuestLocale::Facet* self, SceV2InputIterator<Text> first, SceV2InputIterator<Text> last, GuestLocale::SceV2IosBase* stream, std::int32_t* state, std::uint32_t* value);
    SceV2InputIterator<Text> (APS5_VABI *getLong)(const GuestLocale::Facet* self, SceV2InputIterator<Text> first, SceV2InputIterator<Text> last, GuestLocale::SceV2IosBase* stream, std::int32_t* state, std::int64_t* value);
    SceV2InputIterator<Text> (APS5_VABI *getUnsignedLong)(const GuestLocale::Facet* self, SceV2InputIterator<Text> first, SceV2InputIterator<Text> last, GuestLocale::SceV2IosBase* stream, std::int32_t* state, std::uint64_t* value);
    SceV2InputIterator<Text> (APS5_VABI *getLongLong)(const GuestLocale::Facet* self, SceV2InputIterator<Text> first, SceV2InputIterator<Text> last, GuestLocale::SceV2IosBase* stream, std::int32_t* state, std::int64_t* value);
    SceV2InputIterator<Text> (APS5_VABI *getUnsignedLongLong)(const GuestLocale::Facet* self, SceV2InputIterator<Text> first, SceV2InputIterator<Text> last, GuestLocale::SceV2IosBase* stream, std::int32_t* state, std::uint64_t* value);
    void (APS5_VABI *getFloating[3])();
    SceV2InputIterator<Text> (APS5_VABI *getPointer)(const GuestLocale::Facet* self, SceV2InputIterator<Text> first, SceV2InputIterator<Text> last, GuestLocale::SceV2IosBase* stream, std::int32_t* state, void** value);
};

template<typename Text> SceV2InputIterator<Text> APS5_VABI SceV2NumGetBool_nid_no_patch(const GuestLocale::Facet* self, SceV2InputIterator<Text> first, SceV2InputIterator<Text> last, GuestLocale::SceV2IosBase* stream, std::int32_t* state, bool* value);
template<typename Text> SceV2InputIterator<Text> APS5_VABI SceV2NumGetUnsignedShort_nid_no_patch(const GuestLocale::Facet* self, SceV2InputIterator<Text> first, SceV2InputIterator<Text> last, GuestLocale::SceV2IosBase* stream, std::int32_t* state, std::uint16_t* value);
template<typename Text> SceV2InputIterator<Text> APS5_VABI SceV2NumGetUnsignedInt_nid_no_patch(const GuestLocale::Facet* self, SceV2InputIterator<Text> first, SceV2InputIterator<Text> last, GuestLocale::SceV2IosBase* stream, std::int32_t* state, std::uint32_t* value);
template<typename Text> SceV2InputIterator<Text> APS5_VABI SceV2NumGetLong_nid_no_patch(const GuestLocale::Facet* self, SceV2InputIterator<Text> first, SceV2InputIterator<Text> last, GuestLocale::SceV2IosBase* stream, std::int32_t* state, std::int64_t* value);
template<typename Text> SceV2InputIterator<Text> APS5_VABI SceV2NumGetUnsignedLong_nid_no_patch(const GuestLocale::Facet* self, SceV2InputIterator<Text> first, SceV2InputIterator<Text> last, GuestLocale::SceV2IosBase* stream, std::int32_t* state, std::uint64_t* value);
template<typename Text> SceV2InputIterator<Text> APS5_VABI SceV2NumGetLongLong_nid_no_patch(const GuestLocale::Facet* self, SceV2InputIterator<Text> first, SceV2InputIterator<Text> last, GuestLocale::SceV2IosBase* stream, std::int32_t* state, std::int64_t* value);
template<typename Text> SceV2InputIterator<Text> APS5_VABI SceV2NumGetUnsignedLongLong_nid_no_patch(const GuestLocale::Facet* self, SceV2InputIterator<Text> first, SceV2InputIterator<Text> last, GuestLocale::SceV2IosBase* stream, std::int32_t* state, std::uint64_t* value);
template<typename Text> SceV2InputIterator<Text> APS5_VABI SceV2NumGetPointer_nid_no_patch(const GuestLocale::Facet* self, SceV2InputIterator<Text> first, SceV2InputIterator<Text> last, GuestLocale::SceV2IosBase* stream, std::int32_t* state, void** value);
template<typename Text> SceV2InputIterator<Text> APS5_VABI SceV2NumGetFloat_nid_no_patch(const GuestLocale::Facet* self, SceV2InputIterator<Text> first, SceV2InputIterator<Text> last, GuestLocale::SceV2IosBase* stream, std::int32_t* state, float* value);
template<typename Text> SceV2InputIterator<Text> APS5_VABI SceV2NumGetDouble_nid_no_patch(const GuestLocale::Facet* self, SceV2InputIterator<Text> first, SceV2InputIterator<Text> last, GuestLocale::SceV2IosBase* stream, std::int32_t* state, double* value);
template<typename Text> SceV2InputIterator<Text> APS5_VABI SceV2NumGetLongDouble_nid_no_patch(const GuestLocale::Facet* self, SceV2InputIterator<Text> first, SceV2InputIterator<Text> last, GuestLocale::SceV2IosBase* stream, std::int32_t* state, long double* value);

template<typename Text> auto SceV2NumGetFloatingSlots() {
    using Virtual = void (APS5_VABI*)();
    return std::array<Virtual, 3>{reinterpret_cast<Virtual>(&SceV2NumGetFloat_nid_no_patch<Text>), reinterpret_cast<Virtual>(&SceV2NumGetDouble_nid_no_patch<Text>), reinterpret_cast<Virtual>(&SceV2NumGetLongDouble_nid_no_patch<Text>)};
}

template<typename Text> const SceV2NumGetVtable<Text> g_sceV2NumGetVtable{
    {DestroySceV2PlainFacet_nid_no_patch, DeleteSceV2Facet_nid_no_patch<DestroySceV2PlainFacet_nid_no_patch>, RetainFacet, ReleaseFacet},
    SceV2NumGetBool_nid_no_patch<Text>, SceV2NumGetUnsignedShort_nid_no_patch<Text>, SceV2NumGetUnsignedInt_nid_no_patch<Text>, SceV2NumGetLong_nid_no_patch<Text>,
    SceV2NumGetUnsignedLong_nid_no_patch<Text>, SceV2NumGetLongLong_nid_no_patch<Text>, SceV2NumGetUnsignedLongLong_nid_no_patch<Text>,
    {SceV2NumGetFloatingSlots<Text>()[0], SceV2NumGetFloatingSlots<Text>()[1], SceV2NumGetFloatingSlots<Text>()[2]},
    SceV2NumGetPointer_nid_no_patch<Text>};
template<typename Text> struct SceV2NumPutVtable {
    GuestLocale::FacetVtable facet;
    GuestLocale::OutputIterator (APS5_VABI *putBool)(const GuestLocale::Facet* self, GuestLocale::OutputIterator out, GuestLocale::SceV2IosBase* stream, Text fill, unsigned char value);
    GuestLocale::OutputIterator (APS5_VABI *putLong)(const GuestLocale::Facet* self, GuestLocale::OutputIterator out, GuestLocale::SceV2IosBase* stream, Text fill, std::int64_t value);
    GuestLocale::OutputIterator (APS5_VABI *putUnsignedLong)(const GuestLocale::Facet* self, GuestLocale::OutputIterator out, GuestLocale::SceV2IosBase* stream, Text fill, std::uint64_t value);
    GuestLocale::OutputIterator (APS5_VABI *putLongLong)(const GuestLocale::Facet* self, GuestLocale::OutputIterator out, GuestLocale::SceV2IosBase* stream, Text fill, std::int64_t value);
    GuestLocale::OutputIterator (APS5_VABI *putUnsignedLongLong)(const GuestLocale::Facet* self, GuestLocale::OutputIterator out, GuestLocale::SceV2IosBase* stream, Text fill, std::uint64_t value);
    GuestLocale::OutputIterator (APS5_VABI *putDouble)(const GuestLocale::Facet* self, GuestLocale::OutputIterator out, GuestLocale::SceV2IosBase* stream, Text fill, double value);
    GuestLocale::OutputIterator (APS5_VABI *putLongDouble)(const GuestLocale::Facet* self, GuestLocale::OutputIterator out, GuestLocale::SceV2IosBase* stream, Text fill, long double value);
    GuestLocale::OutputIterator (APS5_VABI *putPointer)(const GuestLocale::Facet* self, GuestLocale::OutputIterator out, GuestLocale::SceV2IosBase* stream, Text fill, const void* value);
};

template<typename Text> GuestLocale::OutputIterator APS5_VABI SceV2NumPutBool_nid_no_patch(const GuestLocale::Facet* self, GuestLocale::OutputIterator out, GuestLocale::SceV2IosBase* stream, Text fill, unsigned char value);
template<typename Text> GuestLocale::OutputIterator APS5_VABI SceV2NumPutLong_nid_no_patch(const GuestLocale::Facet* self, GuestLocale::OutputIterator out, GuestLocale::SceV2IosBase* stream, Text fill, std::int64_t value);
template<typename Text> GuestLocale::OutputIterator APS5_VABI SceV2NumPutUnsignedLong_nid_no_patch(const GuestLocale::Facet* self, GuestLocale::OutputIterator out, GuestLocale::SceV2IosBase* stream, Text fill, std::uint64_t value);
template<typename Text> GuestLocale::OutputIterator APS5_VABI SceV2NumPutLongLong_nid_no_patch(const GuestLocale::Facet* self, GuestLocale::OutputIterator out, GuestLocale::SceV2IosBase* stream, Text fill, std::int64_t value);
template<typename Text> GuestLocale::OutputIterator APS5_VABI SceV2NumPutUnsignedLongLong_nid_no_patch(const GuestLocale::Facet* self, GuestLocale::OutputIterator out, GuestLocale::SceV2IosBase* stream, Text fill, std::uint64_t value);
template<typename Text> GuestLocale::OutputIterator APS5_VABI SceV2NumPutDouble_nid_no_patch(const GuestLocale::Facet* self, GuestLocale::OutputIterator out, GuestLocale::SceV2IosBase* stream, Text fill, double value);
template<typename Text> GuestLocale::OutputIterator APS5_VABI SceV2NumPutLongDouble_nid_no_patch(const GuestLocale::Facet* self, GuestLocale::OutputIterator out, GuestLocale::SceV2IosBase* stream, Text fill, long double value);
template<typename Text> GuestLocale::OutputIterator APS5_VABI SceV2NumPutPointer_nid_no_patch(const GuestLocale::Facet* self, GuestLocale::OutputIterator out, GuestLocale::SceV2IosBase* stream, Text fill, const void* value);

template<typename Text> const SceV2NumPutVtable<Text> g_sceV2NumPutVtable{
    {DestroySceV2PlainFacet_nid_no_patch, DeleteSceV2Facet_nid_no_patch<DestroySceV2PlainFacet_nid_no_patch>, RetainFacet, ReleaseFacet},
    SceV2NumPutBool_nid_no_patch<Text>, SceV2NumPutLong_nid_no_patch<Text>, SceV2NumPutUnsignedLong_nid_no_patch<Text>, SceV2NumPutLongLong_nid_no_patch<Text>,
    SceV2NumPutUnsignedLongLong_nid_no_patch<Text>, SceV2NumPutDouble_nid_no_patch<Text>, SceV2NumPutLongDouble_nid_no_patch<Text>, SceV2NumPutPointer_nid_no_patch<Text>};
const SceV2NumpunctWideVtable g_sceV2NumpunctWideVtable{
    {DestroySceV2PlainFacet_nid_no_patch, DeleteSceV2Facet_nid_no_patch<DestroySceV2PlainFacet_nid_no_patch>, RetainFacet, ReleaseFacet},
    SceV2WideNumpunctDecimalPoint_nid_no_patch, SceV2WideNumpunctThousandsSep_nid_no_patch, SceV2WideNumpunctGrouping_nid_no_patch,
    SceV2WideNumpunctFalsename_nid_no_patch, SceV2WideNumpunctTruename_nid_no_patch};
const SceV2NumpunctWideVtable g_sceV2NumpunctUshortVtable{
    {DestroySceV2UshortNumpunctFacet_nid_no_patch, DeleteSceV2Facet_nid_no_patch<DestroySceV2UshortNumpunctFacet_nid_no_patch>, RetainFacet, ReleaseFacet},
    SceV2WideNumpunctDecimalPoint_nid_no_patch, SceV2WideNumpunctThousandsSep_nid_no_patch, SceV2WideNumpunctGrouping_nid_no_patch,
    SceV2WideNumpunctFalsename_nid_no_patch, SceV2WideNumpunctTruename_nid_no_patch};
const SceV2CodecvtWideVtable g_sceV2CodecvtWideVtable{
    {DestroySceV2PlainFacet_nid_no_patch, DeleteSceV2Facet_nid_no_patch<DestroySceV2PlainFacet_nid_no_patch>, RetainFacet, ReleaseFacet},
    SceV2WideCodecvtAlwaysNoconv_nid_no_patch, SceV2WideCodecvtMaxLength_nid_no_patch, SceV2WideCodecvtEncoding_nid_no_patch, SceV2WideCodecvtIn_nid_no_patch,
    SceV2WideCodecvtOut_nid_no_patch, SceV2WideCodecvtUnshift_nid_no_patch, SceV2WideCodecvtLength_nid_no_patch};
const SceV2CodecvtWideVtable g_sceV2CodecvtUshortVtable = g_sceV2CodecvtWideVtable;
template<typename Text> struct SceV2CollateVtable {
    GuestLocale::FacetVtable facet;
    int (APS5_VABI *compare)(const GuestLocale::Facet* self, const Text* first1, const Text* last1, const Text* first2, const Text* last2);
    SceV2BasicString<Text>* (APS5_VABI *transform)(SceV2BasicString<Text>* result, const GuestLocale::Facet* self, const Text* first, const Text* last);
    std::int64_t (APS5_VABI *hash)(const GuestLocale::Facet* self, const Text* first, const Text* last);
};

template<typename Text> std::size_t SceV2CollationLength(const Text* first, const Text* last) {
    return static_cast<std::size_t>(reinterpret_cast<const char*>(last) - reinterpret_cast<const char*>(first)) / sizeof(Text);
}

template<typename Text> int APS5_VABI SceV2CollateCompare_nid_no_patch(const GuestLocale::Facet*, const Text* first1, const Text* last1, const Text* first2, const Text* last2) {
    using Unit = std::make_unsigned_t<Text>;
    const std::size_t length1 = SceV2CollationLength(first1, last1);
    const std::size_t length2 = SceV2CollationLength(first2, last2);
    const std::size_t common = std::min(length1, length2);
    for (std::size_t index = 0; index < common; ++index) {
        const auto left = static_cast<Unit>(first1[index]);
        const auto right = static_cast<Unit>(first2[index]);
        if (left != right) return left < right ? -1 : 1;
    }
    if (length1 == length2) return 0;
    return length1 < length2 ? -1 : 1;
}

template<typename Text> SceV2BasicString<Text>* APS5_VABI SceV2CollateTransform_nid_no_patch(SceV2BasicString<Text>* result, const GuestLocale::Facet*, const Text* first, const Text* last) {
    const auto bytes = static_cast<std::size_t>(reinterpret_cast<const char*>(last) - reinterpret_cast<const char*>(first));
    const std::size_t length = bytes / sizeof(Text);
    Text* data = ReserveSceV2String(result, length);
    if (bytes % sizeof(Text) != 0) std::fill_n(data, length, Text{});
    else if (length != 0) std::memcpy(data, first, bytes);
    return result;
}

template<typename Text> std::int64_t APS5_VABI SceV2CollateHash_nid_no_patch(const GuestLocale::Facet*, const Text* first, const Text* last) {
    const auto* byte = reinterpret_cast<const unsigned char*>(first);
    const auto* end = reinterpret_cast<const unsigned char*>(last);
    std::uint64_t hash = 0xcbf29ce484222325;
    for (; byte != end; ++byte) hash = (hash ^ *byte) * 0x100000001b3;
    return static_cast<std::int64_t>(hash);
}

template<typename Text> const SceV2CollateVtable<Text> g_sceV2CollateVtable{
    {DestroySceV2PlainFacet_nid_no_patch, DeleteSceV2Facet_nid_no_patch<DestroySceV2PlainFacet_nid_no_patch>, RetainFacet, ReleaseFacet},
    SceV2CollateCompare_nid_no_patch<Text>, SceV2CollateTransform_nid_no_patch<Text>, SceV2CollateHash_nid_no_patch<Text>};

int APS5_VABI SceV2UshortCollateCompare_nid_no_patch(const GuestLocale::Facet*, const std::uint16_t* first1, const std::uint16_t* last1, const std::uint16_t* first2, const std::uint16_t* last2) {
    for (; first1 != last1 && first2 != last2; ++first1, ++first2) {
        if (*first1 < *first2) return -1;
        if (*first1 > *first2) return 1;
    }
    if (first2 != last2) return -1;
    return first1 != last1 ? 1 : 0;
}

const SceV2CollateVtable<std::uint16_t> g_sceV2CollateUshortVtable{
    {DestroySceV2PlainFacet_nid_no_patch, DeleteSceV2Facet_nid_no_patch<DestroySceV2PlainFacet_nid_no_patch>, RetainFacet, ReleaseFacet},
    SceV2UshortCollateCompare_nid_no_patch, SceV2CollateTransform_nid_no_patch<std::uint16_t>, SceV2CollateHash_nid_no_patch<std::uint16_t>};

template<typename Text> using SceV2MoneyString = std::conditional_t<std::is_same_v<Text, char>, SceV2String, SceV2WideString>;

template<typename Text> struct SceV2MoneypunctVtable {
    GuestLocale::FacetVtable facet;
    Text (APS5_VABI *decimalPoint)(const SceV2MoneypunctFacet<Text>* self);
    Text (APS5_VABI *thousandsSep)(const SceV2MoneypunctFacet<Text>* self);
    SceV2String* (APS5_VABI *grouping)(SceV2String* result, const SceV2MoneypunctFacet<Text>* self);
    SceV2MoneyString<Text>* (APS5_VABI *currencySymbol)(SceV2MoneyString<Text>* result, const SceV2MoneypunctFacet<Text>* self);
    SceV2MoneyString<Text>* (APS5_VABI *positiveSign)(SceV2MoneyString<Text>* result, const SceV2MoneypunctFacet<Text>* self);
    SceV2MoneyString<Text>* (APS5_VABI *negativeSign)(SceV2MoneyString<Text>* result, const SceV2MoneypunctFacet<Text>* self);
    int (APS5_VABI *fractionDigits)(const SceV2MoneypunctFacet<Text>* self);
    std::uint32_t (APS5_VABI *positiveFormat)(const SceV2MoneypunctFacet<Text>* self);
    std::uint32_t (APS5_VABI *negativeFormat)(const SceV2MoneypunctFacet<Text>* self);
};

SceV2String* AssignSceV2MoneyText(SceV2String* result, const char* text) {
    return AssignSceV2String(result, text);
}

SceV2WideString* AssignSceV2MoneyText(SceV2WideString* result, const std::uint16_t* text) {
    return AssignSceV2WideString(result, text);
}

template<typename Text> Text APS5_VABI SceV2MoneypunctDecimalPoint_nid_no_patch(const SceV2MoneypunctFacet<Text>* self) {
    return self->decimalPoint;
}

template<typename Text> Text APS5_VABI SceV2MoneypunctThousandsSep_nid_no_patch(const SceV2MoneypunctFacet<Text>* self) {
    return self->thousandsSep;
}

template<typename Text> SceV2String* APS5_VABI SceV2MoneypunctGrouping_nid_no_patch(SceV2String* result, const SceV2MoneypunctFacet<Text>* self) {
    return AssignSceV2String(result, self->grouping);
}

template<typename Text> SceV2MoneyString<Text>* APS5_VABI SceV2MoneypunctCurrencySymbol_nid_no_patch(SceV2MoneyString<Text>* result, const SceV2MoneypunctFacet<Text>* self) {
    return AssignSceV2MoneyText(result, self->currency);
}

template<typename Text> SceV2MoneyString<Text>* APS5_VABI SceV2MoneypunctPositiveSign_nid_no_patch(SceV2MoneyString<Text>* result, const SceV2MoneypunctFacet<Text>* self) {
    return AssignSceV2MoneyText(result, self->positiveSign);
}

template<typename Text> SceV2MoneyString<Text>* APS5_VABI SceV2MoneypunctNegativeSign_nid_no_patch(SceV2MoneyString<Text>* result, const SceV2MoneypunctFacet<Text>* self) {
    return AssignSceV2MoneyText(result, self->negativeSign);
}

template<typename Text> int APS5_VABI SceV2MoneypunctFractionDigits_nid_no_patch(const SceV2MoneypunctFacet<Text>* self) {
    return self->fractionDigits;
}

template<typename Text> std::uint32_t APS5_VABI SceV2MoneypunctPositiveFormat_nid_no_patch(const SceV2MoneypunctFacet<Text>* self) {
    std::uint32_t pattern;
    std::memcpy(&pattern, self->positivePattern, sizeof(pattern));
    return pattern;
}

template<typename Text> std::uint32_t APS5_VABI SceV2MoneypunctNegativeFormat_nid_no_patch(const SceV2MoneypunctFacet<Text>* self) {
    std::uint32_t pattern;
    std::memcpy(&pattern, self->negativePattern, sizeof(pattern));
    return pattern;
}

template<typename Text, SceV2FacetDestroy Destroy = DestroySceV2PlainFacet_nid_no_patch> const SceV2MoneypunctVtable<Text> g_sceV2MoneypunctVtable{
    {Destroy, DeleteSceV2Facet_nid_no_patch<Destroy>, RetainFacet, ReleaseFacet},
    SceV2MoneypunctDecimalPoint_nid_no_patch<Text>, SceV2MoneypunctThousandsSep_nid_no_patch<Text>, SceV2MoneypunctGrouping_nid_no_patch<Text>,
    SceV2MoneypunctCurrencySymbol_nid_no_patch<Text>, SceV2MoneypunctPositiveSign_nid_no_patch<Text>, SceV2MoneypunctNegativeSign_nid_no_patch<Text>,
    SceV2MoneypunctFractionDigits_nid_no_patch<Text>, SceV2MoneypunctPositiveFormat_nid_no_patch<Text>, SceV2MoneypunctNegativeFormat_nid_no_patch<Text>};

template<typename Text> struct SceV2MessagesVtable {
    GuestLocale::FacetVtable facet;
    int (APS5_VABI *open)(const GuestLocale::Facet* self, const SceV2String* name, const void* locale);
    SceV2BasicString<Text>* (APS5_VABI *get)(SceV2BasicString<Text>* result, const GuestLocale::Facet* self, int catalog, int set, int message, const SceV2BasicString<Text>* fallback);
    void (APS5_VABI *close)(const GuestLocale::Facet* self, int catalog);
};

int APS5_VABI SceV2MessagesOpen_nid_no_patch(const GuestLocale::Facet*, const SceV2String*, const void*) {
    return -1;
}

template<typename Text> SceV2BasicString<Text>* APS5_VABI SceV2MessagesGet_nid_no_patch(SceV2BasicString<Text>* result, const GuestLocale::Facet*, int, int, int, const SceV2BasicString<Text>* fallback) {
    constexpr std::size_t InlineCapacity = SceV2BasicString<Text>::InlineCapacity;
    const Text* source = fallback->capacity > InlineCapacity ? fallback->pointer : fallback->buffer;
    const std::size_t length = fallback->size;
    Text* data = ReserveSceV2String(result, length);
    std::memcpy(data, source, length > InlineCapacity ? (length + 1) * sizeof(Text) : sizeof(result->buffer));
    return result;
}

void APS5_VABI SceV2MessagesClose_nid_no_patch(const GuestLocale::Facet*, int) {}

template<typename Text> const SceV2MessagesVtable<Text> g_sceV2MessagesVtable{
    {DestroySceV2PlainFacet_nid_no_patch, DeleteSceV2Facet_nid_no_patch<DestroySceV2PlainFacet_nid_no_patch>, RetainFacet, ReleaseFacet},
    SceV2MessagesOpen_nid_no_patch, SceV2MessagesGet_nid_no_patch<Text>, SceV2MessagesClose_nid_no_patch};

template<typename Text> struct SceV2MoneyGetVtable {
    GuestLocale::FacetVtable facet;
    SceV2InputIterator<Text> (APS5_VABI *getLongDouble)(const GuestLocale::Facet* self, SceV2InputIterator<Text> first, SceV2InputIterator<Text> last, std::int32_t international, GuestLocale::SceV2IosBase* stream, std::int32_t* state, long double* value);
    SceV2InputIterator<Text> (APS5_VABI *getDigits)(const GuestLocale::Facet* self, SceV2InputIterator<Text> first, SceV2InputIterator<Text> last, std::int32_t international, GuestLocale::SceV2IosBase* stream, std::int32_t* state, SceV2BasicString<Text>* value);
};

template<typename Text> SceV2InputIterator<Text> APS5_VABI SceV2MoneyGetLongDouble_nid_no_patch(const GuestLocale::Facet* self, SceV2InputIterator<Text> first, SceV2InputIterator<Text> last, std::int32_t international, GuestLocale::SceV2IosBase* stream, std::int32_t* state, long double* value);
template<typename Text> SceV2InputIterator<Text> APS5_VABI SceV2MoneyGetDigits_nid_no_patch(const GuestLocale::Facet* self, SceV2InputIterator<Text> first, SceV2InputIterator<Text> last, std::int32_t international, GuestLocale::SceV2IosBase* stream, std::int32_t* state, SceV2BasicString<Text>* value);

template<typename Text> const SceV2MoneyGetVtable<Text> g_sceV2MoneyGetVtable{
    {DestroySceV2PlainFacet_nid_no_patch, DeleteSceV2Facet_nid_no_patch<DestroySceV2PlainFacet_nid_no_patch>, RetainFacet, ReleaseFacet},
    SceV2MoneyGetLongDouble_nid_no_patch<Text>, SceV2MoneyGetDigits_nid_no_patch<Text>};
template<typename Text> struct SceV2MoneyPutVtable {
    GuestLocale::FacetVtable facet;
    GuestLocale::OutputIterator (APS5_VABI *putLongDouble)(const GuestLocale::Facet* self, GuestLocale::OutputIterator out, bool international, GuestLocale::SceV2IosBase* stream, Text fill, long double value);
    GuestLocale::OutputIterator (APS5_VABI *putDigits)(const GuestLocale::Facet* self, GuestLocale::OutputIterator out, bool international, GuestLocale::SceV2IosBase* stream, Text fill, const SceV2BasicString<Text>* digits);
};

template<typename Text> GuestLocale::OutputIterator APS5_VABI SceV2MoneyPutLongDouble_nid_no_patch(const GuestLocale::Facet* self, GuestLocale::OutputIterator out, bool international, GuestLocale::SceV2IosBase* stream, Text fill, long double value);
template<typename Text> GuestLocale::OutputIterator APS5_VABI SceV2MoneyPutDigits_nid_no_patch(const GuestLocale::Facet* self, GuestLocale::OutputIterator out, bool international, GuestLocale::SceV2IosBase* stream, Text fill, const SceV2BasicString<Text>* digits);

template<typename Text> const SceV2MoneyPutVtable<Text> g_sceV2MoneyPutVtable{
    {DestroySceV2PlainFacet_nid_no_patch, DeleteSceV2Facet_nid_no_patch<DestroySceV2PlainFacet_nid_no_patch>, RetainFacet, ReleaseFacet},
    SceV2MoneyPutLongDouble_nid_no_patch<Text>, SceV2MoneyPutDigits_nid_no_patch<Text>};
template<typename Text> struct SceV2TimeGetVtable {
    using Get = SceV2InputIterator<Text> (APS5_VABI *)(const GuestLocale::Facet* self, SceV2InputIterator<Text> first, SceV2InputIterator<Text> last, GuestLocale::SceV2IosBase* stream, std::int32_t* state, GuestTm* time);
    GuestLocale::FacetVtable facet;
    std::int32_t (APS5_VABI *dateOrder)(const GuestLocale::Facet* self);
    Get getTime;
    Get getDate;
    Get getWeekday;
    Get getMonthname;
    Get getYear;
    SceV2InputIterator<Text> (APS5_VABI *get)(const GuestLocale::Facet* self, SceV2InputIterator<Text> first, SceV2InputIterator<Text> last, GuestLocale::SceV2IosBase* stream, std::int32_t* state, GuestTm* time, char specifier, char modifier);
};

std::int32_t APS5_VABI SceV2TimeGetDateOrder_nid_no_patch(const GuestLocale::Facet* self);
template<typename Text> SceV2InputIterator<Text> APS5_VABI SceV2TimeGetTime_nid_no_patch(const GuestLocale::Facet* self, SceV2InputIterator<Text> first, SceV2InputIterator<Text> last, GuestLocale::SceV2IosBase* stream, std::int32_t* state, GuestTm* time);
template<typename Text> SceV2InputIterator<Text> APS5_VABI SceV2TimeGetDate_nid_no_patch(const GuestLocale::Facet* self, SceV2InputIterator<Text> first, SceV2InputIterator<Text> last, GuestLocale::SceV2IosBase* stream, std::int32_t* state, GuestTm* time);
template<typename Text> SceV2InputIterator<Text> APS5_VABI SceV2TimeGetWeekday_nid_no_patch(const GuestLocale::Facet* self, SceV2InputIterator<Text> first, SceV2InputIterator<Text> last, GuestLocale::SceV2IosBase* stream, std::int32_t* state, GuestTm* time);
template<typename Text> SceV2InputIterator<Text> APS5_VABI SceV2TimeGetMonthname_nid_no_patch(const GuestLocale::Facet* self, SceV2InputIterator<Text> first, SceV2InputIterator<Text> last, GuestLocale::SceV2IosBase* stream, std::int32_t* state, GuestTm* time);
template<typename Text> SceV2InputIterator<Text> APS5_VABI SceV2TimeGetYear_nid_no_patch(const GuestLocale::Facet* self, SceV2InputIterator<Text> first, SceV2InputIterator<Text> last, GuestLocale::SceV2IosBase* stream, std::int32_t* state, GuestTm* time);
template<typename Text> SceV2InputIterator<Text> APS5_VABI SceV2TimeGetField_nid_no_patch(const GuestLocale::Facet* self, SceV2InputIterator<Text> first, SceV2InputIterator<Text> last, GuestLocale::SceV2IosBase* stream, std::int32_t* state, GuestTm* time, char specifier, char modifier);

template<typename Text, SceV2FacetDestroy Destroy = DestroySceV2PlainFacet_nid_no_patch> const SceV2TimeGetVtable<Text> g_sceV2TimeGetVtable{
    {Destroy, DeleteSceV2Facet_nid_no_patch<Destroy>, RetainFacet, ReleaseFacet},
    SceV2TimeGetDateOrder_nid_no_patch, SceV2TimeGetTime_nid_no_patch<Text>, SceV2TimeGetDate_nid_no_patch<Text>, SceV2TimeGetWeekday_nid_no_patch<Text>,
    SceV2TimeGetMonthname_nid_no_patch<Text>, SceV2TimeGetYear_nid_no_patch<Text>, SceV2TimeGetField_nid_no_patch<Text>};
template<typename Text> struct SceV2TimePutVtable {
    GuestLocale::FacetVtable facet;
    GuestLocale::OutputIterator (APS5_VABI *put)(const GuestLocale::Facet* self, GuestLocale::OutputIterator out, GuestLocale::SceV2IosBase* stream, Text fill, const GuestTm* time, char specifier, char modifier);
};

template<typename Text> GuestLocale::OutputIterator APS5_VABI SceV2TimePut_nid_no_patch(const GuestLocale::Facet* self, GuestLocale::OutputIterator out, GuestLocale::SceV2IosBase* stream, Text fill, const GuestTm* time, char specifier, char modifier);

template<typename Text> const SceV2TimePutVtable<Text> g_sceV2TimePutVtable{
    {DestroySceV2TimePutFacet_nid_no_patch, DeleteSceV2Facet_nid_no_patch<DestroySceV2TimePutFacet_nid_no_patch>, RetainFacet, ReleaseFacet},
    SceV2TimePut_nid_no_patch<Text>};

constexpr char g_sceV2Empty[] = "";
constexpr char g_sceV2FalseName[] = "false";
constexpr char g_sceV2TrueName[] = "true";
constexpr char g_sceV2Minus[] = "-";
constexpr char g_sceV2Days[] = ":Sun:Sunday:Mon:Monday:Tue:Tuesday:Wed:Wednesday:Thu:Thursday:Fri:Friday:Sat:Saturday";
constexpr char g_sceV2Months[] = ":Jan:January:Feb:February:Mar:March:Apr:April:May:May:Jun:June:Jul:July:Aug:August:Sep:September:Oct:October:Nov:November:Dec:December";
constexpr char g_sceV2AmPm[] = ":AM:am:PM:pm";
constexpr char g_sceV2DefaultMoneyPattern[] = "$+xv";
constexpr auto g_sceV2WideEmpty = WidenSceV2Text(g_sceV2Empty);
constexpr auto g_sceV2WideFalseName = WidenSceV2Text(g_sceV2FalseName);
constexpr auto g_sceV2WideTrueName = WidenSceV2Text(g_sceV2TrueName);
constexpr auto g_sceV2WideMinus = WidenSceV2Text(g_sceV2Minus);
constexpr auto g_sceV2WideDays = WidenSceV2Text(g_sceV2Days);
constexpr auto g_sceV2WideMonths = WidenSceV2Text(g_sceV2Months);
constexpr auto g_sceV2WideAmPm = WidenSceV2Text(g_sceV2AmPm);

constexpr std::array<char, 4> MakeSceV2MoneyPattern(unsigned csPrecedes, unsigned sepBySpace, unsigned signPosn) {
    const char first = csPrecedes ? '$' : 'v';
    const char second = csPrecedes ? 'v' : '$';
    std::array<char, 3> items{};
    if (signPosn < 2) items = {'+', first, second};
    else if (signPosn == 2) items = {first, second, '+'};
    else if (signPosn == 3) items = csPrecedes ? std::array<char, 3>{'+', '$', 'v'} : std::array<char, 3>{'v', '+', '$'};
    else items = csPrecedes ? std::array<char, 3>{'$', '+', 'v'} : std::array<char, 3>{'v', '$', '+'};
    std::size_t anchor = 0;
    std::size_t symbol = 0;
    for (std::size_t i = 0; i < items.size(); ++i) {
        if (items[i] == (sepBySpace == 1 ? 'v' : '+')) anchor = i;
        if (items[i] == '$') symbol = i;
    }
    const std::size_t gap = sepBySpace == 0 ? items.size() : anchor < symbol ? anchor + 1 : anchor;
    const char filler = sepBySpace == 0 || (sepBySpace == 2 && signPosn == 0) ? 'x' : ' ';
    std::array<char, 4> pattern{};
    for (std::size_t i = 0, j = 0; i < pattern.size(); ++i) pattern[i] = i == gap ? filler : items[j++];
    return pattern;
}

}

void* GuestLocale::AllocateStringStorage(std::size_t bytes) {
    return GuestStringStorage::Allocate(bytes);
}

void GuestLocale::ReleaseStringStorage(String& string) {
    if (string.capacity > StringInlineCapacity) GuestStringStorage::Deallocate(string.pointer, string.capacity + 1);
    string = {};
    string.capacity = StringInlineCapacity;
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
std::uintptr_t _ZTVSt5ctypeIwE_nid_postfix[18] {};
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
std::uintptr_t _ZTVSt8numpunctIcE_nid_postfix[11] {};
std::uintptr_t _ZTVSt8messagesIcE_nid_postfix[9] {};
std::uint64_t _ZNSt7codecvtIcc9_MbstatetE2idE_nid_postfix = 0;
std::uintptr_t _ZTVSt7codecvtIwc9_MbstatetE_nid_postfix[13] {};
std::uintptr_t _ZTVSt7codecvtIcc9_MbstatetE_nid_postfix[kCodecvtCharVtableSlots] {};
std::uint64_t _ZNSt7num_putIcSt19ostreambuf_iteratorIcSt11char_traitsIcEEE2idE_nid_postfix = 0;
std::uintptr_t _ZTVSt7num_putIcSt19ostreambuf_iteratorIcSt11char_traitsIcEEE_nid_postfix[kNumPutCharVtableSlots] {};
std::uint64_t _ZNSt7num_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE2idE_nid_postfix = 0;
std::uintptr_t _ZTVSt7num_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE_nid_postfix[17] {};
std::uint64_t _ZNSt8time_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE2idE_nid_postfix = 0;
std::uint64_t _ZNSt7num_putIwSt19ostreambuf_iteratorIwSt11char_traitsIwEEE2idE_nid_postfix = 0;
std::uintptr_t _ZTVSt7num_putIwSt19ostreambuf_iteratorIwSt11char_traitsIwEEE_nid_postfix[14] {};
std::uint64_t _ZNSt8numpunctIcE2idE_nid_postfix = 0;
std::uint64_t _ZNSt8numpunctIwE2idE_nid_postfix = 0;
std::uint64_t _ZNSt8messagesIcE2idE_nid_postfix = 0;
std::uint64_t _ZNSt8messagesIwE2idE_nid_postfix = 0;
std::uint64_t _ZNSt8time_putIcSt19ostreambuf_iteratorIcSt11char_traitsIcEEE2idE_nid_postfix = 0;
std::uintptr_t _ZTVSt8time_putIcSt19ostreambuf_iteratorIcSt11char_traitsIcEEE_nid_postfix[kTimePutCharVtableSlots] {};
std::uintptr_t _ZTVSt8time_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE_nid_postfix[13] {};
std::uintptr_t _ZTVSt8time_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE_nid_postfix[13] {};
std::uintptr_t _ZTVSt7num_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE_nid_postfix[17] {};
std::uintptr_t _ZTVSt9money_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE_nid_postfix[kMoneyGetCharVtableSlots] {};

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

constexpr std::int64_t SceV2StandardStreamSlots = 8;
std::mutex g_sceV2StandardStreamMutex;
GuestLocale::SceV2IosBase* g_sceV2StandardStreams[SceV2StandardStreamSlots + 1] = {};
unsigned char g_sceV2StandardStreamOpens[SceV2StandardStreamSlots + 1] = {};

struct SceV2StreamNode {
    SceV2StreamNode* next;
    unsigned char reserved[24];
};

struct SceV2CallbackNode {
    SceV2CallbackNode* next;
    std::int32_t id;
    std::uint32_t reserved;
    void (APS5_VABI *callback)(int, void*, std::int32_t);
};

static_assert(sizeof(SceV2StreamNode) == 0x20);
static_assert(sizeof(SceV2CallbackNode) == 0x18);

struct SceV2FacetNode {
    SceV2FacetNode* next;
    GuestLocale::Facet* facet;
};

static_assert(sizeof(SceV2FacetNode) == 0x10);

SceV2FacetNode* g_sceV2FacetNodes = nullptr;
std::atomic<GuestLocale::Implementation*> g_sceV2GlobalLocale{&g_classicLocale};
GuestLocaleFacade g_sceV2ClassicLocaleFacade{&g_classicLocale};
constexpr std::uint32_t SceV2AllLocaleCategories = 0x3f;
constexpr int SceV2LocaleCategoryCount = 6;

std::recursive_mutex g_sceV2LocimpMutex;

void APS5_VABI DestroySceV2Locimp_nid_no_patch(GuestLocale::Facet* self) {
    _ZNSt8__sce_v26locale7_Locimp12_Locimp_dtorEPS1__nid_postfix(reinterpret_cast<GuestLocale::Implementation*>(self));
}

void APS5_VABI DeleteSceV2Locimp_nid_no_patch(GuestLocale::Facet* self) {
    _ZNSt8__sce_v26locale7_Locimp12_Locimp_dtorEPS1__nid_postfix(reinterpret_cast<GuestLocale::Implementation*>(self));
    _ZdlPv_nid_postfix(self);
}

const GuestLocale::FacetVtable g_sceV2LocimpVtable{DestroySceV2Locimp_nid_no_patch, DeleteSceV2Locimp_nid_no_patch, RetainFacet, ReleaseFacet};

void ReleaseSceV2Facet(GuestLocale::Facet* facet) {
    if (auto* released = facet->vtable->release(facet)) released->vtable->deleteObject(released);
}

bool CopySceV2Facets(GuestLocale::Implementation* self, const GuestLocale::Implementation* source) {
    std::lock_guard lock(g_sceV2LocimpMutex);
    const std::size_t count = self->facetCount;
    if (count == 0) return true;
    if (count > SIZE_MAX / sizeof(GuestLocale::Facet*)) return false;
    auto* facets = static_cast<GuestLocale::Facet**>(malloc_nid_postfix(count * sizeof(GuestLocale::Facet*)));
    if (facets == nullptr) return false;
    self->facets = facets;
    for (std::size_t index = count; index-- != 0;) {
        GuestLocale::Facet* facet = source->facets[index];
        facets[index] = facet;
        if (facet != nullptr) facet->vtable->retain(facet);
    }
    return true;
}

bool AddSceV2Facet(GuestLocale::Implementation* self, GuestLocale::Facet* facet, std::size_t id) {
    std::lock_guard lock(g_sceV2LocimpMutex);
    if (id >= self->facetCount) {
        if (id >= SIZE_MAX / sizeof(GuestLocale::Facet*)) return false;
        const std::size_t count = std::max<std::size_t>(id + 1, 40);
        GuestLocale::Facet** grown;
        if (self->facets == g_classicFacets) {
            grown = static_cast<GuestLocale::Facet**>(malloc_nid_postfix(count * sizeof(GuestLocale::Facet*)));
            if (grown != nullptr) std::memcpy(grown, g_classicFacets, self->facetCount * sizeof(GuestLocale::Facet*));
        } else {
            grown = static_cast<GuestLocale::Facet**>(realloc_nid_postfix(self->facets, count * sizeof(GuestLocale::Facet*)));
        }
        if (grown == nullptr) return false;
        self->facets = grown;
        std::fill(grown + self->facetCount, grown + count, nullptr);
        self->facetCount = count;
    }
    facet->vtable->retain(facet);
    if (GuestLocale::Facet* old = self->facets[id]) ReleaseSceV2Facet(old);
    self->facets[id] = facet;
    return true;
}

GuestLocale::Implementation* NewSceV2Locimp(bool transparent) {
    auto* self = static_cast<GuestLocale::Implementation*>(_Znwm_nid_postfix(sizeof(GuestLocale::Implementation)));
    *self = {{&g_sceV2LocimpVtable, 1, 0}, nullptr, 0, 0, transparent, "*"};
    return self;
}

bool ValidSceV2LocaleName(const char* localeName) {
    return localeName != nullptr && (*localeName == 0 || std::strcmp(localeName, "C") == 0);
}

void CheckSceV2LocaleName(const char* localeName) {
    if (!ValidSceV2LocaleName(localeName)) _ZNSt8__sce_v215_Xruntime_errorEPKc_nid_postfix("bad locale name");
}

}

std::uint64_t _ZNSt8__sce_v25ctypeIcE2idE_nid_postfix = 0;
std::uint64_t _ZNSt8__sce_v25ctypeIwE2idE_nid_postfix = 0;
std::uint64_t _ZNSt8__sce_v28numpunctIcE2idE_nid_postfix = 0;
std::uint64_t _ZNSt8__sce_v27num_putIcNS_19ostreambuf_iteratorIcNS_11char_traitsIcEEEEE2idE_nid_postfix = 0;
std::uint64_t _ZNSt8__sce_v27codecvtIwc9_MbstatetE2idE_nid_postfix = 0;
std::uint64_t _ZNSt8__sce_v25ctypeItE2idE_nid_postfix = 0;
std::uint64_t _ZNSt8__sce_v27codecvtItc9_MbstatetE2idE_nid_postfix = 0;
std::uint64_t _ZNSt8__sce_v27num_getIcNS_19istreambuf_iteratorIcNS_11char_traitsIcEEEEE2idE_nid_postfix = 0;
std::uint64_t _ZNSt8__sce_v27num_getIwNS_19istreambuf_iteratorIwNS_11char_traitsIwEEEEE2idE_nid_postfix = 0;
std::uint64_t _ZNSt8__sce_v27num_putIwNS_19ostreambuf_iteratorIwNS_11char_traitsIwEEEEE2idE_nid_postfix = 0;
std::uint64_t _ZNSt8__sce_v28numpunctIwE2idE_nid_postfix = 0;
std::uint64_t _ZNSt8__sce_v27codecvtIcc9_MbstatetE2idE_nid_postfix = 0;
std::uint64_t _ZNSt8__sce_v27collateIcE2idE_nid_postfix = 0;
std::uint64_t _ZNSt8__sce_v27collateIwE2idE_nid_postfix = 0;
std::uint64_t _ZNSt8__sce_v28messagesIcE2idE_nid_postfix = 0;
std::uint64_t _ZNSt8__sce_v28messagesIwE2idE_nid_postfix = 0;
std::uint64_t _ZNSt8__sce_v210moneypunctIcLb0EE2idE_nid_postfix = 0;
std::uint64_t _ZNSt8__sce_v210moneypunctIcLb1EE2idE_nid_postfix = 0;
std::uint64_t _ZNSt8__sce_v210moneypunctIwLb0EE2idE_nid_postfix = 0;
std::uint64_t _ZNSt8__sce_v210moneypunctIwLb1EE2idE_nid_postfix = 0;
std::uint64_t _ZNSt8__sce_v29money_getIcNS_19istreambuf_iteratorIcNS_11char_traitsIcEEEEE2idE_nid_postfix = 0;
std::uint64_t _ZNSt8__sce_v29money_getIwNS_19istreambuf_iteratorIwNS_11char_traitsIwEEEEE2idE_nid_postfix = 0;
std::uint64_t _ZNSt8__sce_v29money_putIcNS_19ostreambuf_iteratorIcNS_11char_traitsIcEEEEE2idE_nid_postfix = 0;
std::uint64_t _ZNSt8__sce_v29money_putIwNS_19ostreambuf_iteratorIwNS_11char_traitsIwEEEEE2idE_nid_postfix = 0;
std::uint64_t _ZNSt8__sce_v28time_getIcNS_19istreambuf_iteratorIcNS_11char_traitsIcEEEEE2idE_nid_postfix = 0;
std::uint64_t _ZNSt8__sce_v28time_getIwNS_19istreambuf_iteratorIwNS_11char_traitsIwEEEEE2idE_nid_postfix = 0;
std::uint64_t _ZNSt8__sce_v28time_putIcNS_19ostreambuf_iteratorIcNS_11char_traitsIcEEEEE2idE_nid_postfix = 0;
std::uint64_t _ZNSt8__sce_v28time_putIwNS_19ostreambuf_iteratorIwNS_11char_traitsIwEEEEE2idE_nid_postfix = 0;
std::uint64_t _ZNSt8__sce_v27codecvtIDsDu9_MbstatetE2idE_nid_postfix = 0;
std::uint64_t _ZNSt8__sce_v27codecvtIDiDu9_MbstatetE2idE_nid_postfix = 0;
std::uint64_t _ZNSt8__sce_v27codecvtIDsc9_MbstatetE2idE_nid_postfix = 0;
std::uint64_t _ZNSt8__sce_v27codecvtIDic9_MbstatetE2idE_nid_postfix = 0;
bool _ZNSt8__sce_v28ios_base5_SyncE_nid_postfix = true;
std::int32_t _ZNSt8__sce_v28ios_base6_IndexE_nid_postfix = 0;
std::int32_t _ZNSt8__sce_v26locale2id7_Id_cntE_nid_postfix = 0;
GuestLocale::Implementation* _ZNSt8__sce_v221_sceLibcClassicLocaleE_nid_postfix = &g_classicLocale;
alignas(16) unsigned char _ZNSt8__sce_v24coutE_nid_postfix[0x400] {};
alignas(16) unsigned char _ZNSt8__sce_v24cerrE_nid_postfix[0x400] {};
alignas(16) unsigned char _ZNSt8__sce_v25wcoutE_nid_postfix[0x400] {};
alignas(16) unsigned char _ZNSt8__sce_v25wcerrE_nid_postfix[0x400] {};
alignas(16) unsigned char _ZNSt8__sce_v23cinE_nid_postfix[0x400] {};
alignas(16) unsigned char _ZNSt8__sce_v24wcinE_nid_postfix[0x400] {};
alignas(16) unsigned char _ZNSt8__sce_v24clogE_nid_postfix[0x400] {};
alignas(16) unsigned char _ZNSt8__sce_v25wclogE_nid_postfix[0x400] {};
unsigned char* _ZNSt8__sce_v28_Ptr_cinE_nid_postfix = _ZNSt8__sce_v23cinE_nid_postfix;
unsigned char* _ZNSt8__sce_v29_Ptr_coutE_nid_postfix = _ZNSt8__sce_v24coutE_nid_postfix;
unsigned char* _ZNSt8__sce_v29_Ptr_cerrE_nid_postfix = _ZNSt8__sce_v24cerrE_nid_postfix;
unsigned char* _ZNSt8__sce_v29_Ptr_clogE_nid_postfix = _ZNSt8__sce_v24clogE_nid_postfix;
unsigned char* _ZNSt8__sce_v29_Ptr_wcinE_nid_postfix = _ZNSt8__sce_v24wcinE_nid_postfix;
unsigned char* _ZNSt8__sce_v210_Ptr_wcoutE_nid_postfix = _ZNSt8__sce_v25wcoutE_nid_postfix;
unsigned char* _ZNSt8__sce_v210_Ptr_wcerrE_nid_postfix = _ZNSt8__sce_v25wcerrE_nid_postfix;
unsigned char* _ZNSt8__sce_v210_Ptr_wclogE_nid_postfix = _ZNSt8__sce_v25wclogE_nid_postfix;

GuestLocale::Implementation* APS5_VABI _ZNSt8__sce_v26locale5_InitEb_nid_postfix(bool doIncref) {
    std::lock_guard<std::mutex> lock(g_localeInitMutex);
    auto* global = g_sceV2GlobalLocale.load();
    if (doIncref) global->base.vtable->retain(&global->base);
    return global;
}

GuestLocale::Implementation* APS5_VABI _ZNSt8__sce_v26locale16_GetgloballocaleEv_nid_postfix() {
    return g_sceV2GlobalLocale.load();
}

void APS5_VABI _ZNSt8__sce_v28ios_base7_AddstdEPS0__nid_postfix(GuestLocale::SceV2IosBase* self) {
    if (self == nullptr) throw std::invalid_argument("ios_base _Addstd: null stream");
    std::lock_guard lock(g_sceV2StandardStreamMutex);
    for (self->standardStream = 1; self->standardStream < SceV2StandardStreamSlots; ++self->standardStream) {
        if (g_sceV2StandardStreams[self->standardStream] == nullptr || g_sceV2StandardStreams[self->standardStream] == self) break;
    }
    g_sceV2StandardStreams[self->standardStream] = self;
    ++g_sceV2StandardStreamOpens[self->standardStream];
}

void APS5_VABI _ZNSt8__sce_v28ios_base14_Ios_base_dtorEPS0__nid_postfix(GuestLocale::SceV2IosBase* self) {
    if (self == nullptr) throw std::invalid_argument("ios_base destructor: null object");
    {
        std::lock_guard lock(g_sceV2StandardStreamMutex);
        if (self->standardStream > 0) {
            if (self->standardStream > SceV2StandardStreamSlots) throw std::runtime_error("ios_base destructor: unknown standard stream slot");
            if (0 < --g_sceV2StandardStreamOpens[self->standardStream]) return;
        }
    }
    for (auto* node = static_cast<SceV2CallbackNode*>(self->callbackNodes); node != nullptr; node = node->next) {
        node->callback(0, self, node->id);
    }
    for (auto* node = static_cast<SceV2StreamNode*>(self->streamNodes); node != nullptr;) {
        auto* next = node->next;
        _ZdlPv_nid_postfix(node);
        node = next;
    }
    self->streamNodes = nullptr;
    for (auto* node = static_cast<SceV2CallbackNode*>(self->callbackNodes); node != nullptr;) {
        auto* next = node->next;
        _ZdlPv_nid_postfix(node);
        node = next;
    }
    self->callbackNodes = nullptr;
    auto** locale = self->locale;
    if (locale != nullptr && locale != &_ZNSt8__sce_v221_sceLibcClassicLocaleE_nid_postfix) {
        GuestLocale::Implementation* implementation = *locale;
        if (implementation != nullptr) {
            const GuestLocale::FacetVtable* vtable = implementation->base.vtable;
            GuestLocale::Facet* released = vtable != nullptr ? vtable->release(&implementation->base) : nullptr;
            if (released != nullptr && released->vtable != nullptr) released->vtable->deleteObject(released);
        }
    }
    self->locale = nullptr;
}

void APS5_VABI _ZNSt8__sce_v215_Facet_RegisterEPNS_11_Facet_baseE_nid_postfix(GuestLocale::Facet* facet) {
    auto* node = static_cast<SceV2FacetNode*>(_Znwm_nid_postfix(sizeof(SceV2FacetNode)));
    std::lock_guard lock(g_sceV2LocimpMutex);
    *node = {g_sceV2FacetNodes, facet};
    g_sceV2FacetNodes = node;
}

void APS5_VABI _ZNSt8__sce_v26locale7_Locimp12_Locimp_ctorEPS1_RKS1__nid_postfix(GuestLocale::Implementation* self, const GuestLocale::Implementation* source) {
    if (self == nullptr || source == nullptr) throw std::invalid_argument("_Locimp_ctor: null object");
    if (!CopySceV2Facets(self, source)) _ZNSt8__sce_v211_Xbad_allocEv_nid_postfix();
}

void APS5_VABI _ZNSt8__sce_v26locale7_Locimp12_Locimp_dtorEPS1__nid_postfix(GuestLocale::Implementation* self) {
    if (self == nullptr) throw std::invalid_argument("_Locimp_dtor: null object");
    std::lock_guard lock(g_sceV2LocimpMutex);
    for (std::size_t index = self->facetCount; index-- != 0;) {
        if (GuestLocale::Facet* facet = self->facets[index]) ReleaseSceV2Facet(facet);
    }
    if (self->facets != g_classicFacets) free_nid_postfix(self->facets);
}

GuestLocale::Implementation* APS5_VABI _ZNSt8__sce_v26locale7_Locimp11_New_LocimpERKS1__nid_postfix(const GuestLocale::Implementation* source) {
    if (source == nullptr) throw std::invalid_argument("_Locimp constructor: null source");
    auto* self = static_cast<GuestLocale::Implementation*>(_Znwm_nid_postfix(sizeof(GuestLocale::Implementation)));
    *self = {{&g_sceV2LocimpVtable, 1, 0}, nullptr, source->facetCount, source->category, source->transparent, source->name};
    if (!CopySceV2Facets(self, source)) {
        _ZdlPv_nid_postfix(self);
        _ZNSt8__sce_v211_Xbad_allocEv_nid_postfix();
    }
    return self;
}

void APS5_VABI _ZNSt8__sce_v26locale7_Locimp14_Locimp_AddfacEPS1_PNS0_5facetEm_nid_postfix(GuestLocale::Implementation* self, GuestLocale::Facet* facet, std::size_t id) {
    if (self == nullptr || facet == nullptr) throw std::invalid_argument("_Locimp_Addfac: null object");
    if (!AddSceV2Facet(self, facet, id)) _ZNSt8__sce_v211_Xbad_allocEv_nid_postfix();
}

GuestLocale::Implementation* _ZNSt8__sce_v26locale7_Locimp8_ClocptrE_nid_postfix = &g_classicLocale;

GuestLocaleFacade* APS5_VABI _ZNSt8__sce_v26locale7classicEv_nid_postfix() {
    return &g_sceV2ClassicLocaleFacade;
}

GuestLocaleFacade* APS5_VABI _ZNSt8__sce_v26locale5emptyEv_nid_postfix(GuestLocaleFacade* result) {
    if (result == nullptr) throw std::invalid_argument("locale::empty: null result");
    result->implementation = NewSceV2Locimp(true);
    return result;
}

GuestLocale::Implementation* APS5_VABI _ZNSt8__sce_v26locale7_Locimp11_New_LocimpEb_nid_postfix(bool transparent) {
    return NewSceV2Locimp(transparent);
}

void APS5_VABI _ZNSt8__sce_v26locale16_SetgloballocaleEPv_nid_postfix(void* locale) {
    if (locale == nullptr) throw std::invalid_argument("locale::_Setgloballocale: null locale");
    g_sceV2GlobalLocale.store(static_cast<GuestLocale::Implementation*>(locale));
}

GuestLocaleFacade* APS5_VABI _ZNSt8__sce_v26locale6globalERKS0__nid_postfix(GuestLocaleFacade* result, const GuestLocaleFacade* locale) {
    if (result == nullptr || locale == nullptr || locale->implementation == nullptr) throw std::invalid_argument("locale::global: null locale");
    result->implementation = _ZNSt8__sce_v26locale5_InitEb_nid_postfix(true);
    std::lock_guard<std::mutex> lock(g_localeInitMutex);
    auto* current = g_sceV2GlobalLocale.load();
    auto* next = locale->implementation;
    if (current == next) return result;
    if (auto* released = current->base.vtable->release(&current->base)) released->vtable->deleteObject(released);
    _ZNSt8__sce_v26locale16_SetgloballocaleEPv_nid_postfix(next);
    next->base.vtable->retain(&next->base);
    if ((next->category & SceV2AllLocaleCategories) == SceV2AllLocaleCategories) {
        setlocale_nid_postfix(SceV2AllLocaleCategories, next->name);
        return result;
    }
    for (int category = 0; category < SceV2LocaleCategoryCount; ++category) {
        if ((next->category & (1u << category)) != 0) setlocale_nid_postfix(category + 1, next->name);
    }
    return result;
}

void APS5_VABI _ZNSt8__sce_v215_sceLibcLocinfoEPKc_nid_postfix(const char* localeName) {
    CheckSceV2LocaleName(localeName);
}

void APS5_VABI _ZNSt8__sce_v28_Locinfo13_Locinfo_ctorEPS0_iPKc_nid_postfix(GuestLocale::SceV2Locinfo* self, int, const char* localeName) {
    if (self == nullptr) throw std::invalid_argument("_Locinfo: null object");
    const char* current = setlocale_nid_postfix(SceV2AllLocaleCategories, nullptr);
    self->name = current != nullptr ? current : "";
    CheckSceV2LocaleName(localeName);
    self->classicName = setlocale_nid_postfix(SceV2AllLocaleCategories, nullptr);
}

GuestLocale::SceV2Locinfo* APS5_VABI _ZNSt8__sce_v28_Locinfo16_Locinfo_AddcatsEPS0_iPKc_nid_postfix(GuestLocale::SceV2Locinfo* self, int, const char* localeName) {
    CheckSceV2LocaleName(localeName);
    if (self == nullptr) throw std::invalid_argument("_Locinfo::_Addcats: null object");
    self->classicName = setlocale_nid_postfix(SceV2AllLocaleCategories, nullptr);
    return self;
}

extern "C++" {
namespace {

std::uint64_t g_sceV2NumGetUshortId = 0;
std::uint64_t g_sceV2NumPutUshortId = 0;
std::uint64_t g_sceV2NumpunctUshortId = 0;
std::uint64_t g_sceV2CollateUshortId = 0;
std::uint64_t g_sceV2MessagesUshortId = 0;
std::uint64_t g_sceV2MoneyGetUshortId = 0;
std::uint64_t g_sceV2MoneyPutUshortId = 0;
std::uint64_t g_sceV2MoneypunctUshortId = 0;
std::uint64_t g_sceV2MoneypunctUshortIntlId = 0;
std::uint64_t g_sceV2TimeGetUshortId = 0;
std::uint64_t g_sceV2TimePutUshortId = 0;

struct SceV2FacetKind {
    std::uint64_t* id;
    std::size_t size;
    const GuestLocale::FacetVtable* vtable;
    bool (*initialize)(GuestLocale::Facet* facet, bool isDefault);
    GuestLocale::Facet* cached;
};

struct SceV2FacetStep {
    std::int32_t category;
    SceV2FacetKind* kind;
};

constexpr std::int32_t SceV2CollateCategory = 0x1;
constexpr std::int32_t SceV2CtypeCategory = 0x2;
constexpr std::int32_t SceV2MonetaryCategory = 0x4;
constexpr std::int32_t SceV2NumericCategory = 0x8;
constexpr std::int32_t SceV2TimeCategory = 0x10;
constexpr std::int32_t SceV2MessagesCategory = 0x20;

std::uint16_t WidenSceV2Byte(char byte) {
    return static_cast<unsigned char>(byte);
}

char* CopySceV2Bytes(const char* text) {
    const std::size_t size = std::strlen(text) + 1;
    auto* copy = static_cast<char*>(calloc_nid_postfix(size, 1));
    if (copy != nullptr) std::memcpy(copy, text, size);
    return copy;
}

std::uint16_t* CopySceV2Ushorts(const char* text) {
    const std::size_t length = std::strlen(text);
    auto* copy = static_cast<std::uint16_t*>(calloc_nid_postfix(length + 1, sizeof(std::uint16_t)));
    if (copy == nullptr) return nullptr;
    for (std::size_t index = 0; index < length; ++index) copy[index] = WidenSceV2Byte(text[index]);
    return copy;
}

bool InitializeSceV2PlainFacet(GuestLocale::Facet*, bool) {
    return true;
}

bool InitializeSceV2CtypeFacet(GuestLocale::Facet* self, bool) {
    auto* facet = reinterpret_cast<SceV2CtypeFacet*>(self);
    facet->table = _Getpctype_nid_postfix();
    facet->deleteMode = 0;
    return true;
}

bool InitializeSceV2WideCtypeFacet(GuestLocale::Facet* self, bool) {
    auto* facet = reinterpret_cast<SceV2WideCtypeFacet*>(self);
    facet->table = _Getpctype_nid_postfix();
    facet->deleteMode = 0;
    facet->conversion = SceV2ClassicCvtvec;
    return true;
}

bool InitializeSceV2WideCodecvtFacet(GuestLocale::Facet* self, bool) {
    reinterpret_cast<SceV2WideCodecvtFacet*>(self)->conversion = SceV2ClassicCvtvec;
    return true;
}

bool InitializeSceV2NumpunctFacet(GuestLocale::Facet* self, bool isDefault) {
    auto* facet = reinterpret_cast<GuestLocale::NumpunctFacet*>(self);
    const SonyLconv* conventions = localeconv_nid_postfix();
    facet->grouping = g_sceV2Empty;
    facet->falsename = g_sceV2FalseName;
    facet->truename = g_sceV2TrueName;
    facet->decimalPoint = isDefault ? '.' : *conventions->decimal_point;
    facet->thousandsSep = isDefault ? ',' : *conventions->thousands_sep;
    return true;
}

void SetSceV2WideNumpunctSeparators(GuestLocale::WideNumpunctFacet* facet, const SonyLconv* conventions, bool isDefault) {
    facet->decimalPoint = isDefault ? '.' : WidenSceV2Byte(*conventions->decimal_point);
    facet->thousandsSep = isDefault ? ',' : WidenSceV2Byte(*conventions->thousands_sep);
}

bool InitializeSceV2WideNumpunctFacet(GuestLocale::Facet* self, bool isDefault) {
    auto* facet = reinterpret_cast<GuestLocale::WideNumpunctFacet*>(self);
    facet->grouping = g_sceV2Empty;
    facet->falsename = g_sceV2WideFalseName.data();
    facet->truename = g_sceV2WideTrueName.data();
    SetSceV2WideNumpunctSeparators(facet, localeconv_nid_postfix(), isDefault);
    return true;
}

bool InitializeSceV2UshortNumpunctFacet(GuestLocale::Facet* self, bool isDefault) {
    auto* facet = reinterpret_cast<GuestLocale::WideNumpunctFacet*>(self);
    const SonyLconv* conventions = localeconv_nid_postfix();
    facet->grouping = nullptr;
    facet->falsename = nullptr;
    facet->truename = nullptr;
    facet->grouping = CopySceV2Bytes(isDefault ? g_sceV2Empty : conventions->grouping);
    if (facet->grouping == nullptr) return false;
    facet->falsename = CopySceV2Ushorts(g_sceV2FalseName);
    if (facet->falsename == nullptr) return false;
    facet->truename = CopySceV2Ushorts(g_sceV2TrueName);
    if (facet->truename == nullptr) return false;
    SetSceV2WideNumpunctSeparators(facet, conventions, isDefault);
    return true;
}

template<typename Text> void SetSceV2MoneyFormats(SceV2MoneypunctFacet<Text>* facet, const SonyLconv* conventions, bool isDefault) {
    const auto* chars = reinterpret_cast<const unsigned char*>(conventions->chars);
    facet->conversion = SceV2ClassicCvtvec;
    const unsigned char digits = chars[facet->international ? 7 : 0];
    facet->fractionDigits = digits < CHAR_MAX ? digits : 0;
    std::memcpy(facet->positivePattern, g_sceV2DefaultMoneyPattern, sizeof(facet->positivePattern));
    if (!facet->international && chars[6] < 5 && chars[5] < 3 && chars[4] < 2) std::memcpy(facet->positivePattern, MakeSceV2MoneyPattern(chars[4], chars[5], chars[6]).data(), sizeof(facet->positivePattern));
    std::memcpy(facet->negativePattern, g_sceV2DefaultMoneyPattern, sizeof(facet->negativePattern));
    if (!facet->international && chars[3] < 5 && chars[1] < 2 && chars[2] < 3) std::memcpy(facet->negativePattern, MakeSceV2MoneyPattern(chars[1], chars[2], chars[3]).data(), sizeof(facet->negativePattern));
    if (isDefault) {
        std::memcpy(facet->positivePattern, g_sceV2DefaultMoneyPattern, sizeof(facet->positivePattern));
        std::memcpy(facet->negativePattern, g_sceV2DefaultMoneyPattern, sizeof(facet->negativePattern));
    }
}

template<bool International> bool InitializeSceV2MoneypunctFacet(GuestLocale::Facet* self, bool isDefault) {
    auto* facet = reinterpret_cast<SceV2MoneypunctFacet<char>*>(self);
    const SonyLconv* conventions = localeconv_nid_postfix();
    facet->international = International;
    facet->grouping = g_sceV2Empty;
    facet->currency = g_sceV2Empty;
    facet->positiveSign = g_sceV2Empty;
    facet->negativeSign = g_sceV2Minus;
    facet->decimalPoint = *conventions->monetary[2];
    facet->thousandsSep = *conventions->monetary[4];
    SetSceV2MoneyFormats(facet, conventions, isDefault);
    return true;
}

template<bool International> bool InitializeSceV2WideMoneypunctFacet(GuestLocale::Facet* self, bool isDefault) {
    auto* facet = reinterpret_cast<SceV2MoneypunctFacet<std::uint16_t>*>(self);
    const SonyLconv* conventions = localeconv_nid_postfix();
    facet->international = International;
    facet->grouping = g_sceV2Empty;
    facet->currency = g_sceV2WideEmpty.data();
    facet->positiveSign = g_sceV2WideEmpty.data();
    facet->negativeSign = g_sceV2WideMinus.data();
    facet->decimalPoint = WidenSceV2Byte(*conventions->monetary[2]);
    facet->thousandsSep = WidenSceV2Byte(*conventions->monetary[4]);
    SetSceV2MoneyFormats(facet, conventions, isDefault);
    return true;
}

template<bool International> bool InitializeSceV2UshortMoneypunctFacet(GuestLocale::Facet* self, bool isDefault) {
    auto* facet = reinterpret_cast<SceV2MoneypunctFacet<std::uint16_t>*>(self);
    const SonyLconv* conventions = localeconv_nid_postfix();
    const auto* chars = reinterpret_cast<const unsigned char*>(conventions->chars);
    facet->international = International;
    facet->grouping = nullptr;
    facet->currency = nullptr;
    facet->positiveSign = nullptr;
    facet->negativeSign = nullptr;
    facet->grouping = CopySceV2Bytes(conventions->monetary[3]);
    if (facet->grouping == nullptr) return false;
    facet->currency = CopySceV2Ushorts(conventions->monetary[International ? 1 : 0]);
    if (facet->currency == nullptr) return false;
    facet->positiveSign = CopySceV2Ushorts(chars[6] < 5 ? conventions->monetary[6] : g_sceV2Empty);
    if (facet->positiveSign == nullptr) return false;
    facet->negativeSign = CopySceV2Ushorts(chars[3] < 5 ? conventions->monetary[5] : g_sceV2Minus);
    if (facet->negativeSign == nullptr) return false;
    facet->decimalPoint = WidenSceV2Byte(*conventions->monetary[2]);
    facet->thousandsSep = WidenSceV2Byte(*conventions->monetary[4]);
    SetSceV2MoneyFormats(facet, conventions, isDefault);
    return true;
}

bool InitializeSceV2TimeGetFacet(GuestLocale::Facet* self, bool) {
    auto* facet = reinterpret_cast<SceV2TimeGetFacet<char>*>(self);
    facet->days = g_sceV2Days;
    facet->months = g_sceV2Months;
    facet->amPm = g_sceV2AmPm;
    facet->dateOrder = 2;
    facet->conversion = SceV2ClassicCvtvec;
    return true;
}

bool InitializeSceV2WideTimeGetFacet(GuestLocale::Facet* self, bool) {
    auto* facet = reinterpret_cast<SceV2TimeGetFacet<std::uint16_t>*>(self);
    facet->days = g_sceV2WideDays.data();
    facet->months = g_sceV2WideMonths.data();
    facet->amPm = g_sceV2WideAmPm.data();
    facet->dateOrder = 2;
    facet->conversion = SceV2ClassicCvtvec;
    return true;
}

bool InitializeSceV2UshortTimeGetFacet(GuestLocale::Facet* self, bool) {
    auto* facet = reinterpret_cast<SceV2TimeGetFacet<std::uint16_t>*>(self);
    facet->days = nullptr;
    facet->months = nullptr;
    facet->amPm = nullptr;
    facet->conversion = SceV2ClassicCvtvec;
    facet->days = CopySceV2Ushorts(g_sceV2Days);
    if (facet->days == nullptr) return false;
    facet->months = CopySceV2Ushorts(g_sceV2Months);
    if (facet->months == nullptr) return false;
    facet->amPm = CopySceV2Ushorts(g_sceV2AmPm);
    if (facet->amPm == nullptr) return false;
    facet->dateOrder = 2;
    return true;
}

bool InitializeSceV2TimePutFacet(GuestLocale::Facet* self, bool) {
    reinterpret_cast<SceV2TimePutFacet*>(self)->names = nullptr;
    return true;
}

SceV2FacetKind g_sceV2CtypeChar{&_ZNSt8__sce_v25ctypeIcE2idE_nid_postfix, sizeof(SceV2CtypeFacet), &g_sceV2CtypeCharVtable.facet, InitializeSceV2CtypeFacet, nullptr};
SceV2FacetKind g_sceV2NumGetChar{&_ZNSt8__sce_v27num_getIcNS_19istreambuf_iteratorIcNS_11char_traitsIcEEEEE2idE_nid_postfix, sizeof(GuestLocale::Facet), &g_sceV2NumGetVtable<char>.facet, InitializeSceV2PlainFacet, nullptr};
SceV2FacetKind g_sceV2NumPutChar{&_ZNSt8__sce_v27num_putIcNS_19ostreambuf_iteratorIcNS_11char_traitsIcEEEEE2idE_nid_postfix, sizeof(GuestLocale::Facet), &g_sceV2NumPutVtable<char>.facet, InitializeSceV2PlainFacet, nullptr};
SceV2FacetKind g_sceV2NumpunctChar{&_ZNSt8__sce_v28numpunctIcE2idE_nid_postfix, sizeof(GuestLocale::NumpunctFacet), &g_sceV2NumpunctCharVtable.facet, InitializeSceV2NumpunctFacet, nullptr};
SceV2FacetKind g_sceV2CodecvtChar{&_ZNSt8__sce_v27codecvtIcc9_MbstatetE2idE_nid_postfix, sizeof(GuestLocale::Facet), &g_sceV2CodecvtCharVtable.facet, InitializeSceV2PlainFacet, nullptr};
SceV2FacetKind g_sceV2CollateChar{&_ZNSt8__sce_v27collateIcE2idE_nid_postfix, sizeof(GuestLocale::Facet), &g_sceV2CollateVtable<char>.facet, InitializeSceV2PlainFacet, nullptr};
SceV2FacetKind g_sceV2MessagesChar{&_ZNSt8__sce_v28messagesIcE2idE_nid_postfix, sizeof(GuestLocale::Facet), &g_sceV2MessagesVtable<char>.facet, InitializeSceV2PlainFacet, nullptr};
SceV2FacetKind g_sceV2MoneyGetChar{&_ZNSt8__sce_v29money_getIcNS_19istreambuf_iteratorIcNS_11char_traitsIcEEEEE2idE_nid_postfix, sizeof(GuestLocale::Facet), &g_sceV2MoneyGetVtable<char>.facet, InitializeSceV2PlainFacet, nullptr};
SceV2FacetKind g_sceV2MoneyPutChar{&_ZNSt8__sce_v29money_putIcNS_19ostreambuf_iteratorIcNS_11char_traitsIcEEEEE2idE_nid_postfix, sizeof(GuestLocale::Facet), &g_sceV2MoneyPutVtable<char>.facet, InitializeSceV2PlainFacet, nullptr};
SceV2FacetKind g_sceV2MoneypunctChar{&_ZNSt8__sce_v210moneypunctIcLb0EE2idE_nid_postfix, sizeof(SceV2MoneypunctFacet<char>), &g_sceV2MoneypunctVtable<char>.facet, InitializeSceV2MoneypunctFacet<false>, nullptr};
SceV2FacetKind g_sceV2MoneypunctCharIntl{&_ZNSt8__sce_v210moneypunctIcLb1EE2idE_nid_postfix, sizeof(SceV2MoneypunctFacet<char>), &g_sceV2MoneypunctVtable<char>.facet, InitializeSceV2MoneypunctFacet<true>, nullptr};
SceV2FacetKind g_sceV2TimeGetChar{&_ZNSt8__sce_v28time_getIcNS_19istreambuf_iteratorIcNS_11char_traitsIcEEEEE2idE_nid_postfix, sizeof(SceV2TimeGetFacet<char>), &g_sceV2TimeGetVtable<char>.facet, InitializeSceV2TimeGetFacet, nullptr};
SceV2FacetKind g_sceV2TimePutChar{&_ZNSt8__sce_v28time_putIcNS_19ostreambuf_iteratorIcNS_11char_traitsIcEEEEE2idE_nid_postfix, sizeof(SceV2TimePutFacet), &g_sceV2TimePutVtable<char>.facet, InitializeSceV2TimePutFacet, nullptr};
SceV2FacetKind g_sceV2CtypeWide{&_ZNSt8__sce_v25ctypeIwE2idE_nid_postfix, sizeof(SceV2WideCtypeFacet), &g_sceV2CtypeWideVtable.facet, InitializeSceV2WideCtypeFacet, nullptr};
SceV2FacetKind g_sceV2NumGetWide{&_ZNSt8__sce_v27num_getIwNS_19istreambuf_iteratorIwNS_11char_traitsIwEEEEE2idE_nid_postfix, sizeof(GuestLocale::Facet), &g_sceV2NumGetVtable<char16_t>.facet, InitializeSceV2PlainFacet, nullptr};
SceV2FacetKind g_sceV2NumPutWide{&_ZNSt8__sce_v27num_putIwNS_19ostreambuf_iteratorIwNS_11char_traitsIwEEEEE2idE_nid_postfix, sizeof(GuestLocale::Facet), &g_sceV2NumPutVtable<char16_t>.facet, InitializeSceV2PlainFacet, nullptr};
SceV2FacetKind g_sceV2NumpunctWide{&_ZNSt8__sce_v28numpunctIwE2idE_nid_postfix, sizeof(GuestLocale::WideNumpunctFacet), &g_sceV2NumpunctWideVtable.facet, InitializeSceV2WideNumpunctFacet, nullptr};
SceV2FacetKind g_sceV2CollateWide{&_ZNSt8__sce_v27collateIwE2idE_nid_postfix, sizeof(GuestLocale::Facet), &g_sceV2CollateVtable<char16_t>.facet, InitializeSceV2PlainFacet, nullptr};
SceV2FacetKind g_sceV2MessagesWide{&_ZNSt8__sce_v28messagesIwE2idE_nid_postfix, sizeof(GuestLocale::Facet), &g_sceV2MessagesVtable<char16_t>.facet, InitializeSceV2PlainFacet, nullptr};
SceV2FacetKind g_sceV2MoneyGetWide{&_ZNSt8__sce_v29money_getIwNS_19istreambuf_iteratorIwNS_11char_traitsIwEEEEE2idE_nid_postfix, sizeof(GuestLocale::Facet), &g_sceV2MoneyGetVtable<char16_t>.facet, InitializeSceV2PlainFacet, nullptr};
SceV2FacetKind g_sceV2MoneyPutWide{&_ZNSt8__sce_v29money_putIwNS_19ostreambuf_iteratorIwNS_11char_traitsIwEEEEE2idE_nid_postfix, sizeof(GuestLocale::Facet), &g_sceV2MoneyPutVtable<char16_t>.facet, InitializeSceV2PlainFacet, nullptr};
SceV2FacetKind g_sceV2MoneypunctWide{&_ZNSt8__sce_v210moneypunctIwLb0EE2idE_nid_postfix, sizeof(SceV2MoneypunctFacet<std::uint16_t>), &g_sceV2MoneypunctVtable<std::uint16_t>.facet, InitializeSceV2WideMoneypunctFacet<false>, nullptr};
SceV2FacetKind g_sceV2MoneypunctWideIntl{&_ZNSt8__sce_v210moneypunctIwLb1EE2idE_nid_postfix, sizeof(SceV2MoneypunctFacet<std::uint16_t>), &g_sceV2MoneypunctVtable<std::uint16_t>.facet, InitializeSceV2WideMoneypunctFacet<true>, nullptr};
SceV2FacetKind g_sceV2TimeGetWide{&_ZNSt8__sce_v28time_getIwNS_19istreambuf_iteratorIwNS_11char_traitsIwEEEEE2idE_nid_postfix, sizeof(SceV2TimeGetFacet<std::uint16_t>), &g_sceV2TimeGetVtable<char16_t>.facet, InitializeSceV2WideTimeGetFacet, nullptr};
SceV2FacetKind g_sceV2TimePutWide{&_ZNSt8__sce_v28time_putIwNS_19ostreambuf_iteratorIwNS_11char_traitsIwEEEEE2idE_nid_postfix, sizeof(SceV2TimePutFacet), &g_sceV2TimePutVtable<char16_t>.facet, InitializeSceV2TimePutFacet, nullptr};
SceV2FacetKind g_sceV2CodecvtWide{&_ZNSt8__sce_v27codecvtIwc9_MbstatetE2idE_nid_postfix, sizeof(SceV2WideCodecvtFacet), &g_sceV2CodecvtWideVtable.facet, InitializeSceV2WideCodecvtFacet, nullptr};
SceV2FacetKind g_sceV2CtypeUshort{&_ZNSt8__sce_v25ctypeItE2idE_nid_postfix, sizeof(SceV2WideCtypeFacet), &g_sceV2CtypeUshortVtable.facet, InitializeSceV2WideCtypeFacet, nullptr};
SceV2FacetKind g_sceV2NumGetUshort{&g_sceV2NumGetUshortId, sizeof(GuestLocale::Facet), &g_sceV2NumGetVtable<std::uint16_t>.facet, InitializeSceV2PlainFacet, nullptr};
SceV2FacetKind g_sceV2NumPutUshort{&g_sceV2NumPutUshortId, sizeof(GuestLocale::Facet), &g_sceV2NumPutVtable<std::uint16_t>.facet, InitializeSceV2PlainFacet, nullptr};
SceV2FacetKind g_sceV2NumpunctUshort{&g_sceV2NumpunctUshortId, sizeof(GuestLocale::WideNumpunctFacet), &g_sceV2NumpunctUshortVtable.facet, InitializeSceV2UshortNumpunctFacet, nullptr};
SceV2FacetKind g_sceV2CollateUshort{&g_sceV2CollateUshortId, sizeof(GuestLocale::Facet), &g_sceV2CollateUshortVtable.facet, InitializeSceV2PlainFacet, nullptr};
SceV2FacetKind g_sceV2MessagesUshort{&g_sceV2MessagesUshortId, sizeof(GuestLocale::Facet), &g_sceV2MessagesVtable<std::uint16_t>.facet, InitializeSceV2PlainFacet, nullptr};
SceV2FacetKind g_sceV2MoneyGetUshort{&g_sceV2MoneyGetUshortId, sizeof(GuestLocale::Facet), &g_sceV2MoneyGetVtable<std::uint16_t>.facet, InitializeSceV2PlainFacet, nullptr};
SceV2FacetKind g_sceV2MoneyPutUshort{&g_sceV2MoneyPutUshortId, sizeof(GuestLocale::Facet), &g_sceV2MoneyPutVtable<std::uint16_t>.facet, InitializeSceV2PlainFacet, nullptr};
SceV2FacetKind g_sceV2MoneypunctUshort{&g_sceV2MoneypunctUshortId, sizeof(SceV2MoneypunctFacet<std::uint16_t>), &g_sceV2MoneypunctVtable<std::uint16_t, DestroySceV2UshortMoneypunctFacet_nid_no_patch>.facet, InitializeSceV2UshortMoneypunctFacet<false>, nullptr};
SceV2FacetKind g_sceV2MoneypunctUshortIntl{&g_sceV2MoneypunctUshortIntlId, sizeof(SceV2MoneypunctFacet<std::uint16_t>), &g_sceV2MoneypunctVtable<std::uint16_t, DestroySceV2UshortMoneypunctFacet_nid_no_patch>.facet, InitializeSceV2UshortMoneypunctFacet<true>, nullptr};
SceV2FacetKind g_sceV2TimeGetUshort{&g_sceV2TimeGetUshortId, sizeof(SceV2TimeGetFacet<std::uint16_t>), &g_sceV2TimeGetVtable<std::uint16_t, DestroySceV2UshortTimeGetFacet_nid_no_patch>.facet, InitializeSceV2UshortTimeGetFacet, nullptr};
SceV2FacetKind g_sceV2TimePutUshort{&g_sceV2TimePutUshortId, sizeof(SceV2TimePutFacet), &g_sceV2TimePutVtable<std::uint16_t>.facet, InitializeSceV2TimePutFacet, nullptr};
SceV2FacetKind g_sceV2CodecvtUshort{&_ZNSt8__sce_v27codecvtItc9_MbstatetE2idE_nid_postfix, sizeof(SceV2WideCodecvtFacet), &g_sceV2CodecvtUshortVtable.facet, InitializeSceV2WideCodecvtFacet, nullptr};

const SceV2FacetStep g_sceV2NarrowLeadSteps[] = {
    {SceV2CtypeCategory, &g_sceV2CtypeChar},
    {SceV2NumericCategory, &g_sceV2NumGetChar},
    {SceV2NumericCategory, &g_sceV2NumPutChar},
    {SceV2NumericCategory, &g_sceV2NumpunctChar},
    {SceV2CtypeCategory, &g_sceV2CodecvtChar},
};

const SceV2FacetStep g_sceV2NarrowExtraSteps[] = {
    {SceV2CollateCategory, &g_sceV2CollateChar},
    {SceV2MessagesCategory, &g_sceV2MessagesChar},
    {SceV2MonetaryCategory, &g_sceV2MoneyGetChar},
    {SceV2MonetaryCategory, &g_sceV2MoneyPutChar},
    {SceV2MonetaryCategory, &g_sceV2MoneypunctChar},
    {SceV2MonetaryCategory, &g_sceV2MoneypunctCharIntl},
    {SceV2TimeCategory, &g_sceV2TimeGetChar},
    {SceV2TimeCategory, &g_sceV2TimePutChar},
};

const SceV2FacetStep g_sceV2WideSteps[] = {
    {SceV2CtypeCategory, &g_sceV2CtypeWide},
    {SceV2NumericCategory, &g_sceV2NumGetWide},
    {SceV2NumericCategory, &g_sceV2NumPutWide},
    {SceV2NumericCategory, &g_sceV2NumpunctWide},
    {SceV2CollateCategory, &g_sceV2CollateWide},
    {SceV2MessagesCategory, &g_sceV2MessagesWide},
    {SceV2MonetaryCategory, &g_sceV2MoneyGetWide},
    {SceV2MonetaryCategory, &g_sceV2MoneyPutWide},
    {SceV2MonetaryCategory, &g_sceV2MoneypunctWide},
    {SceV2MonetaryCategory, &g_sceV2MoneypunctWideIntl},
    {SceV2TimeCategory, &g_sceV2TimeGetWide},
    {SceV2TimeCategory, &g_sceV2TimePutWide},
    {SceV2CtypeCategory, &g_sceV2CodecvtWide},
};

const SceV2FacetStep g_sceV2UshortSteps[] = {
    {SceV2CtypeCategory, &g_sceV2CtypeUshort},
    {SceV2NumericCategory, &g_sceV2NumGetUshort},
    {SceV2NumericCategory, &g_sceV2NumPutUshort},
    {SceV2NumericCategory, &g_sceV2NumpunctUshort},
    {SceV2CollateCategory, &g_sceV2CollateUshort},
    {SceV2MessagesCategory, &g_sceV2MessagesUshort},
    {SceV2MonetaryCategory, &g_sceV2MoneyGetUshort},
    {SceV2MonetaryCategory, &g_sceV2MoneyPutUshort},
    {SceV2MonetaryCategory, &g_sceV2MoneypunctUshort},
    {SceV2MonetaryCategory, &g_sceV2MoneypunctUshortIntl},
    {SceV2TimeCategory, &g_sceV2TimeGetUshort},
    {SceV2TimeCategory, &g_sceV2TimePutUshort},
    {SceV2CtypeCategory, &g_sceV2CodecvtUshort},
};

std::uint64_t SceV2FacetId(std::uint64_t& id) {
    std::atomic_ref<std::uint64_t> slot(id);
    if (slot.load() == 0) {
        _Locksyslock_nid_postfix();
        if (slot.load() == 0) slot.store(static_cast<std::uint64_t>(++_ZNSt8__sce_v26locale2id7_Id_cntE_nid_postfix));
        _Unlocksyslock_nid_postfix();
    }
    return slot.load();
}

GuestLocale::Facet* BuildSceV2Facet(const SceV2FacetKind& kind, GuestLocale::Facet* facet, bool isDefault) {
    facet->vtable = kind.vtable;
    facet->references = 0;
    if (!kind.initialize(facet, isDefault)) {
        facet->vtable->deleteObject(facet);
        _ZNSt8__sce_v211_Xbad_allocEv_nid_postfix();
    }
    return facet;
}

GuestLocale::Facet* FindSceV2Facet(const SceV2FacetKind& kind, const GuestLocale::Implementation* locale, std::uint64_t id) {
    std::lock_guard lock(g_sceV2LocimpMutex);
    if (id < locale->facetCount && locale->facets[id] != nullptr) return locale->facets[id];
    if (locale->transparent) {
        const GuestLocale::Implementation* global = g_sceV2GlobalLocale.load();
        if (id < global->facetCount && global->facets[id] != nullptr) return global->facets[id];
    }
    return kind.cached;
}

GuestLocale::Facet* CacheSceV2Facet(SceV2FacetKind& kind, GuestLocale::Facet* facet, SceV2FacetNode* node) {
    std::lock_guard lock(g_sceV2LocimpMutex);
    if (kind.cached != nullptr) return kind.cached;
    *node = {g_sceV2FacetNodes, facet};
    g_sceV2FacetNodes = node;
    facet->vtable->retain(facet);
    kind.cached = facet;
    return facet;
}

GuestLocale::Facet* UseSceV2Facet(SceV2FacetKind& kind, const GuestLocale::Implementation* locale) {
    if (GuestLocale::Facet* found = FindSceV2Facet(kind, locale, SceV2FacetId(*kind.id))) return found;
    auto* storage = static_cast<GuestLocale::Facet*>(_Znwm_nid_postfix(kind.size));
    if (!ValidSceV2LocaleName(locale->name)) {
        _ZdlPv_nid_postfix(storage);
        _ZNSt8__sce_v215_Xruntime_errorEPKc_nid_postfix("bad locale name");
    }
    GuestLocale::Facet* facet = BuildSceV2Facet(kind, storage, true);
    auto* node = static_cast<SceV2FacetNode*>(_Znwm_nid_postfix(sizeof(SceV2FacetNode)));
    GuestLocale::Facet* cached = CacheSceV2Facet(kind, facet, node);
    if (cached != facet) {
        _ZdlPv_nid_postfix(node);
        facet->vtable->deleteObject(facet);
    }
    return cached;
}

template<std::size_t Count> void InstallSceV2Facets(const SceV2FacetStep (&steps)[Count], std::int32_t category, GuestLocale::Implementation* self, const GuestLocaleFacade* locale) {
    for (const SceV2FacetStep& step : steps) {
        if ((category & step.category) == 0) continue;
        SceV2FacetKind& kind = *step.kind;
        GuestLocale::Facet* facet = locale != nullptr
            ? UseSceV2Facet(kind, locale->implementation)
            : BuildSceV2Facet(kind, static_cast<GuestLocale::Facet*>(_Znwm_nid_postfix(kind.size)), false);
        if (!AddSceV2Facet(self, facet, SceV2FacetId(*kind.id))) _ZNSt8__sce_v211_Xbad_allocEv_nid_postfix();
    }
}

constexpr std::int32_t SceV2IosUppercase = 0x4;
constexpr std::int32_t SceV2IosShowBase = 0x8;
constexpr std::int32_t SceV2IosShowPoint = 0x10;
constexpr std::int32_t SceV2IosShowPos = 0x20;
constexpr std::int32_t SceV2IosLeft = 0x40;
constexpr std::int32_t SceV2IosInternal = 0x100;
constexpr std::int32_t SceV2IosAdjustField = 0x1c0;
constexpr std::int32_t SceV2IosOct = 0x400;
constexpr std::int32_t SceV2IosHex = 0x800;
constexpr std::int32_t SceV2IosBaseField = 0xe00;
constexpr std::int32_t SceV2IosScientific = 0x1000;
constexpr std::int32_t SceV2IosFixed = 0x2000;
constexpr std::int32_t SceV2IosFloatField = 0x3000;
constexpr std::int32_t SceV2IosBoolAlpha = 0x4000;

template<typename Text> struct SceV2TextFacets;

template<> struct SceV2TextFacets<char> {
    using Ctype = SceV2CtypeCharVtable;
    using Numpunct = SceV2NumpunctCharVtable;
    using Punct = GuestLocale::NumpunctFacet;
    static SceV2FacetKind& CtypeKind() { return g_sceV2CtypeChar; }
    static SceV2FacetKind& NumpunctKind() { return g_sceV2NumpunctChar; }
};

template<> struct SceV2TextFacets<char16_t> {
    using Ctype = SceV2CtypeWideVtable;
    using Numpunct = SceV2NumpunctWideVtable;
    using Punct = GuestLocale::WideNumpunctFacet;
    static SceV2FacetKind& CtypeKind() { return g_sceV2CtypeWide; }
    static SceV2FacetKind& NumpunctKind() { return g_sceV2NumpunctWide; }
};

template<> struct SceV2TextFacets<std::uint16_t> {
    using Ctype = SceV2CtypeWideVtable;
    using Numpunct = SceV2NumpunctWideVtable;
    using Punct = GuestLocale::WideNumpunctFacet;
    static SceV2FacetKind& CtypeKind() { return g_sceV2CtypeUshort; }
    static SceV2FacetKind& NumpunctKind() { return g_sceV2NumpunctUshort; }
};

template<typename Text> void PutSceV2Character(GuestLocale::OutputIterator& out, Text character) {
    GuestLocale::Streambuf* buffer = out.buffer;
    if (buffer == nullptr) {
        out.failed = true;
        return;
    }
    auto* next = reinterpret_cast<Text*>(*buffer->putNext);
    auto* count = static_cast<std::int32_t*>(const_cast<void*>(buffer->reservedCounts[2]));
    if (next != nullptr && *count > 0) {
        --*count;
        *buffer->putNext = reinterpret_cast<char*>(next + 1);
        *next = character;
        return;
    }
    if (buffer->vtable->overflow(buffer, static_cast<std::make_unsigned_t<Text>>(character)) == -1) out.failed = true;
}

template<typename Text> GuestLocale::OutputIterator PutSceV2Field(GuestLocale::OutputIterator out, GuestLocale::SceV2IosBase* stream, Text fill, const std::basic_string<Text>& text, std::size_t prefix) {
    const std::int64_t width = stream->width;
    std::size_t fillCount = width <= 0 || static_cast<std::size_t>(width) <= text.size() ? 0 : static_cast<std::size_t>(width) - text.size();
    const std::int32_t adjust = stream->flags & SceV2IosAdjustField;
    if (adjust != SceV2IosLeft && adjust != SceV2IosInternal) {
        for (; fillCount != 0; --fillCount) PutSceV2Character(out, fill);
    }
    for (std::size_t index = 0; index < prefix; ++index) PutSceV2Character(out, text[index]);
    if (adjust == SceV2IosInternal) {
        for (; fillCount != 0; --fillCount) PutSceV2Character(out, fill);
    }
    for (std::size_t index = prefix; index < text.size(); ++index) PutSceV2Character(out, text[index]);
    stream->width = 0;
    for (; fillCount != 0; --fillCount) PutSceV2Character(out, fill);
    return out;
}

template<typename Text, typename Punct> std::basic_string<Text> TakeSceV2String(SceV2BasicString<Text>* (APS5_VABI* method)(SceV2BasicString<Text>*, const Punct*), const GuestLocale::Facet* facet) {
    SceV2BasicString<Text> value{};
    method(&value, reinterpret_cast<const Punct*>(facet));
    const bool onHeap = value.capacity > SceV2BasicString<Text>::InlineCapacity;
    std::basic_string<Text> text(onHeap ? value.pointer : value.buffer, value.size);
    if (onHeap) GuestStringStorage::Deallocate(value.pointer, (value.capacity + 1) * sizeof(Text));
    return text;
}

template<typename Text, typename Source> std::basic_string<Text> AsSceV2Text(std::basic_string<Source> text) {
    if constexpr (std::is_same_v<Text, Source>) return text;
    else return std::basic_string<Text>(text.begin(), text.end());
}

template<typename Text> auto* SceV2CtypeUnits(Text* units) {
    if constexpr (std::is_same_v<Text, std::uint16_t>) return reinterpret_cast<char16_t*>(units);
    else return units;
}

template<typename Text> const typename SceV2TextFacets<Text>::Numpunct& SceV2Punctuation(const GuestLocale::Facet* facet) {
    return *reinterpret_cast<const typename SceV2TextFacets<Text>::Numpunct*>(facet->vtable);
}

template<typename Text> const typename SceV2TextFacets<Text>::Punct* AsSceV2Punct(const GuestLocale::Facet* facet) {
    return reinterpret_cast<const typename SceV2TextFacets<Text>::Punct*>(facet);
}

template<typename Text> std::basic_string<Text> WidenSceV2Digits(const GuestLocale::SceV2IosBase* stream, const char* digits, int count) {
    if (count < 0) _ZNSt8__sce_v214_Xlength_errorEPKc_nid_postfix("string too long");
    const GuestLocale::Facet* ctype = UseSceV2Facet(SceV2TextFacets<Text>::CtypeKind(), *stream->locale);
    std::basic_string<Text> text(static_cast<std::size_t>(count), Text{});
    reinterpret_cast<const typename SceV2TextFacets<Text>::Ctype*>(ctype->vtable)->widenRange(ctype, digits, digits + count, SceV2CtypeUnits(text.data()));
    return text;
}

template<typename Text> GuestLocale::OutputIterator PutSceV2IntegerDigits(GuestLocale::OutputIterator out, GuestLocale::SceV2IosBase* stream, Text fill, const char* digits, int count) {
    const std::size_t size = count < 0 ? 0 : static_cast<std::size_t>(count);
    std::size_t prefix = SignPrefix(digits, size);
    if ((stream->flags & SceV2IosBaseField) == SceV2IosHex && HexPrefix(digits, prefix, size)) prefix += 2;
    std::basic_string<Text> text = WidenSceV2Digits<Text>(stream, digits, count);
    const GuestLocale::Facet* punct = UseSceV2Facet(SceV2TextFacets<Text>::NumpunctKind(), *stream->locale);
    const std::string grouping = TakeSceV2String(SceV2Punctuation<Text>(punct).grouping, punct);
    if (static_cast<signed char>(grouping[0]) != CHAR_MAX && static_cast<signed char>(grouping[0]) > 0) InsertSeparators(text, grouping, static_cast<Text>(SceV2Punctuation<Text>(punct).thousandsSep(AsSceV2Punct<Text>(punct))), size, prefix);
    return PutSceV2Field(out, stream, fill, text, prefix);
}

template<typename Text, typename Value> GuestLocale::OutputIterator PutSceV2Integer(GuestLocale::OutputIterator out, GuestLocale::SceV2IosBase* stream, Text fill, const char* qualifier, char decimal, Value value) {
    const std::int32_t flags = stream->flags;
    const std::int32_t base = flags & SceV2IosBaseField;
    const char conversion = base == SceV2IosOct ? 'o' : base != SceV2IosHex ? decimal : (flags & SceV2IosUppercase) != 0 ? 'X' : 'x';
    const std::string format = std::string("%") + ((flags & SceV2IosShowPos) != 0 ? "+" : "") + ((flags & SceV2IosShowBase) != 0 ? "#" : "") + qualifier + conversion;
    char digits[64];
    return PutSceV2IntegerDigits(out, stream, fill, digits, sprintf_s_nid_postfix(digits, sizeof(digits), format.c_str(), value));
}

template<typename Text> GuestLocale::OutputIterator PutSceV2FloatingDigits(GuestLocale::OutputIterator out, GuestLocale::SceV2IosBase* stream, Text fill, const char* digits, int count, bool finite) {
    const std::size_t size = count < 0 ? 0 : static_cast<std::size_t>(count);
    const std::size_t sign = SignPrefix(digits, size);
    const bool hex = (stream->flags & SceV2IosFloatField) == SceV2IosFloatField;
    const std::size_t prefix = hex && HexPrefix(digits, sign, size) ? sign + 2 : sign;
    const std::size_t exponent = std::strcspn(digits, hex ? "pP" : "eE");
    const char point[2] = {localeconv_nid_postfix()->decimal_point[0], '\0'};
    const std::size_t pointOffset = std::strcspn(digits, point);
    std::basic_string<Text> text = WidenSceV2Digits<Text>(stream, digits, count);
    const GuestLocale::Facet* punct = UseSceV2Facet(SceV2TextFacets<Text>::NumpunctKind(), *stream->locale);
    const std::string grouping = TakeSceV2String(SceV2Punctuation<Text>(punct).grouping, punct);
    const Text separator = SceV2Punctuation<Text>(punct).thousandsSep(AsSceV2Punct<Text>(punct));
    if (pointOffset != size) text[pointOffset] = SceV2Punctuation<Text>(punct).decimalPoint(AsSceV2Punct<Text>(punct));
    if (finite) InsertSeparators(text, grouping, separator, pointOffset != size ? pointOffset : exponent, prefix);
    return PutSceV2Field(out, stream, fill, text, prefix);
}

template<typename Text, typename Value> GuestLocale::OutputIterator PutSceV2Floating(GuestLocale::OutputIterator out, GuestLocale::SceV2IosBase* stream, Text fill, const char* qualifier, std::int64_t hexDigits, bool finite, Value value) {
    const std::int32_t field = stream->flags & SceV2IosFloatField;
    std::int64_t size = hexDigits;
    std::int64_t precision = -1;
    if (field != SceV2IosFloatField) {
        precision = stream->precision;
        size = precision > 0 ? static_cast<std::int32_t>(precision) : precision < 0 ? 6 : field == 0 ? 1 : 0;
        if (field == SceV2IosFixed && std::fabs(value) > Value(1e10)) {
            int exponent = 0;
            std::frexp(value, &exponent);
            size += static_cast<std::int64_t>(std::abs(exponent)) * 30103 / 100000;
        }
    }
    size += 50;
    if (size < 0) _ZNSt8__sce_v214_Xlength_errorEPKc_nid_postfix("string too long");
    const std::int32_t flags = finite ? stream->flags : stream->flags & ~SceV2IosShowPoint;
    const char conversion = field == SceV2IosFloatField ? 'a' : field == SceV2IosFixed ? 'f' : field == SceV2IosScientific ? 'e' : 'g';
    const bool upper = (flags & SceV2IosUppercase) != 0 && field != SceV2IosFixed;
    const std::string format = std::string("%") + ((flags & SceV2IosShowPos) != 0 ? "+" : "") + ((flags & SceV2IosShowPoint) != 0 ? "#" : "") + ".*" + qualifier + static_cast<char>(upper ? conversion - 'a' + 'A' : conversion);
    std::string digits(static_cast<std::size_t>(size), '\0');
    const int count = sprintf_s_nid_postfix(digits.data(), digits.size(), format.c_str(), static_cast<int>(precision), value);
    if (count >= size) _ZNSt8__sce_v214_Xlength_errorEPKc_nid_postfix("string too long");
    return PutSceV2FloatingDigits(out, stream, fill, digits.c_str(), count, finite);
}

template<typename Text> GuestLocale::OutputIterator APS5_VABI SceV2NumPutBool_nid_no_patch(const GuestLocale::Facet* self, GuestLocale::OutputIterator out, GuestLocale::SceV2IosBase* stream, Text fill, unsigned char value) {
    if ((stream->flags & SceV2IosBoolAlpha) == 0) return reinterpret_cast<const SceV2NumPutVtable<Text>*>(self->vtable)->putLong(self, out, stream, fill, value);
    const GuestLocale::Facet* punct = UseSceV2Facet(SceV2TextFacets<Text>::NumpunctKind(), *stream->locale);
    return PutSceV2Field(out, stream, fill, AsSceV2Text<Text>(TakeSceV2String(value != 0 ? SceV2Punctuation<Text>(punct).truename : SceV2Punctuation<Text>(punct).falsename, punct)), 0);
}

template<typename Text> GuestLocale::OutputIterator APS5_VABI SceV2NumPutLong_nid_no_patch(const GuestLocale::Facet*, GuestLocale::OutputIterator out, GuestLocale::SceV2IosBase* stream, Text fill, std::int64_t value) {
    return PutSceV2Integer(out, stream, fill, "l", 'd', value);
}

template<typename Text> GuestLocale::OutputIterator APS5_VABI SceV2NumPutUnsignedLong_nid_no_patch(const GuestLocale::Facet*, GuestLocale::OutputIterator out, GuestLocale::SceV2IosBase* stream, Text fill, std::uint64_t value) {
    return PutSceV2Integer(out, stream, fill, "l", 'u', value);
}

template<typename Text> GuestLocale::OutputIterator APS5_VABI SceV2NumPutLongLong_nid_no_patch(const GuestLocale::Facet*, GuestLocale::OutputIterator out, GuestLocale::SceV2IosBase* stream, Text fill, std::int64_t value) {
    return PutSceV2Integer(out, stream, fill, "ll", 'd', value);
}

template<typename Text> GuestLocale::OutputIterator APS5_VABI SceV2NumPutUnsignedLongLong_nid_no_patch(const GuestLocale::Facet*, GuestLocale::OutputIterator out, GuestLocale::SceV2IosBase* stream, Text fill, std::uint64_t value) {
    return PutSceV2Integer(out, stream, fill, "ll", 'u', value);
}

template<typename Text> GuestLocale::OutputIterator APS5_VABI SceV2NumPutDouble_nid_no_patch(const GuestLocale::Facet*, GuestLocale::OutputIterator out, GuestLocale::SceV2IosBase* stream, Text fill, double value) {
    return PutSceV2Floating(out, stream, fill, "", 13, __isfinite_nid_postfix(value) != 0, value);
}

template<typename Text> GuestLocale::OutputIterator APS5_VABI SceV2NumPutLongDouble_nid_no_patch(const GuestLocale::Facet*, GuestLocale::OutputIterator out, GuestLocale::SceV2IosBase* stream, Text fill, long double value) {
    return PutSceV2Floating(out, stream, fill, "L", 16, __isfinitel_nid_postfix(value) != 0, value);
}

template<typename Text> GuestLocale::OutputIterator APS5_VABI SceV2NumPutPointer_nid_no_patch(const GuestLocale::Facet*, GuestLocale::OutputIterator out, GuestLocale::SceV2IosBase* stream, Text fill, const void* value) {
    char digits[64];
    return PutSceV2IntegerDigits(out, stream, fill, digits, sprintf_s_nid_postfix(digits, sizeof(digits), "%p", value));
}

template<typename Text> using SceV2MoneypunctText = std::conditional_t<std::is_same_v<Text, char>, char, std::uint16_t>;

template<typename Text> SceV2FacetKind& SceV2MoneypunctKind(bool international) {
    if constexpr (std::is_same_v<Text, char>) return international ? g_sceV2MoneypunctCharIntl : g_sceV2MoneypunctChar;
    else if constexpr (std::is_same_v<Text, char16_t>) return international ? g_sceV2MoneypunctWideIntl : g_sceV2MoneypunctWide;
    else return international ? g_sceV2MoneypunctUshortIntl : g_sceV2MoneypunctUshort;
}

template<typename Text> void PutSceV2Characters(GuestLocale::OutputIterator& out, const Text* characters, std::size_t count) {
    for (std::size_t index = 0; index < count; ++index) PutSceV2Character(out, characters[index]);
}

template<typename Text> void RepeatSceV2Character(GuestLocale::OutputIterator& out, Text character, std::size_t count) {
    for (; count != 0; --count) PutSceV2Character(out, character);
}

constexpr char SceV2MoneySymbol = '$';
constexpr char SceV2MoneySign = '+';
constexpr char SceV2MoneyValue = 'v';
constexpr char SceV2MoneyNone = 'x';
constexpr char SceV2MoneySpace = ' ';

template<typename Text> GuestLocale::OutputIterator PutSceV2Money(GuestLocale::OutputIterator out, bool international, GuestLocale::SceV2IosBase* stream, Text fill, bool negative, std::basic_string<Text> value, Text zero) {
    using PunctText = SceV2MoneypunctText<Text>;
    const GuestLocale::Facet* facet = UseSceV2Facet(SceV2MoneypunctKind<Text>(international), *stream->locale);
    const auto& punct = *reinterpret_cast<const SceV2MoneypunctVtable<PunctText>*>(facet->vtable);
    const auto* fields = reinterpret_cast<const SceV2MoneypunctFacet<PunctText>*>(facet);
    const std::string grouping = TakeSceV2String(punct.grouping, facet);
    const int signedDigits = punct.fractionDigits(fields);
    const std::size_t fractionDigits = signedDigits < 0 ? 0u - static_cast<std::uint32_t>(signedDigits) : static_cast<std::uint32_t>(signedDigits);
    const auto validGroup = [](char group) { return static_cast<unsigned char>(group - 1) < CHAR_MAX - 1; };
    if (value.size() <= fractionDigits) {
        value.insert(0, fractionDigits - value.size() + 1, zero);
    } else if (validGroup(grouping[0])) {
        const Text separator = static_cast<Text>(punct.thousandsSep(fields));
        const char* group = grouping.c_str();
        std::size_t offset = value.size() - fractionDigits;
        while (validGroup(*group) && static_cast<unsigned char>(*group) < offset) {
            offset -= static_cast<unsigned char>(*group);
            value.insert(offset, 1, separator);
            if (static_cast<signed char>(group[1]) > 0) ++group;
        }
    }
    const std::uint32_t pattern = negative ? punct.negativeFormat(fields) : punct.positiveFormat(fields);
    const std::basic_string<Text> sign = AsSceV2Text<Text>(TakeSceV2String(negative ? punct.negativeSign : punct.positiveSign, facet));
    std::basic_string<Text> symbol;
    if ((stream->flags & SceV2IosShowBase) != 0) symbol = AsSceV2Text<Text>(TakeSceV2String(punct.currencySymbol, facet));
    char field[4];
    std::memcpy(field, &pattern, sizeof(field));
    const std::size_t valueWidth = value.size() + (signedDigits != 0 ? 1 : 0) + (value.size() <= fractionDigits ? fractionDigits - value.size() + 1 : 0);
    bool internalFill = false;
    std::size_t length = 0;
    for (std::size_t index = 0; index < 4; ++index) {
        switch (field[index]) {
            case SceV2MoneySymbol: length += symbol.size(); break;
            case SceV2MoneySign: length += sign.size(); break;
            case SceV2MoneyValue: length += valueWidth; break;
            case SceV2MoneySpace: ++length; [[fallthrough]];
            case SceV2MoneyNone: if (index != 3) internalFill = true; break;
            default: break;
        }
    }
    const std::int64_t width = stream->width;
    std::size_t fillCount = width <= 0 || static_cast<std::size_t>(width) <= length ? 0 : static_cast<std::size_t>(width) - length;
    const std::int32_t adjust = stream->flags & SceV2IosAdjustField;
    if (adjust != SceV2IosLeft && (adjust != SceV2IosInternal || !internalFill)) {
        RepeatSceV2Character(out, fill, fillCount);
        fillCount = 0;
    }
    for (std::size_t index = 0; index < 4; ++index) {
        switch (field[index]) {
            case SceV2MoneySymbol: PutSceV2Characters(out, symbol.data(), symbol.size()); break;
            case SceV2MoneySign: if (!sign.empty()) PutSceV2Character(out, sign[0]); break;
            case SceV2MoneyValue:
                if (signedDigits == 0) {
                    PutSceV2Characters(out, value.data(), value.size());
                } else if (value.size() <= fractionDigits) {
                    PutSceV2Character(out, zero);
                    PutSceV2Character(out, static_cast<Text>(punct.decimalPoint(fields)));
                    RepeatSceV2Character(out, zero, fractionDigits - value.size());
                    PutSceV2Characters(out, value.data(), value.size());
                } else {
                    PutSceV2Characters(out, value.data(), value.size() - fractionDigits);
                    PutSceV2Character(out, static_cast<Text>(punct.decimalPoint(fields)));
                    PutSceV2Characters(out, value.data() + value.size() - fractionDigits, fractionDigits);
                }
                break;
            case SceV2MoneySpace: PutSceV2Character(out, fill); [[fallthrough]];
            case SceV2MoneyNone:
                if (adjust == SceV2IosInternal) {
                    RepeatSceV2Character(out, fill, fillCount);
                    fillCount = 0;
                }
                break;
            default: break;
        }
    }
    if (sign.size() > 1) PutSceV2Characters(out, sign.data() + 1, sign.size() - 1);
    stream->width = 0;
    RepeatSceV2Character(out, fill, fillCount);
    return out;
}

template<typename Text> GuestLocale::OutputIterator APS5_VABI SceV2MoneyPutLongDouble_nid_no_patch(const GuestLocale::Facet*, GuestLocale::OutputIterator out, bool international, GuestLocale::SceV2IosBase* stream, Text fill, long double value) {
    const bool negative = 0 > value;
    long double magnitude = -value > value ? -value : value;
    std::size_t exponent = 0;
    while (magnitude >= static_cast<long double>(1e35) && exponent < 5000) {
        magnitude /= 1e10f;
        exponent += 10;
    }
    char digits[40];
    const int count = sprintf_s_nid_postfix(digits, sizeof(digits), "%.0Lf", magnitude);
    if (count < 0) return out;
    const GuestLocale::Facet* ctype = UseSceV2Facet(SceV2TextFacets<Text>::CtypeKind(), *stream->locale);
    const auto& virtuals = *reinterpret_cast<const typename SceV2TextFacets<Text>::Ctype*>(ctype->vtable);
    const Text zero = virtuals.widen(ctype, '0');
    std::basic_string<Text> text(static_cast<std::size_t>(count), Text{});
    virtuals.widenRange(ctype, digits, digits + count, SceV2CtypeUnits(text.data()));
    text.append(exponent, zero);
    return PutSceV2Money(out, international, stream, fill, negative, std::move(text), zero);
}

template<typename Text> GuestLocale::OutputIterator APS5_VABI SceV2MoneyPutDigits_nid_no_patch(const GuestLocale::Facet*, GuestLocale::OutputIterator out, bool international, GuestLocale::SceV2IosBase* stream, Text fill, const SceV2BasicString<Text>* digits) {
    static constexpr char Source[] = "0123456789-";
    constexpr std::size_t MinusAtom = 10;
    const GuestLocale::Facet* ctype = UseSceV2Facet(SceV2TextFacets<Text>::CtypeKind(), *stream->locale);
    Text atoms[sizeof(Source)];
    reinterpret_cast<const typename SceV2TextFacets<Text>::Ctype*>(ctype->vtable)->widenRange(ctype, Source, Source + sizeof(Source), SceV2CtypeUnits(atoms));
    const Text* data = digits->capacity > SceV2BasicString<Text>::InlineCapacity ? digits->pointer : digits->buffer;
    const std::size_t size = digits->size;
    const bool negative = size != 0 && data[0] == atoms[MinusAtom];
    const std::size_t start = negative ? 1 : 0;
    std::size_t end = start;
    while (end < size && std::find(atoms, atoms + MinusAtom + 1, data[end]) - atoms < static_cast<std::ptrdiff_t>(MinusAtom)) ++end;
    std::basic_string<Text> value(data + start, end - start);
    if (value.empty()) value.push_back(atoms[0]);
    return PutSceV2Money(out, international, stream, fill, negative, std::move(value), atoms[0]);
}

constexpr char g_sceV2IntegerAtoms[] = "0123456789ABCDEFabcdef-+Xx";
constexpr std::size_t SceV2AtomZero = 0;
constexpr std::size_t SceV2AtomMinus = 22;
constexpr std::size_t SceV2AtomPlus = 23;
constexpr std::size_t SceV2AtomUpperX = 24;
constexpr std::size_t SceV2AtomLowerX = 25;
constexpr std::size_t SceV2AtomCount = 26;
constexpr std::size_t SceV2IntegerDigits = 32;
constexpr std::uint32_t SceV2BadGrouping = 0x100;
constexpr std::int32_t SceV2IosEofBit = 0x1;
constexpr std::int32_t SceV2IosFailBit = 0x2;

struct SceV2InputStreambufVtable {
    const void* reserved[7];
    int (APS5_VABI *underflow)(GuestLocale::Streambuf* self);
    int (APS5_VABI *uflow)(GuestLocale::Streambuf* self);
};

const SceV2InputStreambufVtable& SceV2InputVirtuals(const GuestLocale::Streambuf* buffer) {
    return *reinterpret_cast<const SceV2InputStreambufVtable*>(buffer->vtable);
}

char** SceV2GetNext(const GuestLocale::Streambuf* buffer) {
    return static_cast<char**>(const_cast<void*>(buffer->reserved[6]));
}

std::int32_t* SceV2GetCount(const GuestLocale::Streambuf* buffer) {
    return static_cast<std::int32_t*>(const_cast<void*>(buffer->reservedCounts[1]));
}

template<typename Text> Text PeekSceV2Character(SceV2InputIterator<Text>& in) {
    if (in.got) return in.value;
    if (in.buffer != nullptr) {
        const auto* next = reinterpret_cast<const Text*>(*SceV2GetNext(in.buffer));
        if (next != nullptr && *SceV2GetCount(in.buffer) > 0) {
            in.value = *next;
        } else {
            const int character = SceV2InputVirtuals(in.buffer).underflow(in.buffer);
            if (character == -1) in.buffer = nullptr;
            else in.value = static_cast<Text>(character);
        }
    }
    in.got = true;
    return in.value;
}

template<typename Text> void AdvanceSceV2Character(SceV2InputIterator<Text>& in) {
    if (in.buffer != nullptr) {
        char** next = SceV2GetNext(in.buffer);
        std::int32_t* count = SceV2GetCount(in.buffer);
        if (*next != nullptr && *count > 0) {
            --*count;
            *next += sizeof(Text);
            in.got = false;
            return;
        }
        if (SceV2InputVirtuals(in.buffer).uflow(in.buffer) != -1) {
            in.got = false;
            return;
        }
        in.buffer = nullptr;
    }
    in.got = true;
}

template<typename Text> bool SceV2InputEnded(SceV2InputIterator<Text>& first, SceV2InputIterator<Text>& last) {
    PeekSceV2Character(first);
    PeekSceV2Character(last);
    return (first.buffer == nullptr) == (last.buffer == nullptr);
}

template<typename Text> SceV2InputIterator<Text> FinishSceV2Get(SceV2InputIterator<Text> first, SceV2InputIterator<Text> last, std::int32_t* state) {
    if (SceV2InputEnded(first, last)) *state |= SceV2IosEofBit;
    return first;
}

template<typename Text> bool SkipSceV2Separators(SceV2InputIterator<Text>& first, SceV2InputIterator<Text>& last, Text separator) {
    bool skipped = false;
    if (separator == Text{}) return skipped;
    while (!SceV2InputEnded(first, last) && PeekSceV2Character(first) == separator) {
        AdvanceSceV2Character(first);
        skipped = true;
    }
    return skipped;
}

bool SceV2GroupsMatch(const std::string& grouping, const std::string& groups, std::size_t count) {
    std::size_t next = 0;
    unsigned char size = 0;
    for (;;) {
        if (next != grouping.size()) size = static_cast<unsigned char>(grouping[next++]);
        if (static_cast<unsigned char>(size - 1) <= 0x7d) {
            if (count == 1) return static_cast<signed char>(groups[0]) <= static_cast<signed char>(size);
            if (static_cast<unsigned char>(groups[count - 1]) != size) return false;
        }
        if (--count == 0) return true;
    }
}

template<typename Text> std::uint32_t GetSceV2IntegerField(char* digits, SceV2InputIterator<Text>& first, SceV2InputIterator<Text>& last, std::int32_t flags, const GuestLocale::Implementation* locale) {
    const GuestLocale::Facet* punct = UseSceV2Facet(SceV2TextFacets<Text>::NumpunctKind(), locale);
    const std::string grouping = TakeSceV2String(SceV2Punctuation<Text>(punct).grouping, punct);
    const Text separator = grouping.empty() ? Text{} : SceV2Punctuation<Text>(punct).thousandsSep(AsSceV2Punct<Text>(punct));
    const GuestLocale::Facet* ctype = UseSceV2Facet(SceV2TextFacets<Text>::CtypeKind(), locale);
    Text atoms[sizeof(g_sceV2IntegerAtoms)];
    reinterpret_cast<const typename SceV2TextFacets<Text>::Ctype*>(ctype->vtable)->widenRange(ctype, g_sceV2IntegerAtoms, g_sceV2IntegerAtoms + sizeof(g_sceV2IntegerAtoms), SceV2CtypeUnits(atoms));
    bool bad = SkipSceV2Separators(first, last, separator);
    std::size_t length = 0;
    if (!SceV2InputEnded(first, last)) {
        const Text sign = PeekSceV2Character(first);
        if (sign == atoms[SceV2AtomPlus] || sign == atoms[SceV2AtomMinus]) {
            digits[length++] = sign == atoms[SceV2AtomPlus] ? '+' : '-';
            AdvanceSceV2Character(first);
        }
    }
    bad = SkipSceV2Separators(first, last, separator) || bad;
    const std::int32_t field = flags & SceV2IosBaseField;
    const bool prefixAllowed = field == 0 || field == SceV2IosHex;
    int base = field == 0 ? 0 : field == SceV2IosOct ? 8 : field == SceV2IosHex ? 16 : 10;
    bool seen = false;
    if (!SceV2InputEnded(first, last) && PeekSceV2Character(first) == atoms[SceV2AtomZero]) {
        AdvanceSceV2Character(first);
        seen = true;
        if (prefixAllowed && !SceV2InputEnded(first, last) && (PeekSceV2Character(first) == atoms[SceV2AtomLowerX] || PeekSceV2Character(first) == atoms[SceV2AtomUpperX])) {
            AdvanceSceV2Character(first);
            base = 16;
            seen = false;
        } else if (field == 0) {
            base = 8;
        }
    }
    const std::size_t digitLimit = base == 8 ? 8 : base == 16 ? 22 : 10;
    std::string groups(1, static_cast<char>(seen));
    std::size_t group = 0;
    bool nonzero = false;
    while (!SceV2InputEnded(first, last)) {
        const Text character = PeekSceV2Character(first);
        const auto index = static_cast<std::size_t>(std::find(atoms, atoms + SceV2AtomCount, character) - atoms);
        if (index < digitLimit) {
            digits[length] = g_sceV2IntegerAtoms[index];
            if ((nonzero || index != SceV2AtomZero) && length < SceV2IntegerDigits - 1) {
                ++length;
                nonzero = true;
            }
            seen = true;
            if (groups[group] != CHAR_MAX) ++groups[group];
        } else if (separator == Text{} || character != separator) {
            break;
        } else if (groups[group] == '\0') {
            bad = true;
        } else {
            groups.push_back('\0');
            ++group;
        }
        AdvanceSceV2Character(first);
    }
    std::size_t count = 0;
    if (group != 0) {
        count = groups[group] > 0 ? group + 1 : group;
        if (groups[group] <= 0) bad = true;
    }
    if (seen && !bad && count != 0) bad = !SceV2GroupsMatch(grouping, groups, count);
    if (!seen) return static_cast<unsigned char>(~base);
    if (!nonzero) digits[length++] = '0';
    digits[length] = '\0';
    return static_cast<unsigned char>(base) | (bad ? SceV2BadGrouping : 0);
}

int SceV2FieldBase(std::uint32_t field) {
    return static_cast<signed char>(field & 0xff);
}

template<typename Text, typename Name> bool SceV2LocaleTextMatches(SceV2InputIterator<Text>& first, Name name, const GuestLocale::Facet* ctype) {
    if (ctype == nullptr) {
        if constexpr (std::is_same_v<Text, Name>) return name == PeekSceV2Character(first);
        else return static_cast<std::int32_t>(name) == static_cast<std::int32_t>(static_cast<std::make_unsigned_t<Text>>(PeekSceV2Character(first)));
    }
    const auto& virtuals = *reinterpret_cast<const typename SceV2TextFacets<Name>::Ctype*>(ctype->vtable);
    const Name lowered = virtuals.toLower(ctype, name);
    return lowered == virtuals.toLower(ctype, static_cast<Name>(PeekSceV2Character(first)));
}

template<typename Text, typename Name> int GetSceV2LocaleText(SceV2InputIterator<Text>& first, SceV2InputIterator<Text>& last, std::size_t fields, const Name* names, bool caseSensitive = true) {
    for (std::size_t offset = 0; names[0] != Name{} && names[offset] != Name{}; ++offset) {
        if (names[offset] == names[0]) ++fields;
    }
    const GuestLocale::Facet* ctype = nullptr;
    if (!caseSensitive) {
        auto* global = _ZNSt8__sce_v26locale5_InitEb_nid_postfix(true);
        ctype = UseSceV2Facet(SceV2TextFacets<Name>::CtypeKind(), global);
        ReleaseSceV2Facet(reinterpret_cast<GuestLocale::Facet*>(global));
    }
    if (fields == 0) return -2;
    std::string columns(fields, '\0');
    int answer = -2;
    for (std::size_t column = 1;; ++column) {
        const char matched = static_cast<char>(column < 127 ? column : 127);
        bool prefix = false;
        std::size_t offset = 0;
        for (std::size_t field = 0; field < fields; ++field) {
            while (names[offset] != Text{} && names[offset] != names[0]) ++offset;
            if (columns[field] != '\0') {
                offset += static_cast<std::size_t>(static_cast<signed char>(columns[field]));
            } else if (names[offset += column] == Name{} || names[offset] == names[0]) {
                columns[field] = matched;
                answer = static_cast<int>(field);
            } else if (SceV2InputEnded(first, last) || !SceV2LocaleTextMatches(first, names[offset], ctype)) {
                columns[field] = matched;
            } else {
                prefix = true;
            }
        }
        if (!prefix || SceV2InputEnded(first, last)) return answer;
        AdvanceSceV2Character(first);
        answer = -1;
    }
}

template<typename Text> SceV2InputIterator<Text> APS5_VABI SceV2NumGetBool_nid_no_patch(const GuestLocale::Facet*, SceV2InputIterator<Text> first, SceV2InputIterator<Text> last, GuestLocale::SceV2IosBase* stream, std::int32_t* state, bool* value) {
    if ((stream->flags & SceV2IosBoolAlpha) == 0) {
        char digits[SceV2IntegerDigits];
        const std::uint32_t field = GetSceV2IntegerField(digits, first, last, stream->flags, *stream->locale);
        if (SceV2FieldBase(field) < 0) {
            *value = false;
            *state = SceV2IosFailBit;
            return FinishSceV2Get(first, last, state);
        }
        char* end = nullptr;
        int error = 0;
        const std::int64_t parsed = _ZNSt8__sce_v26_StolxEPKcPPciPi_nid_postfix(digits, &end, SceV2FieldBase(field), &error);
        if ((field & SceV2BadGrouping) != 0 || end == digits || error != 0) {
            *value = true;
            *state = SceV2IosFailBit;
        } else {
            *value = parsed != 0;
            if (static_cast<std::uint64_t>(parsed) >= 2) *state = SceV2IosFailBit;
        }
        return FinishSceV2Get(first, last, state);
    }
    const GuestLocale::Facet* punct = UseSceV2Facet(SceV2TextFacets<Text>::NumpunctKind(), *stream->locale);
    std::basic_string<Text> names(1, Text{});
    names += AsSceV2Text<Text>(TakeSceV2String(SceV2Punctuation<Text>(punct).falsename, punct));
    names.push_back(Text{});
    names += AsSceV2Text<Text>(TakeSceV2String(SceV2Punctuation<Text>(punct).truename, punct));
    const int match = GetSceV2LocaleText(first, last, 2, names.c_str());
    *value = match == 1;
    if (match != 0 && match != 1) *state = SceV2IosFailBit;
    return FinishSceV2Get(first, last, state);
}

template<typename Text> SceV2InputIterator<Text> APS5_VABI SceV2NumGetUnsignedShort_nid_no_patch(const GuestLocale::Facet*, SceV2InputIterator<Text> first, SceV2InputIterator<Text> last, GuestLocale::SceV2IosBase* stream, std::int32_t* state, std::uint16_t* value) {
    char digits[SceV2IntegerDigits];
    const std::uint32_t field = GetSceV2IntegerField(digits, first, last, stream->flags, *stream->locale);
    if (SceV2FieldBase(field) < 0) {
        *state = SceV2IosFailBit;
        *value = 0;
        return FinishSceV2Get(first, last, state);
    }
    const bool negative = digits[0] == '-';
    char* start = digits + (negative ? 1 : 0);
    char* end = nullptr;
    int error = 0;
    const std::uint64_t parsed = _ZNSt8__sce_v27_StoulxEPKcPPciPi_nid_postfix(start, &end, SceV2FieldBase(field), &error);
    if (parsed > 0xffff || end == start || error != 0) {
        *state = SceV2IosFailBit;
        *value = 0xffff;
    } else {
        *value = static_cast<std::uint16_t>(negative ? 0 - parsed : parsed);
    }
    if ((field & SceV2BadGrouping) != 0) *state = SceV2IosFailBit;
    return FinishSceV2Get(first, last, state);
}

template<typename Text> SceV2InputIterator<Text> APS5_VABI SceV2NumGetUnsignedInt_nid_no_patch(const GuestLocale::Facet*, SceV2InputIterator<Text> first, SceV2InputIterator<Text> last, GuestLocale::SceV2IosBase* stream, std::int32_t* state, std::uint32_t* value) {
    char digits[SceV2IntegerDigits];
    const std::uint32_t field = GetSceV2IntegerField(digits, first, last, stream->flags, *stream->locale);
    if (SceV2FieldBase(field) < 0 || (field & SceV2BadGrouping) != 0) digits[0] = '\0';
    if (digits[0] == '\0') {
        *state = SceV2IosFailBit;
        *value = 0;
        return FinishSceV2Get(first, last, state);
    }
    char* end = nullptr;
    int error = 0;
    *value = static_cast<std::uint32_t>(_ZNSt8__sce_v27_StoulxEPKcPPciPi_nid_postfix(digits, &end, SceV2FieldBase(field), &error));
    if (end == digits || error != 0) *state = SceV2IosFailBit;
    return FinishSceV2Get(first, last, state);
}

template<typename Text> SceV2InputIterator<Text> APS5_VABI SceV2NumGetLongLong_nid_no_patch(const GuestLocale::Facet*, SceV2InputIterator<Text> first, SceV2InputIterator<Text> last, GuestLocale::SceV2IosBase* stream, std::int32_t* state, std::int64_t* value) {
    char digits[SceV2IntegerDigits];
    const std::uint32_t field = GetSceV2IntegerField(digits, first, last, stream->flags, *stream->locale);
    if (SceV2FieldBase(field) < 0) {
        *state = SceV2IosFailBit;
        *value = 0;
        return FinishSceV2Get(first, last, state);
    }
    char* end = nullptr;
    int error = 0;
    *value = _ZNSt8__sce_v27_StollxEPKcPPciPi_nid_postfix(digits, &end, SceV2FieldBase(field), &error);
    if ((field & SceV2BadGrouping) != 0 || end == digits || error != 0) *state = SceV2IosFailBit;
    return FinishSceV2Get(first, last, state);
}

template<typename Text> SceV2InputIterator<Text> APS5_VABI SceV2NumGetUnsignedLongLong_nid_no_patch(const GuestLocale::Facet*, SceV2InputIterator<Text> first, SceV2InputIterator<Text> last, GuestLocale::SceV2IosBase* stream, std::int32_t* state, std::uint64_t* value) {
    char digits[SceV2IntegerDigits];
    const std::uint32_t field = GetSceV2IntegerField(digits, first, last, stream->flags, *stream->locale);
    if (SceV2FieldBase(field) < 0) {
        *state = SceV2IosFailBit;
        *value = 0;
        return FinishSceV2Get(first, last, state);
    }
    char* end = nullptr;
    int error = 0;
    *value = _ZNSt8__sce_v28_StoullxEPKcPPciPi_nid_postfix(digits, &end, SceV2FieldBase(field), &error);
    if ((field & SceV2BadGrouping) != 0 || end == digits || error != 0) *state = SceV2IosFailBit;
    return FinishSceV2Get(first, last, state);
}

template<typename Text> SceV2InputIterator<Text> APS5_VABI SceV2NumGetLong_nid_no_patch(const GuestLocale::Facet* self, SceV2InputIterator<Text> first, SceV2InputIterator<Text> last, GuestLocale::SceV2IosBase* stream, std::int32_t* state, std::int64_t* value) {
    return SceV2NumGetLongLong_nid_no_patch<Text>(self, first, last, stream, state, value);
}

template<typename Text> SceV2InputIterator<Text> APS5_VABI SceV2NumGetUnsignedLong_nid_no_patch(const GuestLocale::Facet* self, SceV2InputIterator<Text> first, SceV2InputIterator<Text> last, GuestLocale::SceV2IosBase* stream, std::int32_t* state, std::uint64_t* value) {
    return SceV2NumGetUnsignedLongLong_nid_no_patch<Text>(self, first, last, stream, state, value);
}

template<typename Text> SceV2InputIterator<Text> APS5_VABI SceV2NumGetPointer_nid_no_patch(const GuestLocale::Facet*, SceV2InputIterator<Text> first, SceV2InputIterator<Text> last, GuestLocale::SceV2IosBase* stream, std::int32_t* state, void** value) {
    char digits[SceV2IntegerDigits];
    const std::uint32_t field = GetSceV2IntegerField(digits, first, last, SceV2IosHex, *stream->locale);
    if (SceV2FieldBase(field) < 0) {
        *state = SceV2IosFailBit;
        *value = nullptr;
        return FinishSceV2Get(first, last, state);
    }
    char* end = nullptr;
    int error = 0;
    *value = reinterpret_cast<void*>(static_cast<std::uintptr_t>(_ZNSt8__sce_v28_StoullxEPKcPPciPi_nid_postfix(digits, &end, SceV2FieldBase(field), &error)));
    if (end == digits || error != 0) {
        *state = SceV2IosFailBit;
        *value = nullptr;
    }
    if ((field & SceV2BadGrouping) != 0) *state = SceV2IosFailBit;
    return FinishSceV2Get(first, last, state);
}

constexpr char g_sceV2FloatingAtoms[] = "0123456789ABCDEFabcdef-+XxPp";
constexpr char g_sceV2DecimalAtoms[] = "0123456789-+Ee";
constexpr std::size_t SceV2FloatUpperE = 14;
constexpr std::size_t SceV2FloatLowerE = 20;
constexpr std::size_t SceV2FloatUpperP = 26;
constexpr std::size_t SceV2FloatLowerP = 27;
constexpr std::size_t SceV2DecimalMinus = 10;
constexpr std::size_t SceV2DecimalPlus = 11;
constexpr std::size_t SceV2DecimalUpperE = 12;
constexpr std::size_t SceV2DecimalLowerE = 13;
constexpr int SceV2FloatSignificant = 768;
constexpr std::size_t SceV2FloatDigits = SceV2FloatSignificant + 32;
constexpr int SceV2ClassicSignificant = 48;
constexpr int SceV2ClassicExponent = 8;
constexpr int SceV2HexSignificant = 36;

template<typename Text> std::size_t FindSceV2Atom(const Text* atoms, std::size_t count, Text character) {
    return static_cast<std::size_t>(std::find(atoms, atoms + count, character) - atoms);
}

template<typename Text> bool NextSceV2Is(SceV2InputIterator<Text>& first, SceV2InputIterator<Text>& last, Text character) {
    return !SceV2InputEnded(first, last) && PeekSceV2Character(first) == character;
}

template<typename Text> std::uint32_t GetSceV2FloatingField(char* digits, int maxSignificant, SceV2InputIterator<Text>& first, SceV2InputIterator<Text>& last, const GuestLocale::Implementation* locale) {
    const GuestLocale::Facet* ctype = UseSceV2Facet(SceV2TextFacets<Text>::CtypeKind(), locale);
    Text atoms[sizeof(g_sceV2FloatingAtoms)];
    reinterpret_cast<const typename SceV2TextFacets<Text>::Ctype*>(ctype->vtable)->widenRange(ctype, g_sceV2FloatingAtoms, g_sceV2FloatingAtoms + sizeof(g_sceV2FloatingAtoms), SceV2CtypeUnits(atoms));
    const GuestLocale::Facet* punct = UseSceV2Facet(SceV2TextFacets<Text>::NumpunctKind(), locale);
    const std::string grouping = TakeSceV2String(SceV2Punctuation<Text>(punct).grouping, punct);
    const Text separator = grouping.empty() ? Text{} : SceV2Punctuation<Text>(punct).thousandsSep(AsSceV2Punct<Text>(punct));
    bool bad = false;
    if (!grouping.empty()) {
        while (NextSceV2Is(first, last, separator)) {
            AdvanceSceV2Character(first);
            bad = true;
        }
    }
    char* out = digits;
    if (!SceV2InputEnded(first, last)) {
        const Text sign = PeekSceV2Character(first);
        if (sign == atoms[SceV2AtomPlus] || sign == atoms[SceV2AtomMinus]) {
            *out++ = sign == atoms[SceV2AtomPlus] ? '+' : '-';
            AdvanceSceV2Character(first);
        }
    }
    *out++ = '0';
    bool hex = false;
    bool seen = false;
    char leadingZero = '\0';
    if (NextSceV2Is(first, last, atoms[SceV2AtomZero])) {
        AdvanceSceV2Character(first);
        if (SceV2InputEnded(first, last)) {
            *out = '\0';
            return 10 | (bad ? SceV2BadGrouping : 0);
        }
        const Text next = PeekSceV2Character(first);
        if (next == atoms[SceV2AtomLowerX] || next == atoms[SceV2AtomUpperX]) {
            hex = true;
            AdvanceSceV2Character(first);
            *out++ = 'x';
        } else {
            seen = true;
            ++leadingZero;
        }
    }
    bool lost = false;
    int significant = 0;
    std::int64_t power = 0;
    const std::size_t digitLimit = hex ? 22 : 10;
    const auto takeDigit = [&](std::size_t index) {
        if (significant >= maxSignificant) {
            ++power;
            if (index > 0) lost = true;
        } else if (index != 0 || significant != 0) {
            *out++ = g_sceV2FloatingAtoms[index];
            ++significant;
        }
    };
    if (grouping.empty()) {
        std::size_t index = 0;
        while (!SceV2InputEnded(first, last) && (index = FindSceV2Atom(atoms, SceV2AtomCount + 2, PeekSceV2Character(first))) < digitLimit) {
            takeDigit(index);
            seen = true;
            AdvanceSceV2Character(first);
        }
    } else {
        while (NextSceV2Is(first, last, separator)) {
            AdvanceSceV2Character(first);
            bad = true;
        }
        std::string groups(1, leadingZero);
        std::size_t group = 0;
        while (!SceV2InputEnded(first, last)) {
            const Text character = PeekSceV2Character(first);
            const std::size_t index = FindSceV2Atom(atoms, SceV2AtomCount + 2, character);
            if (index < digitLimit) {
                seen = true;
                takeDigit(index);
                if (groups[group] != CHAR_MAX) ++groups[group];
            } else if (character != separator) {
                break;
            } else if (groups[group] == '\0') {
                bad = true;
            } else {
                groups.push_back('\0');
                ++group;
            }
            AdvanceSceV2Character(first);
        }
        if (group != 0) {
            if (groups[group] > '\0') ++group;
            else bad = true;
        }
        while (NextSceV2Is(first, last, separator)) {
            AdvanceSceV2Character(first);
            bad = true;
        }
        std::size_t next = 0;
        char size = '\0';
        while (!bad && group > 0) {
            if (next != grouping.size()) size = grouping[next++];
            --group;
            if (size > '\0' && size != CHAR_MAX && ((group > 0 && groups[group] != size) || (group == 0 && groups[group] > size))) bad = true;
        }
    }
    if (hex && seen && significant == 0) *out++ = '0';
    const char point = localeconv_nid_postfix()->decimal_point[0];
    if (!SceV2InputEnded(first, last) && PeekSceV2Character(first) == SceV2Punctuation<Text>(punct).decimalPoint(AsSceV2Punct<Text>(punct))) {
        *out++ = point;
        AdvanceSceV2Character(first);
    }
    if (significant == 0) {
        while (NextSceV2Is(first, last, atoms[SceV2AtomZero])) {
            --power;
            seen = true;
            AdvanceSceV2Character(first);
        }
    }
    std::size_t index = 0;
    while (!SceV2InputEnded(first, last) && (index = FindSceV2Atom(atoms, SceV2AtomCount + 2, PeekSceV2Character(first))) < digitLimit) {
        if (significant < maxSignificant) {
            *out++ = g_sceV2FloatingAtoms[index];
            ++significant;
        } else if (index > 0) {
            lost = true;
        }
        seen = true;
        AdvanceSceV2Character(first);
    }
    if (lost) {
        char& lastDigit = out[-1] == point ? out[-2] : out[-1];
        if (lastDigit == '0' || lastDigit == (hex ? '8' : '5')) ++lastDigit;
    }
    const Text lowerExponent = atoms[hex ? SceV2FloatLowerP : SceV2FloatLowerE];
    const Text upperExponent = atoms[hex ? SceV2FloatUpperP : SceV2FloatUpperE];
    std::int64_t exponent = 0;
    if (seen && !SceV2InputEnded(first, last) && (PeekSceV2Character(first) == lowerExponent || PeekSceV2Character(first) == upperExponent)) {
        AdvanceSceV2Character(first);
        seen = false;
        bool negative = false;
        if (!SceV2InputEnded(first, last)) {
            const Text sign = PeekSceV2Character(first);
            if (sign == atoms[SceV2AtomPlus]) {
                AdvanceSceV2Character(first);
            } else if (sign == atoms[SceV2AtomMinus]) {
                negative = true;
                AdvanceSceV2Character(first);
            }
        }
        while (NextSceV2Is(first, last, atoms[SceV2AtomZero])) {
            seen = true;
            AdvanceSceV2Character(first);
        }
        while (!SceV2InputEnded(first, last) && (index = FindSceV2Atom(atoms, SceV2AtomCount + 2, PeekSceV2Character(first))) < 10) {
            constexpr std::int64_t limit = std::numeric_limits<std::int64_t>::max();
            if (exponent < limit / 10 || (exponent == limit / 10 && static_cast<std::int64_t>(index) <= limit % 10)) exponent = exponent * 10 + static_cast<std::int64_t>(index);
            else exponent = limit;
            seen = true;
            AdvanceSceV2Character(first);
        }
        if (negative) exponent = -exponent;
    }
    if (!seen) return 0;
    const std::int64_t bound = hex ? 4200 : 1100;
    const std::int64_t powerBound = hex ? 4200 / 4 : 1100;
    for (std::int64_t adjusted = power;;) {
        if (exponent >= 0 && adjusted >= 0 && (exponent >= bound || adjusted >= powerBound)) {
            exponent = bound;
            break;
        }
        if (exponent <= 0 && adjusted <= 0 && (exponent <= -bound || adjusted <= -powerBound)) {
            exponent = -bound;
            break;
        }
        if (std::llabs(exponent) <= bound && std::llabs(adjusted) <= powerBound) {
            exponent += hex ? adjusted * 4 : adjusted;
            exponent = std::clamp(exponent, -bound, bound);
            break;
        }
        const std::int64_t roundedUp = hex ? (std::llabs(exponent) - 1) / 4 + 1 : std::llabs(exponent);
        const std::int64_t adjustment = std::min<std::int64_t>(roundedUp, std::llabs(power));
        if (exponent >= 0) {
            exponent -= hex ? adjustment * 4 : adjustment;
            adjusted += adjustment;
        } else {
            exponent += hex ? adjustment * 4 : adjustment;
            adjusted -= adjustment;
        }
    }
    if (exponent != 0) {
        *out++ = hex ? 'p' : 'e';
        if (exponent < 0) *out++ = '-';
        char* reversed = out;
        for (std::int64_t magnitude = std::llabs(exponent); magnitude != 0; magnitude /= 10) *out++ = static_cast<char>('0' + magnitude % 10);
        std::reverse(reversed, out);
    }
    *out = '\0';
    return (hex ? 16u : 10u) | (bad ? SceV2BadGrouping : 0);
}

bool SceV2ClassicGroupsMatch(const std::string& grouping, const std::string& groups, std::size_t count) {
    const char* size = grouping.c_str();
    while (count > 0) {
        if (*size == CHAR_MAX) return true;
        --count;
        if ((count > 0 && *size != groups[count]) || (count == 0 && *size < groups[count])) return false;
        if (size[1] > '\0') ++size;
    }
    return true;
}

template<typename Text> int GetSceV2ClassicFloatingField(char* digits, SceV2InputIterator<Text>& first, SceV2InputIterator<Text>& last, const GuestLocale::SceV2IosBase* stream) {
    const GuestLocale::Implementation* locale = *stream->locale;
    const char* source = g_sceV2DecimalAtoms;
    const std::size_t sourceSize = sizeof(g_sceV2DecimalAtoms);
    const GuestLocale::Facet* punct = UseSceV2Facet(SceV2TextFacets<Text>::NumpunctKind(), locale);
    const std::string grouping = TakeSceV2String(SceV2Punctuation<Text>(punct).grouping, punct);
    const GuestLocale::Facet* ctype = UseSceV2Facet(SceV2TextFacets<Text>::CtypeKind(), locale);
    Text atoms[sizeof(g_sceV2DecimalAtoms)];
    reinterpret_cast<const typename SceV2TextFacets<Text>::Ctype*>(ctype->vtable)->widenRange(ctype, source, source + sourceSize, SceV2CtypeUnits(atoms));
    const Text plus = atoms[SceV2DecimalPlus];
    const Text minus = atoms[SceV2DecimalMinus];
    const std::size_t digitLimit = 10;
    const std::size_t atomCount = sourceSize - 1;
    char* out = digits;
    bool bad = false;
    bool sticky = false;
    if (!SceV2InputEnded(first, last)) {
        const Text sign = PeekSceV2Character(first);
        if (sign == plus || sign == minus) {
            *out++ = sign == plus ? '+' : '-';
            AdvanceSceV2Character(first);
        }
    }
    char* leading = out;
    *out++ = '0';
    bool seen = false;
    int significant = 0;
    int power = 0;
    const auto takeDigit = [&](std::size_t index) {
        if (SceV2ClassicSignificant <= significant) {
            ++power;
            if (index > 0) sticky = true;
        } else if (index != 0 || significant != 0) {
            *out++ = source[index];
            ++significant;
        }
    };
    const signed char firstSize = grouping.empty() ? 0 : static_cast<signed char>(grouping[0]);
    if (firstSize == CHAR_MAX || firstSize <= 0) {
        std::size_t index = 0;
        while (!SceV2InputEnded(first, last) && (index = FindSceV2Atom(atoms, atomCount, PeekSceV2Character(first))) < digitLimit) {
            takeDigit(index);
            seen = true;
            AdvanceSceV2Character(first);
        }
    } else {
        const Text separator = SceV2Punctuation<Text>(punct).thousandsSep(AsSceV2Punct<Text>(punct));
        std::string groups(1, '\0');
        std::size_t group = 0;
        while (!SceV2InputEnded(first, last)) {
            const Text character = PeekSceV2Character(first);
            const std::size_t index = FindSceV2Atom(atoms, atomCount, character);
            if (index < digitLimit) {
                seen = true;
                takeDigit(index);
                if (groups[group] != CHAR_MAX) ++groups[group];
            } else if (separator == Text{} || groups[group] == '\0' || character != separator) {
                break;
            } else {
                groups.push_back('\0');
                ++group;
            }
            AdvanceSceV2Character(first);
        }
        if (group != 0) {
            if (groups[group] > '\0') ++group;
            else bad = true;
        }
        if (!bad) bad = !SceV2ClassicGroupsMatch(grouping, groups, group);
    }
    const char point = localeconv_nid_postfix()->decimal_point[0];
    if (!SceV2InputEnded(first, last) && PeekSceV2Character(first) == SceV2Punctuation<Text>(punct).decimalPoint(AsSceV2Punct<Text>(punct))) {
        *out++ = point;
        AdvanceSceV2Character(first);
    }
    if (significant == 0) {
        while (NextSceV2Is(first, last, atoms[SceV2AtomZero])) {
            --power;
            seen = true;
            AdvanceSceV2Character(first);
        }
        if (power < 0) {
            *out++ = '0';
            ++power;
        }
    }
    std::size_t index = 0;
    while (!SceV2InputEnded(first, last) && (index = FindSceV2Atom(atoms, atomCount, PeekSceV2Character(first))) < digitLimit) {
        if (significant < SceV2ClassicSignificant) {
            *out++ = source[index];
            ++significant;
        } else if (index > 0) {
            sticky = true;
        }
        seen = true;
        AdvanceSceV2Character(first);
    }
    if (sticky) {
        char* digit = out;
        while (--digit != leading) {
            if (*digit == point) continue;
            if (*digit != '9') {
                ++*digit;
                break;
            }
            *digit = '0';
        }
        if (digit == leading) {
            *digit = '1';
            ++power;
        }
    }
    const Text lowerExponent = atoms[SceV2DecimalLowerE];
    const Text upperExponent = atoms[SceV2DecimalUpperE];
    if (seen && !SceV2InputEnded(first, last) && (PeekSceV2Character(first) == lowerExponent || PeekSceV2Character(first) == upperExponent)) {
        *out++ = 'e';
        AdvanceSceV2Character(first);
        seen = false;
        significant = 0;
        if (!SceV2InputEnded(first, last)) {
            const Text sign = PeekSceV2Character(first);
            if (sign == plus || sign == minus) {
                *out++ = sign == plus ? '+' : '-';
                AdvanceSceV2Character(first);
            }
        }
        while (NextSceV2Is(first, last, atoms[SceV2AtomZero])) {
            seen = true;
            AdvanceSceV2Character(first);
        }
        if (seen) *out++ = '0';
        while (!SceV2InputEnded(first, last) && (index = FindSceV2Atom(atoms, atomCount, PeekSceV2Character(first))) < 10) {
            if (significant < SceV2ClassicExponent) {
                *out++ = source[index];
                ++significant;
            }
            seen = true;
            AdvanceSceV2Character(first);
        }
    }
    if (bad || !seen) out = digits;
    *out = '\0';
    return power;
}

template<typename Value, typename Text> SceV2InputIterator<Text> GetSceV2Floating(SceV2InputIterator<Text> first, SceV2InputIterator<Text> last, GuestLocale::SceV2IosBase* stream, std::int32_t* state, Value* value, Value (APS5_VABI *convert)(const char*, char**, std::int64_t)) {
    char digits[SceV2FloatDigits];
    const std::uint32_t field = GetSceV2FloatingField(digits, SceV2FloatSignificant, first, last, *stream->locale);
    if ((field & 0xff) == 0) {
        *state = SceV2IosFailBit;
        *value = 0;
        return FinishSceV2Get(first, last, state);
    }
    int& guestErrno = *__error_nid_postfix();
    const int saved = guestErrno;
    guestErrno = 0;
    char* end = nullptr;
    *value = convert(digits, &end, 0);
    const int error = guestErrno;
    guestErrno = saved;
    if ((field & SceV2BadGrouping) != 0 || end == digits || error != 0) {
        *state = SceV2IosFailBit;
        if (end == digits) *value = 0;
    }
    return FinishSceV2Get(first, last, state);
}

template<typename Text> SceV2InputIterator<Text> APS5_VABI SceV2NumGetFloat_nid_no_patch(const GuestLocale::Facet*, SceV2InputIterator<Text> first, SceV2InputIterator<Text> last, GuestLocale::SceV2IosBase* stream, std::int32_t* state, float* value) {
    return GetSceV2Floating(first, last, stream, state, value, _Stof_nid_postfix);
}

template<typename Text> SceV2InputIterator<Text> APS5_VABI SceV2NumGetDouble_nid_no_patch(const GuestLocale::Facet*, SceV2InputIterator<Text> first, SceV2InputIterator<Text> last, GuestLocale::SceV2IosBase* stream, std::int32_t* state, double* value) {
    return GetSceV2Floating(first, last, stream, state, value, _Stod_nid_postfix);
}

template<typename Text> SceV2InputIterator<Text> APS5_VABI SceV2NumGetLongDouble_nid_no_patch(const GuestLocale::Facet*, SceV2InputIterator<Text> first, SceV2InputIterator<Text> last, GuestLocale::SceV2IosBase* stream, std::int32_t* state, long double* value) {
    char digits[SceV2FloatDigits];
    int power = 0;
    if ((stream->flags & SceV2IosFloatField) == SceV2IosFloatField) {
        const std::uint32_t field = GetSceV2FloatingField(digits, SceV2HexSignificant, first, last, *stream->locale);
        if ((field & 0xff) == 0 || (field & SceV2BadGrouping) != 0) digits[0] = '\0';
    } else {
        power = GetSceV2ClassicFloatingField(digits, first, last, stream);
    }
    char* end = nullptr;
    int error = 0;
    const long double parsed = _Stoldx_nid_postfix(digits, &end, power, &error);
    if (SceV2InputEnded(first, last)) *state |= SceV2IosEofBit;
    if (end == digits || error != 0) *state |= SceV2IosFailBit;
    *value = parsed;
    return first;
}

constexpr char g_sceV2MoneyAtoms[] = "0123456789-";
constexpr std::size_t SceV2MoneyMinusAtom = 10;
constexpr short SceV2MoneySpaceMask = 0x44;

template<typename Text> bool IsSceV2MoneySpace(const GuestLocale::Facet* ctype, Text character) {
    if constexpr (std::is_same_v<Text, char>) return (reinterpret_cast<const SceV2CtypeFacet*>(ctype)->table[static_cast<unsigned char>(character)] & SceV2MoneySpaceMask) != 0;
    else return reinterpret_cast<const SceV2CtypeWideVtable*>(ctype->vtable)->is(ctype, SceV2MoneySpaceMask, character);
}

template<typename Text> std::size_t FindSceV2MoneyDigit(const Text (&atoms)[sizeof(g_sceV2MoneyAtoms)], Text character) {
    return FindSceV2Atom(atoms, SceV2MoneyMinusAtom + 1, character);
}

template<typename Text> std::string GetSceV2MoneyField(SceV2InputIterator<Text>& first, SceV2InputIterator<Text>& last, std::int32_t international, GuestLocale::SceV2IosBase* stream, Text (&atoms)[sizeof(g_sceV2MoneyAtoms)]) {
    using PunctText = SceV2MoneypunctText<Text>;
    const GuestLocale::Facet* facet = UseSceV2Facet(SceV2MoneypunctKind<Text>(international != 0), *stream->locale);
    const auto& punct = *reinterpret_cast<const SceV2MoneypunctVtable<PunctText>*>(facet->vtable);
    const auto* fields = reinterpret_cast<const SceV2MoneypunctFacet<PunctText>*>(facet);
    std::basic_string<Text> sign;
    bool negative = false;
    const std::uint32_t pattern = punct.negativeFormat(fields);
    std::string value;
    const GuestLocale::Facet* ctype = UseSceV2Facet(SceV2TextFacets<Text>::CtypeKind(), *stream->locale);
    reinterpret_cast<const typename SceV2TextFacets<Text>::Ctype*>(ctype->vtable)->widenRange(ctype, g_sceV2MoneyAtoms, g_sceV2MoneyAtoms + sizeof(g_sceV2MoneyAtoms), SceV2CtypeUnits(atoms));
    char field[4];
    std::memcpy(field, &pattern, sizeof(field));
    bool bad = false;
    for (std::size_t index = 0; !bad && index < 4; ++index) {
        switch (field[index]) {
            case SceV2MoneySymbol: {
                std::basic_string<Text> symbol = AsSceV2Text<Text>(TakeSceV2String(punct.currencySymbol, facet));
                if (((stream->flags & SceV2IosShowBase) == 0 && !SceV2InputEnded(first, last) && PeekSceV2Character(first) != symbol[0])
                    || (index == 3 && sign.size() <= 1 && (SceV2InputEnded(first, last) || PeekSceV2Character(first) != symbol[0]))) symbol.clear();
                std::size_t matched = 0;
                while (!SceV2InputEnded(first, last) && matched != symbol.size() && PeekSceV2Character(first) == symbol[matched]) {
                    ++matched;
                    AdvanceSceV2Character(first);
                }
                bad = matched != symbol.size();
                break;
            }
            case SceV2MoneySign:
                if (SceV2InputEnded(first, last)) break;
                if (!TakeSceV2String(punct.positiveSign, facet).empty() && TakeSceV2String(punct.positiveSign, facet)[0] == PeekSceV2Character(first)) {
                    AdvanceSceV2Character(first);
                    sign = AsSceV2Text<Text>(TakeSceV2String(punct.positiveSign, facet));
                } else if (!TakeSceV2String(punct.negativeSign, facet).empty() && TakeSceV2String(punct.negativeSign, facet)[0] == PeekSceV2Character(first)) {
                    AdvanceSceV2Character(first);
                    sign = AsSceV2Text<Text>(TakeSceV2String(punct.negativeSign, facet));
                    negative = true;
                } else if (!TakeSceV2String(punct.positiveSign, facet).empty() && TakeSceV2String(punct.negativeSign, facet).empty()) {
                    negative = true;
                }
                break;
            case SceV2MoneyValue: {
                const int fractionDigits = punct.fractionDigits(fields);
                const std::string grouping = TakeSceV2String(punct.grouping, facet);
                const Text separator = grouping.empty() ? Text{} : static_cast<Text>(punct.thousandsSep(fields));
                if (separator == Text{} || static_cast<unsigned char>(grouping[0]) >= CHAR_MAX) {
                    std::size_t digit;
                    while (!SceV2InputEnded(first, last) && (digit = FindSceV2MoneyDigit(atoms, PeekSceV2Character(first))) < 10) {
                        value += g_sceV2MoneyAtoms[digit];
                        AdvanceSceV2Character(first);
                    }
                } else {
                    std::string groups(1, '\0');
                    std::size_t group = 0;
                    while (!SceV2InputEnded(first, last)) {
                        const std::size_t digit = FindSceV2MoneyDigit(atoms, PeekSceV2Character(first));
                        if (digit < 10) {
                            value += g_sceV2MoneyAtoms[digit];
                            if (groups[group] != CHAR_MAX) ++groups[group];
                        } else if (groups[group] == '\0' || PeekSceV2Character(first) != separator) {
                            break;
                        } else {
                            groups.push_back('\0');
                            ++group;
                        }
                        AdvanceSceV2Character(first);
                    }
                    if (group != 0) {
                        if (static_cast<signed char>(groups[group]) > 0) ++group;
                        else bad = true;
                    }
                    for (const char* size = grouping.c_str(); !bad && group > 0;) {
                        if (*size == CHAR_MAX) break;
                        if ((--group > 0 && *size != groups[group]) || (group == 0 && static_cast<signed char>(*size) < static_cast<signed char>(groups[group]))) bad = true;
                        else if (static_cast<signed char>(size[1]) > 0) ++size;
                    }
                    if (bad) break;
                }
                const Text point = static_cast<Text>(punct.decimalPoint(fields));
                int seen = 0;
                if (point != Text{} && !SceV2InputEnded(first, last) && PeekSceV2Character(first) == point) {
                    for (;;) {
                        AdvanceSceV2Character(first);
                        if (SceV2InputEnded(first, last) || seen >= fractionDigits) break;
                        const std::size_t digit = FindSceV2MoneyDigit(atoms, PeekSceV2Character(first));
                        if (digit >= 10) break;
                        value += g_sceV2MoneyAtoms[digit];
                        ++seen;
                    }
                    if (seen < fractionDigits) bad = true;
                }
                if (value.empty()) bad = true;
                else for (; seen < fractionDigits; ++seen) value += '0';
                break;
            }
            case SceV2MoneySpace:
            case SceV2MoneyNone: {
                if (index == 3) break;
                bool seen = false;
                while (!SceV2InputEnded(first, last) && IsSceV2MoneySpace(ctype, PeekSceV2Character(first))) {
                    AdvanceSceV2Character(first);
                    seen = true;
                }
                if (field[index] == SceV2MoneySpace && !seen) bad = true;
                break;
            }
            default: break;
        }
    }
    if (!bad && sign.size() > 1) {
        std::size_t matched = 1;
        while (!SceV2InputEnded(first, last) && PeekSceV2Character(first) == sign[matched]) {
            AdvanceSceV2Character(first);
            if (++matched == sign.size()) break;
        }
        bad = matched != sign.size();
    }
    if (bad) value.clear();
    else if (negative) value.insert(0, 1, '-');
    return value;
}

template<typename Text> Text* ResizeSceV2String(SceV2BasicString<Text>* string, std::size_t length) {
    constexpr std::size_t InlineCapacity = SceV2BasicString<Text>::InlineCapacity;
    constexpr std::size_t Limit = std::is_same_v<Text, char> ? 0x7fffffffffffffff : 0x7ffffffffffffffe;
    const std::size_t size = string->size;
    const std::size_t capacity = string->capacity;
    Text* data = capacity > InlineCapacity ? string->pointer : string->buffer;
    if (length > capacity) {
        if (length > Limit) _ZNSt8__sce_v214_Xlength_errorEPKc_nid_postfix("string too long");
        const std::size_t requested = length | InlineCapacity;
        const std::size_t grown = requested > Limit || capacity > Limit - capacity / 2 ? Limit : std::max(requested, capacity + capacity / 2);
        auto* storage = static_cast<Text*>(GuestLocale::AllocateStringStorage((grown + 1) * sizeof(Text)));
        std::memcpy(storage, data, size * sizeof(Text));
        if (capacity > InlineCapacity) GuestStringStorage::Deallocate(data, (capacity + 1) * sizeof(Text));
        string->pointer = storage;
        string->capacity = grown;
        data = storage;
    }
    if (length > size) std::fill(data + size, data + length, Text{});
    data[length] = Text{};
    string->size = length;
    return data;
}

template<typename Text> SceV2InputIterator<Text> APS5_VABI SceV2MoneyGetLongDouble_nid_no_patch(const GuestLocale::Facet*, SceV2InputIterator<Text> first, SceV2InputIterator<Text> last, std::int32_t international, GuestLocale::SceV2IosBase* stream, std::int32_t* state, long double* value) {
    Text atoms[sizeof(g_sceV2MoneyAtoms)];
    const std::string digits = GetSceV2MoneyField(first, last, international, stream, atoms);
    if (SceV2InputEnded(first, last)) *state |= SceV2IosEofBit;
    if (digits.empty()) {
        *state |= SceV2IosFailBit;
        return first;
    }
    int* error = __error_nid_postfix();
    const int saved = *error;
    *error = 0;
    char* end = nullptr;
    const double parsed = _Stod_nid_postfix(digits.c_str(), &end, 0);
    const int failed = *error;
    *error = saved;
    if (end == digits.c_str() || failed != 0) *state |= SceV2IosFailBit;
    else *value = parsed;
    return first;
}

template<typename Text> SceV2InputIterator<Text> APS5_VABI SceV2MoneyGetDigits_nid_no_patch(const GuestLocale::Facet*, SceV2InputIterator<Text> first, SceV2InputIterator<Text> last, std::int32_t international, GuestLocale::SceV2IosBase* stream, std::int32_t* state, SceV2BasicString<Text>* value) {
    Text atoms[sizeof(g_sceV2MoneyAtoms)];
    const std::string digits = GetSceV2MoneyField(first, last, international, stream, atoms);
    if (SceV2InputEnded(first, last)) *state |= SceV2IosEofBit;
    if (digits.empty()) {
        *state |= SceV2IosFailBit;
        return first;
    }
    Text* data = ResizeSceV2String(value, digits.size());
    std::size_t index = 0;
    if (digits[0] == '-') data[index++] = atoms[SceV2MoneyMinusAtom];
    for (; index < digits.size(); ++index) data[index] = atoms[digits[index] - '0'];
    return first;
}

constexpr short SceV2CtypeDigitMask = 0x20;
constexpr std::int32_t SceV2DateDmy = 1;
constexpr std::int32_t SceV2DateMdy = 2;
constexpr std::int32_t SceV2DateYmd = 3;
constexpr std::int32_t SceV2DateYdm = 4;

template<typename Text> bool IsSceV2Class(const GuestLocale::Facet* ctype, short mask, Text character) {
    if constexpr (std::is_same_v<Text, char>) return (reinterpret_cast<const SceV2CtypeFacet*>(ctype)->table[static_cast<unsigned char>(character)] & mask) != 0;
    else return reinterpret_cast<const SceV2CtypeWideVtable*>(ctype->vtable)->is(ctype, mask, character);
}

template<typename Text> char NarrowSceV2Character(const GuestLocale::Facet* ctype, Text character) {
    return reinterpret_cast<const typename SceV2TextFacets<Text>::Ctype*>(ctype->vtable)->narrow(ctype, character, '\0');
}

template<typename Text> std::int32_t GetSceV2TimeInteger(SceV2InputIterator<Text>& first, SceV2InputIterator<Text>& last, int low, int high, int* value, int* digitsRead, const GuestLocale::Facet* ctype) {
    const int limit = high < 10 ? 1 : high < 100 ? 2 : high < 1000 ? 3 : 4;
    char digits[32];
    std::size_t length = 0;
    *digitsRead = 0;
    while (!SceV2InputEnded(first, last) && *digitsRead < limit && IsSceV2Class(ctype, SceV2MoneySpaceMask, PeekSceV2Character(first))) {
        AdvanceSceV2Character(first);
        ++*digitsRead;
    }
    if (!SceV2InputEnded(first, last) && *digitsRead < limit) {
        const char sign = NarrowSceV2Character(ctype, PeekSceV2Character(first));
        if (sign == '-' || sign == '+') {
            digits[length++] = sign;
            AdvanceSceV2Character(first);
        }
    }
    while (!SceV2InputEnded(first, last) && *digitsRead < limit && NarrowSceV2Character(ctype, PeekSceV2Character(first)) == '0') {
        ++*digitsRead;
        AdvanceSceV2Character(first);
    }
    if (*digitsRead > 0) digits[length++] = '0';
    while (!SceV2InputEnded(first, last)) {
        const char digit = NarrowSceV2Character(ctype, PeekSceV2Character(first));
        if (static_cast<unsigned char>(digit - '0') > 9 || *digitsRead >= limit) break;
        digits[length] = digit;
        if (length < sizeof(digits) - 1) ++length;
        ++*digitsRead;
        AdvanceSceV2Character(first);
    }
    if (*digitsRead == 0) length = 0;
    digits[length] = '\0';
    int error = 0;
    char* end = nullptr;
    const std::int64_t parsed = _ZNSt8__sce_v26_StolxEPKcPPciPi_nid_postfix(digits, &end, 10, &error);
    std::int32_t state = SceV2InputEnded(first, last) ? SceV2IosEofBit : 0;
    if (parsed > high || parsed < low || end == digits || error != 0) state |= SceV2IosFailBit;
    else *value = static_cast<int>(parsed);
    return state;
}

template<typename Text> std::int32_t GetSceV2TimeInteger(SceV2InputIterator<Text>& first, SceV2InputIterator<Text>& last, int low, int high, int* value, const GuestLocale::Facet* ctype) {
    int digitsRead = 0;
    return GetSceV2TimeInteger(first, last, low, high, value, &digitsRead, ctype);
}

template<typename Text> const SceV2TimeGetVtable<Text>& SceV2TimeGetVirtuals(const GuestLocale::Facet* self) {
    return *reinterpret_cast<const SceV2TimeGetVtable<Text>*>(self->vtable);
}

template<typename Text> int GetSceV2TimeName(SceV2InputIterator<Text>& first, SceV2InputIterator<Text>& last, const GuestLocale::Facet* self, bool months) {
    if constexpr (std::is_same_v<Text, char>) {
        const auto* facet = reinterpret_cast<const SceV2TimeGetFacet<char>*>(self);
        return GetSceV2LocaleText(first, last, 0, months ? facet->months : facet->days, false);
    } else {
        const auto* facet = reinterpret_cast<const SceV2TimeGetFacet<std::uint16_t>*>(self);
        return GetSceV2LocaleText(first, last, 0, reinterpret_cast<const Text*>(months ? facet->months : facet->days), false);
    }
}

template<typename Text> void SkipSceV2Spaces(SceV2InputIterator<Text>& first, SceV2InputIterator<Text>& last, const GuestLocale::Facet* ctype) {
    while (!SceV2InputEnded(first, last) && IsSceV2Class(ctype, SceV2MoneySpaceMask, PeekSceV2Character(first))) AdvanceSceV2Character(first);
}

template<typename Text> void SkipSceV2DateSeparator(SceV2InputIterator<Text>& first, SceV2InputIterator<Text>& last, const GuestLocale::Facet* ctype) {
    SkipSceV2Spaces(first, last, ctype);
    if (!SceV2InputEnded(first, last)) {
        const char separator = NarrowSceV2Character(ctype, PeekSceV2Character(first));
        if (separator == ':' || separator == ',' || separator == '/') AdvanceSceV2Character(first);
    }
    SkipSceV2Spaces(first, last, ctype);
}

template<typename Text> SceV2InputIterator<Text> GetSceV2TimeFormat(const GuestLocale::Facet* self, SceV2InputIterator<Text> first, SceV2InputIterator<Text> last, GuestLocale::SceV2IosBase* stream, std::int32_t* state, GuestTm* time, const char* format) {
    const GuestLocale::Facet* ctype = UseSceV2Facet(SceV2TextFacets<Text>::CtypeKind(), *stream->locale);
    for (; *format != '\0'; ++format) {
        if (SceV2InputEnded(first, last)) {
            *state |= SceV2IosFailBit;
            break;
        }
        if (*format == '%') {
            ++format;
            first = SceV2TimeGetVirtuals<Text>(self).get(self, first, last, stream, state, time, *format, 0);
        } else if (*format == ' ') {
            SkipSceV2Spaces(first, last, ctype);
        } else if (NarrowSceV2Character(ctype, PeekSceV2Character(first)) != *format) {
            *state |= SceV2IosFailBit;
            break;
        } else {
            AdvanceSceV2Character(first);
        }
    }
    if (SceV2InputEnded(first, last)) *state |= SceV2IosEofBit;
    return first;
}

std::int32_t APS5_VABI SceV2TimeGetDateOrder_nid_no_patch(const GuestLocale::Facet* self) {
    return reinterpret_cast<const SceV2TimeGetFacet<char>*>(self)->dateOrder;
}

template<typename Text> SceV2InputIterator<Text> APS5_VABI SceV2TimeGetTime_nid_no_patch(const GuestLocale::Facet*, SceV2InputIterator<Text> first, SceV2InputIterator<Text> last, GuestLocale::SceV2IosBase* stream, std::int32_t* state, GuestTm* time) {
    const GuestLocale::Facet* ctype = UseSceV2Facet(SceV2TextFacets<Text>::CtypeKind(), *stream->locale);
    *state |= GetSceV2TimeInteger(first, last, 0, 23, &time->tm_hour, ctype);
    if (*state != 0 || NarrowSceV2Character(ctype, PeekSceV2Character(first)) != ':') {
        *state |= SceV2IosFailBit;
    } else {
        AdvanceSceV2Character(first);
        *state |= GetSceV2TimeInteger(first, last, 0, 59, &time->tm_min, ctype);
    }
    if (*state != 0 || NarrowSceV2Character(ctype, PeekSceV2Character(first)) != ':') {
        *state |= SceV2IosFailBit;
    } else {
        AdvanceSceV2Character(first);
        *state |= GetSceV2TimeInteger(first, last, 0, 60, &time->tm_sec, ctype);
    }
    return first;
}

template<typename Text> SceV2InputIterator<Text> APS5_VABI SceV2TimeGetDate_nid_no_patch(const GuestLocale::Facet* self, SceV2InputIterator<Text> first, SceV2InputIterator<Text> last, GuestLocale::SceV2IosBase* stream, std::int32_t* state, GuestTm* time) {
    const GuestLocale::Facet* ctype = UseSceV2Facet(SceV2TextFacets<Text>::CtypeKind(), *stream->locale);
    const auto& virtuals = SceV2TimeGetVirtuals<Text>(self);
    std::int32_t order = virtuals.dateOrder(self);
    if (order == 0) order = SceV2DateMdy;
    if (!SceV2InputEnded(first, last)) {
        if (!IsSceV2Class(ctype, SceV2CtypeDigitMask, PeekSceV2Character(first))) {
            first = virtuals.getMonthname(self, first, last, stream, state, time);
            order = SceV2DateMdy;
        } else if (order == SceV2DateMdy) {
            *state |= GetSceV2TimeInteger(first, last, 1, 12, &time->tm_mon, ctype);
            --time->tm_mon;
        } else if (order == SceV2DateDmy) {
            *state |= GetSceV2TimeInteger(first, last, 1, 31, &time->tm_mday, ctype);
        } else {
            first = virtuals.getYear(self, first, last, stream, state, time);
        }
    }
    SkipSceV2DateSeparator(first, last, ctype);
    if (!SceV2InputEnded(first, last)) {
        if (!IsSceV2Class(ctype, SceV2CtypeDigitMask, PeekSceV2Character(first))) {
            if (order == SceV2DateMdy) {
                *state |= SceV2IosFailBit;
            } else {
                first = virtuals.getMonthname(self, first, last, stream, state, time);
                if (order == SceV2DateYdm) order = SceV2DateYmd;
            }
        } else if (order == SceV2DateDmy || order == SceV2DateYmd) {
            *state |= GetSceV2TimeInteger(first, last, 1, 12, &time->tm_mon, ctype);
            --time->tm_mon;
        } else {
            *state |= GetSceV2TimeInteger(first, last, 1, 31, &time->tm_mday, ctype);
        }
    }
    SkipSceV2DateSeparator(first, last, ctype);
    if (SceV2InputEnded(first, last)) {
        *state |= SceV2IosFailBit;
    } else if (!IsSceV2Class(ctype, SceV2CtypeDigitMask, PeekSceV2Character(first))) {
        if (order != SceV2DateYdm) *state |= SceV2IosFailBit;
        else first = virtuals.getMonthname(self, first, last, stream, state, time);
    } else if (order == SceV2DateYdm) {
        *state |= GetSceV2TimeInteger(first, last, 1, 12, &time->tm_mon, ctype);
        --time->tm_mon;
    } else if (order == SceV2DateYmd) {
        *state |= GetSceV2TimeInteger(first, last, 1, 31, &time->tm_mday, ctype);
    } else {
        first = virtuals.getYear(self, first, last, stream, state, time);
    }
    if (SceV2InputEnded(first, last)) *state |= SceV2IosEofBit;
    return first;
}

template<typename Text> SceV2InputIterator<Text> APS5_VABI SceV2TimeGetWeekday_nid_no_patch(const GuestLocale::Facet* self, SceV2InputIterator<Text> first, SceV2InputIterator<Text> last, GuestLocale::SceV2IosBase*, std::int32_t* state, GuestTm* time) {
    const int number = GetSceV2TimeName(first, last, self, false);
    if (number < 0) *state |= SceV2IosFailBit;
    else time->tm_wday = static_cast<int>(static_cast<unsigned>(number) >> 1);
    return first;
}

template<typename Text> SceV2InputIterator<Text> APS5_VABI SceV2TimeGetMonthname_nid_no_patch(const GuestLocale::Facet* self, SceV2InputIterator<Text> first, SceV2InputIterator<Text> last, GuestLocale::SceV2IosBase*, std::int32_t* state, GuestTm* time) {
    const int number = GetSceV2TimeName(first, last, self, true);
    if (number < 0) *state |= SceV2IosFailBit;
    else time->tm_mon = static_cast<int>(static_cast<unsigned>(number) >> 1);
    return first;
}

template<typename Text> SceV2InputIterator<Text> APS5_VABI SceV2TimeGetYear_nid_no_patch(const GuestLocale::Facet*, SceV2InputIterator<Text> first, SceV2InputIterator<Text> last, GuestLocale::SceV2IosBase* stream, std::int32_t* state, GuestTm* time) {
    const GuestLocale::Facet* ctype = UseSceV2Facet(SceV2TextFacets<Text>::CtypeKind(), *stream->locale);
    int answer = 0;
    int digitsRead = 0;
    const std::int32_t result = GetSceV2TimeInteger(first, last, 0, 9999, &answer, &digitsRead, ctype);
    *state |= result;
    if ((result & SceV2IosFailBit) == 0) {
        if (digitsRead > 2) time->tm_year = answer - 1900;
        else if (answer < 69) time->tm_year = answer + 100;
        else if (answer < 100) time->tm_year = answer;
    }
    return first;
}

template<typename Text> SceV2InputIterator<Text> APS5_VABI SceV2TimeGetField_nid_no_patch(const GuestLocale::Facet* self, SceV2InputIterator<Text> first, SceV2InputIterator<Text> last, GuestLocale::SceV2IosBase* stream, std::int32_t* state, GuestTm* time, char specifier, char) {
    const GuestLocale::Facet* ctype = UseSceV2Facet(SceV2TextFacets<Text>::CtypeKind(), *stream->locale);
    const auto& virtuals = SceV2TimeGetVirtuals<Text>(self);
    int answer = 0;
    *state = 0;
    const auto integer = [&](int low, int high, int* value) { *state |= GetSceV2TimeInteger(first, last, low, high, value, ctype); };
    const auto format = [&](const char* text) { first = GetSceV2TimeFormat(self, first, last, stream, state, time, text); };
    switch (specifier) {
        case 'a': case 'A': first = virtuals.getWeekday(self, first, last, stream, state, time); break;
        case 'b': case 'B': case 'h': first = virtuals.getMonthname(self, first, last, stream, state, time); break;
        case 'c': format("%a %b %d %H : %M : %S %Y"); break;
        case 'C':
            integer(0, 99, &answer);
            if ((*state & SceV2IosFailBit) == 0) time->tm_year = answer * 100 - 1900;
            break;
        case 'd': case 'e': integer(1, 31, &time->tm_mday); break;
        case 'D': format("%m / %d / %y"); break;
        case 'H': integer(0, 23, &time->tm_hour); break;
        case 'I':
            integer(1, 12, &answer);
            if ((*state & SceV2IosFailBit) == 0) time->tm_hour = answer == 12 ? 0 : answer;
            break;
        case 'j': integer(1, 366, &time->tm_yday); break;
        case 'm':
            integer(1, 12, &answer);
            if ((*state & SceV2IosFailBit) == 0) time->tm_mon = answer - 1;
            break;
        case 'M': integer(0, 59, &time->tm_min); break;
        case 'n': case 't': format(" "); break;
        case 'p':
            answer = GetSceV2LocaleText(first, last, 0, ":AM:am:PM:pm", false);
            if (answer < 0) *state |= SceV2IosFailBit;
            else if (answer >= 2) time->tm_hour += 12;
            break;
        case 'r': format("%I : %M : %S %p"); break;
        case 'R': format("%H : %M"); break;
        case 'S': integer(0, 60, &time->tm_sec); break;
        case 'T': case 'X': format("%H : %M : %S"); break;
        case 'U': case 'W': integer(0, 53, &time->tm_yday); break;
        case 'w': integer(0, 6, &time->tm_wday); break;
        case 'x': format("%d / %m / %y"); break;
        case 'y':
            integer(0, 99, &answer);
            if ((*state & SceV2IosFailBit) == 0) time->tm_year = answer < 69 ? answer + 100 : answer;
            break;
        case 'Y':
            integer(0, 9999, &answer);
            if ((*state & SceV2IosFailBit) == 0) time->tm_year = answer - 1900;
            break;
        case '%':
            if (!SceV2InputEnded(first, last) && PeekSceV2Character(first) == static_cast<Text>('%')) AdvanceSceV2Character(first);
            break;
        default: *state = SceV2IosFailBit; break;
    }
    if (SceV2InputEnded(first, last)) *state |= SceV2IosEofBit;
    return first;
}

constexpr std::int32_t SceV2IosBadBit = 0x4;
constexpr std::int32_t SceV2IosKeptStates = 0x13;
constexpr int SceV2InvalidArgument = 22;

template<typename Text> GuestLocale::OutputIterator APS5_VABI SceV2TimePut_nid_no_patch(const GuestLocale::Facet*, GuestLocale::OutputIterator out, GuestLocale::SceV2IosBase* stream, Text, const GuestTm* time, char specifier, char modifier) {
    using Unit = std::conditional_t<std::is_same_v<Text, char>, char, std::uint16_t>;
    const Unit format[5] = {Unit('!'), Unit('%'), static_cast<Unit>(modifier != 0 ? modifier : specifier), static_cast<Unit>(modifier != 0 ? specifier : '\0'), Unit{}};
    int* error = __error_nid_postfix();
    const int saved = *error;
    std::basic_string<Unit> text;
    std::size_t count = 0;
    for (std::size_t grow = 16;; grow *= 2) {
        text.append(grow, Unit{});
        if constexpr (std::is_same_v<Text, char>) {
            count = strftime_nid_postfix(text.data(), text.size(), format, time);
        } else {
            count = wcsftime_nid_postfix(text.data(), text.size(), format, time);
            if (count == 0 && *error == SceV2InvalidArgument) {
                stream->state = (stream->state & SceV2IosKeptStates) | SceV2IosBadBit;
                if ((stream->state & stream->exceptions) != 0) NotImplemented_nid_no_patch("std::__sce_v2::time_put<wchar_t>::do_put throwing ios_base::failure");
                return out;
            }
        }
        if (count != 0) break;
    }
    *error = saved;
    for (std::size_t index = 1; index < count; ++index) PutSceV2Character(out, static_cast<Text>(text[index]));
    return out;
}

}
}

void APS5_VABI _ZNSt8__sce_v26locale7_Locimp9_MakexlocERKNS_8_LocinfoEiPS1_PKS0__nid_postfix(const GuestLocale::SceV2Locinfo*, std::int32_t category, GuestLocale::Implementation* self, const GuestLocaleFacade* locale) {
    if (self == nullptr) throw std::invalid_argument("_Makexloc: null object");
    InstallSceV2Facets(g_sceV2NarrowExtraSteps, category, self, locale);
}

void APS5_VABI _ZNSt8__sce_v26locale7_Locimp9_MakewlocERKNS_8_LocinfoEiPS1_PKS0__nid_postfix(const GuestLocale::SceV2Locinfo*, std::int32_t category, GuestLocale::Implementation* self, const GuestLocaleFacade* locale) {
    if (self == nullptr) throw std::invalid_argument("_Makewloc: null object");
    InstallSceV2Facets(g_sceV2WideSteps, category, self, locale);
}

void APS5_VABI _ZNSt8__sce_v26locale7_Locimp11_MakeushlocERKNS_8_LocinfoEiPS1_PKS0__nid_postfix(const GuestLocale::SceV2Locinfo*, std::int32_t category, GuestLocale::Implementation* self, const GuestLocaleFacade* locale) {
    if (self == nullptr) throw std::invalid_argument("_Makeushloc: null object");
    InstallSceV2Facets(g_sceV2UshortSteps, category, self, locale);
}

GuestLocale::Implementation* APS5_VABI _ZNSt8__sce_v26locale7_Locimp8_MakelocERKNS_8_LocinfoEiPS1_PKS0__nid_postfix(const GuestLocale::SceV2Locinfo* info, std::int32_t category, GuestLocale::Implementation* self, const GuestLocaleFacade* locale) {
    if (info == nullptr || self == nullptr) throw std::invalid_argument("_Makeloc: null object");
    InstallSceV2Facets(g_sceV2NarrowLeadSteps, category, self, locale);
    _ZNSt8__sce_v26locale7_Locimp9_MakexlocERKNS_8_LocinfoEiPS1_PKS0__nid_postfix(info, category, self, locale);
    _ZNSt8__sce_v26locale7_Locimp9_MakewlocERKNS_8_LocinfoEiPS1_PKS0__nid_postfix(info, category, self, locale);
    _ZNSt8__sce_v26locale7_Locimp11_MakeushlocERKNS_8_LocinfoEiPS1_PKS0__nid_postfix(info, category, self, locale);
    self->category |= static_cast<std::uint32_t>(category);
    self->name = info->classicName;
    return self;
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

GuestLocale::String* APS5_VABI _ZNKSt7collateIcE9transformEPKcS2__nid_postfix(GuestLocale::String* result, const GuestLocale::CollateFacet* self, const char* first, const char* last) {
    if (self == nullptr || self->base.vtable == nullptr) throw std::invalid_argument("collate::transform: null facet");
    return reinterpret_cast<const GuestLocale::CollateVtable*>(self->base.vtable)->transform(result, self, first, last);
}

static const char* CopyCtypeRange(const char* first, const char* last, char* destination) {
    if (first != last) std::memcpy(destination, first, static_cast<std::size_t>(last - first));
    return last;
}

char APS5_VABI _ZNKSt5ctypeIcE8do_widenEc_nid_postfix(const GuestLocale::Facet*, char character) {
    return character;
}

const char* APS5_VABI _ZNKSt5ctypeIcE8do_widenEPKcS2_Pc_nid_postfix(const GuestLocale::Facet*, const char* first, const char* last, char* destination) {
    return CopyCtypeRange(first, last, destination);
}

char APS5_VABI _ZNKSt5ctypeIcE9do_narrowEcc_nid_postfix(const GuestLocale::Facet*, char character, char) {
    return character;
}

const char* APS5_VABI _ZNKSt5ctypeIcE9do_narrowEPKcS2_cPc_nid_postfix(const GuestLocale::Facet*, const char* first, const char* last, char, char* destination) {
    return CopyCtypeRange(first, last, destination);
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

std::size_t APS5_VABI _ZNSt8numpunctIwE7_GetcatEPPKNSt6locale5facetEPKS1__nid_postfix(GuestLocale::Facet** facet, const GuestLocale::Implementation* const*) {
    if (facet != nullptr && *facet == nullptr) {
        auto* punct = static_cast<GuestLocale::WideNumpunctFacet*>(_Znwm_nid_postfix(sizeof(GuestLocale::WideNumpunctFacet)));
        *punct = {{&g_wideNumpunctVtable.facet, 0, 0}, g_wideNumpunctGrouping, '.', ',', g_wideNumpunctFalseName, g_wideNumpunctTrueName};
        *facet = &punct->base;
    }
    return 4;
}

char APS5_VABI _ZNKSt8numpunctIcE16do_decimal_pointEv_nid_postfix(const GuestLocale::NumpunctFacet* self) {
    return RequireNumpunct(self, "numpunct::do_decimal_point")->decimalPoint;
}

char APS5_VABI _ZNKSt8numpunctIcE16do_thousands_sepEv_nid_postfix(const GuestLocale::NumpunctFacet* self) {
    return RequireNumpunct(self, "numpunct::do_thousands_sep")->thousandsSep;
}

GuestLocale::String* APS5_VABI _ZNKSt8numpunctIcE11do_groupingEv_nid_postfix(GuestLocale::String* result, const GuestLocale::NumpunctFacet* self) {
    return AssignCString(result, RequireNumpunct(self, "numpunct::do_grouping")->grouping);
}

GuestLocale::String* APS5_VABI _ZNKSt8numpunctIcE12do_falsenameEv_nid_postfix(GuestLocale::String* result, const GuestLocale::NumpunctFacet* self) {
    return AssignCString(result, RequireNumpunct(self, "numpunct::do_falsename")->falsename);
}

GuestLocale::String* APS5_VABI _ZNKSt8numpunctIcE11do_truenameEv_nid_postfix(GuestLocale::String* result, const GuestLocale::NumpunctFacet* self) {
    return AssignCString(result, RequireNumpunct(self, "numpunct::do_truename")->truename);
}

char APS5_VABI _ZNKSt8numpunctIcE13decimal_pointEv_nid_postfix(const GuestLocale::NumpunctFacet* self) {
    return Punctuation(&RequireNumpunct(self, "numpunct::decimal_point")->base).decimalPoint(self);
}

char APS5_VABI _ZNKSt8numpunctIcE13thousands_sepEv_nid_postfix(const GuestLocale::NumpunctFacet* self) {
    return Punctuation(&RequireNumpunct(self, "numpunct::thousands_sep")->base).thousandsSep(self);
}

GuestLocale::String* APS5_VABI _ZNKSt8numpunctIcE8groupingEv_nid_postfix(GuestLocale::String* result, const GuestLocale::NumpunctFacet* self) {
    return Punctuation(&RequireNumpunct(self, "numpunct::grouping")->base).grouping(result, self);
}

GuestLocale::String* APS5_VABI _ZNKSt8numpunctIcE9falsenameEv_nid_postfix(GuestLocale::String* result, const GuestLocale::NumpunctFacet* self) {
    return Punctuation(&RequireNumpunct(self, "numpunct::falsename")->base).falsename(result, self);
}

GuestLocale::String* APS5_VABI _ZNKSt8numpunctIcE8truenameEv_nid_postfix(GuestLocale::String* result, const GuestLocale::NumpunctFacet* self) {
    return Punctuation(&RequireNumpunct(self, "numpunct::truename")->base).truename(result, self);
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

void APS5_VABI _ZNSt8__sce_v28_Locinfo13_Locinfo_ctorEPS0_PKc_nid_postfix(GuestLocale::SceV2Locinfo* self, const char* localeName) {
    if (self == nullptr) throw std::invalid_argument("_Locinfo: null object");
    self->name = setlocale_nid_postfix(0x3f, nullptr);
    if (localeName == nullptr || (*localeName != 0 && std::strcmp(localeName, "C") != 0))
        _ZNSt8__sce_v215_Xruntime_errorEPKc_nid_postfix("bad locale name");
    self->classicName = self->name;
}

void APS5_VABI _ZNSt8__sce_v28_Locinfo13_Locinfo_dtorEPS0__nid_postfix(GuestLocale::SceV2Locinfo* self) {
    if (self == nullptr) throw std::invalid_argument("_Locinfo destructor: null object");
    if (self->name != nullptr) setlocale_nid_postfix(0x3f, self->name);
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

GuestLocale::TimeInfo* APS5_VABI _Getptimes_nid_postfix() {
    static_assert(sizeof(GuestLocale::TimeInfo) == 0xa8);
    static constexpr char days[] = ":Sun:Sunday:Mon:Monday:Tue:Tuesday:Wed:Wednesday:Thu:Thursday:Fri:Friday:Sat:Saturday";
    static constexpr char months[] = ":Jan:January:Feb:February:Mar:March:Apr:April:May:May:Jun:June:Jul:July:Aug:August"
        ":Sep:September:Oct:October:Nov:November:Dec:December";
    static constexpr char formats[] = "|%a %b %e %T %Y|%m/%d/%y|%H:%M:%S|%I:%M:%S %p";
    static constexpr char era[] = "";
    static constexpr char alternativeDigits[] = "";
    static constexpr char daylightSaving[] = "";
    static constexpr char timeZone[] = "";
    static GuestLocale::TimeInfo times {":AM:PM", days, days, days, months, months, months, formats, formats, formats, formats,
        formats, formats, formats, formats, formats, formats, era, alternativeDigits, daylightSaving, timeZone};
    return &times;
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
