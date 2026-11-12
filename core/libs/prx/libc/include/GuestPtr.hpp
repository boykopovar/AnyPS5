#ifndef CORE_LIBS_PRX_LIBC_INCLUDE_GUESTPTR_HPP
#define CORE_LIBS_PRX_LIBC_INCLUDE_GUESTPTR_HPP

#include "prx/libc/include/General.hpp"
#include <cstddef>
#include <limits>
#include <type_traits>

template <typename T>
class GuestPtr {
public:
    using Value = std::remove_const_t<T>;
    static_assert(std::is_trivially_copyable_v<Value>, "a guest pointer value must be trivially copyable");

    constexpr GuestPtr() = default;
    explicit constexpr GuestPtr(const void* address) : address(address) {}

    const void* Address() const { return address; }

    bool CopyIn(Value& value) const {
        return address != nullptr && GuestCopyIn_nid_no_patch(&value, address, sizeof(Value));
    }

    bool CopyIn(Value* values, std::size_t count) const {
        return address != nullptr && count != 0 && count <= std::numeric_limits<std::size_t>::max() / sizeof(Value) &&
               GuestCopyIn_nid_no_patch(values, address, count * sizeof(Value));
    }

    bool CopyOut(const Value& value) const {
        static_assert(!std::is_const_v<T>, "a const guest pointer cannot be written");
        return address != nullptr && GuestCopyOut_nid_no_patch(const_cast<void*>(address), &value, sizeof(Value));
    }

    bool CopyOut(const Value* values, std::size_t count) const {
        static_assert(!std::is_const_v<T>, "a const guest pointer cannot be written");
        return address != nullptr && count != 0 && count <= std::numeric_limits<std::size_t>::max() / sizeof(Value) &&
               GuestCopyOut_nid_no_patch(const_cast<void*>(address), values, count * sizeof(Value));
    }

private:
    const void* address = nullptr;
};

class GuestBytes {
public:
    constexpr GuestBytes() = default;
    explicit constexpr GuestBytes(const void* address) : address(address) {}

    const void* Address() const { return address; }

    bool CopyIn(void* destination, std::size_t bytes) const {
        return address != nullptr && bytes != 0 && GuestCopyIn_nid_no_patch(destination, address, bytes);
    }

    bool CopyOut(const void* source, std::size_t bytes) const {
        return address != nullptr && bytes != 0 && GuestCopyOut_nid_no_patch(const_cast<void*>(address), source, bytes);
    }

private:
    const void* address = nullptr;
};

#endif
