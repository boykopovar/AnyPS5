// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
#include "prx/libSceIcu/libSceIcu.hpp"
#include <bit>
#include <limits>
#include <stdexcept>

namespace {

using namespace Icu::Abi;

template<typename TValue>
TValue& Require(TValue* value) {
    if (!value) throw std::invalid_argument("ICU bytes trie: null pointer");
    return *value;
}

std::int32_t Value(const std::uint8_t* position, std::int32_t lead) {
    if (lead < 0x51) {
        if (lead < std::numeric_limits<std::int32_t>::min() + 0x10) throw std::invalid_argument("ICU bytes trie: value overflow");
        return lead - 0x10;
    }
    Require(position);
    if (lead < 0x6c) return ((lead - 0x51) << 8) | position[0];
    if (lead < 0x7e) return ((lead - 0x6c) << 16) | (position[0] << 8) | position[1];
    if (lead == 0x7e) return (position[0] << 16) | (position[1] << 8) | position[2];
    return std::bit_cast<std::int32_t>((static_cast<std::uint32_t>(position[0]) << 24) | (position[1] << 16) | (position[2] << 8) | position[3]);
}

const std::uint8_t* SkipValue(const std::uint8_t* position, std::int32_t lead) {
    if (lead < 0xa2) return position;
    Require(position);
    return position + (lead < 0xd8 ? 1 : lead < 0xfc ? 2 : 3 + ((lead >> 1) & 1));
}

const std::uint8_t* SkipValue(const std::uint8_t* position) {
    const auto lead = Require(position);
    return SkipValue(position + 1, lead);
}

const std::uint8_t* Delta(const std::uint8_t* position, bool jump) {
    Require(position);
    const auto lead = *position++;
    std::int32_t delta = lead;
    if (lead >= 0xc0) {
        if (lead < 0xf0) delta = ((lead - 0xc0) << 8) | *position++;
        else if (lead < 0xfe) {
            delta = ((lead - 0xf0) << 16) | (position[0] << 8) | position[1];
            position += 2;
        } else if (lead == 0xfe) {
            delta = (position[0] << 16) | (position[1] << 8) | position[2];
            position += 3;
        } else {
            delta = std::bit_cast<std::int32_t>((static_cast<std::uint32_t>(position[0]) << 24) | (position[1] << 16) | (position[2] << 8) | position[3]);
            position += 4;
        }
    }
    return jump ? position + delta : position;
}

std::int32_t Current(const BytesTrie& trie) {
    return !trie.position ? 0 : trie.remaining < 0 && *trie.position >= 0x20 ? 3 - (*trie.position & 1) : 1;
}

std::int32_t Stop(BytesTrie& trie) { trie.position = nullptr; return 0; }

std::int32_t Branch(BytesTrie& trie, const std::uint8_t* position, std::int32_t length, std::int32_t input) {
    Require(position);
    if (length == 0) length = *position++;
    if (length == std::numeric_limits<std::int32_t>::max()) throw std::invalid_argument("ICU bytes trie: branch length overflow");
    ++length;
    while (length > 5) {
        const bool less = input < *position++;
        position = Delta(position, less);
        length = less ? length / 2 : length - length / 2;
    }
    do {
        if (input == *position++) {
            auto node = *position;
            if (!(node & 1)) {
                const auto delta = Value(position + 1, node >> 1);
                position = SkipValue(position + 1, node) + delta;
                node = *position;
            }
            trie.position = position;
            return node >= 0x20 ? 3 - (node & 1) : 1;
        }
        --length;
        position = SkipValue(position);
    } while (length > 1);
    if (input != *position++) return Stop(trie);
    trie.position = position;
    return *position >= 0x20 ? 3 - (*position & 1) : 1;
}

std::int32_t NextImpl(BytesTrie& trie, const std::uint8_t* position, std::int32_t input) {
    Require(position);
    for (;;) {
        const auto node = *position++;
        if (node < 0x10) return Branch(trie, position, node, input);
        if (node < 0x20) {
            if (input != *position++) return Stop(trie);
            trie.remaining = node - 0x11;
            trie.position = position;
            return Current(trie);
        }
        if (node & 1) return Stop(trie);
        position = SkipValue(position, node);
    }
}

bool Unique(const std::uint8_t*, bool, std::int32_t&);

const std::uint8_t* UniqueBranch(const std::uint8_t* position, std::int32_t length, bool haveValue, std::int32_t& value) {
    Require(position);
    if (length == std::numeric_limits<std::int32_t>::min()) throw std::invalid_argument("ICU bytes trie: branch length underflow");
    while (length > 5) {
        ++position;
        if (!UniqueBranch(Delta(position, true), length / 2, haveValue, value)) return nullptr;
        length -= length / 2;
        position = Delta(position, false);
    }
    do {
        ++position;
        const auto node = *position++;
        const auto current = Value(position, node >> 1);
        position = SkipValue(position, node);
        if (node & 1) {
            if (haveValue && current != value) return nullptr;
            value = current;
        } else if (!Unique(position + current, haveValue, value)) return nullptr;
        haveValue = true;
    } while (--length > 1);
    return position + 1;
}

bool Unique(const std::uint8_t* position, bool haveValue, std::int32_t& value) {
    Require(position);
    for (;;) {
        auto node = *position++;
        if (node < 0x10) {
            if (node == 0) node = *position++;
            position = UniqueBranch(position, node + 1, haveValue, value);
            if (!position) return false;
            haveValue = true;
        } else if (node < 0x20) position += node - 0x10 + 1;
        else {
            const auto current = Value(position, node >> 1);
            if (haveValue && current != value) return false;
            value = current;
            haveValue = true;
            if (node & 1) return true;
            position = SkipValue(position, node);
        }
    }
}

void Append(ByteSink* output, std::int32_t value) {
    auto& sink = Require(output);
    const auto& table = Require(static_cast<const ByteSinkFunctions*>(sink.vtable));
    if (!table.append) throw std::invalid_argument("ICU bytes trie: null sink callback");
    const auto byte = static_cast<char>(value);
    table.append(output, &byte, 1);
}

void BranchBytes(const std::uint8_t* position, std::int32_t length, ByteSink* output) {
    Require(position);
    if (length == std::numeric_limits<std::int32_t>::min()) throw std::invalid_argument("ICU bytes trie: branch length underflow");
    while (length > 5) {
        ++position;
        BranchBytes(Delta(position, true), length / 2, output);
        length -= length / 2;
        position = Delta(position, false);
    }
    do {
        Append(output, *position++);
        position = SkipValue(position);
    } while (--length > 1);
    Append(output, *position);
}

}

