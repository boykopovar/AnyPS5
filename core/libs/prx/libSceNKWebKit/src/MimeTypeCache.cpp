#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/include/MimeTypeCacheExports.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"

#include <cstddef>
#include <cstdint>
#include <stdexcept>

namespace {

using NKWebKit::MimeTypeCacheAbi::MIMETypeCache;
using NKWebKit::MimeTypeCacheAbi::SupportsTypeBucket;
using NKWebKit::WtfAbi::StringImpl;

constexpr std::size_t MetadataSize = 4 * sizeof(unsigned);
constexpr int TableSizeOffset = -1;
constexpr int KeyCountOffset = -3;

const unsigned* Metadata(const void* table) {
    return static_cast<const unsigned*>(table);
}

bool IsDeletedKey(const StringImpl* key) {
    return reinterpret_cast<std::uintptr_t>(key) == ~std::uintptr_t(0);
}

void ReleaseKey(StringImpl* key) {
    if (!IsDeletedKey(key)) NKWebKit::WtfAbi::Release(key);
}

void DeallocateTable(void* table) {
    _ZN3WTF8fastFreeEPv(static_cast<char*>(table) - MetadataSize);
}

void DestroySupportedTypes(StringImpl** table) {
    if (!table) return;
    const unsigned size = Metadata(table)[TableSizeOffset];
    for (unsigned i = 0; i < size; ++i) ReleaseKey(table[i]);
    DeallocateTable(table);
}

void DestroyCachedResults(SupportsTypeBucket* table) {
    if (!table) return;
    const unsigned size = Metadata(table)[TableSizeOffset];
    for (unsigned i = 0; i < size; ++i) ReleaseKey(table[i].key);
    DeallocateTable(table);
}

}

extern "C" {

void APS5_VABI _ZN7WebCore13MIMETypeCacheD2Ev(MIMETypeCache* self) {
    if (!self) throw std::invalid_argument("MIMETypeCache::~MIMETypeCache: null this");
    if (self->hasCachedResults) DestroyCachedResults(self->cachedResults);
    if (self->hasSupportedTypes) DestroySupportedTypes(self->supportedTypes);
}

void APS5_VABI _ZN7WebCore13MIMETypeCacheD0Ev(MIMETypeCache* self) {
    _ZN7WebCore13MIMETypeCacheD2Ev(self);
    _ZN3WTF8fastFreeEPv(self);
}

bool APS5_VABI _ZNK7WebCore13MIMETypeCache7isEmptyEv(const MIMETypeCache* self) {
    if (!self) throw std::invalid_argument("MIMETypeCache::isEmpty: null this");
    if (!self->hasSupportedTypes) return false;
    return !self->supportedTypes || !Metadata(self->supportedTypes)[KeyCountOffset];
}

}
