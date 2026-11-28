#include "prx/libSceLibcInternal/include/ErrExports.hpp"
#include "prx/libSceLibcInternal/include/InventoryExports6.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include <algorithm>
#include <array>
#include <atomic>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>

namespace {

constexpr std::size_t WideChunk = 256;
constexpr std::size_t NumericCategory = 4;

struct ErrnoGuard {
    int saved = errno;
    ~ErrnoGuard() { errno = saved; }
};

struct GuestWriteScope {
    ErrnoGuard savedErrno;
    GuestArena::HostWrite write;
    bool ok = false;

    GuestWriteScope(void* address, std::size_t size) : write(address, size) {
        ok = address != nullptr && write.Open() && GuestWritable_nid_no_patch(address, size);
    }
};

bool RangeReadable(const void* source, std::size_t size) {
    if (size == 0) return true;
    if (source == nullptr) return false;
    const std::uintptr_t start = reinterpret_cast<std::uintptr_t>(source);
    if (size - 1 > std::numeric_limits<std::uintptr_t>::max() - start) return false;
    const ErrnoGuard savedErrno;
    GuestArena::GuestArenaPrepareHostRead_nid_no_patch(source, size);
    return GuestReadable_nid_no_patch(source, size);
}

bool CopyInBytes(void* destination, const void* source, std::size_t size) {
    if (size == 0) return true;
    if (!RangeReadable(source, size)) return false;
    const ErrnoGuard savedErrno;
    return GuestCopyIn_nid_no_patch(destination, source, size);
}

template <typename T>
bool CopyIn(const void* source, T& out) {
    return CopyInBytes(&out, source, sizeof(T));
}

template <typename T>
bool CopyOut(void* destination, const T& value) {
    const GuestWriteScope scope(destination, sizeof(T));
    return scope.ok && GuestCopyOut_nid_no_patch(destination, &value, sizeof(T));
}

void* FieldAddress(void* base, std::size_t offset, std::size_t size) {
    if (base == nullptr) return nullptr;
    const std::uintptr_t start = reinterpret_cast<std::uintptr_t>(base);
    if (offset > std::numeric_limits<std::uintptr_t>::max() - start) return nullptr;
    const std::uintptr_t field = start + offset;
    if (size != 0 && size - 1 > std::numeric_limits<std::uintptr_t>::max() - field) return nullptr;
    return reinterpret_cast<void*>(field);
}

struct WideNumPutVtable {
    GuestLocale::FacetVtable facet;
    GuestLocale::OutputIterator (APS5_VABI *putBool)(const GuestLocale::Facet* self, GuestLocale::OutputIterator out, GuestLocale::IosBase* stream, std::uint16_t fill, bool value);
    GuestLocale::OutputIterator (APS5_VABI *putLong)(const GuestLocale::Facet* self, GuestLocale::OutputIterator out, GuestLocale::IosBase* stream, std::uint16_t fill, std::int64_t value);
    GuestLocale::OutputIterator (APS5_VABI *putUnsignedLong)(const GuestLocale::Facet* self, GuestLocale::OutputIterator out, GuestLocale::IosBase* stream, std::uint16_t fill, std::uint64_t value);
    GuestLocale::OutputIterator (APS5_VABI *putLongLong)(const GuestLocale::Facet* self, GuestLocale::OutputIterator out, GuestLocale::IosBase* stream, std::uint16_t fill, std::int64_t value);
    GuestLocale::OutputIterator (APS5_VABI *putUnsignedLongLong)(const GuestLocale::Facet* self, GuestLocale::OutputIterator out, GuestLocale::IosBase* stream, std::uint16_t fill, std::uint64_t value);
    GuestLocale::OutputIterator (APS5_VABI *putDouble)(const GuestLocale::Facet* self, GuestLocale::OutputIterator out, GuestLocale::IosBase* stream, std::uint16_t fill, double value);
    GuestLocale::OutputIterator (APS5_VABI *putLongDouble)(const GuestLocale::Facet* self, GuestLocale::OutputIterator out, GuestLocale::IosBase* stream, std::uint16_t fill, long double value);
    GuestLocale::OutputIterator (APS5_VABI *putPointer)(const GuestLocale::Facet* self, GuestLocale::OutputIterator out, GuestLocale::IosBase* stream, std::uint16_t fill, const void* value);
};

static_assert(offsetof(WideNumPutVtable, putBool) == 0x20);
static_assert(offsetof(WideNumPutVtable, putPointer) == 0x58);
static_assert(sizeof(WideNumPutVtable) == 0x60);

void APS5_VABI DestroyWideNumPutFacet(GuestLocale::Facet*) {}

void APS5_VABI DeleteWideNumPutFacet(GuestLocale::Facet* self) {
    if (self == nullptr) throw std::invalid_argument("num_put<wchar_t>::~num_put: null object");
    _ZdlPv_nid_postfix(self);
}

void APS5_VABI RetainWideNumPutFacet(GuestLocale::Facet* self) {
    void* refAddress = FieldAddress(self, offsetof(GuestLocale::Facet, references), sizeof(std::uint32_t));
    if (refAddress == nullptr || reinterpret_cast<std::uintptr_t>(refAddress) % alignof(std::uint32_t) != 0) {
        throw std::invalid_argument("num_put<wchar_t>::_Incref: null object");
    }
    const GuestWriteScope write(refAddress, sizeof(std::uint32_t));
    if (!write.ok) throw std::invalid_argument("num_put<wchar_t>::_Incref: unwritable object");
    std::atomic_ref<std::uint32_t>(*static_cast<std::uint32_t*>(refAddress)).fetch_add(1);
}

GuestLocale::Facet* APS5_VABI ReleaseWideNumPutFacet(GuestLocale::Facet* self) {
    void* refAddress = FieldAddress(self, offsetof(GuestLocale::Facet, references), sizeof(std::uint32_t));
    if (refAddress == nullptr || reinterpret_cast<std::uintptr_t>(refAddress) % alignof(std::uint32_t) != 0) {
        throw std::invalid_argument("num_put<wchar_t>::_Decref: null object");
    }
    const GuestWriteScope write(refAddress, sizeof(std::uint32_t));
    if (!write.ok) throw std::invalid_argument("num_put<wchar_t>::_Decref: unwritable object");
    return std::atomic_ref<std::uint32_t>(*static_cast<std::uint32_t*>(refAddress)).fetch_sub(1) == 1 ? self : nullptr;
}

GuestLocale::OutputIterator APS5_VABI WideNumDoPutBool(const GuestLocale::Facet*, GuestLocale::OutputIterator out, GuestLocale::IosBase*, std::uint16_t, bool) {
    _ZNKSt7num_putIwSt19ostreambuf_iteratorIwSt11char_traitsIwEEE6do_putES3_RSt8ios_basewb_nid_postfix();
    return out;
}

GuestLocale::OutputIterator APS5_VABI WideNumDoPutLong(const GuestLocale::Facet*, GuestLocale::OutputIterator out, GuestLocale::IosBase*, std::uint16_t, std::int64_t) {
    _ZNKSt7num_putIwSt19ostreambuf_iteratorIwSt11char_traitsIwEEE6do_putES3_RSt8ios_basewl_nid_postfix();
    return out;
}

GuestLocale::OutputIterator APS5_VABI WideNumDoPutUnsignedLong(const GuestLocale::Facet*, GuestLocale::OutputIterator out, GuestLocale::IosBase*, std::uint16_t, std::uint64_t) {
    _ZNKSt7num_putIwSt19ostreambuf_iteratorIwSt11char_traitsIwEEE6do_putES3_RSt8ios_basewm_nid_postfix();
    return out;
}

GuestLocale::OutputIterator APS5_VABI WideNumDoPutLongLong(const GuestLocale::Facet*, GuestLocale::OutputIterator out, GuestLocale::IosBase*, std::uint16_t, std::int64_t) {
    _ZNKSt7num_putIwSt19ostreambuf_iteratorIwSt11char_traitsIwEEE6do_putES3_RSt8ios_basewx_nid_postfix();
    return out;
}

GuestLocale::OutputIterator APS5_VABI WideNumDoPutUnsignedLongLong(const GuestLocale::Facet*, GuestLocale::OutputIterator out, GuestLocale::IosBase*, std::uint16_t, std::uint64_t) {
    _ZNKSt7num_putIwSt19ostreambuf_iteratorIwSt11char_traitsIwEEE6do_putES3_RSt8ios_basewy_nid_postfix();
    return out;
}

GuestLocale::OutputIterator APS5_VABI WideNumDoPutDouble(const GuestLocale::Facet*, GuestLocale::OutputIterator out, GuestLocale::IosBase*, std::uint16_t, double) {
    _ZNKSt7num_putIwSt19ostreambuf_iteratorIwSt11char_traitsIwEEE6do_putES3_RSt8ios_basewd_nid_postfix();
    return out;
}

GuestLocale::OutputIterator APS5_VABI WideNumDoPutLongDouble(const GuestLocale::Facet*, GuestLocale::OutputIterator out, GuestLocale::IosBase*, std::uint16_t, long double) {
    _ZNKSt7num_putIwSt19ostreambuf_iteratorIwSt11char_traitsIwEEE6do_putES3_RSt8ios_basewe_nid_postfix();
    return out;
}

GuestLocale::OutputIterator APS5_VABI WideNumDoPutPointer(const GuestLocale::Facet*, GuestLocale::OutputIterator out, GuestLocale::IosBase*, std::uint16_t, const void*) {
    _ZNKSt7num_putIwSt19ostreambuf_iteratorIwSt11char_traitsIwEEE6do_putES3_RSt8ios_basewPKv_nid_postfix();
    return out;
}

const WideNumPutVtable g_wideNumPutVtable{
    {DestroyWideNumPutFacet, DeleteWideNumPutFacet, RetainWideNumPutFacet, ReleaseWideNumPutFacet},
    WideNumDoPutBool,
    WideNumDoPutLong,
    WideNumDoPutUnsignedLong,
    WideNumDoPutLongLong,
    WideNumDoPutUnsignedLongLong,
    WideNumDoPutDouble,
    WideNumDoPutLongDouble,
    WideNumDoPutPointer,
};

}

