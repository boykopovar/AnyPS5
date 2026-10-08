#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_QUEUESTATE_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_QUEUESTATE_HPP

#include <bit>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <iterator>
#include <stdexcept>
#include <utility>
#include <map>
#include <array>
#include <optional>
#include <string>
#include <vector>

namespace AgcDriver {

class Registers {
public:
    using key_type = std::uint32_t;
    using mapped_type = std::uint32_t;
    using value_type = std::pair<const std::uint32_t, std::uint32_t>;

    class const_iterator {
    public:
        using iterator_category = std::forward_iterator_tag;
        using value_type = Registers::value_type;
        using difference_type = std::ptrdiff_t;
        using pointer = const value_type*;
        using reference = value_type;
        struct Arrow {
            value_type entry;
            const value_type* operator->() const { return &entry; }
        };
        const_iterator() = default;
        const_iterator(const Registers* owner, std::size_t index) : owner(owner), index(index) {}
        value_type operator*() const { return {static_cast<std::uint32_t>(index), owner->values[index]}; }
        Arrow operator->() const { return {**this}; }
        const_iterator& operator++() {
            index = owner->nextPresent(index + 1);
            return *this;
        }
        const_iterator operator++(int) {
            auto previous = *this;
            ++*this;
            return previous;
        }
        bool operator==(const const_iterator& other) const { return index == other.index; }
        bool operator!=(const const_iterator& other) const { return index != other.index; }

    private:
        const Registers* owner = nullptr;
        std::size_t index = 0;
    };
    using iterator = const_iterator;

    Registers() = default;
    Registers(std::initializer_list<std::pair<std::uint32_t, std::uint32_t>> entries) {
        for (const auto& [offset, value] : entries) emplace(offset, value);
    }

