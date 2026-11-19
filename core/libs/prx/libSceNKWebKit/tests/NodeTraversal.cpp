#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <cstdio>
#include <cstdlib>
#include <deque>
#include <source_location>

namespace {

using NKWebKit::WebCoreAbi::ContainerNode;
using NKWebKit::WebCoreAbi::Node;
using NKWebKit::WebCoreAbi::NodeFlagIsContainerNode;

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "node traversal check failed at line %u\n", location.line());
        std::abort();
    }
}

struct Tree {
    std::deque<ContainerNode> containers;
    std::deque<Node> leaves;

    ContainerNode* Container(ContainerNode* parent = nullptr) {
        ContainerNode& node = containers.emplace_back();
        node = {};
        node.nodeFlags = NodeFlagIsContainerNode | 0x40000u;
        Attach(parent, &node);
        return &node;
    }

    Node* Leaf(ContainerNode* parent = nullptr) {
        Node& node = leaves.emplace_back();
        node = {};
        node.nodeFlags = 0x40000u;
        Attach(parent, &node);
        return &node;
    }

    static void Attach(ContainerNode* parent, Node* child) {
        if (!parent) return;
        child->parentNode = parent;
        child->previous = parent->lastChild;
        if (parent->lastChild) parent->lastChild->next = child;
        else parent->firstChild = child;
        parent->lastChild = child;
    }
};

}

int main() {
    Tree tree;
    {
        ContainerNode probe {};
        Node child {};
        probe.firstChild = &child;
        probe.lastChild = &child + 1;
        const auto* words = reinterpret_cast<Node* const*>(&probe);
        Require(words[0x50 / sizeof(Node*)] == &child);
        Require(words[0x58 / sizeof(Node*)] == &child + 1);
    }
    ContainerNode* root = tree.Container();
    ContainerNode* a = tree.Container(root);
    Node* a1 = tree.Leaf(a);
    ContainerNode* a2 = tree.Container(a);
    Node* a21 = tree.Leaf(a2);
    ContainerNode* a22 = tree.Container(a2);
    Node* b = tree.Leaf(root);
    ContainerNode* c = tree.Container(root);
    ContainerNode* c1 = tree.Container(c);
    Node* c11 = tree.Leaf(c1);
    ContainerNode* empty = tree.Container(root);
    Node* loneLeaf = tree.Leaf();
    ContainerNode* loneContainer = tree.Container();

    Require(_ZN7WebCore13NodeTraversal4lastERKNS_13ContainerNodeE(root) == empty);
    Require(_ZN7WebCore13NodeTraversal4lastERKNS_13ContainerNodeE(a) == a22);
    Require(_ZN7WebCore13NodeTraversal4lastERKNS_13ContainerNodeE(a2) == a22);
    Require(_ZN7WebCore13NodeTraversal4lastERKNS_13ContainerNodeE(c) == c11);
    Require(_ZN7WebCore13NodeTraversal4lastERKNS_13ContainerNodeE(c1) == c11);
    Require(_ZN7WebCore13NodeTraversal4lastERKNS_13ContainerNodeE(a22) == nullptr);
    Require(_ZN7WebCore13NodeTraversal4lastERKNS_13ContainerNodeE(empty) == nullptr);
    Require(_ZN7WebCore13NodeTraversal4lastERKNS_13ContainerNodeE(loneContainer) == nullptr);

    Require(_ZN7WebCore13NodeTraversal13deepLastChildERNS_4NodeE(root) == empty);
    Require(_ZN7WebCore13NodeTraversal13deepLastChildERNS_4NodeE(a) == a22);
    Require(_ZN7WebCore13NodeTraversal13deepLastChildERNS_4NodeE(a2) == a22);
    Require(_ZN7WebCore13NodeTraversal13deepLastChildERNS_4NodeE(c) == c11);
    Require(_ZN7WebCore13NodeTraversal13deepLastChildERNS_4NodeE(a22) == a22);
    Require(_ZN7WebCore13NodeTraversal13deepLastChildERNS_4NodeE(empty) == empty);
    Require(_ZN7WebCore13NodeTraversal13deepLastChildERNS_4NodeE(loneContainer) == loneContainer);
    Require(_ZN7WebCore13NodeTraversal13deepLastChildERNS_4NodeE(b) == b);
    Require(_ZN7WebCore13NodeTraversal13deepLastChildERNS_4NodeE(a1) == a1);
    Require(_ZN7WebCore13NodeTraversal13deepLastChildERNS_4NodeE(loneLeaf) == loneLeaf);

    ContainerNode* staleContainer = tree.Container();
    Tree::Attach(staleContainer, tree.Leaf());
    staleContainer->nodeFlags &= ~NodeFlagIsContainerNode;
    Node* leafWithStaleChild = staleContainer;
    Require(_ZN7WebCore13NodeTraversal13deepLastChildERNS_4NodeE(leafWithStaleChild) == leafWithStaleChild);

    Require(_ZN7WebCore13NodeTraversal19nextAncestorSiblingERKNS_4NodeE(a22) == b);
    Require(_ZN7WebCore13NodeTraversal19nextAncestorSiblingERKNS_4NodeE(c11) == empty);
    Require(_ZN7WebCore13NodeTraversal19nextAncestorSiblingERKNS_4NodeE(c1) == empty);
    Require(_ZN7WebCore13NodeTraversal19nextAncestorSiblingERKNS_4NodeE(empty) == nullptr);
    Require(_ZN7WebCore13NodeTraversal19nextAncestorSiblingERKNS_4NodeE(loneLeaf) == nullptr);
    Require(_ZN7WebCore13NodeTraversal19nextAncestorSiblingERKNS_4NodeE(root) == nullptr);

    Require(_ZN7WebCore13NodeTraversal19nextAncestorSiblingERKNS_4NodeEPS2_(a22, nullptr) == b);
    Require(_ZN7WebCore13NodeTraversal19nextAncestorSiblingERKNS_4NodeEPS2_(a22, root) == b);
    Require(_ZN7WebCore13NodeTraversal19nextAncestorSiblingERKNS_4NodeEPS2_(a22, a) == nullptr);
    Require(_ZN7WebCore13NodeTraversal19nextAncestorSiblingERKNS_4NodeEPS2_(a22, a2) == nullptr);
    Require(_ZN7WebCore13NodeTraversal19nextAncestorSiblingERKNS_4NodeEPS2_(c11, c) == nullptr);
    Require(_ZN7WebCore13NodeTraversal19nextAncestorSiblingERKNS_4NodeEPS2_(c11, c1) == nullptr);
    Require(_ZN7WebCore13NodeTraversal19nextAncestorSiblingERKNS_4NodeEPS2_(c11, root) == empty);
    Require(_ZN7WebCore13NodeTraversal19nextAncestorSiblingERKNS_4NodeEPS2_(c11, loneLeaf) == empty);
    Require(_ZN7WebCore13NodeTraversal19nextAncestorSiblingERKNS_4NodeEPS2_(empty, root) == nullptr);
    Require(_ZN7WebCore13NodeTraversal19nextAncestorSiblingERKNS_4NodeEPS2_(empty, nullptr) == nullptr);
    Require(_ZN7WebCore13NodeTraversal19nextAncestorSiblingERKNS_4NodeEPS2_(loneLeaf, nullptr) == nullptr);
    return 0;
}
