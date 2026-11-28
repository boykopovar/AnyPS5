#pragma once

#include <cstddef>
#include <cstdint>

namespace NKWebKit::WebCoreAbi {

struct ContainerNode;

struct Node {
    void* vtable;
    void* eventTargetData[2];
    std::uint32_t refCountAndParentBit;
    std::uint32_t nodeFlags;
    ContainerNode* parentNode;
    void* treeScope;
    Node* previous;
    Node* next;
    void* rendererWithStyleFlags;
    void* rareDataWithBitfields;
};

struct ContainerNode : Node {
    Node* firstChild;
    Node* lastChild;
};

constexpr std::uint32_t NodeFlagIsContainerNode = 1u << 2;
constexpr std::uint32_t NodeFlagIsElementNode = 1u << 3;

static_assert(sizeof(Node) == 0x50);
static_assert(offsetof(Node, refCountAndParentBit) == 0x18);
static_assert(offsetof(Node, nodeFlags) == 0x1c);
static_assert(offsetof(Node, parentNode) == 0x20);
static_assert(offsetof(Node, treeScope) == 0x28);
static_assert(offsetof(Node, previous) == 0x30);
static_assert(offsetof(Node, next) == 0x38);
static_assert(sizeof(ContainerNode) == 0x60);

inline Node* LastChild(const Node& node) {
    if (!(node.nodeFlags & NodeFlagIsContainerNode)) return nullptr;
    return static_cast<const ContainerNode&>(node).lastChild;
}

}
