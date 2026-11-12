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

struct SceV2String {
    union {
        char buffer[16];
        char* pointer;
    };
    std::size_t size;
    std::size_t capacity;
};

static_assert(sizeof(SceV2String) == 0x20);

SceV2String* AssignSceV2String(SceV2String* result, const char* text) {
    std::memset(result, 0, sizeof(SceV2String));
    const std::size_t length = std::strlen(text);
    char* data = result->buffer;
    std::size_t capacity = GuestLocale::StringInlineCapacity;
    if (length > GuestLocale::StringInlineCapacity) {
        capacity = length | GuestLocale::StringCapacityMask;
        data = static_cast<char*>(GuestLocale::AllocateStringStorage(capacity + 1));
        result->pointer = data;
    }
    std::memcpy(data, text, length);
    data[length] = 0;
    result->size = length;
    result->capacity = capacity;
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
constexpr auto g_sceV2CtypeWideVtable = MakeSceV2FacetVtable<12, g_sceV2CtypeFamily, DestroySceV2WideCtypeFacet_nid_no_patch>();
constexpr auto g_sceV2CtypeUshortVtable = MakeSceV2FacetVtable<12, g_sceV2CtypeFamily, DestroySceV2WideCtypeFacet_nid_no_patch>();
constexpr auto g_sceV2NumGetCharVtable = MakeSceV2FacetVtable<11, g_sceV2NumGetFamily>();
constexpr auto g_sceV2NumGetWideVtable = MakeSceV2FacetVtable<11, g_sceV2NumGetFamily>();
constexpr auto g_sceV2NumGetUshortVtable = MakeSceV2FacetVtable<11, g_sceV2NumGetFamily>();
struct SceV2NumPutCharVtable {
    GuestLocale::FacetVtable facet;
    GuestLocale::OutputIterator (APS5_VABI *putBool)(const GuestLocale::Facet* self, GuestLocale::OutputIterator out, GuestLocale::SceV2IosBase* stream, char fill, unsigned char value);
    GuestLocale::OutputIterator (APS5_VABI *putLong)(const GuestLocale::Facet* self, GuestLocale::OutputIterator out, GuestLocale::SceV2IosBase* stream, char fill, std::int64_t value);
    GuestLocale::OutputIterator (APS5_VABI *putUnsignedLong)(const GuestLocale::Facet* self, GuestLocale::OutputIterator out, GuestLocale::SceV2IosBase* stream, char fill, std::uint64_t value);
    GuestLocale::OutputIterator (APS5_VABI *putLongLong)(const GuestLocale::Facet* self, GuestLocale::OutputIterator out, GuestLocale::SceV2IosBase* stream, char fill, std::int64_t value);
    GuestLocale::OutputIterator (APS5_VABI *putUnsignedLongLong)(const GuestLocale::Facet* self, GuestLocale::OutputIterator out, GuestLocale::SceV2IosBase* stream, char fill, std::uint64_t value);
    GuestLocale::OutputIterator (APS5_VABI *putDouble)(const GuestLocale::Facet* self, GuestLocale::OutputIterator out, GuestLocale::SceV2IosBase* stream, char fill, double value);
    GuestLocale::OutputIterator (APS5_VABI *putLongDouble)(const GuestLocale::Facet* self, GuestLocale::OutputIterator out, GuestLocale::SceV2IosBase* stream, char fill, long double value);
    GuestLocale::OutputIterator (APS5_VABI *putPointer)(const GuestLocale::Facet* self, GuestLocale::OutputIterator out, GuestLocale::SceV2IosBase* stream, char fill, const void* value);
};

GuestLocale::OutputIterator APS5_VABI SceV2NumPutBool_nid_no_patch(const GuestLocale::Facet* self, GuestLocale::OutputIterator out, GuestLocale::SceV2IosBase* stream, char fill, unsigned char value);
GuestLocale::OutputIterator APS5_VABI SceV2NumPutLong_nid_no_patch(const GuestLocale::Facet* self, GuestLocale::OutputIterator out, GuestLocale::SceV2IosBase* stream, char fill, std::int64_t value);
GuestLocale::OutputIterator APS5_VABI SceV2NumPutUnsignedLong_nid_no_patch(const GuestLocale::Facet* self, GuestLocale::OutputIterator out, GuestLocale::SceV2IosBase* stream, char fill, std::uint64_t value);
GuestLocale::OutputIterator APS5_VABI SceV2NumPutLongLong_nid_no_patch(const GuestLocale::Facet* self, GuestLocale::OutputIterator out, GuestLocale::SceV2IosBase* stream, char fill, std::int64_t value);
GuestLocale::OutputIterator APS5_VABI SceV2NumPutUnsignedLongLong_nid_no_patch(const GuestLocale::Facet* self, GuestLocale::OutputIterator out, GuestLocale::SceV2IosBase* stream, char fill, std::uint64_t value);
GuestLocale::OutputIterator APS5_VABI SceV2NumPutDouble_nid_no_patch(const GuestLocale::Facet* self, GuestLocale::OutputIterator out, GuestLocale::SceV2IosBase* stream, char fill, double value);
GuestLocale::OutputIterator APS5_VABI SceV2NumPutLongDouble_nid_no_patch(const GuestLocale::Facet* self, GuestLocale::OutputIterator out, GuestLocale::SceV2IosBase* stream, char fill, long double value);
GuestLocale::OutputIterator APS5_VABI SceV2NumPutPointer_nid_no_patch(const GuestLocale::Facet* self, GuestLocale::OutputIterator out, GuestLocale::SceV2IosBase* stream, char fill, const void* value);

const SceV2NumPutCharVtable g_sceV2NumPutCharVtable{
    {DestroySceV2PlainFacet_nid_no_patch, DeleteSceV2Facet_nid_no_patch<DestroySceV2PlainFacet_nid_no_patch>, RetainFacet, ReleaseFacet},
    SceV2NumPutBool_nid_no_patch, SceV2NumPutLong_nid_no_patch, SceV2NumPutUnsignedLong_nid_no_patch, SceV2NumPutLongLong_nid_no_patch,
    SceV2NumPutUnsignedLongLong_nid_no_patch, SceV2NumPutDouble_nid_no_patch, SceV2NumPutLongDouble_nid_no_patch, SceV2NumPutPointer_nid_no_patch};
constexpr auto g_sceV2NumPutWideVtable = MakeSceV2FacetVtable<8, g_sceV2NumPutFamily>();
constexpr auto g_sceV2NumPutUshortVtable = MakeSceV2FacetVtable<8, g_sceV2NumPutFamily>();
constexpr auto g_sceV2NumpunctWideVtable = MakeSceV2FacetVtable<5, g_sceV2NumpunctFamily>();
constexpr auto g_sceV2NumpunctUshortVtable = MakeSceV2FacetVtable<5, g_sceV2NumpunctFamily, DestroySceV2UshortNumpunctFacet_nid_no_patch>();
constexpr auto g_sceV2CodecvtWideVtable = MakeSceV2FacetVtable<7, g_sceV2CodecvtFamily>();
constexpr auto g_sceV2CodecvtUshortVtable = MakeSceV2FacetVtable<7, g_sceV2CodecvtFamily>();
constexpr auto g_sceV2CollateCharVtable = MakeSceV2FacetVtable<3, g_sceV2CollateFamily>();
constexpr auto g_sceV2CollateWideVtable = MakeSceV2FacetVtable<3, g_sceV2CollateFamily>();
constexpr auto g_sceV2CollateUshortVtable = MakeSceV2FacetVtable<3, g_sceV2CollateFamily>();
constexpr auto g_sceV2MessagesCharVtable = MakeSceV2FacetVtable<3, g_sceV2MessagesFamily>();
constexpr auto g_sceV2MessagesWideVtable = MakeSceV2FacetVtable<3, g_sceV2MessagesFamily>();
constexpr auto g_sceV2MessagesUshortVtable = MakeSceV2FacetVtable<3, g_sceV2MessagesFamily>();
constexpr auto g_sceV2MoneyGetCharVtable = MakeSceV2FacetVtable<2, g_sceV2MoneyGetFamily>();
constexpr auto g_sceV2MoneyGetWideVtable = MakeSceV2FacetVtable<2, g_sceV2MoneyGetFamily>();
constexpr auto g_sceV2MoneyGetUshortVtable = MakeSceV2FacetVtable<2, g_sceV2MoneyGetFamily>();
constexpr auto g_sceV2MoneyPutCharVtable = MakeSceV2FacetVtable<2, g_sceV2MoneyPutFamily>();
constexpr auto g_sceV2MoneyPutWideVtable = MakeSceV2FacetVtable<2, g_sceV2MoneyPutFamily>();
constexpr auto g_sceV2MoneyPutUshortVtable = MakeSceV2FacetVtable<2, g_sceV2MoneyPutFamily>();
constexpr auto g_sceV2MoneypunctCharVtable = MakeSceV2FacetVtable<9, g_sceV2MoneypunctFamily>();
constexpr auto g_sceV2MoneypunctCharIntlVtable = MakeSceV2FacetVtable<9, g_sceV2MoneypunctFamily>();
constexpr auto g_sceV2MoneypunctWideVtable = MakeSceV2FacetVtable<9, g_sceV2MoneypunctFamily>();
constexpr auto g_sceV2MoneypunctWideIntlVtable = MakeSceV2FacetVtable<9, g_sceV2MoneypunctFamily>();
constexpr auto g_sceV2MoneypunctUshortVtable = MakeSceV2FacetVtable<9, g_sceV2MoneypunctFamily, DestroySceV2UshortMoneypunctFacet_nid_no_patch>();
constexpr auto g_sceV2MoneypunctUshortIntlVtable = MakeSceV2FacetVtable<9, g_sceV2MoneypunctFamily, DestroySceV2UshortMoneypunctFacet_nid_no_patch>();
constexpr auto g_sceV2TimeGetCharVtable = MakeSceV2FacetVtable<7, g_sceV2TimeGetFamily>();
constexpr auto g_sceV2TimeGetWideVtable = MakeSceV2FacetVtable<7, g_sceV2TimeGetFamily>();
constexpr auto g_sceV2TimeGetUshortVtable = MakeSceV2FacetVtable<7, g_sceV2TimeGetFamily, DestroySceV2UshortTimeGetFacet_nid_no_patch>();
constexpr auto g_sceV2TimePutCharVtable = MakeSceV2FacetVtable<1, g_sceV2TimePutFamily, DestroySceV2TimePutFacet_nid_no_patch>();
constexpr auto g_sceV2TimePutWideVtable = MakeSceV2FacetVtable<1, g_sceV2TimePutFamily, DestroySceV2TimePutFacet_nid_no_patch>();
constexpr auto g_sceV2TimePutUshortVtable = MakeSceV2FacetVtable<1, g_sceV2TimePutFamily, DestroySceV2TimePutFacet_nid_no_patch>();

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
SceV2FacetKind g_sceV2NumGetChar{&_ZNSt8__sce_v27num_getIcNS_19istreambuf_iteratorIcNS_11char_traitsIcEEEEE2idE_nid_postfix, sizeof(GuestLocale::Facet), &g_sceV2NumGetCharVtable.facet, InitializeSceV2PlainFacet, nullptr};
SceV2FacetKind g_sceV2NumPutChar{&_ZNSt8__sce_v27num_putIcNS_19ostreambuf_iteratorIcNS_11char_traitsIcEEEEE2idE_nid_postfix, sizeof(GuestLocale::Facet), &g_sceV2NumPutCharVtable.facet, InitializeSceV2PlainFacet, nullptr};
SceV2FacetKind g_sceV2NumpunctChar{&_ZNSt8__sce_v28numpunctIcE2idE_nid_postfix, sizeof(GuestLocale::NumpunctFacet), &g_sceV2NumpunctCharVtable.facet, InitializeSceV2NumpunctFacet, nullptr};
SceV2FacetKind g_sceV2CodecvtChar{&_ZNSt8__sce_v27codecvtIcc9_MbstatetE2idE_nid_postfix, sizeof(GuestLocale::Facet), &g_sceV2CodecvtCharVtable.facet, InitializeSceV2PlainFacet, nullptr};
SceV2FacetKind g_sceV2CollateChar{&_ZNSt8__sce_v27collateIcE2idE_nid_postfix, sizeof(GuestLocale::Facet), &g_sceV2CollateCharVtable.facet, InitializeSceV2PlainFacet, nullptr};
SceV2FacetKind g_sceV2MessagesChar{&_ZNSt8__sce_v28messagesIcE2idE_nid_postfix, sizeof(GuestLocale::Facet), &g_sceV2MessagesCharVtable.facet, InitializeSceV2PlainFacet, nullptr};
SceV2FacetKind g_sceV2MoneyGetChar{&_ZNSt8__sce_v29money_getIcNS_19istreambuf_iteratorIcNS_11char_traitsIcEEEEE2idE_nid_postfix, sizeof(GuestLocale::Facet), &g_sceV2MoneyGetCharVtable.facet, InitializeSceV2PlainFacet, nullptr};
SceV2FacetKind g_sceV2MoneyPutChar{&_ZNSt8__sce_v29money_putIcNS_19ostreambuf_iteratorIcNS_11char_traitsIcEEEEE2idE_nid_postfix, sizeof(GuestLocale::Facet), &g_sceV2MoneyPutCharVtable.facet, InitializeSceV2PlainFacet, nullptr};
SceV2FacetKind g_sceV2MoneypunctChar{&_ZNSt8__sce_v210moneypunctIcLb0EE2idE_nid_postfix, sizeof(SceV2MoneypunctFacet<char>), &g_sceV2MoneypunctCharVtable.facet, InitializeSceV2MoneypunctFacet<false>, nullptr};
SceV2FacetKind g_sceV2MoneypunctCharIntl{&_ZNSt8__sce_v210moneypunctIcLb1EE2idE_nid_postfix, sizeof(SceV2MoneypunctFacet<char>), &g_sceV2MoneypunctCharIntlVtable.facet, InitializeSceV2MoneypunctFacet<true>, nullptr};
SceV2FacetKind g_sceV2TimeGetChar{&_ZNSt8__sce_v28time_getIcNS_19istreambuf_iteratorIcNS_11char_traitsIcEEEEE2idE_nid_postfix, sizeof(SceV2TimeGetFacet<char>), &g_sceV2TimeGetCharVtable.facet, InitializeSceV2TimeGetFacet, nullptr};
SceV2FacetKind g_sceV2TimePutChar{&_ZNSt8__sce_v28time_putIcNS_19ostreambuf_iteratorIcNS_11char_traitsIcEEEEE2idE_nid_postfix, sizeof(SceV2TimePutFacet), &g_sceV2TimePutCharVtable.facet, InitializeSceV2TimePutFacet, nullptr};
SceV2FacetKind g_sceV2CtypeWide{&_ZNSt8__sce_v25ctypeIwE2idE_nid_postfix, sizeof(SceV2WideCtypeFacet), &g_sceV2CtypeWideVtable.facet, InitializeSceV2WideCtypeFacet, nullptr};
SceV2FacetKind g_sceV2NumGetWide{&_ZNSt8__sce_v27num_getIwNS_19istreambuf_iteratorIwNS_11char_traitsIwEEEEE2idE_nid_postfix, sizeof(GuestLocale::Facet), &g_sceV2NumGetWideVtable.facet, InitializeSceV2PlainFacet, nullptr};
SceV2FacetKind g_sceV2NumPutWide{&_ZNSt8__sce_v27num_putIwNS_19ostreambuf_iteratorIwNS_11char_traitsIwEEEEE2idE_nid_postfix, sizeof(GuestLocale::Facet), &g_sceV2NumPutWideVtable.facet, InitializeSceV2PlainFacet, nullptr};
SceV2FacetKind g_sceV2NumpunctWide{&_ZNSt8__sce_v28numpunctIwE2idE_nid_postfix, sizeof(GuestLocale::WideNumpunctFacet), &g_sceV2NumpunctWideVtable.facet, InitializeSceV2WideNumpunctFacet, nullptr};
SceV2FacetKind g_sceV2CollateWide{&_ZNSt8__sce_v27collateIwE2idE_nid_postfix, sizeof(GuestLocale::Facet), &g_sceV2CollateWideVtable.facet, InitializeSceV2PlainFacet, nullptr};
SceV2FacetKind g_sceV2MessagesWide{&_ZNSt8__sce_v28messagesIwE2idE_nid_postfix, sizeof(GuestLocale::Facet), &g_sceV2MessagesWideVtable.facet, InitializeSceV2PlainFacet, nullptr};
SceV2FacetKind g_sceV2MoneyGetWide{&_ZNSt8__sce_v29money_getIwNS_19istreambuf_iteratorIwNS_11char_traitsIwEEEEE2idE_nid_postfix, sizeof(GuestLocale::Facet), &g_sceV2MoneyGetWideVtable.facet, InitializeSceV2PlainFacet, nullptr};
SceV2FacetKind g_sceV2MoneyPutWide{&_ZNSt8__sce_v29money_putIwNS_19ostreambuf_iteratorIwNS_11char_traitsIwEEEEE2idE_nid_postfix, sizeof(GuestLocale::Facet), &g_sceV2MoneyPutWideVtable.facet, InitializeSceV2PlainFacet, nullptr};
SceV2FacetKind g_sceV2MoneypunctWide{&_ZNSt8__sce_v210moneypunctIwLb0EE2idE_nid_postfix, sizeof(SceV2MoneypunctFacet<std::uint16_t>), &g_sceV2MoneypunctWideVtable.facet, InitializeSceV2WideMoneypunctFacet<false>, nullptr};
SceV2FacetKind g_sceV2MoneypunctWideIntl{&_ZNSt8__sce_v210moneypunctIwLb1EE2idE_nid_postfix, sizeof(SceV2MoneypunctFacet<std::uint16_t>), &g_sceV2MoneypunctWideIntlVtable.facet, InitializeSceV2WideMoneypunctFacet<true>, nullptr};
SceV2FacetKind g_sceV2TimeGetWide{&_ZNSt8__sce_v28time_getIwNS_19istreambuf_iteratorIwNS_11char_traitsIwEEEEE2idE_nid_postfix, sizeof(SceV2TimeGetFacet<std::uint16_t>), &g_sceV2TimeGetWideVtable.facet, InitializeSceV2WideTimeGetFacet, nullptr};
SceV2FacetKind g_sceV2TimePutWide{&_ZNSt8__sce_v28time_putIwNS_19ostreambuf_iteratorIwNS_11char_traitsIwEEEEE2idE_nid_postfix, sizeof(SceV2TimePutFacet), &g_sceV2TimePutWideVtable.facet, InitializeSceV2TimePutFacet, nullptr};
SceV2FacetKind g_sceV2CodecvtWide{&_ZNSt8__sce_v27codecvtIwc9_MbstatetE2idE_nid_postfix, sizeof(SceV2WideCodecvtFacet), &g_sceV2CodecvtWideVtable.facet, InitializeSceV2WideCodecvtFacet, nullptr};
SceV2FacetKind g_sceV2CtypeUshort{&_ZNSt8__sce_v25ctypeItE2idE_nid_postfix, sizeof(SceV2WideCtypeFacet), &g_sceV2CtypeUshortVtable.facet, InitializeSceV2WideCtypeFacet, nullptr};
SceV2FacetKind g_sceV2NumGetUshort{&g_sceV2NumGetUshortId, sizeof(GuestLocale::Facet), &g_sceV2NumGetUshortVtable.facet, InitializeSceV2PlainFacet, nullptr};
SceV2FacetKind g_sceV2NumPutUshort{&g_sceV2NumPutUshortId, sizeof(GuestLocale::Facet), &g_sceV2NumPutUshortVtable.facet, InitializeSceV2PlainFacet, nullptr};
SceV2FacetKind g_sceV2NumpunctUshort{&g_sceV2NumpunctUshortId, sizeof(GuestLocale::WideNumpunctFacet), &g_sceV2NumpunctUshortVtable.facet, InitializeSceV2UshortNumpunctFacet, nullptr};
SceV2FacetKind g_sceV2CollateUshort{&g_sceV2CollateUshortId, sizeof(GuestLocale::Facet), &g_sceV2CollateUshortVtable.facet, InitializeSceV2PlainFacet, nullptr};
SceV2FacetKind g_sceV2MessagesUshort{&g_sceV2MessagesUshortId, sizeof(GuestLocale::Facet), &g_sceV2MessagesUshortVtable.facet, InitializeSceV2PlainFacet, nullptr};
SceV2FacetKind g_sceV2MoneyGetUshort{&g_sceV2MoneyGetUshortId, sizeof(GuestLocale::Facet), &g_sceV2MoneyGetUshortVtable.facet, InitializeSceV2PlainFacet, nullptr};
SceV2FacetKind g_sceV2MoneyPutUshort{&g_sceV2MoneyPutUshortId, sizeof(GuestLocale::Facet), &g_sceV2MoneyPutUshortVtable.facet, InitializeSceV2PlainFacet, nullptr};
SceV2FacetKind g_sceV2MoneypunctUshort{&g_sceV2MoneypunctUshortId, sizeof(SceV2MoneypunctFacet<std::uint16_t>), &g_sceV2MoneypunctUshortVtable.facet, InitializeSceV2UshortMoneypunctFacet<false>, nullptr};
SceV2FacetKind g_sceV2MoneypunctUshortIntl{&g_sceV2MoneypunctUshortIntlId, sizeof(SceV2MoneypunctFacet<std::uint16_t>), &g_sceV2MoneypunctUshortIntlVtable.facet, InitializeSceV2UshortMoneypunctFacet<true>, nullptr};
SceV2FacetKind g_sceV2TimeGetUshort{&g_sceV2TimeGetUshortId, sizeof(SceV2TimeGetFacet<std::uint16_t>), &g_sceV2TimeGetUshortVtable.facet, InitializeSceV2UshortTimeGetFacet, nullptr};
SceV2FacetKind g_sceV2TimePutUshort{&g_sceV2TimePutUshortId, sizeof(SceV2TimePutFacet), &g_sceV2TimePutUshortVtable.facet, InitializeSceV2TimePutFacet, nullptr};
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

void PutSceV2Character(GuestLocale::OutputIterator& out, char character) {
    GuestLocale::Streambuf* buffer = out.buffer;
    if (buffer == nullptr) {
        out.failed = true;
        return;
    }
    char* next = *buffer->putNext;
    auto* count = static_cast<std::int32_t*>(const_cast<void*>(buffer->reservedCounts[2]));
    if (next != nullptr && *count > 0) {
        --*count;
        *buffer->putNext = next + 1;
        *next = character;
        return;
    }
    if (buffer->vtable->overflow(buffer, static_cast<unsigned char>(character)) == -1) out.failed = true;
}

GuestLocale::OutputIterator PutSceV2Field(GuestLocale::OutputIterator out, GuestLocale::SceV2IosBase* stream, char fill, const std::string& text, std::size_t prefix) {
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

std::string TakeSceV2String(SceV2String* (APS5_VABI* method)(SceV2String*, const GuestLocale::NumpunctFacet*), const GuestLocale::Facet* facet) {
    SceV2String value{};
    method(&value, AsNumpunct(facet));
    const bool onHeap = value.capacity > GuestLocale::StringInlineCapacity;
    std::string text(onHeap ? value.pointer : value.buffer, value.size);
    if (onHeap) GuestStringStorage::Deallocate(value.pointer, value.capacity + 1);
    return text;
}

const SceV2NumpunctCharVtable& SceV2Punctuation(const GuestLocale::Facet* facet) {
    return *reinterpret_cast<const SceV2NumpunctCharVtable*>(facet->vtable);
}

std::string WidenSceV2Digits(const GuestLocale::SceV2IosBase* stream, const char* digits, int count) {
    if (count < 0) _ZNSt8__sce_v214_Xlength_errorEPKc_nid_postfix("string too long");
    const GuestLocale::Facet* ctype = UseSceV2Facet(g_sceV2CtypeChar, *stream->locale);
    std::string text(static_cast<std::size_t>(count), '\0');
    reinterpret_cast<const SceV2CtypeCharVtable*>(ctype->vtable)->widenRange(ctype, digits, digits + count, text.data());
    return text;
}

GuestLocale::OutputIterator PutSceV2IntegerDigits(GuestLocale::OutputIterator out, GuestLocale::SceV2IosBase* stream, char fill, const char* digits, int count) {
    const std::size_t size = count < 0 ? 0 : static_cast<std::size_t>(count);
    std::size_t prefix = SignPrefix(digits, size);
    if ((stream->flags & SceV2IosBaseField) == SceV2IosHex && HexPrefix(digits, prefix, size)) prefix += 2;
    std::string text = WidenSceV2Digits(stream, digits, count);
    const GuestLocale::Facet* punct = UseSceV2Facet(g_sceV2NumpunctChar, *stream->locale);
    const std::string grouping = TakeSceV2String(SceV2Punctuation(punct).grouping, punct);
    if (static_cast<signed char>(grouping[0]) != CHAR_MAX && static_cast<signed char>(grouping[0]) > 0) InsertSeparators(text, grouping, SceV2Punctuation(punct).thousandsSep(AsNumpunct(punct)), size, prefix);
    return PutSceV2Field(out, stream, fill, text, prefix);
}

template<typename Value> GuestLocale::OutputIterator PutSceV2Integer(GuestLocale::OutputIterator out, GuestLocale::SceV2IosBase* stream, char fill, const char* qualifier, char decimal, Value value) {
    const std::int32_t flags = stream->flags;
    const std::int32_t base = flags & SceV2IosBaseField;
    const char conversion = base == SceV2IosOct ? 'o' : base != SceV2IosHex ? decimal : (flags & SceV2IosUppercase) != 0 ? 'X' : 'x';
    const std::string format = std::string("%") + ((flags & SceV2IosShowPos) != 0 ? "+" : "") + ((flags & SceV2IosShowBase) != 0 ? "#" : "") + qualifier + conversion;
    char digits[64];
    return PutSceV2IntegerDigits(out, stream, fill, digits, sprintf_s_nid_postfix(digits, sizeof(digits), format.c_str(), value));
}

GuestLocale::OutputIterator PutSceV2FloatingDigits(GuestLocale::OutputIterator out, GuestLocale::SceV2IosBase* stream, char fill, const char* digits, int count, bool finite) {
    const std::size_t size = count < 0 ? 0 : static_cast<std::size_t>(count);
    const std::size_t sign = SignPrefix(digits, size);
    const bool hex = (stream->flags & SceV2IosFloatField) == SceV2IosFloatField;
    const std::size_t prefix = hex && HexPrefix(digits, sign, size) ? sign + 2 : sign;
    const std::size_t exponent = std::strcspn(digits, hex ? "pP" : "eE");
    const char point[2] = {localeconv_nid_postfix()->decimal_point[0], '\0'};
    const std::size_t pointOffset = std::strcspn(digits, point);
    std::string text = WidenSceV2Digits(stream, digits, count);
    const GuestLocale::Facet* punct = UseSceV2Facet(g_sceV2NumpunctChar, *stream->locale);
    const std::string grouping = TakeSceV2String(SceV2Punctuation(punct).grouping, punct);
    const char separator = SceV2Punctuation(punct).thousandsSep(AsNumpunct(punct));
    if (pointOffset != size) text[pointOffset] = SceV2Punctuation(punct).decimalPoint(AsNumpunct(punct));
    if (finite) InsertSeparators(text, grouping, separator, pointOffset != size ? pointOffset : exponent, prefix);
    return PutSceV2Field(out, stream, fill, text, prefix);
}

template<typename Value> GuestLocale::OutputIterator PutSceV2Floating(GuestLocale::OutputIterator out, GuestLocale::SceV2IosBase* stream, char fill, const char* qualifier, std::int64_t hexDigits, bool finite, Value value) {
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

GuestLocale::OutputIterator APS5_VABI SceV2NumPutBool_nid_no_patch(const GuestLocale::Facet* self, GuestLocale::OutputIterator out, GuestLocale::SceV2IosBase* stream, char fill, unsigned char value) {
    if ((stream->flags & SceV2IosBoolAlpha) == 0) return reinterpret_cast<const SceV2NumPutCharVtable*>(self->vtable)->putLong(self, out, stream, fill, value);
    const GuestLocale::Facet* punct = UseSceV2Facet(g_sceV2NumpunctChar, *stream->locale);
    return PutSceV2Field(out, stream, fill, TakeSceV2String(value != 0 ? SceV2Punctuation(punct).truename : SceV2Punctuation(punct).falsename, punct), 0);
}

GuestLocale::OutputIterator APS5_VABI SceV2NumPutLong_nid_no_patch(const GuestLocale::Facet*, GuestLocale::OutputIterator out, GuestLocale::SceV2IosBase* stream, char fill, std::int64_t value) {
    return PutSceV2Integer(out, stream, fill, "l", 'd', value);
}

GuestLocale::OutputIterator APS5_VABI SceV2NumPutUnsignedLong_nid_no_patch(const GuestLocale::Facet*, GuestLocale::OutputIterator out, GuestLocale::SceV2IosBase* stream, char fill, std::uint64_t value) {
    return PutSceV2Integer(out, stream, fill, "l", 'u', value);
}

GuestLocale::OutputIterator APS5_VABI SceV2NumPutLongLong_nid_no_patch(const GuestLocale::Facet*, GuestLocale::OutputIterator out, GuestLocale::SceV2IosBase* stream, char fill, std::int64_t value) {
    return PutSceV2Integer(out, stream, fill, "ll", 'd', value);
}

GuestLocale::OutputIterator APS5_VABI SceV2NumPutUnsignedLongLong_nid_no_patch(const GuestLocale::Facet*, GuestLocale::OutputIterator out, GuestLocale::SceV2IosBase* stream, char fill, std::uint64_t value) {
    return PutSceV2Integer(out, stream, fill, "ll", 'u', value);
}

GuestLocale::OutputIterator APS5_VABI SceV2NumPutDouble_nid_no_patch(const GuestLocale::Facet*, GuestLocale::OutputIterator out, GuestLocale::SceV2IosBase* stream, char fill, double value) {
    return PutSceV2Floating(out, stream, fill, "", 13, __isfinite_nid_postfix(value) != 0, value);
}

GuestLocale::OutputIterator APS5_VABI SceV2NumPutLongDouble_nid_no_patch(const GuestLocale::Facet*, GuestLocale::OutputIterator out, GuestLocale::SceV2IosBase* stream, char fill, long double value) {
    return PutSceV2Floating(out, stream, fill, "L", 16, __isfinitel_nid_postfix(value) != 0, value);
}

GuestLocale::OutputIterator APS5_VABI SceV2NumPutPointer_nid_no_patch(const GuestLocale::Facet*, GuestLocale::OutputIterator out, GuestLocale::SceV2IosBase* stream, char fill, const void* value) {
    char digits[64];
    return PutSceV2IntegerDigits(out, stream, fill, digits, sprintf_s_nid_postfix(digits, sizeof(digits), "%p", value));
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
