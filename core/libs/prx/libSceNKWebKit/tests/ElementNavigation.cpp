#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <deque>
#include <source_location>
#include <stdexcept>

namespace {

using NKWebKit::WebCoreAbi::ContainerNode;
using NKWebKit::WebCoreAbi::Node;
using NKWebKit::WebCoreAbi::NodeFlagIsContainerNode;
using NKWebKit::WebCoreAbi::NodeFlagIsElementNode;

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "element navigation check failed at line %u\n", location.line());
        std::abort();
    }
}

template<typename Fn>
void ThrowsInvalidArgument(Fn&& fn, int testErrno, std::source_location location = std::source_location::current()) {
    errno = testErrno;
    bool caught = false;
    try {
        fn();
    } catch (const std::invalid_argument&) {
        caught = true;
    }
    Require(caught, location);
    Require(errno == testErrno, location);
}

struct Tree {
    std::deque<ContainerNode> nodes;

    ContainerNode* Add(ContainerNode* parent, std::uint32_t flags) {
        ContainerNode& node = nodes.emplace_back();
        node = {};
        node.nodeFlags = flags | 0x40000u;
        if (parent) {
            node.parentNode = parent;
            node.previous = parent->lastChild;
            if (parent->lastChild) parent->lastChild->next = &node;
            else parent->firstChild = &node;
            parent->lastChild = &node;
        }
        return &node;
    }

    ContainerNode* Element(ContainerNode* parent) { return Add(parent, NodeFlagIsContainerNode | NodeFlagIsElementNode); }
    ContainerNode* Other(ContainerNode* parent) { return Add(parent, NodeFlagIsContainerNode); }
    ContainerNode* Text(ContainerNode* parent) { return Add(parent, 0); }
};

void TestChildren() {
    Tree tree;
    ContainerNode* parent = tree.Element(nullptr);
    ContainerNode* text0 = tree.Text(parent);
    ContainerNode* other1 = tree.Other(parent);
    ContainerNode* element2 = tree.Element(parent);
    ContainerNode* text3 = tree.Text(parent);
    ContainerNode* element4 = tree.Element(parent);
    ContainerNode* other5 = tree.Other(parent);
    ContainerNode* text6 = tree.Text(parent);
    Node* children[] = {text0, other1, element2, text3, element4, other5, text6};

    Require(_ZNK7WebCore13ContainerNode17firstElementChildEv(parent) == element2);
    Require(_ZNK7WebCore13ContainerNode16lastElementChildEv(parent) == element4);
    Require(_ZNK7WebCore13ContainerNode17childElementCountEv(parent) == 2);
    for (std::uint32_t index = 0; index < 7; ++index)
        Require(_ZNK7WebCore13ContainerNode17traverseToChildAtEj(parent, index) == children[index]);
    Require(_ZNK7WebCore13ContainerNode17traverseToChildAtEj(parent, 7) == nullptr);
    Require(_ZNK7WebCore13ContainerNode17traverseToChildAtEj(parent, 0xffffffffu) == nullptr);

    Require(_ZNK7WebCore4Node18nextElementSiblingEv(text0) == element2);
    Require(_ZNK7WebCore4Node18nextElementSiblingEv(other1) == element2);
    Require(_ZNK7WebCore4Node18nextElementSiblingEv(element2) == element4);
    Require(_ZNK7WebCore4Node18nextElementSiblingEv(text3) == element4);
    Require(_ZNK7WebCore4Node18nextElementSiblingEv(element4) == nullptr);
    Require(_ZNK7WebCore4Node18nextElementSiblingEv(text6) == nullptr);

    Require(_ZNK7WebCore4Node22previousElementSiblingEv(text6) == element4);
    Require(_ZNK7WebCore4Node22previousElementSiblingEv(element4) == element2);
    Require(_ZNK7WebCore4Node22previousElementSiblingEv(text3) == element2);
    Require(_ZNK7WebCore4Node22previousElementSiblingEv(element2) == nullptr);
    Require(_ZNK7WebCore4Node22previousElementSiblingEv(other1) == nullptr);
    Require(_ZNK7WebCore4Node22previousElementSiblingEv(parent) == nullptr);
}

