#pragma once

#include "prx/libSceNKWebKit/src/WtfStringAbi.hpp"

#include <cstddef>
#include <cstdint>

namespace NKWebKit::WebCoreAbi {

struct FormDataByteVector {
    std::uint8_t* buffer;
    unsigned capacity;
    unsigned size;
};

struct FormDataEncodedFile {
    WtfAbi::StringImpl* filename;
    std::int64_t fileStart;
    std::int64_t fileLength;
    double expectedFileModificationTime;
    bool hasExpectedFileModificationTime;
};

struct FormDataEncodedBlob {
    WtfAbi::URL url;
};

union FormDataElementStorage {
    FormDataByteVector bytes;
    FormDataEncodedFile file;
    FormDataEncodedBlob blob;
};

struct FormDataElement {
    FormDataElementStorage storage;
    std::uint32_t index;
};

struct FormDataElementVector {
    FormDataElement* buffer;
    unsigned capacity;
    unsigned size;
};

struct FormData {
    mutable unsigned refCount;
    FormDataElementVector elements;
    std::int64_t identifier;
    bool alwaysStream;
    FormDataByteVector boundary;
    mutable std::uint64_t lengthInBytes;
    mutable bool hasLengthInBytes;

    void ref() const { ++refCount; }
    void deref() const;
};

static_assert(sizeof(FormDataByteVector) == 16);
static_assert(offsetof(FormDataByteVector, buffer) == 0);
static_assert(offsetof(FormDataByteVector, capacity) == 8);
static_assert(offsetof(FormDataByteVector, size) == 12);

static_assert(sizeof(FormDataEncodedFile) == 40);
static_assert(offsetof(FormDataEncodedFile, filename) == 0);
static_assert(offsetof(FormDataEncodedFile, fileStart) == 8);
static_assert(offsetof(FormDataEncodedFile, fileLength) == 16);
static_assert(offsetof(FormDataEncodedFile, expectedFileModificationTime) == 24);
static_assert(offsetof(FormDataEncodedFile, hasExpectedFileModificationTime) == 32);

static_assert(sizeof(FormDataEncodedBlob) == 40);
static_assert(offsetof(FormDataEncodedBlob, url) == 0);

static_assert(sizeof(FormDataElementStorage) == 40);
static_assert(sizeof(FormDataElement) == 48);
static_assert(alignof(FormDataElement) == 8);
static_assert(offsetof(FormDataElement, storage) == 0);
static_assert(offsetof(FormDataElement, index) == 40);

static_assert(sizeof(FormDataElementVector) == 16);
static_assert(offsetof(FormDataElementVector, buffer) == 0);
static_assert(offsetof(FormDataElementVector, capacity) == 8);
static_assert(offsetof(FormDataElementVector, size) == 12);

static_assert(sizeof(FormData) == 72);
static_assert(alignof(FormData) == 8);
static_assert(offsetof(FormData, refCount) == 0);
static_assert(offsetof(FormData, elements) == 8);
static_assert(offsetof(FormData, identifier) == 24);
static_assert(offsetof(FormData, alwaysStream) == 32);
static_assert(offsetof(FormData, boundary) == 40);
static_assert(offsetof(FormData, lengthInBytes) == 56);
static_assert(offsetof(FormData, hasLengthInBytes) == 64);

}
