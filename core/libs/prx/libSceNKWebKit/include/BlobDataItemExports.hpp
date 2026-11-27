#pragma once

#include "SceTypes.hpp"
#include "prx/libSceNKWebKit/src/BlobDataFileReferenceAbi.hpp"

#include <cstddef>
#include <cstdint>

namespace NKWebKit::WebCoreAbi {

struct BlobDataItem {
    std::int32_t type;
    const void* data;
    BlobDataFileReference* file;
    std::int64_t offset;
    std::int64_t length;
};

constexpr std::int32_t kBlobDataItemData = 0;
constexpr std::int32_t kBlobDataItemFile = 1;
constexpr std::int64_t kBlobDataItemToEndOfFile = -1;

static_assert(sizeof(BlobDataItem) == 40);
static_assert(alignof(BlobDataItem) == 8);
static_assert(offsetof(BlobDataItem, type) == 0);
static_assert(offsetof(BlobDataItem, data) == 8);
static_assert(offsetof(BlobDataItem, file) == 16);
static_assert(offsetof(BlobDataItem, offset) == 24);
static_assert(offsetof(BlobDataItem, length) == 32);

}

extern "C" {

long long APS5_VABI _ZNK7WebCore12BlobDataItem6lengthEv(const NKWebKit::WebCoreAbi::BlobDataItem* self);

}