extern "C" {

void APS5_VABI _ZN6icu_689BytesTrieD1Ev_nid_postfix(BytesTrie* self) { Storage::Release(Require(self).ownedArray); }
void APS5_VABI _ZN6icu_689BytesTrieD2Ev_nid_postfix(BytesTrie* self) { Storage::Release(Require(self).ownedArray); }
std::int32_t APS5_VABI _ZN6icu_689BytesTrie9readValueEPKhi_nid_postfix(const std::uint8_t* position, std::int32_t lead) { return Value(position, lead); }
const std::uint8_t* APS5_VABI _ZN6icu_689BytesTrie11jumpByDeltaEPKh_nid_postfix(const std::uint8_t* position) { return Delta(position, true); }
std::int32_t APS5_VABI _ZNK6icu_689BytesTrie7currentEv_nid_postfix(const BytesTrie* self) { return Current(Require(self)); }
std::int32_t APS5_VABI _ZN6icu_689BytesTrie10branchNextEPKhii_nid_postfix(BytesTrie* self, const std::uint8_t* position, std::int32_t length, std::int32_t input) { return Branch(Require(self), position, length, input); }
std::int32_t APS5_VABI _ZN6icu_689BytesTrie8nextImplEPKhi_nid_postfix(BytesTrie* self, const std::uint8_t* position, std::int32_t input) { return NextImpl(Require(self), position, input); }

std::int32_t APS5_VABI _ZN6icu_689BytesTrie4nextEi_nid_postfix(BytesTrie* self, std::int32_t input) {
    auto& trie = Require(self);
    if (!trie.position) return 0;
    if (input < 0) input += 256;
    if (trie.remaining < 0) return NextImpl(trie, trie.position, input);
    if (input != *trie.position) return Stop(trie);
    ++trie.position;
    --trie.remaining;
    return Current(trie);
}

std::int32_t APS5_VABI _ZN6icu_689BytesTrie4nextEPKci_nid_postfix(BytesTrie* self, const char* text, std::int32_t length) {
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
            input = static_cast<std::int8_t>(*text++);
            if (length > 0) --length;
            if (remaining < 0) {
                trie.remaining = remaining;
                break;
            }
            if (input != *position++) return Stop(trie);
            --remaining;
        }
        for (;;) {
            const auto node = *position++;
            if (node < 0x10) {
                const auto result = Branch(trie, position, node, input);
                if (!result) return 0;
                if (length == 0 || (length < 0 && !Require(text))) return result;
                Require(text);
                input = static_cast<std::int8_t>(*text++);
                if (length > 0) --length;
                if (result == 2) return Stop(trie);
                position = trie.position;
            } else if (node < 0x20) {
                remaining = node - 0x11;
                if (input != *position++) return Stop(trie);
                break;
            } else if (node & 1) return Stop(trie);
            else position = SkipValue(position, node);
        }
    }
}

std::int8_t APS5_VABI _ZN6icu_689BytesTrie15findUniqueValueEPKhaRi_nid_postfix(const std::uint8_t* position, std::int8_t haveValue, std::int32_t* value) { return Unique(position, haveValue != 0, Require(value)); }
const std::uint8_t* APS5_VABI _ZN6icu_689BytesTrie25findUniqueValueFromBranchEPKhiaRi_nid_postfix(const std::uint8_t* position, std::int32_t length, std::int8_t haveValue, std::int32_t* value) { return UniqueBranch(position, length, haveValue != 0, Require(value)); }
void APS5_VABI _ZN6icu_689BytesTrie6appendERNS_8ByteSinkEi_nid_postfix(ByteSink* output, std::int32_t value) { Append(output, value); }
void APS5_VABI _ZN6icu_689BytesTrie18getNextBranchBytesEPKhiRNS_8ByteSinkE_nid_postfix(const std::uint8_t* position, std::int32_t length, ByteSink* output) { BranchBytes(position, length, output); }

std::int32_t APS5_VABI _ZNK6icu_689BytesTrie12getNextBytesERNS_8ByteSinkE_nid_postfix(const BytesTrie* self, ByteSink* output) {
    const auto& trie = Require(self);
    auto* position = trie.position;
    if (!position) return 0;
    if (trie.remaining >= 0) {
        Append(output, *position);
        return 1;
    }
    auto node = *position++;
    if (node >= 0x20) {
        if (node & 1) return 0;
        position = SkipValue(position, node);
        node = *position++;
    }
    if (node < 0x10) {
        if (node == 0) node = *position++;
        const auto length = static_cast<std::int32_t>(node) + 1;
        BranchBytes(position, length, output);
        return length;
    }
    Append(output, *position);
    return 1;
}

}
