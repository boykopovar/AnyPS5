// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
#include "prx/libSceIcu/libSceIcu.hpp"
#include <bit>
#include <limits>
#include <stdexcept>

namespace {

using namespace Icu::Abi;

template<typename TValue>
TValue& Require(TValue* pointer) {
    if (!pointer) throw std::invalid_argument("ICU UCharsTrie: null pointer");
    return *pointer;
}

std::int32_t Value(const char16_t* position, std::int32_t lead) {
    if (lead < 0x4000) return lead;
    Require(position);
    if (lead < 0x7fff) return ((lead - 0x4000) << 16) | *position;
    return std::bit_cast<std::int32_t>((static_cast<std::uint32_t>(position[0]) << 16) | position[1]);
}

std::int32_t NodeValue(const char16_t* position, std::int32_t lead) {
    if (lead < 0x4040) return (lead >> 6) - 1;
    Require(position);
    if (lead < 0x7fc0) return (((lead & 0x7fc0) - 0x4040) << 10) | *position;
    return std::bit_cast<std::int32_t>((static_cast<std::uint32_t>(position[0]) << 16) | position[1]);
}

const char16_t* SkipValue(const char16_t* position, std::int32_t lead) {
    return position + (lead < 0x4000 ? 0 : lead < 0x7fff ? 1 : 2);
}

const char16_t* SkipNode(const char16_t* position, std::int32_t lead) {
    return position + (lead < 0x4040 ? 0 : lead < 0x7fc0 ? 1 : 2);
}

const char16_t* Delta(const char16_t* position, bool jump) {
    Require(position);
    auto delta = static_cast<std::int32_t>(*position++);
    if (delta >= 0xfc00) {
        if (delta == 0xffff) {
            delta = std::bit_cast<std::int32_t>((static_cast<std::uint32_t>(position[0]) << 16) | position[1]);
            position += 2;
        } else delta = ((delta - 0xfc00) << 16) | *position++;
    }
    return jump ? position + delta : position;
}

std::int32_t Current(const UCharsTrie& trie) {
    return !trie.position ? 0 : trie.remaining < 0 && *trie.position >= 0x40 ? 3 - (*trie.position >> 15) : 1;
}

std::int32_t Stop(UCharsTrie& trie) { trie.position = nullptr; return 0; }

std::int32_t Branch(UCharsTrie& trie, const char16_t* position, std::int32_t length, std::int32_t input) {
    Require(position);
    if (!length) length = *position++;
    if (length == std::numeric_limits<std::int32_t>::max()) throw std::invalid_argument("ICU UCharsTrie: branch length overflow");
    ++length;
    while (length > 5) {
        const bool less = input < *position++;
        length = less ? length / 2 : length - length / 2;
        position = Delta(position, less);
    }
    do {
        if (input == *position++) {
            auto node = *position;
            if (!(node & 0x8000)) {
                const auto delta = Value(position + 1, node);
                position = SkipValue(position + 1, node) + delta;
                node = *position;
            }
            trie.position = position;
            return node >= 0x40 ? 3 - (node >> 15) : 1;
        }
        --length;
        const auto node = *position++;
        position = SkipValue(position, node & 0x7fff);
    } while (length > 1);
    if (input != *position++) return Stop(trie);
    trie.position = position;
    return *position >= 0x40 ? 3 - (*position >> 15) : 1;
}

std::int32_t NextImpl(UCharsTrie& trie, const char16_t* position, std::int32_t input) {
    Require(position);
    auto node = static_cast<std::int32_t>(*position++);
    for (;;) {
        if (node < 0x30) return Branch(trie, position, node, input);
        if (node < 0x40) {
            if (input != *position++) return Stop(trie);
            trie.position = position;
            trie.remaining = node - 0x31;
            return Current(trie);
        }
        if (node & 0x8000) return Stop(trie);
        position = SkipNode(position, node);
        node &= 0x3f;
    }
}

std::int32_t Next(UCharsTrie& trie, std::int32_t input) {
    if (!trie.position) return 0;
    if (trie.remaining < 0) return NextImpl(trie, trie.position, input);
    if (input != *trie.position) return Stop(trie);
    ++trie.position;
    --trie.remaining;
    return Current(trie);
}

bool Unique(const char16_t*, bool, std::int32_t&);

const char16_t* UniqueBranch(const char16_t* position, std::int32_t length, bool haveValue, std::int32_t& value) {
    Require(position);
    if (length == std::numeric_limits<std::int32_t>::min()) throw std::invalid_argument("ICU UCharsTrie: branch length underflow");
    while (length > 5) {
        ++position;
        if (!UniqueBranch(Delta(position, true), length / 2, haveValue, value)) return nullptr;
        length -= length / 2;
        position = Delta(position, false);
    }
    do {
        ++position;
        const auto node = *position++;
        const auto current = Value(position, node & 0x7fff);
        position = SkipValue(position, node & 0x7fff);
        if (node & 0x8000) {
            if (haveValue && current != value) return nullptr;
            value = current;
        } else if (!Unique(position + current, haveValue, value)) return nullptr;
        haveValue = true;
    } while (--length > 1);
    return position + 1;
}

bool Unique(const char16_t* position, bool haveValue, std::int32_t& value) {
    Require(position);
    auto node = static_cast<std::int32_t>(*position++);
    for (;;) {
        if (node < 0x30) {
            if (!node) node = *position++;
            position = UniqueBranch(position, node + 1, haveValue, value);
            if (!position) return false;
            haveValue = true;
            node = *position++;
        } else if (node < 0x40) {
            position += node - 0x30 + 1;
            node = *position++;
        } else {
            const auto current = node & 0x8000 ? Value(position, node & 0x7fff) : NodeValue(position, node);
            if (haveValue && current != value) return false;
            value = current;
            haveValue = true;
            if (node & 0x8000) return true;
            position = SkipNode(position, node);
            node &= 0x3f;
        }
    }
}

void Append(Appendable* output, char16_t value) {
    const auto& table = Require(static_cast<const AppendableFunctions*>(Require(output).vtable));
    if (!table.appendUnit) throw std::invalid_argument("ICU UCharsTrie: null append callback");
    table.appendUnit(output, value);
}

void BranchUnits(const char16_t* position, std::int32_t length, Appendable* output) {
    Require(position);
    if (length == std::numeric_limits<std::int32_t>::min()) throw std::invalid_argument("ICU UCharsTrie: branch length underflow");
    while (length > 5) {
        ++position;
        BranchUnits(Delta(position, true), length / 2, output);
        length -= length / 2;
        position = Delta(position, false);
    }
    do {
        Append(output, *position++);
        const auto node = *position++;
        position = SkipValue(position, node & 0x7fff);
    } while (--length > 1);
    Append(output, *position);
}

}

