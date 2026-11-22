#pragma once

#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <utility>

namespace NKWebKit::WebCoreAbi {

struct Pasteboard {
    std::uint64_t changeCount{0};
};

struct PasteboardUniquePtr {
    Pasteboard* pointer;

    explicit PasteboardUniquePtr(Pasteboard* value) : pointer(value) {}
    PasteboardUniquePtr(PasteboardUniquePtr&& other) noexcept : pointer(std::exchange(other.pointer, nullptr)) {}
    PasteboardUniquePtr(const PasteboardUniquePtr&) = delete;
    PasteboardUniquePtr& operator=(const PasteboardUniquePtr&) = delete;
    ~PasteboardUniquePtr() {}
};

enum SmartReplaceOption {
    CanSmartReplace,
    CannotSmartReplace,
};

static_assert(sizeof(Pasteboard) == 8);
static_assert(offsetof(Pasteboard, changeCount) == 0);
static_assert(sizeof(PasteboardUniquePtr) == 8);
static_assert(alignof(PasteboardUniquePtr) == 8);
static_assert(!std::is_trivially_destructible_v<PasteboardUniquePtr>);
static_assert(!std::is_trivially_move_constructible_v<PasteboardUniquePtr>);
static_assert(sizeof(SmartReplaceOption) == 4);
static_assert(CanSmartReplace == 0);
static_assert(CannotSmartReplace == 1);

}  // namespace NKWebKit::WebCoreAbi