    const_iterator begin() const { return {this, nextPresent(0)}; }
    const_iterator end() const { return {this, End}; }
    const_iterator find(std::uint32_t offset) const { return contains(offset) ? const_iterator{this, offset} : end(); }
    const_iterator lower_bound(std::uint32_t offset) const { return {this, nextPresent(offset)}; }
    const_iterator upper_bound(std::uint32_t offset) const { return {this, nextPresent(static_cast<std::size_t>(offset) + 1)}; }
    bool contains(std::uint32_t offset) const { return offset < values.size() && ((present[offset / 64] >> (offset % 64)) & 1u) != 0; }
    std::size_t count(std::uint32_t offset) const { return contains(offset) ? 1 : 0; }
    std::size_t size() const { return entries; }
    bool empty() const { return entries == 0; }
    void clear() {
        values.clear();
        present.clear();
        entries = 0;
    }
    std::pair<const_iterator, bool> emplace(std::uint32_t offset, std::uint32_t value) {
        if (contains(offset)) return {const_iterator{this, offset}, false};
        mark(offset) = value;
        return {const_iterator{this, offset}, true};
    }
    std::pair<const_iterator, bool> insert_or_assign(std::uint32_t offset, std::uint32_t value) {
        if (contains(offset)) {
            values[offset] = value;
            return {const_iterator{this, offset}, false};
        }
        mark(offset) = value;
        return {const_iterator{this, offset}, true};
    }
    std::uint32_t& operator[](std::uint32_t offset) {
        if (contains(offset)) return values[offset];
        return mark(offset) = 0;
    }
    std::uint32_t& at(std::uint32_t offset) {
        if (!contains(offset)) throw std::out_of_range("register is not set");
        return values[offset];
    }
    const std::uint32_t& at(std::uint32_t offset) const {
        if (!contains(offset)) throw std::out_of_range("register is not set");
        return values[offset];
    }
    std::size_t erase(std::uint32_t offset) {
        if (!contains(offset)) return 0;
        present[offset / 64] &= ~(std::uint64_t{1} << (offset % 64));
        --entries;
        return 1;
    }
    bool operator==(const Registers& other) const {
        auto a = begin();
        auto b = other.begin();
        for (; a != end() && b != other.end(); ++a, ++b) {
            if ((*a).first != (*b).first || (*a).second != (*b).second) return false;
        }
        return a == end() && b == other.end();
    }

private:
    static constexpr std::size_t End = ~std::size_t{0};
    std::uint32_t& mark(std::uint32_t offset) {
        if (offset >= values.size()) {
            const auto words = static_cast<std::size_t>(offset) / 64 + 1;
            values.resize(words * 64, 0);
            present.resize(words, 0);
        }
        present[offset / 64] |= std::uint64_t{1} << (offset % 64);
        ++entries;
        return values[offset];
    }
    std::size_t nextPresent(std::size_t index) const {
        for (std::size_t word = index / 64; word < present.size(); ++word) {
            auto bits = present[word];
            if (word == index / 64) bits &= ~std::uint64_t{0} << (index % 64);
            if (bits != 0) return word * 64 + static_cast<std::size_t>(std::countr_zero(bits));
        }
        return End;
    }
    std::vector<std::uint32_t> values;
    std::vector<std::uint64_t> present;
    std::size_t entries = 0;
};

inline Registers InitialContextRegisters() {
    Registers result{
        {0x200, 0}, {0x201, 0}, {0x202, 0xcc0010}, {0x203, 0},
        {0x204, 0}, {0x205, 0}, {0x206, 1087}, {0x207, 0},
        {0x0, 0}, {0x2, 0}, {0x3, 0}, {0x4, 0}, {0x8, 0}, {0x9, 0x3f800000}, {0xa, 0}, {0xb, 0},
        {0x80, 0}, {0x83, 0xffff}, {0x8c, 0xaa99aaaa}, {0x8d, 0}, {0x8e, 0}, {0x8f, 0},
        {0xc, 0}, {0xd, 0x40004000}, {0x81, 0x80000000}, {0x82, 0x40004000},
        {0x90, 0x80000000}, {0x91, 0x40004000},
        {0x105, 0}, {0x106, 0}, {0x107, 0}, {0x108, 0},
        {0x1b1, 0}, {0x1b6, 0}, {0x1c3, 0}, {0x1c4, 0}, {0x1c5, 0},
        {0x1ff, 0}, {0x292, 2}, {0x293, 0}, {0x29b, 0},
        {0x2ce, 0}, {0x2d3, 0}, {0x2d5, 0}, {0x2d6, 0}, {0x2db, 0},
        {0x2dc, 0xaa00}, {0x2df, 0}, {0x2e4, 0}, {0x2f8, 0}, {0x2f9, 0x2d},
        {0x30e, 0xffffffff}, {0x30f, 0xffffffff}, {0x313, 0x6000},
        {0x318, 0}, {0x31b, 0}, {0x31c, 0}, {0x31d, 0},
        {0x390, 0}, {0x3b0, 0}, {0x3b8, 0}
    };
    for (std::uint32_t i = 0; i < 8; ++i) {
        result.emplace(0x1e0 + i, 0x20010001);
        result.emplace(0x31c + 0xfu * i, 0);
    }
    for (std::uint32_t i = 0; i < 16; ++i) {
        result.emplace(0x94 + 2 * i, 0x80000000);
        result.emplace(0x95 + 2 * i, 0x40004000);
        result.emplace(0xb4 + 2 * i, 0);
        result.emplace(0xb5 + 2 * i, 0);
        for (std::uint32_t j = 0; j < 6; ++j) result.emplace(0x10f + 6 * i + j, j % 2 == 0 ? 0x3f800000 : 0);
    }
    return result;
}

struct Predication {
    std::uint64_t address = 0;
    std::uint32_t operation = 0;
    bool executeWhenSet = false;
};

struct QueueState {
    Registers shader;
    Registers context = InitialContextRegisters();
    Registers userConfig{{0x24a, 0}, {0x24b, 0}};
    std::optional<Registers> savedContext;
    std::array<std::uint32_t, 0x3000> constantRam{};
    std::uint64_t indexBase = 0;
    std::uint64_t drawIndirectBase = 0;
    std::uint64_t dispatchIndirectBase = 0;
    std::uint32_t indexBufferSize = 0;
    std::uint32_t indexType = 0;
    std::uint32_t instanceCount = 1;
    std::vector<std::string> markers;
    Predication predication;

    void ClearContext() {
        context = InitialContextRegisters();
    }
};

}

#endif