void TestWithoutElements() {
    Tree tree;
    ContainerNode* empty = tree.Element(nullptr);
    Require(_ZNK7WebCore13ContainerNode17firstElementChildEv(empty) == nullptr);
    Require(_ZNK7WebCore13ContainerNode16lastElementChildEv(empty) == nullptr);
    Require(_ZNK7WebCore13ContainerNode17childElementCountEv(empty) == 0);
    Require(_ZNK7WebCore13ContainerNode17traverseToChildAtEj(empty, 0) == nullptr);
    Require(_ZNK7WebCore13ContainerNode17traverseToChildAtEj(empty, 3) == nullptr);

    ContainerNode* parent = tree.Other(nullptr);
    ContainerNode* text = tree.Text(parent);
    ContainerNode* other = tree.Other(parent);
    Require(_ZNK7WebCore13ContainerNode17firstElementChildEv(parent) == nullptr);
    Require(_ZNK7WebCore13ContainerNode16lastElementChildEv(parent) == nullptr);
    Require(_ZNK7WebCore13ContainerNode17childElementCountEv(parent) == 0);
    Require(_ZNK7WebCore13ContainerNode17traverseToChildAtEj(parent, 0) == text);
    Require(_ZNK7WebCore13ContainerNode17traverseToChildAtEj(parent, 1) == other);
    Require(_ZNK7WebCore4Node18nextElementSiblingEv(text) == nullptr);
    Require(_ZNK7WebCore4Node22previousElementSiblingEv(other) == nullptr);
}

void TestSingleElement() {
    Tree tree;
    ContainerNode* parent = tree.Other(nullptr);
    ContainerNode* only = tree.Element(parent);
    Require(_ZNK7WebCore13ContainerNode17firstElementChildEv(parent) == only);
    Require(_ZNK7WebCore13ContainerNode16lastElementChildEv(parent) == only);
    Require(_ZNK7WebCore13ContainerNode17childElementCountEv(parent) == 1);
    Require(_ZNK7WebCore4Node18nextElementSiblingEv(only) == nullptr);
    Require(_ZNK7WebCore4Node22previousElementSiblingEv(only) == nullptr);
}

void TestGuestPointerValidation() {
    auto* unmappedNode = reinterpret_cast<Node*>(std::uintptr_t{0x1});
    auto* unmappedContainer = reinterpret_cast<ContainerNode*>(std::uintptr_t{0x1});

    ThrowsInvalidArgument([] { _ZNK7WebCore13ContainerNode17firstElementChildEv(nullptr); }, EAGAIN);
    ThrowsInvalidArgument([&] { _ZNK7WebCore13ContainerNode17firstElementChildEv(unmappedContainer); }, EINVAL);
    ThrowsInvalidArgument([] { _ZNK7WebCore13ContainerNode16lastElementChildEv(nullptr); }, ENOMEM);
    ThrowsInvalidArgument([&] { _ZNK7WebCore13ContainerNode16lastElementChildEv(unmappedContainer); }, EFAULT);
    ThrowsInvalidArgument([] { _ZNK7WebCore13ContainerNode17childElementCountEv(nullptr); }, EAGAIN);
    ThrowsInvalidArgument([&] { _ZNK7WebCore13ContainerNode17childElementCountEv(unmappedContainer); }, EINVAL);
    ThrowsInvalidArgument([] { _ZNK7WebCore13ContainerNode17traverseToChildAtEj(nullptr, 0); }, ENOMEM);
    ThrowsInvalidArgument([&] { _ZNK7WebCore13ContainerNode17traverseToChildAtEj(unmappedContainer, 0); }, EFAULT);
    ThrowsInvalidArgument([] { _ZNK7WebCore4Node18nextElementSiblingEv(nullptr); }, EAGAIN);
    ThrowsInvalidArgument([&] { _ZNK7WebCore4Node18nextElementSiblingEv(unmappedNode); }, EINVAL);
    ThrowsInvalidArgument([] { _ZNK7WebCore4Node22previousElementSiblingEv(nullptr); }, ENOMEM);
    ThrowsInvalidArgument([&] { _ZNK7WebCore4Node22previousElementSiblingEv(unmappedNode); }, EFAULT);

    ContainerNode parent{};
    parent.firstChild = unmappedNode;
    ThrowsInvalidArgument([&] { _ZNK7WebCore13ContainerNode17firstElementChildEv(&parent); }, EAGAIN);
    ThrowsInvalidArgument([&] { _ZNK7WebCore13ContainerNode17childElementCountEv(&parent); }, EINVAL);
    ThrowsInvalidArgument([&] { _ZNK7WebCore13ContainerNode17traverseToChildAtEj(&parent, 1); }, ENOMEM);
    Require(_ZNK7WebCore13ContainerNode17traverseToChildAtEj(&parent, 0) == unmappedNode);
}

}

int main() {
    TestChildren();
    TestWithoutElements();
    TestSingleElement();
    TestGuestPointerValidation();
    return 0;
}
