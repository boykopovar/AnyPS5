#include "prx/libSceLibcInternal/include/ErrExports.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/GuestLocale.hpp"
#include <atomic>
#include <climits>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

extern "C" {
extern std::int32_t _ZNSt6locale2id7_Id_cntE_nid_postfix;
extern std::uint64_t _ZNSt5ctypeIwE2idE_nid_postfix;
}

namespace {

constexpr std::int32_t IosLeft = 0x2;
constexpr std::int32_t IosInternal = 0x8;
constexpr std::int32_t IosAdjustField = 0x0e;
constexpr std::int32_t IosHex = 0x40;
constexpr std::int32_t IosBaseField = 0x70;

const GuestLocale::Facet* FindFacet(const GuestLocale::IosBase* stream, std::uint64_t& id) {
    if (stream == nullptr || stream->locale == nullptr || *stream->locale == nullptr) return nullptr;
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

void InsertWideSeparators(std::vector<std::uint16_t>& text, const std::string& grouping, std::uint16_t separator, std::size_t offset, std::size_t prefix) {
    if (grouping.empty() || offset <= prefix) return;
    const char* group = grouping.c_str();
    while (*group != '\0' && static_cast<signed char>(*group) != CHAR_MAX && static_cast<signed char>(*group) > 0 && static_cast<std::size_t>(*group) < offset - prefix) {
        offset -= static_cast<std::size_t>(*group);
        text.insert(text.begin() + static_cast<std::ptrdiff_t>(offset), separator);
        if (static_cast<signed char>(group[1]) == CHAR_MAX) break;
        if (static_cast<signed char>(group[1]) > 0) ++group;
    }
}

GuestLocale::OutputIterator PutWideField(GuestLocale::OutputIterator out, GuestLocale::IosBase* stream, std::uint16_t fill, const std::vector<std::uint16_t>& text, std::size_t prefix) {
    const std::int32_t width = stream->width;
    std::size_t fillCount = width <= 0 || static_cast<std::size_t>(width) <= text.size() ? 0 : static_cast<std::size_t>(width) - text.size();
    const std::int32_t adjust = stream->flags & IosAdjustField;
    if (adjust != IosLeft && adjust != IosInternal && fillCount != 0) {
        out = LibcRepeatWideElement_nid_no_patch(out, fill, fillCount);
        fillCount = 0;
    }
    if (prefix != 0) {
        out = LibcPutWideElements_nid_no_patch(out, text.data(), prefix);
    }
    if (adjust == IosInternal && fillCount != 0) {
        out = LibcRepeatWideElement_nid_no_patch(out, fill, fillCount);
        fillCount = 0;
    }
    if (prefix < text.size()) {
        out = LibcPutWideElements_nid_no_patch(out, text.data() + prefix, text.size() - prefix);
    }
    stream->width = 0;
    if (fillCount != 0) {
        out = LibcRepeatWideElement_nid_no_patch(out, fill, fillCount);
    }
    return out;
}

}

extern "C" {

GuestLocale::OutputIterator APS5_VABI _ZNKSt7num_putIwSt19ostreambuf_iteratorIwSt11char_traitsIwEEE5_IputES3_RSt8ios_basewPcm_nid_postfix(
    const GuestLocale::Facet*, GuestLocale::OutputIterator out, GuestLocale::IosBase* stream, std::uint16_t fill, char* digits, std::size_t count) {
    if (stream == nullptr || stream->locale == nullptr || *stream->locale == nullptr) throw std::invalid_argument("num_put<wchar_t>::_Iput: a stream without a locale");
    if (count != 0 && digits == nullptr) throw std::invalid_argument("num_put<wchar_t>::_Iput: a null digit buffer");
    std::string raw(count, '\0');
    if (count != 0 && !GuestCopyIn_nid_no_patch(raw.data(), digits, count)) throw std::invalid_argument("num_put<wchar_t>::_Iput: an unreadable digit buffer");
    std::size_t prefix = count > 0 && (raw[0] == '+' || raw[0] == '-') ? 1 : 0;
    if ((stream->flags & IosBaseField) == IosHex && prefix + 2 <= count && raw[prefix] == '0' && (raw[prefix + 1] == 'x' || raw[prefix + 1] == 'X')) {
        prefix += 2;
    }
    std::vector<std::uint16_t> text = WidenDigits(stream, raw.data(), count);
    const GuestLocale::Facet* punct = StreamWideNumpunct(stream);
    if (punct != nullptr) {
        const auto* widePunct = reinterpret_cast<const GuestLocale::WideNumpunctFacet*>(punct);
        const auto& vtable = *reinterpret_cast<const GuestLocale::WideNumpunctVtable*>(punct->vtable);
        const std::string grouping = TakeGrouping(widePunct, vtable);
        if (!grouping.empty() && static_cast<signed char>(grouping[0]) != CHAR_MAX && static_cast<signed char>(grouping[0]) > 0) {
            InsertWideSeparators(text, grouping, vtable.thousandsSep(widePunct), count, prefix);
        }
    }
    return PutWideField(out, stream, fill, text, prefix);
}

}
