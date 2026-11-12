#pragma once

#include "prx/libSceNKWebKit/include/WtfExports.hpp"

#include <cstddef>
#include <new>
#include <utility>

namespace NKWebKit::WtfAbi {

template <typename T>
struct Vector {
    T* buffer{nullptr};
    unsigned capacity{0};
    unsigned size{0};

    Vector() = default;

    explicit Vector(unsigned cap)
        : buffer(cap ? static_cast<T*>(_ZN3WTF10fastMallocEm(cap * sizeof(T))) : nullptr)
        , capacity(cap)
        , size(0) {}

    Vector(const Vector& other)
        : buffer(other.size ? static_cast<T*>(_ZN3WTF10fastMallocEm(other.size * sizeof(T))) : nullptr)
        , capacity(other.size)
        , size(other.size) {
        for (unsigned i = 0; i < size; ++i) {
            new (&buffer[i]) T(other.buffer[i]);
        }
    }

    Vector(Vector&& other) noexcept
        : buffer(std::exchange(other.buffer, nullptr))
        , capacity(std::exchange(other.capacity, 0))
        , size(std::exchange(other.size, 0)) {}

    ~Vector() {
        if (buffer) {
            for (unsigned i = 0; i < size; ++i) {
                buffer[i].~T();
            }
            _ZN3WTF8fastFreeEPv(buffer);
        }
    }

    Vector& operator=(const Vector& other) {
        if (this != &other) {
            Vector temp(other);
            swap(temp);
        }
        return *this;
    }

    Vector& operator=(Vector&& other) noexcept {
        if (this != &other) {
            swap(other);
        }
        return *this;
    }

    void clear() {
        if (buffer) {
            for (unsigned i = 0; i < size; ++i) {
                buffer[i].~T();
            }
            _ZN3WTF8fastFreeEPv(buffer);
        }
        buffer = nullptr;
        capacity = 0;
        size = 0;
    }

    void swap(Vector& other) noexcept {
        std::swap(buffer, other.buffer);
        std::swap(capacity, other.capacity);
        std::swap(size, other.size);
    }

    void append(const T& val) {
        if (size == capacity) {
            unsigned newCap = capacity ? capacity * 2 : 4;
            auto* newBuf = static_cast<T*>(_ZN3WTF10fastMallocEm(newCap * sizeof(T)));
            for (unsigned i = 0; i < size; ++i) {
                new (&newBuf[i]) T(std::move(buffer[i]));
                buffer[i].~T();
            }
            if (buffer) _ZN3WTF8fastFreeEPv(buffer);
            buffer = newBuf;
            capacity = newCap;
        }
        new (&buffer[size++]) T(val);
    }

    T* data() { return buffer; }
    const T* data() const { return buffer; }
    T* begin() { return buffer; }
    const T* begin() const { return buffer; }
    T* end() { return buffer + size; }
    const T* end() const { return buffer + size; }
    T& operator[](unsigned index) { return buffer[index]; }
    const T& operator[](unsigned index) const { return buffer[index]; }
};

}  // namespace NKWebKit::WtfAbi
