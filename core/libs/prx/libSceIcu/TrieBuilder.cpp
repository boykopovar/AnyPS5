// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
// Copyright (C) 2010-2012, International Business Machines Corporation and others. All Rights Reserved.
#include "TrieBuilder.hpp"
#include <algorithm>
#include <array>
#include <cstring>
#include <new>
#include <stdexcept>

namespace {
using namespace Icu::Abi;
using namespace Icu;

template<typename TValue> TValue& Require(TValue* value) {
    if (!value) throw std::invalid_argument("TrieBuilder: null pointer");
    return *value;
}

const void* RequirePointer(const void* pointer) {
    if (!pointer) throw std::invalid_argument("TrieBuilder: null pointer");
    return pointer;
}

void APS5_VABI Abstract() { throw std::invalid_argument("TrieBuilder: abstract method"); }

enum class NodeKind : std::uint8_t {
    FinalValue,
    IntermediateValue,
    LinearMatch,
    ListBranch,
    SplitBranch,
    BranchHead,
    UCTLinearMatch
};

struct Node {
    NodeKind kind;
    std::int32_t hash;
    std::int32_t offset;

    virtual ~Node() = default;
    virtual bool operator==(const Node& other) const = 0;
    virtual std::int32_t markRightEdgesFirst(std::int32_t edgeNumber) {
        if (offset == 0) offset = edgeNumber;
        return edgeNumber;
    }
    virtual void write(StringTrieBuilder& builder) = 0;
    void writeUnlessInsideRightEdge(std::int32_t firstEdgeNumber, std::int32_t rightEdgeNumber, StringTrieBuilder& builder) {
        std::int32_t edgeNumber = offset;
        if (edgeNumber < 0) {
            if (edgeNumber < firstEdgeNumber || rightEdgeNumber < edgeNumber) {
                write(builder);
            }
        }
    }
};

std::int32_t NodeHashCode(const Node* node) {
    return node ? node->hash : 0;
}

struct FinalValueNode : public Node {
    std::int32_t value;

    FinalValueNode(std::int32_t v) : value(v) {
        kind = NodeKind::FinalValue;
        hash = static_cast<std::int32_t>(0x111111u * 37u + static_cast<std::uint32_t>(v));
        offset = 0;
    }

    bool operator==(const Node& other) const override {
        if (this == &other) return true;
        if (other.kind != NodeKind::FinalValue || hash != other.hash) return false;
        return value == static_cast<const FinalValueNode&>(other).value;
    }

    void write(StringTrieBuilder& builder) override {
        auto writeValFinal = Virtual_nid_no_patch<std::int32_t (APS5_VABI*)(void*, std::int32_t, std::int8_t)>(&builder, 17);
        offset = writeValFinal(&builder, value, 1);
    }
};

struct ValueNode : public Node {
    std::int8_t hasValue;
    std::int32_t value;

    ValueNode(std::int32_t initialHash) : hasValue(0), value(0) {
        hash = initialHash;
        offset = 0;
    }

    void setValue(std::int32_t v) {
        hasValue = 1;
        value = v;
        hash = static_cast<std::int32_t>(static_cast<std::uint32_t>(hash) * 37u + static_cast<std::uint32_t>(v));
    }

    bool operator==(const Node& other) const override {
        if (this == &other) return true;
        if (hash != other.hash) return false;
        const auto& o = static_cast<const ValueNode&>(other);
        return hasValue == o.hasValue && (!hasValue || value == o.value);
    }
};

struct IntermediateValueNode : public ValueNode {
    Node* next;

    IntermediateValueNode(std::int32_t v, Node* nextNode)
        : ValueNode(static_cast<std::int32_t>(0x222222u * 37u + static_cast<std::uint32_t>(NodeHashCode(nextNode)))), next(nextNode) {
        kind = NodeKind::IntermediateValue;
        setValue(v);
    }

    bool operator==(const Node& other) const override {
        if (this == &other) return true;
        if (other.kind != NodeKind::IntermediateValue || !ValueNode::operator==(other)) return false;
        return next == static_cast<const IntermediateValueNode&>(other).next;
    }

    std::int32_t markRightEdgesFirst(std::int32_t edgeNumber) override {
        if (offset == 0) {
            offset = edgeNumber = next->markRightEdgesFirst(edgeNumber);
        }
        return edgeNumber;
    }

    void write(StringTrieBuilder& builder) override {
        next->write(builder);
        auto getMinLinear = Virtual_nid_no_patch<std::int32_t (APS5_VABI*)(const void*)>(&builder, 12);
        auto writeValType = Virtual_nid_no_patch<std::int32_t (APS5_VABI*)(void*, std::int8_t, std::int32_t, std::int32_t)>(&builder, 18);
        offset = writeValType(&builder, hasValue, value, getMinLinear(&builder) - 1);
    }
};

struct LinearMatchNode : public ValueNode {
    std::int32_t length;
    Node* next;

    LinearMatchNode(std::int32_t len, Node* nextNode)
        : ValueNode(static_cast<std::int32_t>((0x333333u * 37u + static_cast<std::uint32_t>(len)) * 37u + static_cast<std::uint32_t>(NodeHashCode(nextNode)))),
          length(len), next(nextNode) {
        kind = NodeKind::LinearMatch;
    }

    bool operator==(const Node& other) const override {
        if (this == &other) return true;
        if (!ValueNode::operator==(other)) return false;
        const auto& o = static_cast<const LinearMatchNode&>(other);
        return length == o.length && next == o.next;
    }

    std::int32_t markRightEdgesFirst(std::int32_t edgeNumber) override {
        if (offset == 0) {
            offset = edgeNumber = next->markRightEdgesFirst(edgeNumber);
        }
        return edgeNumber;
    }
};

struct UCTLinearMatchNode : public LinearMatchNode {
    const char16_t* s;

    UCTLinearMatchNode(const char16_t* units, std::int32_t len, Node* nextNode)
        : LinearMatchNode(len, nextNode), s(units) {
        kind = NodeKind::UCTLinearMatch;
        hash = static_cast<std::int32_t>(static_cast<std::uint32_t>(hash) * 37u + static_cast<std::uint32_t>(ustr_hashUCharsN_nid_postfix(units, len)));
    }

    bool operator==(const Node& other) const override {
        if (this == &other) return true;
        if (other.kind != NodeKind::UCTLinearMatch || !LinearMatchNode::operator==(other)) return false;
        const auto& o = static_cast<const UCTLinearMatchNode&>(other);
        return std::memcmp(s, o.s, static_cast<std::size_t>(length) * sizeof(char16_t)) == 0;
    }

    void write(StringTrieBuilder& builder) override {
        next->write(builder);
        auto getMinLinear = Virtual_nid_no_patch<std::int32_t (APS5_VABI*)(const void*)>(&builder, 12);
        auto writeValType = Virtual_nid_no_patch<std::int32_t (APS5_VABI*)(void*, std::int8_t, std::int32_t, std::int32_t)>(&builder, 18);
        _ZN6icu_6817UCharsTrieBuilder5writeEPKDsi_nid_postfix(reinterpret_cast<UCharsTrieBuilder*>(&builder), s, length);
        offset = writeValType(&builder, hasValue, value, getMinLinear(&builder) + length - 1);
    }
};

struct BranchNode : public Node {
    std::int32_t firstEdgeNumber;

    BranchNode(std::int32_t initialHash) : firstEdgeNumber(0) {
        hash = initialHash;
        offset = 0;
    }
};

struct ListBranchNode : public BranchNode {
    Node* equal[5];
    std::int32_t length;
    std::int32_t values[5];
    char16_t units[5];

    ListBranchNode() : BranchNode(0x444444), length(0) {
        kind = NodeKind::ListBranch;
        for (int i = 0; i < 5; ++i) {
            equal[i] = nullptr;
            values[i] = 0;
            units[i] = 0;
        }
    }