extern "C" {

GuestLocale::OutputIterator APS5_VABI _ZNKSt7num_putIwSt19ostreambuf_iteratorIwSt11char_traitsIwEEE4_RepES3_wm_nid_postfix(
    const GuestLocale::Facet*, GuestLocale::OutputIterator out, std::uint16_t element, std::size_t count) {
    return LibcRepeatWideElement_nid_no_patch(out, element, count);
}

GuestLocale::OutputIterator APS5_VABI _ZNKSt7num_putIwSt19ostreambuf_iteratorIwSt11char_traitsIwEEE4_PutES3_PKwm_nid_postfix(
    const GuestLocale::Facet*, GuestLocale::OutputIterator out, const std::uint16_t* elements, std::size_t count) {
    std::array<std::uint16_t, WideChunk> chunk{};
    std::uintptr_t address = reinterpret_cast<std::uintptr_t>(elements);
    while (count != 0) {
        const std::size_t size = std::min(count, WideChunk);
        const std::size_t bytes = size * sizeof(std::uint16_t);
        if (elements == nullptr || !CopyInBytes(chunk.data(), reinterpret_cast<const void*>(address), bytes)) {
            throw std::invalid_argument("num_put<wchar_t>::_Put: an unreadable source");
        }
        out = LibcPutWideElements_nid_no_patch(out, chunk.data(), size);
        count -= size;
        if (count != 0) {
            if (bytes > std::numeric_limits<std::uintptr_t>::max() - address) {
                throw std::invalid_argument("num_put<wchar_t>::_Put: an unreadable source");
            }
            address += bytes;
        }
    }
    return out;
}

GuestLocale::OutputIterator APS5_VABI _ZNKSt7num_putIcSt19ostreambuf_iteratorIcSt11char_traitsIcEEE5_IputES3_RSt8ios_basecPcm_nid_postfix(
    const GuestLocale::Facet*, GuestLocale::OutputIterator out, GuestLocale::IosBase* stream, char fill, char* digits, std::size_t count) {
    GuestLocale::IosBase streamCopy{};
    GuestLocale::Implementation* locale = nullptr;
    if (!CopyIn(stream, streamCopy) || streamCopy.locale == nullptr || !CopyIn(streamCopy.locale, locale) || locale == nullptr) {
        throw std::invalid_argument("num_put<char>::_Iput: a stream without a locale");
    }
    const GuestWriteScope streamWrite(stream, sizeof(*stream));
    if (!streamWrite.ok) throw std::invalid_argument("num_put<char>::_Iput: an unwritable stream");
    if (count != 0 && !RangeReadable(digits, count)) {
        throw std::invalid_argument("num_put<char>::_Iput: an unreadable digit buffer");
    }
    std::string text(count, '\0');
    if (count != 0 && !CopyInBytes(text.data(), digits, count)) {
        throw std::invalid_argument("num_put<char>::_Iput: an unreadable digit buffer");
    }
    return LibcNumPutInteger_nid_no_patch(out, stream, fill, text.data(), count);
}

void APS5_VABI _ZNSt7num_putIwSt19ostreambuf_iteratorIwSt11char_traitsIwEEEC2ERKSt8_Locinfom_nid_postfix(GuestLocale::Facet* self, const GuestLocale::LocinfoStorage* info, std::size_t refs) {
    GuestLocale::LocinfoStorage infoCopy{};
    if (!CopyIn(info, infoCopy)) throw std::invalid_argument("num_put<wchar_t>::num_put: null _Locinfo");
    const GuestLocale::Facet value{
        &g_wideNumPutVtable.facet,
        static_cast<std::uint32_t>(refs),
        static_cast<std::uint32_t>(static_cast<std::uint64_t>(refs) >> 32),
    };
    if (!CopyOut(self, value)) throw std::invalid_argument("num_put<wchar_t>::num_put: null object");
}

std::size_t APS5_VABI _ZNSt7num_putIwSt19ostreambuf_iteratorIwSt11char_traitsIwEEE7_GetcatEPPKNSt6locale5facetEPKS5__nid_postfix(GuestLocale::Facet** facet, const GuestLocale::Implementation* const*) {
    if (facet != nullptr) {
        GuestLocale::Facet* existing = nullptr;
        if (!CopyIn(facet, existing)) throw std::invalid_argument("num_put<wchar_t>::_Getcat: unreadable facet pointer");
        if (existing == nullptr) {
            const GuestWriteScope facetWrite(facet, sizeof(*facet));
            if (!facetWrite.ok) throw std::invalid_argument("num_put<wchar_t>::_Getcat: unwritable facet pointer");
            struct FacetGuard {
                void* ptr;
                ~FacetGuard() noexcept {
                    if (ptr != nullptr) {
                        try {
                            _ZdlPv_nid_postfix(ptr);
                        } catch (...) {
                        }
                    }
                }
            } guard{_Znwm_nid_postfix(sizeof(GuestLocale::Facet))};
            auto* object = static_cast<GuestLocale::Facet*>(guard.ptr);
            GuestLocale::LocinfoStorage info{};
            _ZNSt7num_putIwSt19ostreambuf_iteratorIwSt11char_traitsIwEEEC2ERKSt8_Locinfom_nid_postfix(object, &info, 0);
            if (!GuestCopyOut_nid_no_patch(facet, &object, sizeof(object))) {
                throw std::invalid_argument("num_put<wchar_t>::_Getcat: unwritable facet pointer");
            }
            guard.ptr = nullptr;
        }
    }
    return NumericCategory;
}

}
