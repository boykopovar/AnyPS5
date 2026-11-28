#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_INLINELIST_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_INLINELIST_HPP

#include <array>
#include <cstddef>
#include <span>
#include <utility>
#include <vector>

namespace AgcDriver::Graphics {

template<typename T, std::size_t Inline>
class InlineList {
public:
    template<typename... TArguments>
    void EmplaceBack(TArguments&&... arguments) {
        if (count < Inline) {
            items[count] = T(std::forward<TArguments>(arguments)...);
        } else {
            if (count == Inline) {
                overflow.reserve(Inline * 2 + 1);
                for (auto& item : items) overflow.push_back(std::move(item));
            }
            overflow.emplace_back(std::forward<TArguments>(arguments)...);
        }
        ++count;
    }
    std::span<T> Span() { return count <= Inline ? std::span<T>(items.data(), count) : std::span<T>(overflow); }
    std::span<const T> Span() const { return count <= Inline ? std::span<const T>(items.data(), count) : std::span<const T>(overflow); }

private:
    std::array<T, Inline> items{};
    std::vector<T> overflow;
    std::size_t count = 0;
};

}

#endif