    bool operator==(const Node& other) const override {
        if (this == &other) return true;
        if (other.kind != NodeKind::ListBranch || hash != other.hash) return false;
        const auto& o = static_cast<const ListBranchNode&>(other);
        if (length != o.length) return false;
        for (std::int32_t i = 0; i < length; ++i) {
            if (units[i] != o.units[i] || values[i] != o.values[i] || equal[i] != o.equal[i]) return false;
        }
        return true;
    }

    void add(std::int32_t c, std::int32_t val) {
        units[length] = static_cast<char16_t>(c);
        equal[length] = nullptr;
        values[length] = val;
        ++length;
        hash = static_cast<std::int32_t>((static_cast<std::uint32_t>(hash) * 37u + static_cast<std::uint32_t>(c)) * 37u + static_cast<std::uint32_t>(val));
    }

    void add(std::int32_t c, Node* node) {
        units[length] = static_cast<char16_t>(c);
        equal[length] = node;
        values[length] = 0;
        ++length;
        hash = static_cast<std::int32_t>((static_cast<std::uint32_t>(hash) * 37u + static_cast<std::uint32_t>(c)) * 37u + static_cast<std::uint32_t>(NodeHashCode(node)));
    }

    std::int32_t markRightEdgesFirst(std::int32_t edgeNumber) override {
        if (offset == 0) {
            firstEdgeNumber = edgeNumber;
            std::int32_t step = 0;
            std::int32_t i = length;
            do {
                Node* edge = equal[--i];
                if (edge) {
                    edgeNumber = edge->markRightEdgesFirst(edgeNumber - step);
                }
                step = 1;
            } while (i > 0);
            offset = edgeNumber;
        }
        return edgeNumber;
    }

    void write(StringTrieBuilder& builder) override {
        auto writeUnit = Virtual_nid_no_patch<std::int32_t (APS5_VABI*)(void*, std::int32_t)>(&builder, 15);
        auto writeValFinal = Virtual_nid_no_patch<std::int32_t (APS5_VABI*)(void*, std::int32_t, std::int8_t)>(&builder, 17);

        std::int32_t unitNumber = length - 1;
        Node* rightEdge = equal[unitNumber];
        std::int32_t rightEdgeNumber = rightEdge ? rightEdge->offset : firstEdgeNumber;
        do {
            --unitNumber;
            if (equal[unitNumber]) {
                equal[unitNumber]->writeUnlessInsideRightEdge(firstEdgeNumber, rightEdgeNumber, builder);
            }
        } while (unitNumber > 0);

        unitNumber = length - 1;
        if (!rightEdge) {
            writeValFinal(&builder, values[unitNumber], 1);
        } else {
            rightEdge->write(builder);
        }
        offset = writeUnit(&builder, units[unitNumber]);
        while (--unitNumber >= 0) {
            std::int32_t val;
            std::int8_t isFinal;
            if (!equal[unitNumber]) {
                val = values[unitNumber];
                isFinal = 1;
            } else {
                val = offset - equal[unitNumber]->offset;
                isFinal = 0;
            }
            writeValFinal(&builder, val, isFinal);
            offset = writeUnit(&builder, units[unitNumber]);
        }
    }
};

struct SplitBranchNode : public BranchNode {
    char16_t unit;
    Node* lessThan;
    Node* greaterOrEqual;

    SplitBranchNode(char16_t middleUnit, Node* lessThanNode, Node* greaterOrEqualNode)
        : BranchNode(static_cast<std::int32_t>(((0x555555u * 37u + static_cast<std::uint32_t>(middleUnit)) * 37u + static_cast<std::uint32_t>(NodeHashCode(lessThanNode))) * 37u + static_cast<std::uint32_t>(NodeHashCode(greaterOrEqualNode)))),
          unit(middleUnit), lessThan(lessThanNode), greaterOrEqual(greaterOrEqualNode) {
        kind = NodeKind::SplitBranch;
    }

    bool operator==(const Node& other) const override {
        if (this == &other) return true;
        if (other.kind != NodeKind::SplitBranch || hash != other.hash) return false;
        const auto& o = static_cast<const SplitBranchNode&>(other);
        return unit == o.unit && lessThan == o.lessThan && greaterOrEqual == o.greaterOrEqual;
    }

    std::int32_t markRightEdgesFirst(std::int32_t edgeNumber) override {
        if (offset == 0) {
            firstEdgeNumber = edgeNumber;
            edgeNumber = greaterOrEqual->markRightEdgesFirst(edgeNumber);
            offset = edgeNumber = lessThan->markRightEdgesFirst(edgeNumber - 1);
        }
        return edgeNumber;
    }

    void write(StringTrieBuilder& builder) override {
        auto writeUnit = Virtual_nid_no_patch<std::int32_t (APS5_VABI*)(void*, std::int32_t)>(&builder, 15);
        auto writeDelta = Virtual_nid_no_patch<std::int32_t (APS5_VABI*)(void*, std::int32_t)>(&builder, 19);

        lessThan->writeUnlessInsideRightEdge(firstEdgeNumber, greaterOrEqual->offset, builder);
        greaterOrEqual->write(builder);
        writeDelta(&builder, lessThan->offset);
        offset = writeUnit(&builder, unit);
    }
};

struct BranchHeadNode : public ValueNode {
    std::int32_t length;
    Node* next;

    BranchHeadNode(std::int32_t len, Node* subNode)
        : ValueNode(static_cast<std::int32_t>((0x666666u * 37u + static_cast<std::uint32_t>(len)) * 37u + static_cast<std::uint32_t>(NodeHashCode(subNode)))),
          length(len), next(subNode) {
        kind = NodeKind::BranchHead;
    }

    bool operator==(const Node& other) const override {
        if (this == &other) return true;
        if (other.kind != NodeKind::BranchHead || !ValueNode::operator==(other)) return false;
        const auto& o = static_cast<const BranchHeadNode&>(other);
        return length == o.length && next == o.next;
    }

    std::int32_t markRightEdgesFirst(std::int32_t edgeNumber) override {
        if (offset == 0) {
            offset = edgeNumber = next->markRightEdgesFirst(edgeNumber);
        }
        return edgeNumber;
    }

