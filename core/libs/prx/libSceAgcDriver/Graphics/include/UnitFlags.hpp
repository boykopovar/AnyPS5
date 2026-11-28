#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_UNITFLAGS_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_UNITFLAGS_HPP

#include <atomic>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace AgcDriver::Graphics {

template <bool TConcurrent>
class UnitFlagSet {
    static_assert(!TConcurrent || std::atomic_ref<std::uint64_t>::required_alignment <= alignof(std::uint64_t));

    static std::uint64_t readWord(std::uint64_t& word) {
        if constexpr (TConcurrent) return std::atomic_ref(word).load(std::memory_order_relaxed);
        else return word;
    }

    static void writeMasked(std::uint64_t& word, std::uint64_t mask, bool flag) {
        if constexpr (TConcurrent) {
            if (flag) std::atomic_ref(word).fetch_or(mask, std::memory_order_relaxed);
            else std::atomic_ref(word).fetch_and(~mask, std::memory_order_relaxed);
        } else {
            word = flag ? word | mask : word & ~mask;
        }
    }

public:
    class Reference {
    public:
        Reference(std::uint64_t& word, std::uint64_t bit) : word(word), bit(bit) {}
        operator bool() const { return (readWord(word) & bit) != 0u; }
        Reference& operator=(bool flag) {
            writeMasked(word, bit, flag);
            return *this;
        }

    private:
        std::uint64_t& word;
        std::uint64_t bit;
    };

    void assign(std::size_t size, bool flag) {
        if (size != count) {
            const auto wordCount = (size + WordBits - 1u) / WordBits;
            if (wordCount > words.capacity()) {
                std::vector<std::uint64_t> grown(wordCount, 0u);
                words.swap(grown);
            } else {
                words.assign(wordCount, 0u);
            }
            count = size;
        }
        for (std::size_t index = 0; index < words.size(); ++index) {
            auto value = flag ? ~std::uint64_t{0} : 0u;
            if (flag && index + 1u == words.size() && size % WordBits != 0u) value &= (std::uint64_t{1} << (size % WordBits)) - 1u;
            if constexpr (TConcurrent) std::atomic_ref(words[index]).store(value, std::memory_order_relaxed);
            else words[index] = value;
        }
    }
    void clear() {
        words.clear();
        count = 0;
    }
    std::size_t size() const { return count; }
    bool empty() const { return count == 0; }
    bool operator[](std::size_t index) const { return ((readWord(words[index / WordBits]) >> (index % WordBits)) & 1u) != 0u; }
    Reference operator[](std::size_t index) { return {words[index / WordBits], std::uint64_t{1} << (index % WordBits)}; }
    bool Any() const {
        for (auto& word : words) {
            if (readWord(word) != 0u) return true;
        }
        return false;
    }
    std::size_t Count() const {
        std::size_t total = 0;
        for (auto& word : words) total += static_cast<std::size_t>(std::popcount(readWord(word)));
        return total;
    }
    bool AnyIn(std::size_t first, std::size_t length) const {
        return eachWord(first, length, [&](std::size_t word, std::uint64_t mask) { return (readWord(words[word]) & mask) != 0u; });
    }
    bool AllIn(std::size_t first, std::size_t length) const {
        return !eachWord(first, length, [&](std::size_t word, std::uint64_t mask) { return (readWord(words[word]) & mask) != mask; });
    }
    void SetRange(std::size_t first, std::size_t length, bool flag) {
        eachWord(first, length, [&](std::size_t word, std::uint64_t mask) {
            writeMasked(words[word], mask, flag);
            return false;
        });
    }
    std::vector<bool> Bools() const {
        std::vector<bool> flags(count, false);
        for (std::size_t index = 0; index < count; ++index) flags[index] = (*this)[index];
        return flags;
    }

private:
    static constexpr std::size_t WordBits = 64;

    template <typename TVisit>
    static bool eachWord(std::size_t first, std::size_t length, TVisit&& visit) {
        if (length == 0) return false;
        const auto last = first + length - 1u;
        for (auto word = first / WordBits; word <= last / WordBits; ++word) {
            const auto low = word == first / WordBits ? first % WordBits : 0u;
            const auto high = word == last / WordBits ? last % WordBits : WordBits - 1u;
            const auto mask = (~std::uint64_t{0} >> (WordBits - 1u - high)) & (~std::uint64_t{0} << low);
            if (visit(word, mask)) return true;
        }
        return false;
    }

    mutable std::vector<std::uint64_t> words;
    std::size_t count = 0;
};

using UnitFlags = UnitFlagSet<false>;
using PendingUnitFlags = UnitFlagSet<true>;

}

#endif