extern "C" {

void APS5_VABI _ZN6icu_6810UCharsTrieD1Ev_nid_postfix(UCharsTrie* self) { Storage::Release(Require(self).ownedArray); }
void APS5_VABI _ZN6icu_6810UCharsTrieD2Ev_nid_postfix(UCharsTrie* self) { Storage::Release(Require(self).ownedArray); }
std::int32_t APS5_VABI _ZNK6icu_6810UCharsTrie7currentEv_nid_postfix(const UCharsTrie* self) { return Current(Require(self)); }
std::int32_t APS5_VABI _ZN6icu_6810UCharsTrie8nextImplEPKDsi_nid_postfix(UCharsTrie* self, const char16_t* position, std::int32_t input) { return NextImpl(Require(self), position, input); }
std::int32_t APS5_VABI _ZN6icu_6810UCharsTrie10branchNextEPKDsii_nid_postfix(UCharsTrie* self, const char16_t* position, std::int32_t length, std::int32_t input) { return Branch(Require(self), position, length, input); }
std::int32_t APS5_VABI _ZN6icu_6810UCharsTrie4nextEi_nid_postfix(UCharsTrie* self, std::int32_t input) { return Next(Require(self), input); }

std::int32_t APS5_VABI _ZN6icu_6810UCharsTrie16nextForCodePointEi_nid_postfix(UCharsTrie* self, std::int32_t point) {
    auto& trie = Require(self);
    if (point <= 0xffff) return Next(trie, point);
    const auto result = Next(trie, static_cast<char16_t>((point >> 10) + 0xd7c0));
    return result & 1 ? Next(trie, (point & 0x3ff) | 0xdc00) : 0;
}

std::int32_t APS5_VABI _ZN6icu_6810UCharsTrie17firstForCodePointEi_nid_postfix(UCharsTrie* self, std::int32_t point) {
    auto& trie = Require(self);
    trie.remaining = -1;
    if (point <= 0xffff) return NextImpl(trie, trie.units, point);
    const auto result = NextImpl(trie, trie.units, static_cast<char16_t>((point >> 10) + 0xd7c0));
    return result & 1 ? Next(trie, (point & 0x3ff) | 0xdc00) : 0;
}

std::int32_t APS5_VABI _ZN6icu_6810UCharsTrie4nextENS_14ConstChar16PtrEi_nid_postfix(UCharsTrie* self, const ConstChar16Ptr* pointer, std::int32_t length) {
    auto* text = Require(pointer).pointer;
    if (length < 0 && !Require(text)) return Current(Require(self));
    if (length == 0) return Current(Require(self));
    auto& trie = Require(self);
    if (!trie.position) return 0;
    auto* position = trie.position;
    auto remaining = trie.remaining;
    for (;;) {
        std::int32_t input;
        for (;;) {
            if (length == 0 || (length < 0 && !Require(text))) {
                trie.position = position;
                trie.remaining = remaining;
                return Current(trie);
            }
            Require(text);
            input = *text++;
            if (length > 0) --length;
            if (remaining < 0) {
                trie.remaining = remaining;
                break;
            }
            if (input != *position++) return Stop(trie);
            --remaining;
        }
        auto node = static_cast<std::int32_t>(*position++);
        for (;;) {
            if (node < 0x30) {
                const auto result = Branch(trie, position, node, input);
                if (!result) return 0;
                if (length == 0 || (length < 0 && !Require(text))) return result;
                Require(text);
                input = *text++;
                if (length > 0) --length;
                if (result == 2) return Stop(trie);
                position = trie.position;
                node = *position++;
            } else if (node < 0x40) {
                remaining = node - 0x31;
                if (input != *position++) return Stop(trie);
                break;
            } else if (node & 0x8000) return Stop(trie);
            else {
                position = SkipNode(position, node);
                node &= 0x3f;
            }
        }
    }
}

std::int8_t APS5_VABI _ZN6icu_6810UCharsTrie15findUniqueValueEPKDsaRi_nid_postfix(const char16_t* position, std::int8_t haveValue, std::int32_t* value) { return Unique(position, haveValue != 0, Require(value)); }
const char16_t* APS5_VABI _ZN6icu_6810UCharsTrie25findUniqueValueFromBranchEPKDsiaRi_nid_postfix(const char16_t* position, std::int32_t length, std::int8_t haveValue, std::int32_t* value) { return UniqueBranch(position, length, haveValue != 0, Require(value)); }
void APS5_VABI _ZN6icu_6810UCharsTrie19getNextBranchUCharsEPKDsiRNS_10AppendableE_nid_postfix(const char16_t* position, std::int32_t length, Appendable* output) { BranchUnits(position, length, output); }

std::int32_t APS5_VABI _ZNK6icu_6810UCharsTrie13getNextUCharsERNS_10AppendableE_nid_postfix(const UCharsTrie* self, Appendable* output) {
    const auto& trie = Require(self);
    auto* position = trie.position;
    if (!position) return 0;
    if (trie.remaining >= 0) {
        Append(output, *position);
        return 1;
    }
    auto node = static_cast<std::int32_t>(*position++);
    if (node >= 0x40) {
        if (node & 0x8000) return 0;
        position = SkipNode(position, node);
        node &= 0x3f;
    }
    if (node < 0x30) {
        if (!node) node = *position++;
        const auto length = node + 1;
        const auto& table = Require(static_cast<const AppendableFunctions*>(Require(output).vtable));
        if (!table.reserve) throw std::invalid_argument("ICU UCharsTrie: null reserve callback");
        table.reserve(output, length);
        BranchUnits(position, length, output);
        return length;
    }
    Append(output, *position);
    return 1;
}

}
