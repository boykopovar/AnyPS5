#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_HASHSLOTMAP_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_HASHSLOTMAP_HPP

#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

namespace AgcDriver {

template<typename TValue>
class HashSlotMap {
public:
    explicit HashSlotMap(std::size_t capacity = 64) { reset(capacity); }

    std::size_t size() const { return count; }

    void clear() { reset(16); }

    TValue* find(std::uint64_t key) {
        key = stored(key);
        for (auto index = key & mask; keys[index] != 0; index = (index + 1) & mask) {
            if (keys[index] == key) return &values[index];
        }
        return nullptr;
    }

    std::pair<TValue*, bool> try_emplace(std::uint64_t key) {
        if (auto* found = find(key)) return {found, false};
        if ((count + 1) * 2 > keys.size()) grow();
        key = stored(key);
        auto index = key & mask;
        while (keys[index] != 0) index = (index + 1) & mask;
        keys[index] = key;
        values[index] = TValue{};
        ++count;
        return {&values[index], true};
    }

    bool erase(std::uint64_t key) {
        key = stored(key);
        auto hole = key & mask;
        while (keys[hole] != key) {
            if (keys[hole] == 0) return false;
            hole = (hole + 1) & mask;
        }
        for (auto next = (hole + 1) & mask; keys[next] != 0; next = (next + 1) & mask) {
            const auto home = keys[next] & mask;
            const bool stays = hole <= next ? hole < home && home <= next : hole < home || home <= next;
            if (stays) continue;
            keys[hole] = keys[next];
            values[hole] = std::move(values[next]);
            hole = next;
        }
        keys[hole] = 0;
        values[hole] = TValue{};
        --count;
        return true;
    }

private:
    static std::uint64_t stored(std::uint64_t key) { return key != 0 ? key : 1; }

    void reset(std::size_t capacity) {
        std::size_t slots = 16;
        while (slots < capacity) slots *= 2;
        keys.assign(slots, 0);
        values.clear();
        values.resize(slots);
        mask = slots - 1;
        count = 0;
    }

    void grow() {
        auto oldKeys = std::move(keys);
        auto oldValues = std::move(values);
        reset(oldKeys.size() * 2);
        for (std::size_t old = 0; old < oldKeys.size(); ++old) {
            if (oldKeys[old] == 0) continue;
            auto index = oldKeys[old] & mask;
            while (keys[index] != 0) index = (index + 1) & mask;
            keys[index] = oldKeys[old];
            values[index] = std::move(oldValues[old]);
            ++count;
        }
    }

    std::vector<std::uint64_t> keys;
    std::vector<TValue> values;
    std::size_t mask = 0;
    std::size_t count = 0;
};

}

#endif
