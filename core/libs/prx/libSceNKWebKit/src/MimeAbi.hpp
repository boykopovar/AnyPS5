#pragma once

#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/src/WtfStringAbi.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"

#include <cstddef>
#include <cstdint>
#include <new>
#include <type_traits>
#include <utility>

#include "prx/libSceNKWebKit/src/WtfVector.hpp"

namespace NKWebKit::WtfAbi {

struct ASCIILiteral {
    const char* characters{nullptr};

    constexpr ASCIILiteral() = default;
    constexpr ASCIILiteral(const char* str) : characters(str) {}
    constexpr operator const char*() const { return characters; }
};

static_assert(sizeof(ASCIILiteral) == 8);
static_assert(alignof(ASCIILiteral) == 8);


static_assert(sizeof(Vector<String>) == 16);
static_assert(alignof(Vector<String>) == 8);
static_assert(offsetof(Vector<String>, buffer) == 0);
static_assert(offsetof(Vector<String>, capacity) == 8);
static_assert(offsetof(Vector<String>, size) == 12);
static_assert(!std::is_trivially_destructible_v<Vector<String>>);

struct EmbeddedFixedVectorStorage {
    unsigned size;
    unsigned pad;
};

static_assert(sizeof(EmbeddedFixedVectorStorage) == 8);
static_assert(alignof(EmbeddedFixedVectorStorage) == 4);

template <typename T>
struct FixedVector {
    EmbeddedFixedVectorStorage* storage{nullptr};

    FixedVector() = default;

    explicit FixedVector(unsigned count) {
        if (count) {
            std::size_t bytes = sizeof(EmbeddedFixedVectorStorage) + count * sizeof(T);
            storage = static_cast<EmbeddedFixedVectorStorage*>(_ZN3WTF10fastMallocEm(bytes));
            storage->size = count;
            storage->pad = 0;
            T* dataPtr = reinterpret_cast<T*>(reinterpret_cast<char*>(storage) + sizeof(EmbeddedFixedVectorStorage));
            for (unsigned i = 0; i < count; ++i) {
                new (&dataPtr[i]) T();
            }
        }
    }

    FixedVector(const FixedVector& other) {
        if (other.storage) {
            unsigned count = other.storage->size;
            std::size_t bytes = sizeof(EmbeddedFixedVectorStorage) + count * sizeof(T);
            storage = static_cast<EmbeddedFixedVectorStorage*>(_ZN3WTF10fastMallocEm(bytes));
            storage->size = count;
            storage->pad = 0;
            const T* src = reinterpret_cast<const T*>(reinterpret_cast<const char*>(other.storage) + sizeof(EmbeddedFixedVectorStorage));
            T* dst = reinterpret_cast<T*>(reinterpret_cast<char*>(storage) + sizeof(EmbeddedFixedVectorStorage));
            for (unsigned i = 0; i < count; ++i) {
                new (&dst[i]) T(src[i]);
            }
        }
    }

    FixedVector(FixedVector&& other) noexcept : storage(std::exchange(other.storage, nullptr)) {}

    ~FixedVector() {
        if (storage) {
            unsigned count = storage->size;
            T* dataPtr = reinterpret_cast<T*>(reinterpret_cast<char*>(storage) + sizeof(EmbeddedFixedVectorStorage));
            for (unsigned i = 0; i < count; ++i) {
                dataPtr[i].~T();
            }
            _ZN3WTF8fastFreeEPv(storage);
        }
    }

    FixedVector& operator=(const FixedVector& other) {
        if (this != &other) {
            FixedVector temp(other);
            swap(temp);
        }
        return *this;
    }

    FixedVector& operator=(FixedVector&& other) noexcept {
        if (this != &other) {
            swap(other);
        }
        return *this;
    }

    void swap(FixedVector& other) noexcept {
        std::swap(storage, other.storage);
    }

    unsigned size() const { return storage ? storage->size : 0; }
    T* data() { return storage ? reinterpret_cast<T*>(reinterpret_cast<char*>(storage) + sizeof(EmbeddedFixedVectorStorage)) : nullptr; }
    const T* data() const { return storage ? reinterpret_cast<const T*>(reinterpret_cast<const char*>(storage) + sizeof(EmbeddedFixedVectorStorage)) : nullptr; }
    T* begin() { return data(); }
    const T* begin() const { return data(); }
    T* end() { return data() ? data() + size() : nullptr; }
    const T* end() const { return data() ? data() + size() : nullptr; }
    T& operator[](unsigned index) { return data()[index]; }
    const T& operator[](unsigned index) const { return data()[index]; }
};

static_assert(sizeof(FixedVector<ASCIILiteral>) == 8);
static_assert(alignof(FixedVector<ASCIILiteral>) == 8);
static_assert(offsetof(FixedVector<ASCIILiteral>, storage) == 0);
static_assert(!std::is_trivially_destructible_v<FixedVector<ASCIILiteral>>);

}  // namespace NKWebKit::WtfAbi
