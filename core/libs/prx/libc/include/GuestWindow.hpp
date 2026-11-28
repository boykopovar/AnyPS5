#ifndef CORE_LIBS_PRX_LIBC_INCLUDE_GUESTWINDOW_HPP
#define CORE_LIBS_PRX_LIBC_INCLUDE_GUESTWINDOW_HPP

#include "prx/libc/include/General.hpp"
#include <algorithm>
#include <array>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <stdexcept>

inline void GuestCopyInOrThrow(void* destination, const void* source, std::size_t bytes, const char* message) {
    const int savedErrno = errno;
    const bool copied = GuestCopyIn_nid_no_patch(destination, source, bytes);
    errno = savedErrno;
    if (!copied) throw std::invalid_argument(message);
}

inline void GuestCopyOutOrThrow(void* destination, const void* source, std::size_t bytes, const char* message) {
    const int savedErrno = errno;
    const bool copied = GuestCopyOut_nid_no_patch(destination, source, bytes);
    errno = savedErrno;
    if (!copied) throw std::invalid_argument(message);
}

template<typename TElement, std::size_t Bytes = 4096>
class GuestSourceWindow {
public:
    GuestSourceWindow(const TElement* first, std::size_t count, const char* message) : first(first), count(count), message(message) {}

    GuestSourceWindow(const GuestSourceWindow&) = delete;
    GuestSourceWindow& operator=(const GuestSourceWindow&) = delete;

    TElement operator[](std::size_t index) {
        if (index - start >= size) Fill(index);
        return buffer[index - start];
    }

private:
    static constexpr std::uintptr_t PageBytes = 4096;

    void Fill(std::size_t index) {
        const auto address = reinterpret_cast<std::uintptr_t>(first) + index * sizeof(TElement);
        const std::size_t toPageEnd = (PageBytes - address % PageBytes) / sizeof(TElement);
        const std::size_t length = std::min({count - index, buffer.size(), std::max<std::size_t>(toPageEnd, 1)});
        GuestCopyInOrThrow(buffer.data(), first + index, length * sizeof(TElement), message);
        start = index;
        size = length;
    }

    std::array<TElement, Bytes / sizeof(TElement)> buffer;
    const TElement* first;
    std::size_t count;
    const char* message;
    std::size_t start = 0;
    std::size_t size = 0;
};

template<typename TElement>
class GuestDestinationWindow {
public:
    GuestDestinationWindow(TElement* first, const char* message) : next(first), message(message) {}

    GuestDestinationWindow(const GuestDestinationWindow&) = delete;
    GuestDestinationWindow& operator=(const GuestDestinationWindow&) = delete;

    void Put(TElement value) {
        if (size == buffer.size()) Flush();
        buffer[size++] = value;
    }

    void Flush() {
        if (size == 0) return;
        GuestCopyOutOrThrow(next, buffer.data(), size * sizeof(TElement), message);
        next += size;
        size = 0;
    }

private:
    std::array<TElement, 4096 / sizeof(TElement)> buffer;
    TElement* next;
    const char* message;
    std::size_t size = 0;
};

#endif
