#ifndef CORE_LIBS_PRX_LIBC_INCLUDE_GUESTLOCALE_HPP
#define CORE_LIBS_PRX_LIBC_INCLUDE_GUESTLOCALE_HPP

#include <cstddef>
#include <cstdint>
#include "general/VabiMacros.hpp"

struct GuestTm;

namespace GuestLocale {

struct LocinfoStorage {
    alignas(std::uint64_t) unsigned char bytes[64];
};

struct Facet;

struct FacetVtable {
    void (APS5_VABI *destroy)(Facet* self);
    void (APS5_VABI *deleteObject)(Facet* self);
    void (APS5_VABI *retain)(Facet* self);
    Facet* (APS5_VABI *release)(Facet* self);
};

struct Facet {
    const FacetVtable* vtable;
    std::uint32_t references;
    std::uint32_t reserved;
};

struct Implementation {
    Facet base;
    Facet** facets;
    std::uint64_t facetCount;
    std::uint32_t category;
    bool transparent;
    const char* name;
};

struct String {
    std::uint64_t reserved;
    union {
        char buffer[16];
        char* pointer;
    };
    std::uint64_t size;
    std::uint64_t capacity;
};

void* AllocateStringStorage(std::size_t bytes);
void ReleaseStringStorage(String& string);

struct CollateFacet {
    Facet base;
    const void* collation;
    const void* wideCollation;
};

struct CollateVtable {
    FacetVtable facet;
    int (APS5_VABI *compare)(const CollateFacet* self, const char* first1, const char* last1, const char* first2, const char* last2);
    String* (APS5_VABI *transform)(String* result, const CollateFacet* self, const char* first, const char* last);
    std::int64_t (APS5_VABI *hash)(const CollateFacet* self, const char* first, const char* last);
};

struct CodecvtVtable {
    FacetVtable facet;
    bool (APS5_VABI *alwaysNoconv)(const Facet* self);
    int (APS5_VABI *maxLength)(const Facet* self);
    int (APS5_VABI *encoding)(const Facet* self);
    int (APS5_VABI *in)(Facet* self, std::uint64_t* state, const char* first1, const char* last1, const char** next1, char* first3, char* last3, char** next3);
    int (APS5_VABI *out)(Facet* self, std::uint64_t* state, const char* first1, const char* last1, const char** next1, char* first3, char* last3, char** next3);
    int (APS5_VABI *unshift)(Facet* self, std::uint64_t* state, char* first3, char* last3, char** next3);
    std::size_t (APS5_VABI *length)(Facet* self, std::uint64_t* state, const char* first1, const char* last1, std::size_t count);
};

struct NumpunctFacet {
    Facet base;
    const char* grouping;
    char decimalPoint;
    char thousandsSep;
    const char* falsename;
    const char* truename;
};

struct NumpunctVtable {
    FacetVtable facet;
    char (APS5_VABI *decimalPoint)(const NumpunctFacet* self);
    char (APS5_VABI *thousandsSep)(const NumpunctFacet* self);
    String* (APS5_VABI *grouping)(String* result, const NumpunctFacet* self);
    String* (APS5_VABI *falsename)(String* result, const NumpunctFacet* self);
    String* (APS5_VABI *truename)(String* result, const NumpunctFacet* self);
};

struct IosBase {
    const void* vtable;
    std::int64_t standardStream;
    std::int32_t state;
    std::int32_t exceptions;
    std::int32_t flags;
    std::int32_t precision;
    std::int32_t width;
    std::uint32_t reserved;
    void* storage;
    void* callbacks;
    Implementation** locale;
};

struct Streambuf;

struct StreambufVtable {
    const void* reserved[4];
    int (APS5_VABI *overflow)(Streambuf* self, int character);
};

struct Streambuf {
    const StreambufVtable* vtable;
    const void* reserved[7];
    char** putNext;
    const void* reservedCounts[3];
    char** putEnd;
};

struct OutputIterator {
    bool failed;
    Streambuf* buffer;
};

struct TimePutVtable {
    FacetVtable facet;
    OutputIterator (APS5_VABI *put)(const Facet* self, OutputIterator out, IosBase* stream, char fill, const GuestTm* time, char format, char modifier);
};

struct NumPutVtable {
    FacetVtable facet;
    OutputIterator (APS5_VABI *putBool)(const Facet* self, OutputIterator out, IosBase* stream, char fill, bool value);
    OutputIterator (APS5_VABI *putLong)(const Facet* self, OutputIterator out, IosBase* stream, char fill, std::int64_t value);
    OutputIterator (APS5_VABI *putUnsignedLong)(const Facet* self, OutputIterator out, IosBase* stream, char fill, std::uint64_t value);
    OutputIterator (APS5_VABI *putLongLong)(const Facet* self, OutputIterator out, IosBase* stream, char fill, std::int64_t value);
    OutputIterator (APS5_VABI *putUnsignedLongLong)(const Facet* self, OutputIterator out, IosBase* stream, char fill, std::uint64_t value);
    OutputIterator (APS5_VABI *putDouble)(const Facet* self, OutputIterator out, IosBase* stream, char fill, double value);
    OutputIterator (APS5_VABI *putLongDouble)(const Facet* self, OutputIterator out, IosBase* stream, char fill, long double value);
    OutputIterator (APS5_VABI *putPointer)(const Facet* self, OutputIterator out, IosBase* stream, char fill, const void* value);
};

struct CtypeVtable {
    FacetVtable facet;
    const void* caseMethods[4];
    char (APS5_VABI *widen)(const Facet* self, char character);
    const char* (APS5_VABI *widenRange)(const Facet* self, const char* first, const char* last, char* destination);
};

static_assert(offsetof(StreambufVtable, overflow) == 0x20);
static_assert(offsetof(Streambuf, putNext) == 0x40);
static_assert(offsetof(Streambuf, putEnd) == 0x60);
static_assert(sizeof(OutputIterator) == 0x10);
static_assert(offsetof(TimePutVtable, put) == 0x20);
static_assert(offsetof(NumPutVtable, putBool) == 0x20);
static_assert(offsetof(NumPutVtable, putPointer) == 0x58);
static_assert(offsetof(CtypeVtable, widenRange) == 0x48);
static_assert(sizeof(Facet) == 0x10);
static_assert(offsetof(Implementation, facets) == 0x10);
static_assert(offsetof(Implementation, facetCount) == 0x18);
static_assert(offsetof(Implementation, transparent) == 0x24);
static_assert(offsetof(Implementation, name) == 0x28);
static_assert(offsetof(IosBase, state) == 0x10);
static_assert(offsetof(IosBase, precision) == 0x1c);
static_assert(offsetof(IosBase, width) == 0x20);
static_assert(offsetof(IosBase, storage) == 0x28);
static_assert(offsetof(IosBase, callbacks) == 0x30);
static_assert(offsetof(IosBase, locale) == 0x38);
static_assert(sizeof(IosBase) == 0x40);
static_assert(offsetof(String, buffer) == 0x8);
static_assert(offsetof(String, size) == 0x18);
static_assert(sizeof(String) == 0x28);
static_assert(offsetof(CollateFacet, collation) == 0x10);
static_assert(sizeof(CollateFacet) == 0x20);
static_assert(offsetof(CollateVtable, compare) == 0x20);
static_assert(offsetof(CollateVtable, hash) == 0x30);
static_assert(offsetof(CodecvtVtable, alwaysNoconv) == 0x20);
static_assert(offsetof(CodecvtVtable, in) == 0x38);
static_assert(offsetof(CodecvtVtable, length) == 0x50);
static_assert(sizeof(NumpunctFacet) == 0x30);
static_assert(offsetof(NumpunctFacet, grouping) == 0x10);
static_assert(offsetof(NumpunctFacet, decimalPoint) == 0x18);
static_assert(offsetof(NumpunctFacet, falsename) == 0x20);
static_assert(offsetof(NumpunctFacet, truename) == 0x28);
static_assert(offsetof(NumpunctVtable, decimalPoint) == 0x20);
static_assert(offsetof(NumpunctVtable, truename) == 0x40);
static_assert(alignof(LocinfoStorage) == 8);

}

inline constexpr std::size_t kNumPutCharVtableSlots = 14;
static_assert(kNumPutCharVtableSlots * sizeof(std::uintptr_t) == 112);
extern "C" std::uintptr_t _ZTVSt7num_putIcSt19ostreambuf_iteratorIcSt11char_traitsIcEEE_nid_postfix[kNumPutCharVtableSlots];

#endif
