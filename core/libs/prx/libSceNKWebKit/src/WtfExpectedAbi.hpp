#pragma once

#include "prx/libSceNKWebKit/src/WtfStringAbi.hpp"

#include <cstddef>
#include <type_traits>
#include <utility>

namespace NKWebKit::WtfAbi {

struct ExpectedVoidString {
    StringImpl* error;
    bool hasValue;

    ExpectedVoidString() : error(nullptr), hasValue(true) {}
    explicit ExpectedVoidString(String&& message) : error(std::exchange(message.impl, nullptr)), hasValue(false) {}
    ExpectedVoidString(const ExpectedVoidString& other) : error(other.hasValue ? nullptr : other.error), hasValue(other.hasValue) {
        if (error) error->refCount += 2;
    }
    ExpectedVoidString(ExpectedVoidString&& other) noexcept
        : error(other.hasValue ? nullptr : std::exchange(other.error, nullptr)), hasValue(other.hasValue) {}
    ExpectedVoidString& operator=(const ExpectedVoidString&) = delete;
    ExpectedVoidString& operator=(ExpectedVoidString&&) = delete;
    ~ExpectedVoidString() {
        if (!hasValue) Release(error);
    }
};

static_assert(sizeof(ExpectedVoidString) == 16);
static_assert(alignof(ExpectedVoidString) == 8);
static_assert(offsetof(ExpectedVoidString, error) == 0);
static_assert(offsetof(ExpectedVoidString, hasValue) == 8);
static_assert(std::is_standard_layout_v<ExpectedVoidString>);
static_assert(!std::is_trivially_copy_constructible_v<ExpectedVoidString>);
static_assert(!std::is_trivially_move_constructible_v<ExpectedVoidString>);
static_assert(!std::is_trivially_destructible_v<ExpectedVoidString>);

}  // namespace NKWebKit::WtfAbi