    void write(StringTrieBuilder& builder) override {
        next->write(builder);
        auto getMinLinear = Virtual_nid_no_patch<std::int32_t (APS5_VABI*)(const void*)>(&builder, 12);
        auto writeUnit = Virtual_nid_no_patch<std::int32_t (APS5_VABI*)(void*, std::int32_t)>(&builder, 15);
        auto writeValType = Virtual_nid_no_patch<std::int32_t (APS5_VABI*)(void*, std::int8_t, std::int32_t, std::int32_t)>(&builder, 18);

        if (length <= getMinLinear(&builder)) {
            offset = writeValType(&builder, hasValue, value, length - 1);
        } else {
            writeUnit(&builder, length - 1);
            offset = writeValType(&builder, hasValue, value, 0);
        }
    }
};

void APS5_VABI DeleteNodeCallback(void* ptr) {
    delete static_cast<Node*>(ptr);
}

std::int32_t APS5_VABI HashNodeCallback(UHashTok key) {
    return _ZN6icu_6817StringTrieBuilder8hashNodeEPKv_nid_postfix(key.pointer);
}

std::int8_t APS5_VABI EqualNodeCallback(UHashTok a, UHashTok b) {
    return _ZN6icu_6817StringTrieBuilder10equalNodesEPKvS2__nid_postfix(a.pointer, b.pointer);
}

struct TrieBuilderTableData {
    std::ptrdiff_t offset;
    const TypeInfo* type;
    std::array<GuestFunction, 20> slots;
};

const DerivedTypeInfo& StringTrieBuilderType() {
    static const DerivedTypeInfo type{{SingleTypeTable_nid_no_patch(), "N6icu_6817StringTrieBuilderE"}, Rtti_nid_no_patch(Class::Object)};
    return type;
}

const DerivedTypeInfo& UCharsTrieBuilderType() {
    static const DerivedTypeInfo type{{SingleTypeTable_nid_no_patch(), "N6icu_6817UCharsTrieBuilderE"}, &StringTrieBuilderType().type};
    return type;
}

const StringTrieBuilderFunctions* StringTrieBuilderTable() {
    static const TrieBuilderTableData table = [] {
        TrieBuilderTableData result{0, &StringTrieBuilderType().type, {}};
        result.slots.fill(Abstract);
        result.slots[0] = reinterpret_cast<GuestFunction>(_ZN6icu_6817StringTrieBuilderD1Ev_nid_postfix);
        result.slots[1] = reinterpret_cast<GuestFunction>(_ZN6icu_6817StringTrieBuilderD0Ev_nid_postfix);
        return result;
    }();
    return reinterpret_cast<const StringTrieBuilderFunctions*>(table.slots.data());
}

const StringTrieBuilderFunctions* UCharsTrieBuilderTable() {
    static const TrieBuilderTableData table = [] {
        TrieBuilderTableData result{0, &UCharsTrieBuilderType().type, {}};
        result.slots.fill(Abstract);
        result.slots[0] = reinterpret_cast<GuestFunction>(_ZN6icu_6817UCharsTrieBuilderD1Ev_nid_postfix);
        result.slots[1] = reinterpret_cast<GuestFunction>(_ZN6icu_6817UCharsTrieBuilderD0Ev_nid_postfix);
        result.slots[3] = reinterpret_cast<GuestFunction>(_ZNK6icu_6817UCharsTrieBuilder22getElementStringLengthEi_nid_postfix);
        result.slots[4] = reinterpret_cast<GuestFunction>(_ZNK6icu_6817UCharsTrieBuilder14getElementUnitEii_nid_postfix);
        result.slots[5] = reinterpret_cast<GuestFunction>(_ZNK6icu_6817UCharsTrieBuilder15getElementValueEi_nid_postfix);
        result.slots[6] = reinterpret_cast<GuestFunction>(_ZNK6icu_6817UCharsTrieBuilder21getLimitOfLinearMatchEiii_nid_postfix);
        result.slots[7] = reinterpret_cast<GuestFunction>(_ZNK6icu_6817UCharsTrieBuilder17countElementUnitsEiii_nid_postfix);
        result.slots[8] = reinterpret_cast<GuestFunction>(_ZNK6icu_6817UCharsTrieBuilder23skipElementsBySomeUnitsEiii_nid_postfix);
        result.slots[9] = reinterpret_cast<GuestFunction>(_ZNK6icu_6817UCharsTrieBuilder26indexOfElementWithNextUnitEiiDs_nid_postfix);
        result.slots[10] = reinterpret_cast<GuestFunction>(_ZNK6icu_6817UCharsTrieBuilder23matchNodesCanHaveValuesEv_nid_postfix);
        result.slots[11] = reinterpret_cast<GuestFunction>(_ZNK6icu_6817UCharsTrieBuilder31getMaxBranchLinearSubNodeLengthEv_nid_postfix);
        result.slots[12] = reinterpret_cast<GuestFunction>(_ZNK6icu_6817UCharsTrieBuilder17getMinLinearMatchEv_nid_postfix);
        result.slots[13] = reinterpret_cast<GuestFunction>(_ZNK6icu_6817UCharsTrieBuilder23getMaxLinearMatchLengthEv_nid_postfix);
        result.slots[14] = reinterpret_cast<GuestFunction>(_ZNK6icu_6817UCharsTrieBuilder21createLinearMatchNodeEiiiPNS_17StringTrieBuilder4NodeE_nid_postfix);
        result.slots[15] = reinterpret_cast<GuestFunction>(_ZN6icu_6817UCharsTrieBuilder5writeEi_nid_postfix);
        result.slots[16] = reinterpret_cast<GuestFunction>(_ZN6icu_6817UCharsTrieBuilder17writeElementUnitsEiii_nid_postfix);
        result.slots[17] = reinterpret_cast<GuestFunction>(_ZN6icu_6817UCharsTrieBuilder18writeValueAndFinalEia_nid_postfix);
        result.slots[18] = reinterpret_cast<GuestFunction>(_ZN6icu_6817UCharsTrieBuilder17writeValueAndTypeEaii_nid_postfix);
        result.slots[19] = reinterpret_cast<GuestFunction>(_ZN6icu_6817UCharsTrieBuilder12writeDeltaToEi_nid_postfix);
        return result;
    }();
    return reinterpret_cast<const StringTrieBuilderFunctions*>(table.slots.data());
}

}

