#include "prx/libc/include/Api.hpp"

#include <algorithm>
#include <array>
#include <cstring>
#include <string>

namespace {

constexpr std::uint64_t ShortCapacity = 22;
constexpr std::uint64_t AllocationAlignment = 16;
constexpr std::size_t CurrentDirectoryCapacity = 1024;

std::uint64_t Recommend(std::uint64_t size) {
    if (size <= ShortCapacity) return ShortCapacity;
    return ((size + AllocationAlignment) & ~(AllocationAlignment - 1)) - 1;
}

std::uint64_t Grow(std::uint64_t capacity, std::uint64_t required) {
    return Recommend(std::max(required, 2 * capacity));
}

std::string ReadPath(const void* path) {
    std::array<char, 24> descriptor{};
    if (!GuestCopyIn_nid_no_patch(descriptor.data(), path, descriptor.size())) {
        _ZSt18_Xinvalid_argumentPKc_nid_postfix("filesystem::absolute: unreadable path");
    }
    const auto flags = static_cast<unsigned char>(descriptor[0]);
    if ((flags & 1u) == 0u) {
        const std::uint64_t size = flags >> 1u;
        if (size > ShortCapacity) _ZSt18_Xinvalid_argumentPKc_nid_postfix("filesystem::absolute: invalid short path");
        return std::string(descriptor.data() + 1, size);
    }
    std::uint64_t capacity = 0;
    std::uint64_t size = 0;
    std::uint64_t address = 0;
    std::memcpy(&capacity, descriptor.data(), sizeof(capacity));
    std::memcpy(&size, descriptor.data() + 8, sizeof(size));
    std::memcpy(&address, descriptor.data() + 16, sizeof(address));
    if (address == 0 || address > UINTPTR_MAX || size >= UINTPTR_MAX - address || size >= (capacity & ~std::uint64_t{1})) {
        _ZSt18_Xinvalid_argumentPKc_nid_postfix("filesystem::absolute: invalid long path");
    }
    std::string text(size, '\0');
    if (!GuestCopyIn_nid_no_patch(text.data(), reinterpret_cast<const void*>(static_cast<std::uintptr_t>(address)), size)) {
        _ZSt18_Xinvalid_argumentPKc_nid_postfix("filesystem::absolute: unreadable long path");
    }
    return text;
}

void WritePath(void* result, const std::string& text, std::uint64_t capacity) {
    std::array<char, 24> descriptor{};
    if (capacity == ShortCapacity) {
        descriptor[0] = static_cast<char>(text.size() << 1u);
        std::memcpy(descriptor.data() + 1, text.data(), text.size());
        if (!GuestCopyOut_nid_no_patch(result, descriptor.data(), descriptor.size())) {
            _ZSt18_Xinvalid_argumentPKc_nid_postfix("filesystem::absolute: unwritable result");
        }
        return;
    }
    const std::uint64_t allocation = capacity + 1;
    auto* data = static_cast<char*>(_Znwm_nid_postfix(allocation));
    std::memcpy(data, text.c_str(), text.size() + 1);
    const std::uint64_t flags = allocation | 1u;
    const std::uint64_t size = text.size();
    const auto address = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(data));
    std::memcpy(descriptor.data(), &flags, sizeof(flags));
    std::memcpy(descriptor.data() + 8, &size, sizeof(size));
    std::memcpy(descriptor.data() + 16, &address, sizeof(address));
    if (!GuestCopyOut_nid_no_patch(result, descriptor.data(), descriptor.size())) {
        _ZdlPv_nid_postfix(data);
        _ZSt18_Xinvalid_argumentPKc_nid_postfix("filesystem::absolute: unwritable result");
    }
}

}

extern "C" void* APS5_VABI _ZNSt3__14__fs10filesystem10__absoluteERKNS1_4pathEPNS_10error_codeE_nid_postfix(
    void* result, const void* path, void* error) {
    if (error != nullptr) NotImplemented_nid_no_patch(__func__);
    const auto relative = ReadPath(path);
    if (!relative.empty() && relative.front() == '/') {
        WritePath(result, relative, Recommend(relative.size()));
        return result;
    }
    std::array<char, CurrentDirectoryCapacity> current{};
    if (getcwd_nid_postfix(current.data(), current.size()) == nullptr) NotImplemented_nid_no_patch(__func__);
    std::string joined(current.data());
    auto capacity = Recommend(joined.size());
    if (!joined.empty() && joined.back() != '/') {
        if (joined.size() == capacity) capacity = Grow(capacity, capacity + 1);
        joined.push_back('/');
    }
    if (capacity - joined.size() < relative.size()) capacity = Grow(capacity, joined.size() + relative.size());
    joined += relative;
    WritePath(result, joined, capacity);
    return result;
}
