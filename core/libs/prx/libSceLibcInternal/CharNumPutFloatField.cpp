#include "prx/libSceLibcInternal/include/ErrExports.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/GuestLocale.hpp"
#include <climits>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>

extern "C" {
extern std::uint64_t _ZNSt8numpunctIcE2idE_nid_postfix;
extern std::uint64_t _ZNSt5ctypeIcE2idE_nid_postfix;
}

namespace {

constexpr std::int32_t IosLeft = 0x2;
constexpr std::int32_t IosInternal = 0x8;
constexpr std::int32_t IosAdjustField = 0x0e;
constexpr std::int32_t IosHexFloat = 0x1800;

const GuestLocale::Facet* FindFacet(const GuestLocale::IosBase* stream, std::uint64_t id) {
    if (id == 0 || stream == nullptr || stream->locale == nullptr || *stream->locale == nullptr) return nullptr;
    const GuestLocale::Implementation* locale = *stream->locale;
    const std::size_t index = static_cast<std::size_t>(id);
    if (index < locale->facetCount && locale->facets[index] != nullptr) return locale->facets[index];
    const GuestLocale::Implementation* global = _ZNSt6locale16_GetgloballocaleEv_nid_postfix();
    return global != nullptr && index < global->facetCount ? global->facets[index] : nullptr;
}

const GuestLocale::Facet* StreamCharNumpunct(const GuestLocale::IosBase* stream) {
    if (const GuestLocale::Facet* facet = FindFacet(stream, _ZNSt8numpunctIcE2idE_nid_postfix)) return facet;
    static GuestLocale::Facet* const fallback = [] {
        GuestLocale::Facet* created = nullptr;
        _ZNSt8numpunctIcE7_GetcatEPPKNSt6locale5facetEPKS1__nid_postfix(&created, nullptr);
        return created;
    }();
    return fallback;
}

std::string TakeGrouping(const GuestLocale::NumpunctFacet* punct, const GuestLocale::NumpunctVtable& vtable) {
    GuestLocale::String value{};
    vtable.grouping(&value, punct);
    const bool onHeap = value.capacity > GuestLocale::StringInlineCapacity;
    std::string text(onHeap ? value.pointer : value.buffer, value.size);
    if (onHeap) GuestLocale::ReleaseStringStorage(value);
    return text;
}

void PutCharacter(GuestLocale::OutputIterator& out, char character) {
    if (out.failed) return;
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
    if (buffer->vtable == nullptr || buffer->vtable->overflow(buffer, static_cast<unsigned char>(character)) == -1) {
        out.failed = true;
    }
}

void PutRepeat(GuestLocale::OutputIterator& out, char fill, std::size_t count) {
    for (; count != 0 && !out.failed; --count) {
        PutCharacter(out, fill);
    }
}

void PutRange(GuestLocale::OutputIterator& out, const char* data, std::size_t count) {
    for (std::size_t index = 0; index < count && !out.failed; ++index) {
        PutCharacter(out, data[index]);
    }
}

void InsertSeparators(std::string& text, const std::string& grouping, char separator, std::size_t offset, std::size_t prefix) {
    const char* group = grouping.c_str();
    while (static_cast<signed char>(*group) != CHAR_MAX && static_cast<signed char>(*group) > 0 && static_cast<std::size_t>(*group) < offset - prefix) {
        offset -= static_cast<std::size_t>(*group);
        text.insert(offset, 1, separator);
        if (static_cast<signed char>(group[1]) > 0) ++group;
    }
}

}

extern "C" {

GuestLocale::OutputIterator APS5_VABI _ZNKSt7num_putIcSt19ostreambuf_iteratorIcSt11char_traitsIcEEE5_FputES3_RSt8ios_basecPKcm_nid_postfix(
    const GuestLocale::Facet*, GuestLocale::OutputIterator out, GuestLocale::IosBase* stream, char fill, const char* digits, std::size_t count) {
    if (stream == nullptr || stream->locale == nullptr || *stream->locale == nullptr) throw std::invalid_argument("num_put<char>::_Fput: a stream without a locale");
    std::string raw(count, '\0');
    if (count != 0 && !GuestCopyIn_nid_no_patch(raw.data(), digits, count)) throw std::invalid_argument("num_put<char>::_Fput: an unreadable digit buffer");
    const std::size_t sign = count > 0 && (raw[0] == '+' || raw[0] == '-') ? 1 : 0;
    const bool hasHexPrefix = sign + 2 <= count && raw[sign] == '0' && (raw[sign + 1] == 'x' || raw[sign + 1] == 'X');
    const std::size_t prefix = sign + (hasHexPrefix ? 2 : 0);
    const bool hex = (stream->flags & IosHexFloat) == IosHexFloat;
    const std::size_t foundExp = raw.find_first_of(hex ? "pP" : "eE");
    const std::size_t exponent = foundExp == std::string::npos ? count : foundExp;
    const char hostPoint = localeconv_nid_postfix()->decimal_point[0];
    const std::size_t pointOffset = raw.find(hostPoint);
    std::string text = raw;
    if (count != 0) {
        if (const GuestLocale::Facet* ctype = FindFacet(stream, _ZNSt5ctypeIcE2idE_nid_postfix)) {
            reinterpret_cast<const GuestLocale::CtypeVtable*>(ctype->vtable)->widenRange(ctype, raw.data(), raw.data() + count, text.data());
        }
    }
    const GuestLocale::Facet* punct = StreamCharNumpunct(stream);
    const auto* charPunct = reinterpret_cast<const GuestLocale::NumpunctFacet*>(punct);
    const auto& vtable = *reinterpret_cast<const GuestLocale::NumpunctVtable*>(punct->vtable);
    std::size_t groupEnd = exponent;
    if (pointOffset != std::string::npos) {
        groupEnd = pointOffset;
        text[pointOffset] = vtable.decimalPoint(charPunct);
    }
    if (prefix < count) {
        const char lead = raw[prefix];
        if (lead != 'I' && lead != 'N' && lead != 'i' && lead != 'n') {
            const std::string grouping = TakeGrouping(charPunct, vtable);
            if (!grouping.empty() && static_cast<signed char>(grouping[0]) != CHAR_MAX && static_cast<signed char>(grouping[0]) > 0) {
                InsertSeparators(text, grouping, vtable.thousandsSep(charPunct), groupEnd, prefix);
            }
        }
    }
    const std::int32_t width = stream->width;
    std::size_t fillCount = width <= 0 || static_cast<std::size_t>(width) <= text.size() ? 0 : static_cast<std::size_t>(width) - text.size();
    const std::int32_t adjust = stream->flags & IosAdjustField;
    if (adjust != IosLeft && adjust != IosInternal && fillCount != 0) {
        PutRepeat(out, fill, fillCount);
        fillCount = 0;
    }
    PutRange(out, text.data(), prefix);
    if (adjust == IosInternal && fillCount != 0) {
        PutRepeat(out, fill, fillCount);
        fillCount = 0;
    }
    if (prefix < text.size()) {
        PutRange(out, text.data() + prefix, text.size() - prefix);
    }
    stream->width = 0;
    if (fillCount != 0) {
        PutRepeat(out, fill, fillCount);
    }
    return out;
}

}