extern "C" {

void APS5_VABI _ZN6icu_6817StringTrieBuilderC2Ev_nid_postfix(StringTrieBuilder* self) {
    auto& b = Require(self);
    b.vtable = StringTrieBuilderTable();
    b.nodes = nullptr;
}

void APS5_VABI _ZN6icu_6817StringTrieBuilderD1Ev_nid_postfix(StringTrieBuilder* self) {
    _ZN6icu_6817StringTrieBuilderD2Ev_nid_postfix(self);
}

void APS5_VABI _ZN6icu_6817StringTrieBuilderD2Ev_nid_postfix(StringTrieBuilder* self) {
    _ZN6icu_6817StringTrieBuilder20deleteCompactBuilderEv_nid_postfix(self);
}

void APS5_VABI _ZN6icu_6817StringTrieBuilderD0Ev_nid_postfix(StringTrieBuilder* self) {
    _ZN6icu_6817StringTrieBuilderD1Ev_nid_postfix(self);
    Icu::Abi::Free_nid_no_patch(self);
}

void APS5_VABI _ZN6icu_6817StringTrieBuilder20createCompactBuilderEiR10UErrorCode_nid_postfix(StringTrieBuilder* self, std::int32_t sizeGuess, std::int32_t* errorCode) {
    auto& b = Require(self);
    auto& code = Require(errorCode);
    if (code > 0) return;
    b.nodes = uhash_openSize_nid_postfix(HashNodeCallback, EqualNodeCallback, nullptr, sizeGuess, &code);
    if (code <= 0) {
        if (!b.nodes) {
            code = 7;
        } else {
            uhash_setKeyDeleter_nid_postfix(b.nodes, DeleteNodeCallback);
        }
    }
}

void APS5_VABI _ZN6icu_6817StringTrieBuilder20deleteCompactBuilderEv_nid_postfix(StringTrieBuilder* self) {
    auto& b = Require(self);
    if (b.nodes) {
        uhash_close_nid_postfix(b.nodes);
        b.nodes = nullptr;
    }
}

void APS5_VABI _ZN6icu_6817StringTrieBuilder5buildE22UStringTrieBuildOptioniR10UErrorCode_nid_postfix(StringTrieBuilder* self, std::int32_t buildOption, std::int32_t elementsLength, std::int32_t* errorCode) {
    auto& b = Require(self);
    auto& code = Require(errorCode);
    if (code > 0) return;
    if (buildOption == 0) {
        _ZN6icu_6817StringTrieBuilder9writeNodeEiii_nid_postfix(self, 0, elementsLength, 0);
    } else {
        _ZN6icu_6817StringTrieBuilder20createCompactBuilderEiR10UErrorCode_nid_postfix(self, 2 * elementsLength, errorCode);
        auto* root = static_cast<Node*>(_ZN6icu_6817StringTrieBuilder8makeNodeEiiiR10UErrorCode_nid_postfix(self, 0, elementsLength, 0, errorCode));
        if (code <= 0 && root) {
            root->markRightEdgesFirst(-1);
            root->write(b);
        }
        _ZN6icu_6817StringTrieBuilder20deleteCompactBuilderEv_nid_postfix(self);
    }
}

std::int32_t APS5_VABI _ZN6icu_6817StringTrieBuilder8hashNodeEPKv_nid_postfix(const void* node) {
    return NodeHashCode(static_cast<const Node*>(RequirePointer(node)));
}

std::int8_t APS5_VABI _ZN6icu_6817StringTrieBuilder10equalNodesEPKvS2__nid_postfix(const void* left, const void* right) {
    return *static_cast<const Node*>(RequirePointer(left)) == *static_cast<const Node*>(RequirePointer(right));
}

void* APS5_VABI _ZN6icu_6817StringTrieBuilder18registerFinalValueEiR10UErrorCode_nid_postfix(StringTrieBuilder* self, std::int32_t value, std::int32_t* errorCode) {
    auto& b = Require(self);
    auto& code = Require(errorCode);
    if (code > 0) return nullptr;
    FinalValueNode key(value);
    const auto* old = uhash_find_nid_postfix(b.nodes, &key);
    if (old) {
        return old->key.pointer;
    }
    auto* newNode = new (std::nothrow) FinalValueNode(value);
    if (!newNode) {
        code = 7;
        return nullptr;
    }
    uhash_puti_nid_postfix(b.nodes, newNode, 1, &code);
    if (code > 0) {
        delete newNode;
        return nullptr;
    }
    return newNode;
}

void* APS5_VABI _ZN6icu_6817StringTrieBuilder12registerNodeEPNS0_4NodeER10UErrorCode_nid_postfix(StringTrieBuilder* self, void* newNode, std::int32_t* errorCode) {
    auto& b = Require(self);
    auto& code = Require(errorCode);
    auto* node = static_cast<Node*>(newNode);
    if (code > 0) {
        delete node;
        return nullptr;
    }
    if (!node) {
        code = 7;
        return nullptr;
    }
    const auto* old = uhash_find_nid_postfix(b.nodes, node);
    if (old) {
        delete node;
        return old->key.pointer;
    }
    uhash_puti_nid_postfix(b.nodes, node, 1, &code);
    if (code > 0) {
        delete node;
        return nullptr;
    }
    return node;
}

void* APS5_VABI _ZN6icu_6817StringTrieBuilder8makeNodeEiiiR10UErrorCode_nid_postfix(StringTrieBuilder* self, std::int32_t start, std::int32_t limit, std::int32_t unitIndex, std::int32_t* errorCode) {
    auto& code = Require(errorCode);
    if (code > 0) return nullptr;
    std::int8_t hasValue = 0;
    std::int32_t value = 0;
    auto getElemStrLen = Virtual_nid_no_patch<std::int32_t (APS5_VABI*)(const void*, std::int32_t)>(self, 3);
    auto getElemVal = Virtual_nid_no_patch<std::int32_t (APS5_VABI*)(const void*, std::int32_t)>(self, 5);
    auto getElemUnit = Virtual_nid_no_patch<char16_t (APS5_VABI*)(const void*, std::int32_t, std::int32_t)>(self, 4);
    auto getLimitOfLinearMatch = Virtual_nid_no_patch<std::int32_t (APS5_VABI*)(const void*, std::int32_t, std::int32_t, std::int32_t)>(self, 6);
    auto getMaxLinearMatchLen = Virtual_nid_no_patch<std::int32_t (APS5_VABI*)(const void*)>(self, 13);
    auto createLinearMatchNode = Virtual_nid_no_patch<void* (APS5_VABI*)(const void*, std::int32_t, std::int32_t, std::int32_t, void*)>(self, 14);
    auto countElemUnits = Virtual_nid_no_patch<std::int32_t (APS5_VABI*)(const void*, std::int32_t, std::int32_t, std::int32_t)>(self, 7);
    auto matchNodesCanHaveValues = Virtual_nid_no_patch<std::int8_t (APS5_VABI*)(const void*)>(self, 10);

    if (unitIndex == getElemStrLen(self, start)) {
        value = getElemVal(self, start++);
        if (start == limit) {
            return _ZN6icu_6817StringTrieBuilder18registerFinalValueEiR10UErrorCode_nid_postfix(self, value, errorCode);
        }
        hasValue = 1;
    }
    Node* node = nullptr;
    std::int32_t minUnit = getElemUnit(self, start, unitIndex);
    std::int32_t maxUnit = getElemUnit(self, limit - 1, unitIndex);
    if (minUnit == maxUnit) {
        std::int32_t lastUnitIndex = getLimitOfLinearMatch(self, start, limit - 1, unitIndex);
        auto* nextNode = static_cast<Node*>(_ZN6icu_6817StringTrieBuilder8makeNodeEiiiR10UErrorCode_nid_postfix(self, start, limit, lastUnitIndex, errorCode));
        std::int32_t length = lastUnitIndex - unitIndex;
        std::int32_t maxLinearMatchLength = getMaxLinearMatchLen(self);
        while (length > maxLinearMatchLength) {
            lastUnitIndex -= maxLinearMatchLength;
            length -= maxLinearMatchLength;
            node = static_cast<Node*>(createLinearMatchNode(self, start, lastUnitIndex, maxLinearMatchLength, nextNode));
            nextNode = static_cast<Node*>(_ZN6icu_6817StringTrieBuilder12registerNodeEPNS0_4NodeER10UErrorCode_nid_postfix(self, node, errorCode));
        }
        node = static_cast<Node*>(createLinearMatchNode(self, start, unitIndex, length, nextNode));
    } else {
        std::int32_t length = countElemUnits(self, start, limit, unitIndex);
        auto* subNode = static_cast<Node*>(_ZN6icu_6817StringTrieBuilder17makeBranchSubNodeEiiiiR10UErrorCode_nid_postfix(self, start, limit, unitIndex, length, errorCode));
        node = new (std::nothrow) BranchHeadNode(length, subNode);
    }
    if (hasValue && node) {
        if (matchNodesCanHaveValues(self)) {
            static_cast<ValueNode*>(node)->setValue(value);
        } else {
            node = new (std::nothrow) IntermediateValueNode(value, static_cast<Node*>(_ZN6icu_6817StringTrieBuilder12registerNodeEPNS0_4NodeER10UErrorCode_nid_postfix(self, node, errorCode)));
        }
    }
    return _ZN6icu_6817StringTrieBuilder12registerNodeEPNS0_4NodeER10UErrorCode_nid_postfix(self, node, errorCode);
}

void* APS5_VABI _ZN6icu_6817StringTrieBuilder17makeBranchSubNodeEiiiiR10UErrorCode_nid_postfix(StringTrieBuilder* self, std::int32_t start, std::int32_t limit, std::int32_t unitIndex, std::int32_t length, std::int32_t* errorCode) {
    auto& code = Require(errorCode);
    if (code > 0) return nullptr;
    auto getMaxBranchLinearSubNodeLen = Virtual_nid_no_patch<std::int32_t (APS5_VABI*)(const void*)>(self, 11);
    auto skipElementsBySomeUnits = Virtual_nid_no_patch<std::int32_t (APS5_VABI*)(const void*, std::int32_t, std::int32_t, std::int32_t)>(self, 8);
    auto getElemUnit = Virtual_nid_no_patch<char16_t (APS5_VABI*)(const void*, std::int32_t, std::int32_t)>(self, 4);
    auto getElemStrLen = Virtual_nid_no_patch<std::int32_t (APS5_VABI*)(const void*, std::int32_t)>(self, 3);
    auto getElemVal = Virtual_nid_no_patch<std::int32_t (APS5_VABI*)(const void*, std::int32_t)>(self, 5);
    auto indexOfElemWithNextUnit = Virtual_nid_no_patch<std::int32_t (APS5_VABI*)(const void*, std::int32_t, std::int32_t, char16_t)>(self, 9);

    char16_t middleUnits[14];
    Node* lessThan[14];
    std::int32_t ltLength = 0;
    while (length > getMaxBranchLinearSubNodeLen(self)) {
        std::int32_t i = skipElementsBySomeUnits(self, start, unitIndex, length / 2);
        middleUnits[ltLength] = getElemUnit(self, i, unitIndex);
        lessThan[ltLength] = static_cast<Node*>(_ZN6icu_6817StringTrieBuilder17makeBranchSubNodeEiiiiR10UErrorCode_nid_postfix(self, start, i, unitIndex, length / 2, errorCode));
        ++ltLength;
        start = i;
        length = length - length / 2;
    }
    if (code > 0) return nullptr;
    auto* listNode = new (std::nothrow) ListBranchNode();
    if (!listNode) {
        code = 7;
        return nullptr;
    }
    std::int32_t unitNumber = 0;
    do {
        std::int32_t i = start;
        char16_t unit = getElemUnit(self, i++, unitIndex);
        i = indexOfElemWithNextUnit(self, i, unitIndex, unit);
        if (start == i - 1 && unitIndex + 1 == getElemStrLen(self, start)) {
            listNode->add(unit, getElemVal(self, start));
        } else {
            listNode->add(unit, static_cast<Node*>(_ZN6icu_6817StringTrieBuilder8makeNodeEiiiR10UErrorCode_nid_postfix(self, start, i, unitIndex + 1, errorCode)));
        }
        start = i;
    } while (++unitNumber < length - 1);

    char16_t unit = getElemUnit(self, start, unitIndex);
    if (start == limit - 1 && unitIndex + 1 == getElemStrLen(self, start)) {
        listNode->add(unit, getElemVal(self, start));
    } else {
        listNode->add(unit, static_cast<Node*>(_ZN6icu_6817StringTrieBuilder8makeNodeEiiiR10UErrorCode_nid_postfix(self, start, limit, unitIndex + 1, errorCode)));
    }
    auto* node = static_cast<Node*>(_ZN6icu_6817StringTrieBuilder12registerNodeEPNS0_4NodeER10UErrorCode_nid_postfix(self, listNode, errorCode));
    while (ltLength > 0) {
        --ltLength;
        node = static_cast<Node*>(_ZN6icu_6817StringTrieBuilder12registerNodeEPNS0_4NodeER10UErrorCode_nid_postfix(
            self,
            new (std::nothrow) SplitBranchNode(middleUnits[ltLength], lessThan[ltLength], node),
            errorCode
        ));
    }
    return node;
}

std::int32_t APS5_VABI _ZN6icu_6817StringTrieBuilder9writeNodeEiii_nid_postfix(StringTrieBuilder* self, std::int32_t start, std::int32_t limit, std::int32_t unitIndex) {
    auto getElemStrLen = Virtual_nid_no_patch<std::int32_t (APS5_VABI*)(const void*, std::int32_t)>(self, 3);
    auto getElemVal = Virtual_nid_no_patch<std::int32_t (APS5_VABI*)(const void*, std::int32_t)>(self, 5);
    auto getElemUnit = Virtual_nid_no_patch<char16_t (APS5_VABI*)(const void*, std::int32_t, std::int32_t)>(self, 4);
    auto getLimitOfLinearMatch = Virtual_nid_no_patch<std::int32_t (APS5_VABI*)(const void*, std::int32_t, std::int32_t, std::int32_t)>(self, 6);
    auto getMaxLinearMatchLen = Virtual_nid_no_patch<std::int32_t (APS5_VABI*)(const void*)>(self, 13);
    auto getMinLinearMatch = Virtual_nid_no_patch<std::int32_t (APS5_VABI*)(const void*)>(self, 12);
    auto countElemUnits = Virtual_nid_no_patch<std::int32_t (APS5_VABI*)(const void*, std::int32_t, std::int32_t, std::int32_t)>(self, 7);
    auto writeUnit = Virtual_nid_no_patch<std::int32_t (APS5_VABI*)(void*, std::int32_t)>(self, 15);
    auto writeElemUnits = Virtual_nid_no_patch<std::int32_t (APS5_VABI*)(void*, std::int32_t, std::int32_t, std::int32_t)>(self, 16);
    auto writeValFinal = Virtual_nid_no_patch<std::int32_t (APS5_VABI*)(void*, std::int32_t, std::int8_t)>(self, 17);
    auto writeValType = Virtual_nid_no_patch<std::int32_t (APS5_VABI*)(void*, std::int8_t, std::int32_t, std::int32_t)>(self, 18);

    std::int8_t hasValue = 0;
    std::int32_t value = 0;
    std::int32_t type;
    if (unitIndex == getElemStrLen(self, start)) {
        value = getElemVal(self, start++);
        if (start == limit) {
            return writeValFinal(self, value, 1);
        }
        hasValue = 1;
    }
    std::int32_t minUnit = getElemUnit(self, start, unitIndex);
    std::int32_t maxUnit = getElemUnit(self, limit - 1, unitIndex);
    if (minUnit == maxUnit) {
        std::int32_t lastUnitIndex = getLimitOfLinearMatch(self, start, limit - 1, unitIndex);
        _ZN6icu_6817StringTrieBuilder9writeNodeEiii_nid_postfix(self, start, limit, lastUnitIndex);
        std::int32_t length = lastUnitIndex - unitIndex;
        std::int32_t maxLinearMatchLength = getMaxLinearMatchLen(self);
        while (length > maxLinearMatchLength) {
            lastUnitIndex -= maxLinearMatchLength;
            length -= maxLinearMatchLength;
            writeElemUnits(self, start, lastUnitIndex, maxLinearMatchLength);
            writeUnit(self, getMinLinearMatch(self) + maxLinearMatchLength - 1);
        }
        writeElemUnits(self, start, unitIndex, length);
        type = getMinLinearMatch(self) + length - 1;
    } else {
        std::int32_t length = countElemUnits(self, start, limit, unitIndex);
        _ZN6icu_6817StringTrieBuilder18writeBranchSubNodeEiiii_nid_postfix(self, start, limit, unitIndex, length);
        if (--length < getMinLinearMatch(self)) {
            type = length;
        } else {
            writeUnit(self, length);
            type = 0;
        }
    }
    return writeValType(self, hasValue, value, type);
}

std::int32_t APS5_VABI _ZN6icu_6817StringTrieBuilder18writeBranchSubNodeEiiii_nid_postfix(StringTrieBuilder* self, std::int32_t start, std::int32_t limit, std::int32_t unitIndex, std::int32_t length) {
    auto getMaxBranchLinearSubNodeLen = Virtual_nid_no_patch<std::int32_t (APS5_VABI*)(const void*)>(self, 11);
    auto skipElementsBySomeUnits = Virtual_nid_no_patch<std::int32_t (APS5_VABI*)(const void*, std::int32_t, std::int32_t, std::int32_t)>(self, 8);
    auto getElemUnit = Virtual_nid_no_patch<char16_t (APS5_VABI*)(const void*, std::int32_t, std::int32_t)>(self, 4);
    auto getElemStrLen = Virtual_nid_no_patch<std::int32_t (APS5_VABI*)(const void*, std::int32_t)>(self, 3);
    auto getElemVal = Virtual_nid_no_patch<std::int32_t (APS5_VABI*)(const void*, std::int32_t)>(self, 5);
    auto indexOfElemWithNextUnit = Virtual_nid_no_patch<std::int32_t (APS5_VABI*)(const void*, std::int32_t, std::int32_t, char16_t)>(self, 9);
    auto writeUnit = Virtual_nid_no_patch<std::int32_t (APS5_VABI*)(void*, std::int32_t)>(self, 15);
    auto writeValFinal = Virtual_nid_no_patch<std::int32_t (APS5_VABI*)(void*, std::int32_t, std::int8_t)>(self, 17);
    auto writeDeltaTo = Virtual_nid_no_patch<std::int32_t (APS5_VABI*)(void*, std::int32_t)>(self, 19);

    char16_t middleUnits[14];
    std::int32_t lessThan[14];
    std::int32_t ltLength = 0;
    while (length > getMaxBranchLinearSubNodeLen(self)) {
        std::int32_t i = skipElementsBySomeUnits(self, start, unitIndex, length / 2);
        middleUnits[ltLength] = getElemUnit(self, i, unitIndex);
        lessThan[ltLength] = _ZN6icu_6817StringTrieBuilder18writeBranchSubNodeEiiii_nid_postfix(self, start, i, unitIndex, length / 2);
        ++ltLength;
        start = i;
        length = length - length / 2;
    }
    std::int32_t starts[5];
    std::int8_t isFinal[4];
    std::int32_t unitNumber = 0;
    do {
        std::int32_t i = starts[unitNumber] = start;
        char16_t unit = getElemUnit(self, i++, unitIndex);
        i = indexOfElemWithNextUnit(self, i, unitIndex, unit);
        isFinal[unitNumber] = (start == i - 1 && unitIndex + 1 == getElemStrLen(self, start));
        start = i;
    } while (++unitNumber < length - 1);
    starts[unitNumber] = start;

    std::int32_t jumpTargets[4];
    do {
        --unitNumber;
        if (!isFinal[unitNumber]) {
            jumpTargets[unitNumber] = _ZN6icu_6817StringTrieBuilder9writeNodeEiii_nid_postfix(self, starts[unitNumber], starts[unitNumber + 1], unitIndex + 1);
        }
    } while (unitNumber > 0);

    unitNumber = length - 1;
    _ZN6icu_6817StringTrieBuilder9writeNodeEiii_nid_postfix(self, start, limit, unitIndex + 1);
    std::int32_t offset = writeUnit(self, getElemUnit(self, start, unitIndex));
    while (--unitNumber >= 0) {
        start = starts[unitNumber];
        std::int32_t val;
        if (isFinal[unitNumber]) {
            val = getElemVal(self, start);
        } else {
            val = offset - jumpTargets[unitNumber];
        }
        writeValFinal(self, val, isFinal[unitNumber]);
        offset = writeUnit(self, getElemUnit(self, start, unitIndex));
    }
    while (ltLength > 0) {
        --ltLength;
        writeDeltaTo(self, lessThan[ltLength]);
        offset = writeUnit(self, middleUnits[ltLength]);
    }
    return offset;
}

void APS5_VABI _ZN6icu_6817UCharsTrieBuilderC1ER10UErrorCode_nid_postfix(UCharsTrieBuilder* self, std::int32_t* errorCode) {
    _ZN6icu_6817UCharsTrieBuilderC2ER10UErrorCode_nid_postfix(self, errorCode);
}

void APS5_VABI _ZN6icu_6817UCharsTrieBuilderC2ER10UErrorCode_nid_postfix(UCharsTrieBuilder* self, std::int32_t* errorCode) {
    auto& b = Require(self);
    b.vtable = UCharsTrieBuilderTable();
    b.nodes = nullptr;
    new (&b.strings) UnicodeString();
    b.elements = nullptr;
    b.elementsCapacity = 0;
    b.elementsLength = 0;
    b.uchars = nullptr;
    b.ucharsCapacity = 0;
    b.ucharsLength = 0;
}

void APS5_VABI _ZN6icu_6817UCharsTrieBuilderD1Ev_nid_postfix(UCharsTrieBuilder* self) {
    _ZN6icu_6817UCharsTrieBuilderD2Ev_nid_postfix(self);
}

void APS5_VABI _ZN6icu_6817UCharsTrieBuilderD2Ev_nid_postfix(UCharsTrieBuilder* self) {
    auto& b = Require(self);
    delete[] b.elements;
    b.elements = nullptr;
    if (b.uchars) {
        Icu::Abi::Free_nid_no_patch(b.uchars);
        b.uchars = nullptr;
    }
    b.strings.~UnicodeString();
    _ZN6icu_6817StringTrieBuilderD2Ev_nid_postfix(reinterpret_cast<StringTrieBuilder*>(self));
}

void APS5_VABI _ZN6icu_6817UCharsTrieBuilderD0Ev_nid_postfix(UCharsTrieBuilder* self) {
    _ZN6icu_6817UCharsTrieBuilderD1Ev_nid_postfix(self);
    Icu::Abi::Free_nid_no_patch(self);
}

Icu::Abi::UCharsTrieBuilder* APS5_VABI _ZN6icu_6817UCharsTrieBuilder3addERKNS_13UnicodeStringEiR10UErrorCode_nid_postfix(UCharsTrieBuilder* self, const Icu::Abi::UnicodeString* s, std::int32_t value, std::int32_t* errorCode) {
    auto& b = Require(self);
    const auto& str = Require(s);
    auto& code = Require(errorCode);
    if (code > 0) return self;
    if (b.ucharsLength > 0) {
        code = 30;
        return self;
    }
    if (b.elementsLength == b.elementsCapacity) {
        std::int32_t newCapacity = b.elementsCapacity == 0 ? 1024 : 4 * b.elementsCapacity;
        auto* newElements = new (std::nothrow) UCharsTrieElement[newCapacity];
        if (!newElements) {
            code = 7;
            return self;
        }
        if (b.elementsLength > 0) {
            std::memcpy(newElements, b.elements, static_cast<std::size_t>(b.elementsLength) * sizeof(UCharsTrieElement));
        }
        delete[] b.elements;
        b.elements = newElements;
        b.elementsCapacity = newCapacity;
    }
    std::int32_t strLen = str.Length();
    if (strLen > 0xffff) {
        ++b.elementsLength;
        code = 8;
        return self;
    }
    std::int32_t stringOffset = b.strings.Length();
    _ZN6icu_6813UnicodeString6appendEi_nid_postfix(&b.strings, static_cast<char16_t>(strLen));
    _ZN6icu_6813UnicodeString8doAppendERKS0_ii_nid_postfix(&b.strings, &str, 0, strLen);
    b.elements[b.elementsLength].stringOffset = stringOffset;
    b.elements[b.elementsLength].value = value;
    ++b.elementsLength;
    if (b.strings.Flags() & UnicodeString::IsBogus) {
        code = 7;
    }
    return self;
}

void APS5_VABI _ZN6icu_6817UCharsTrieBuilder11buildUCharsE22UStringTrieBuildOptionR10UErrorCode_nid_postfix(UCharsTrieBuilder* self, std::int32_t buildOption, std::int32_t* errorCode) {
    auto& b = Require(self);
    auto& code = Require(errorCode);
    if (code > 0) return;
    if (b.uchars && b.ucharsLength > 0) return;
    if (b.ucharsLength == 0) {
        if (b.elementsLength == 0) {
            code = 8;
            return;
        }
        if (b.strings.Flags() & UnicodeString::IsBogus) {
            code = 7;
            return;
        }
        const auto* stringsBuffer = b.strings.Buffer();
        std::stable_sort(b.elements, b.elements + b.elementsLength, [stringsBuffer](const UCharsTrieElement& left, const UCharsTrieElement& right) {
            std::int32_t leftLen = stringsBuffer[left.stringOffset];
            std::int32_t rightLen = stringsBuffer[right.stringOffset];
            const char16_t* leftChars = stringsBuffer + left.stringOffset + 1;
            const char16_t* rightChars = stringsBuffer + right.stringOffset + 1;
            std::int32_t minLen = std::min(leftLen, rightLen);
            for (std::int32_t i = 0; i < minLen; ++i) {
                if (leftChars[i] != rightChars[i]) {
                    return leftChars[i] < rightChars[i];
                }
            }
            return leftLen < rightLen;
        });
        for (std::int32_t i = 1; i < b.elementsLength; ++i) {
            std::int32_t prevLen = stringsBuffer[b.elements[i - 1].stringOffset];
            std::int32_t currLen = stringsBuffer[b.elements[i].stringOffset];
            if (prevLen == currLen && std::memcmp(stringsBuffer + b.elements[i - 1].stringOffset + 1, stringsBuffer + b.elements[i].stringOffset + 1, prevLen * sizeof(char16_t)) == 0) {
                code = 1;
                return;
            }
        }
    }
    b.ucharsLength = 0;
    std::int32_t capacity = b.strings.Length();
    if (capacity < 1024) capacity = 1024;
    if (b.ucharsCapacity < capacity) {
        if (b.uchars) Icu::Abi::Free_nid_no_patch(b.uchars);
        b.uchars = static_cast<char16_t*>(Icu::Abi::Allocate_nid_no_patch(capacity * sizeof(char16_t)));
        if (!b.uchars) {
            code = 7;
            b.ucharsCapacity = 0;
            return;
        }
        b.ucharsCapacity = capacity;
    }
    _ZN6icu_6817StringTrieBuilder5buildE22UStringTrieBuildOptioniR10UErrorCode_nid_postfix(
        reinterpret_cast<StringTrieBuilder*>(self),
        buildOption,
        b.elementsLength,
        errorCode
    );
    if (!b.uchars) {
        code = 7;
    }
}

Icu::Abi::UCharsTrie* APS5_VABI _ZN6icu_6817UCharsTrieBuilder5buildE22UStringTrieBuildOptionR10UErrorCode_nid_postfix(UCharsTrieBuilder* self, std::int32_t buildOption, std::int32_t* errorCode) {
    auto& b = Require(self);
    auto& code = Require(errorCode);
    _ZN6icu_6817UCharsTrieBuilder11buildUCharsE22UStringTrieBuildOptionR10UErrorCode_nid_postfix(self, buildOption, errorCode);
    if (code > 0) return nullptr;
    auto* newTrie = static_cast<UCharsTrie*>(Icu::Abi::Allocate_nid_no_patch(sizeof(UCharsTrie)));
    if (!newTrie) {
        code = 7;
        return nullptr;
    }
    newTrie->ownedArray = b.uchars;
    newTrie->units = b.uchars + (b.ucharsCapacity - b.ucharsLength);
    newTrie->position = newTrie->units;
    newTrie->remaining = -1;
    b.uchars = nullptr;
    b.ucharsCapacity = 0;
    return newTrie;
}

Icu::Abi::UnicodeString* APS5_VABI _ZN6icu_6817UCharsTrieBuilder18buildUnicodeStringE22UStringTrieBuildOptionRNS_13UnicodeStringER10UErrorCode_nid_postfix(UCharsTrieBuilder* self, std::int32_t buildOption, Icu::Abi::UnicodeString* result, std::int32_t* errorCode) {
    auto& b = Require(self);
    auto& res = Require(result);
    auto& code = Require(errorCode);
    _ZN6icu_6817UCharsTrieBuilder11buildUCharsE22UStringTrieBuildOptionR10UErrorCode_nid_postfix(self, buildOption, errorCode);
    if (code <= 0) {
        ConstChar16Ptr ptr{b.uchars + (b.ucharsCapacity - b.ucharsLength)};
        _ZN6icu_6813UnicodeString5setToEaNS_14ConstChar16PtrEi_nid_postfix(&res, 0, &ptr, b.ucharsLength);
    }
    return result;
}

std::int8_t APS5_VABI _ZN6icu_6817UCharsTrieBuilder14ensureCapacityEi_nid_postfix(UCharsTrieBuilder* self, std::int32_t length) {
    auto& b = Require(self);
    if (!b.uchars && b.ucharsCapacity > 0) return 0;
    if (length > b.ucharsCapacity) {
        std::int32_t newCapacity = b.ucharsCapacity == 0 ? 1024 : b.ucharsCapacity;
        do {
            newCapacity *= 2;
        } while (newCapacity <= length);
        auto* newUChars = static_cast<char16_t*>(Icu::Abi::Allocate_nid_no_patch(newCapacity * sizeof(char16_t)));
        if (!newUChars) {
            Icu::Abi::Free_nid_no_patch(b.uchars);
            b.uchars = nullptr;
            b.ucharsCapacity = 0;
            return 0;
        }
        if (b.uchars && b.ucharsLength > 0) {
            std::memcpy(
                newUChars + (newCapacity - b.ucharsLength),
                b.uchars + (b.ucharsCapacity - b.ucharsLength),
                b.ucharsLength * sizeof(char16_t)
            );
        }
        Icu::Abi::Free_nid_no_patch(b.uchars);
        b.uchars = newUChars;
        b.ucharsCapacity = newCapacity;
    }
    return 1;
}

std::int32_t APS5_VABI _ZN6icu_6817UCharsTrieBuilder5writeEi_nid_postfix(UCharsTrieBuilder* self, std::int32_t unit) {
    auto& b = Require(self);
    std::int32_t newLength = b.ucharsLength + 1;
    if (_ZN6icu_6817UCharsTrieBuilder14ensureCapacityEi_nid_postfix(self, newLength)) {
        b.ucharsLength = newLength;
        b.uchars[b.ucharsCapacity - b.ucharsLength] = static_cast<char16_t>(unit);
    }
    return b.ucharsLength;
}

std::int32_t APS5_VABI _ZN6icu_6817UCharsTrieBuilder5writeEPKDsi_nid_postfix(UCharsTrieBuilder* self, const char16_t* s, std::int32_t length) {
    auto& b = Require(self);
    if (length < 0) throw std::invalid_argument("TrieBuilder: negative length");
    std::int32_t newLength = b.ucharsLength + length;
    if (_ZN6icu_6817UCharsTrieBuilder14ensureCapacityEi_nid_postfix(self, newLength)) {
        b.ucharsLength = newLength;
        std::memcpy(b.uchars + (b.ucharsCapacity - b.ucharsLength), s, length * sizeof(char16_t));
    }
    return b.ucharsLength;
}

std::int32_t APS5_VABI _ZN6icu_6817UCharsTrieBuilder17writeElementUnitsEiii_nid_postfix(UCharsTrieBuilder* self, std::int32_t i, std::int32_t unitIndex, std::int32_t length) {
    auto& b = Require(self);
    const auto* buf = b.strings.Buffer();
    return _ZN6icu_6817UCharsTrieBuilder5writeEPKDsi_nid_postfix(self, buf + b.elements[i].stringOffset + 1 + unitIndex, length);
}

std::int32_t APS5_VABI _ZN6icu_6817UCharsTrieBuilder18writeValueAndFinalEia_nid_postfix(UCharsTrieBuilder* self, std::int32_t i, std::int8_t isFinal) {
    if (0 <= i && i <= 0x3fff) {
        return _ZN6icu_6817UCharsTrieBuilder5writeEi_nid_postfix(self, i | (static_cast<std::int32_t>(isFinal) << 15));
    }
    char16_t intUnits[3];
    std::int32_t length;
    if (i < 0 || i > 0x3ffeffff) {
        intUnits[0] = 0x7fff;
        intUnits[1] = static_cast<char16_t>(static_cast<std::uint32_t>(i) >> 16);
        intUnits[2] = static_cast<char16_t>(i);
        length = 3;
    } else {
        intUnits[0] = static_cast<char16_t>(0x4000 + (i >> 16));
        intUnits[1] = static_cast<char16_t>(i);
        length = 2;
    }
    intUnits[0] = static_cast<char16_t>(intUnits[0] | (static_cast<std::int32_t>(isFinal) << 15));
    return _ZN6icu_6817UCharsTrieBuilder5writeEPKDsi_nid_postfix(self, intUnits, length);
}

std::int32_t APS5_VABI _ZN6icu_6817UCharsTrieBuilder17writeValueAndTypeEaii_nid_postfix(UCharsTrieBuilder* self, std::int8_t hasValue, std::int32_t value, std::int32_t node) {
    if (!hasValue) {
        return _ZN6icu_6817UCharsTrieBuilder5writeEi_nid_postfix(self, node);
    }
    char16_t intUnits[3];
    std::int32_t length;
    if (value < 0 || value > 0xfdffff) {
        intUnits[0] = 0x7fc0;
        intUnits[1] = static_cast<char16_t>(static_cast<std::uint32_t>(value) >> 16);
        intUnits[2] = static_cast<char16_t>(value);
        length = 3;
    } else if (value <= 0xff) {
        intUnits[0] = static_cast<char16_t>((value + 1) << 6);
        length = 1;
    } else {
        intUnits[0] = static_cast<char16_t>(0x4040 + ((value >> 10) & 0x7fc0));
        intUnits[1] = static_cast<char16_t>(value);
        length = 2;
    }
    intUnits[0] = static_cast<char16_t>(intUnits[0] | static_cast<char16_t>(node));
    return _ZN6icu_6817UCharsTrieBuilder5writeEPKDsi_nid_postfix(self, intUnits, length);
}

std::int32_t APS5_VABI _ZN6icu_6817UCharsTrieBuilder12writeDeltaToEi_nid_postfix(UCharsTrieBuilder* self, std::int32_t jumpTarget) {
    const auto& b = Require(self);
    std::int32_t i = b.ucharsLength - jumpTarget;
    if (i < 0) throw std::invalid_argument("TrieBuilder: invalid jump target");
    if (i <= 0xfbff) {
        return _ZN6icu_6817UCharsTrieBuilder5writeEi_nid_postfix(self, i);
    }
    char16_t intUnits[3];
    std::int32_t length;
    if (i <= 0x3feffff) {
        intUnits[0] = static_cast<char16_t>(0xfc00 + (i >> 16));
        length = 1;
    } else {
        intUnits[0] = static_cast<char16_t>(0xffff);
        intUnits[1] = static_cast<char16_t>(i >> 16);
        length = 2;
    }
    intUnits[length++] = static_cast<char16_t>(i);
    return _ZN6icu_6817UCharsTrieBuilder5writeEPKDsi_nid_postfix(self, intUnits, length);
}

std::int8_t APS5_VABI _ZNK6icu_6817UCharsTrieBuilder23matchNodesCanHaveValuesEv_nid_postfix(const UCharsTrieBuilder* self) {
    Require(self);
    return 1;
}

std::int32_t APS5_VABI _ZNK6icu_6817UCharsTrieBuilder23getMaxLinearMatchLengthEv_nid_postfix(const UCharsTrieBuilder* self) {
    Require(self);
    return 0x10;
}

std::int32_t APS5_VABI _ZNK6icu_6817UCharsTrieBuilder17getMinLinearMatchEv_nid_postfix(const UCharsTrieBuilder* self) {
    Require(self);
    return 0x30;
}

std::int32_t APS5_VABI _ZNK6icu_6817UCharsTrieBuilder22getElementStringLengthEi_nid_postfix(const UCharsTrieBuilder* self, std::int32_t i) {
    const auto& b = Require(self);
    if (i < 0 || i >= b.elementsLength) throw std::invalid_argument("TrieBuilder: element index out of range");
    return b.strings.Buffer()[b.elements[i].stringOffset];
}

char16_t APS5_VABI _ZNK6icu_6817UCharsTrieBuilder14getElementUnitEii_nid_postfix(const UCharsTrieBuilder* self, std::int32_t i, std::int32_t unitIndex) {
    const auto& b = Require(self);
    if (i < 0 || i >= b.elementsLength) throw std::invalid_argument("TrieBuilder: element index out of range");
    return b.strings.Buffer()[b.elements[i].stringOffset + 1 + unitIndex];
}

std::int32_t APS5_VABI _ZNK6icu_6817UCharsTrieBuilder15getElementValueEi_nid_postfix(const UCharsTrieBuilder* self, std::int32_t i) {
    const auto& b = Require(self);
    if (i < 0 || i >= b.elementsLength) throw std::invalid_argument("TrieBuilder: element index out of range");
    return b.elements[i].value;
}

std::int32_t APS5_VABI _ZNK6icu_6817UCharsTrieBuilder21getLimitOfLinearMatchEiii_nid_postfix(const UCharsTrieBuilder* self, std::int32_t first, std::int32_t last, std::int32_t unitIndex) {
    const auto& b = Require(self);
    const auto* buf = b.strings.Buffer();
    std::int32_t firstOff = b.elements[first].stringOffset;
    std::int32_t lastOff = b.elements[last].stringOffset;
    std::int32_t minLen = buf[firstOff];
    while (++unitIndex < minLen && buf[firstOff + 1 + unitIndex] == buf[lastOff + 1 + unitIndex]) {}
    return unitIndex;
}

std::int32_t APS5_VABI _ZNK6icu_6817UCharsTrieBuilder17countElementUnitsEiii_nid_postfix(const UCharsTrieBuilder* self, std::int32_t start, std::int32_t limit, std::int32_t unitIndex) {
    const auto& b = Require(self);
    const auto* buf = b.strings.Buffer();
    std::int32_t length = 0;
    std::int32_t i = start;
    do {
        char16_t unit = buf[b.elements[i++].stringOffset + 1 + unitIndex];
        while (i < limit && unit == buf[b.elements[i].stringOffset + 1 + unitIndex]) {
            ++i;
        }
        ++length;
    } while (i < limit);
    return length;
}

std::int32_t APS5_VABI _ZNK6icu_6817UCharsTrieBuilder23skipElementsBySomeUnitsEiii_nid_postfix(const UCharsTrieBuilder* self, std::int32_t i, std::int32_t unitIndex, std::int32_t count) {
    const auto& b = Require(self);
    const auto* buf = b.strings.Buffer();
    do {
        char16_t unit = buf[b.elements[i++].stringOffset + 1 + unitIndex];
        while (unit == buf[b.elements[i].stringOffset + 1 + unitIndex]) {
            ++i;
        }
    } while (--count > 0);
    return i;
}

std::int32_t APS5_VABI _ZNK6icu_6817UCharsTrieBuilder26indexOfElementWithNextUnitEiiDs_nid_postfix(const UCharsTrieBuilder* self, std::int32_t i, std::int32_t unitIndex, char16_t unit) {
    const auto& b = Require(self);
    const auto* buf = b.strings.Buffer();
    while (unit == buf[b.elements[i].stringOffset + 1 + unitIndex]) {
        ++i;
    }
    return i;
}

std::int32_t APS5_VABI _ZNK6icu_6817UCharsTrieBuilder31getMaxBranchLinearSubNodeLengthEv_nid_postfix(const UCharsTrieBuilder* self) {
    Require(self);
    return 5;
}

void* APS5_VABI _ZNK6icu_6817UCharsTrieBuilder21createLinearMatchNodeEiiiPNS_17StringTrieBuilder4NodeE_nid_postfix(const UCharsTrieBuilder* self, std::int32_t i, std::int32_t unitIndex, std::int32_t length, void* nextNode) {
    const auto& b = Require(self);
    const auto* buf = b.strings.Buffer();
    const char16_t* units = buf + b.elements[i].stringOffset + 1 + unitIndex;
    return new (std::nothrow) UCTLinearMatchNode(units, length, static_cast<Node*>(nextNode));
}

}
