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
#include <vector>

extern "C" {
extern std::uint64_t _ZNSt8numpunctIwE2idE_nid_postfix;
extern std::uint64_t _ZNSt5ctypeIwE2idE_nid_postfix;
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

const GuestLocale::Facet* StreamWideNumpunct(const GuestLocale::IosBase* stream) {
    if (const GuestLocale::Facet* facet = FindFacet(stream, _ZNSt8numpunctIwE2idE_nid_postfix)) return facet;
    static GuestLocale::Facet* const fallback = [] {
        GuestLocale::Facet* created = nullptr;
        _ZNSt8numpunctIwE7_GetcatEPPKNSt6locale5facetEPKS1__nid_postfix(&created, nullptr);
        return created;
    }();
    return fallback;
}

std::string TakeGrouping(const GuestLocale::WideNumpunctFacet* punct, const GuestLocale::WideNumpunctVtable& vtable) {
    GuestLocale::String value{};
    vtable.grouping(&value, punct);
    const bool onHeap = value.capacity > GuestLocale::StringInlineCapacity;
    std::string text(onHeap ? value.pointer : value.buffer, value.size);
    if (onHeap) GuestLocale::ReleaseStringStorage(value);
    return text;
}

std::vector<std::uint16_t> WidenDigits(const GuestLocale::IosBase* stream, const char* digits, std::size_t count) {
    std::vector<std::uint16_t> result(count);
    if (count == 0) return result;
    if (const GuestLocale::Facet* ctype = FindFacet(stream, _ZNSt5ctypeIwE2idE_nid_postfix)) {
        using WideWidenRange = const char* (APS5_VABI*)(const GuestLocale::Facet*, const char*, const char*, std::uint16_t*);
        const auto& vtable = *reinterpret_cast<const GuestLocale::WideCtypeVtable*>(ctype->vtable);
        WideWidenRange widenRange{};
        std::memcpy(&widenRange, &vtable.widenMethods[1], sizeof(widenRange));
        widenRange(ctype, digits, digits + count, result.data());
        return result;
    }
    for (std::size_t index = 0; index < count; ++index) {
        result[index] = static_cast<unsigned char>(digits[index]);
    }
    return result;
}

void PutWideRepeat(GuestLocale::OutputIterator& out, std::uint16_t fill, std::size_t count) {
    if (!out.failed && count != 0) {
        out = LibcRepeatWideElement_nid_no_patch(out, fill, count);
    }
}

void PutWideRange(GuestLocale::OutputIterator& out, const std::uint16_t* data, std::size_t count) {
    if (!out.failed && count != 0) {
        out = LibcPutWideElements_nid_no_patch(out, data, count);
    }
}

void InsertWideSeparators(std::vector<std::uint16_t>& text, const std::string& grouping, std::uint16_t separator, std::size_t offset, std::size_t prefix) {
    if (grouping.empty() || offset <= prefix) return;
    const char* group = grouping.c_str();
    while (*group != '\0' && static_cast<signed char>(*group) != CHAR_MAX && static_cast<signed char>(*group) > 0 && static_cast<std::size_t>(*group) < offset - prefix) {
        offset -= static_cast<std::size_t>(*group);
        text.insert(text.begin() + static_cast<std::ptrdiff_t>(offset), separator);
        if (static_cast<signed char>(group[1]) > 0) ++group;
    }
}

}

extern "C" {

GuestLocale::OutputIterator APS5_VABI _ZNKSt7num_putIwSt19ostreambuf_iteratorIwSt11char_traitsIwEEE5_FputES3_RSt8ios_basewPKcm_nid_postfix(
    const GuestLocale::Facet*, GuestLocale::OutputIterator out, GuestLocale::IosBase* stream, std::uint16_t fill, const char* digits, std::size_t count) {
    if (stream == nullptr || stream->locale == nullptr || *stream->locale == nullptr) throw std::invalid_argument("num_put<wchar_t>::_Fput: a stream without a locale");
    std::string raw(count, '\0');
    if (count != 0 && !GuestCopyIn_nid_no_patch(raw.data(), digits, count)) throw std::invalid_argument("num_put<wchar_t>::_Fput: an unreadable digit buffer");
    const std::size_t sign = count > 0 && (raw[0] == '+' || raw[0] == '-') ? 1 : 0;
    const bool hasHexPrefix = sign + 2 <= count && raw[sign] == '0' && (raw[sign + 1] == 'x' || raw[sign + 1] == 'X');
    const std::size_t prefix = sign + (hasHexPrefix ? 2 : 0);
    const bool hex = (stream->flags & IosHexFloat) == IosHexFloat;
    const std::size_t foundExp = raw.find_first_of(hex ? "pP" : "eE");
    const std::size_t exponent = foundExp == std::string::npos ? count : foundExp;
    const char hostPoint = localeconv_nid_postfix()->decimal_point[0];
    const std::size_t pointOffset = raw.find(hostPoint);
    std::vector<std::uint16_t> text = WidenDigits(stream, raw.data(), count);
    const GuestLocale::Facet* punct = StreamWideNumpunct(stream);
    const auto* widePunct = reinterpret_cast<const GuestLocale::WideNumpunctFacet*>(punct);
    const auto& vtable = *reinterpret_cast<const GuestLocale::WideNumpunctVtable*>(punct->vtable);
    std::size_t groupEnd = exponent;
    if (pointOffset != std::string::npos) {
        groupEnd = pointOffset;
        text[pointOffset] = vtable.decimalPoint(widePunct);
    }
    if (prefix < count) {
        const char lead = raw[prefix];
        if (lead != 'I' && lead != 'N' && lead != 'i' && lead != 'n') {
            const std::string grouping = TakeGrouping(widePunct, vtable);
            if (!grouping.empty() && static_cast<signed char>(grouping[0]) != CHAR_MAX && static_cast<signed char>(grouping[0]) > 0) {
                InsertWideSeparators(text, grouping, vtable.thousandsSep(widePunct), groupEnd, prefix);
            }
        }
    }
    const std::int32_t width = stream->width;
    std::size_t fillCount = width <= 0 || static_cast<std::size_t>(width) <= text.size() ? 0 : static_cast<std::size_t>(width) - text.size();
    const std::int32_t adjust = stream->flags & IosAdjustField;
    if (adjust != IosLeft && adjust != IosInternal && fillCount != 0) {
        PutWideRepeat(out, fill, fillCount);
        fillCount = 0;
    }
    PutWideRange(out, text.data(), prefix);
    if (adjust == IosInternal && fillCount != 0) {
        PutWideRepeat(out, fill, fillCount);
        fillCount = 0;
    }
    if (prefix < text.size()) {
        PutWideRange(out, text.data() + prefix, text.size() - prefix);
    }
    stream->width = 0;
    if (fillCount != 0) {
        PutWideRepeat(out, fill, fillCount);
    }
    return out;
}

}
