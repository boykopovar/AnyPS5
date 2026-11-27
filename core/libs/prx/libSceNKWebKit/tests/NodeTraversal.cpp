#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <source_location>
#include <stdexcept>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

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

template<typename Fn>
void ThrowsInvalidArgument(Fn&& fn, int testErrno = EAGAIN, std::source_location location = std::source_location::current()) {
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

class GuardedMapping {
    unsigned char* base;
    std::size_t pageSize;

public:
    GuardedMapping() {
#ifdef _WIN32
        SYSTEM_INFO info{};
        GetSystemInfo(&info);
        pageSize = info.dwPageSize;
        base = static_cast<unsigned char*>(VirtualAlloc(nullptr, pageSize * 2, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
        Require(base != nullptr);
        DWORD previous = 0;
        Require(VirtualProtect(base + pageSize, pageSize, PAGE_NOACCESS, &previous) != 0);
#else
        const long size = sysconf(_SC_PAGESIZE);
        Require(size > 0);
        pageSize = static_cast<std::size_t>(size);
        void* mapped = mmap(nullptr, pageSize * 2, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, -1, 0);
        Require(mapped != MAP_FAILED);
        base = static_cast<unsigned char*>(mapped);
        Require(mprotect(base + pageSize, pageSize, PROT_NONE) == 0);
#endif
    }

    ~GuardedMapping() {
#ifdef _WIN32
        Require(VirtualFree(base, 0, MEM_RELEASE) != 0);
#else
        Require(munmap(base, pageSize * 2) == 0);
#endif
    }

    GuardedMapping(const GuardedMapping&) = delete;
    GuardedMapping& operator=(const GuardedMapping&) = delete;

    unsigned char* GuardPage() const {
        return base + pageSize;
    }
};

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

void TestGuestPointerValidation() {
    auto* unmappedNode = reinterpret_cast<Node*>(std::uintptr_t{0x1});
    auto* unmappedContainer = reinterpret_cast<ContainerNode*>(std::uintptr_t{0x1});

    ThrowsInvalidArgument([] { _ZN7WebCore13NodeTraversal13deepLastChildERNS_4NodeE(nullptr); }, EAGAIN);
    ThrowsInvalidArgument([&] { _ZN7WebCore13NodeTraversal13deepLastChildERNS_4NodeE(unmappedNode); }, EINVAL);
    ThrowsInvalidArgument([] { _ZN7WebCore13NodeTraversal19nextAncestorSiblingERKNS_4NodeE(nullptr); }, ENOMEM);
    ThrowsInvalidArgument([&] { _ZN7WebCore13NodeTraversal19nextAncestorSiblingERKNS_4NodeE(unmappedNode); }, EFAULT);
    ThrowsInvalidArgument([] { _ZN7WebCore13NodeTraversal19nextAncestorSiblingERKNS_4NodeEPS2_(nullptr, nullptr); }, EAGAIN);
    ThrowsInvalidArgument([&] { _ZN7WebCore13NodeTraversal19nextAncestorSiblingERKNS_4NodeEPS2_(unmappedNode, nullptr); }, EINVAL);
    ThrowsInvalidArgument([] { _ZN7WebCore13NodeTraversal4lastERKNS_13ContainerNodeE(nullptr); }, ENOMEM);
    ThrowsInvalidArgument([&] { _ZN7WebCore13NodeTraversal4lastERKNS_13ContainerNodeE(unmappedContainer); }, EFAULT);

    ContainerNode firstHopParent{};
    firstHopParent.parentNode = unmappedContainer;
    ContainerNode secondHopChild{};
    ContainerNode secondHopParent{};
    secondHopChild.parentNode = &secondHopParent;
    secondHopParent.parentNode = unmappedContainer;
    Node otherStayWithin{};

    ThrowsInvalidArgument([&] { _ZN7WebCore13NodeTraversal19nextAncestorSiblingERKNS_4NodeE(&firstHopParent); }, EAGAIN);
    ThrowsInvalidArgument([&] { _ZN7WebCore13NodeTraversal19nextAncestorSiblingERKNS_4NodeEPS2_(&firstHopParent, nullptr); }, EINVAL);
    ThrowsInvalidArgument([&] { _ZN7WebCore13NodeTraversal19nextAncestorSiblingERKNS_4NodeEPS2_(&firstHopParent, &otherStayWithin); }, EBUSY);
    ThrowsInvalidArgument([&] { _ZN7WebCore13NodeTraversal19nextAncestorSiblingERKNS_4NodeE(&secondHopChild); }, ENOMEM);
    ThrowsInvalidArgument([&] { _ZN7WebCore13NodeTraversal19nextAncestorSiblingERKNS_4NodeEPS2_(&secondHopChild, nullptr); }, EFAULT);
    ThrowsInvalidArgument([&] { _ZN7WebCore13NodeTraversal19nextAncestorSiblingERKNS_4NodeEPS2_(&secondHopChild, &otherStayWithin); }, EAGAIN);

    errno = EPIPE;
    Require(_ZN7WebCore13NodeTraversal19nextAncestorSiblingERKNS_4NodeEPS2_(&firstHopParent, unmappedNode) == nullptr);
    Require(errno == EPIPE);
    errno = EINVAL;
    Require(_ZN7WebCore13NodeTraversal19nextAncestorSiblingERKNS_4NodeEPS2_(&secondHopChild, unmappedNode) == nullptr);
    Require(errno == EINVAL);

    ContainerNode firstHopContainer{};
    firstHopContainer.nodeFlags = NodeFlagIsContainerNode;
    firstHopContainer.lastChild = unmappedNode;
    ContainerNode secondHopContainer{};
    secondHopContainer.nodeFlags = NodeFlagIsContainerNode;
    secondHopContainer.lastChild = &firstHopContainer;

    ThrowsInvalidArgument([&] { _ZN7WebCore13NodeTraversal13deepLastChildERNS_4NodeE(&firstHopContainer); }, EAGAIN);
    ThrowsInvalidArgument([&] { _ZN7WebCore13NodeTraversal4lastERKNS_13ContainerNodeE(&firstHopContainer); }, EINVAL);
    ThrowsInvalidArgument([&] { _ZN7WebCore13NodeTraversal13deepLastChildERNS_4NodeE(&secondHopContainer); }, ENOMEM);
    ThrowsInvalidArgument([&] { _ZN7WebCore13NodeTraversal4lastERKNS_13ContainerNodeE(&secondHopContainer); }, EFAULT);

    GuardedMapping mapping;
    unsigned char* guardPage = mapping.GuardPage();
    auto* noAccessNode = reinterpret_cast<Node*>(guardPage);
    auto* noAccessContainer = reinterpret_cast<ContainerNode*>(guardPage);
    auto* straddlingNode = reinterpret_cast<Node*>(guardPage - sizeof(Node) / 2);
    auto* straddlingContainer = reinterpret_cast<ContainerNode*>(guardPage - sizeof(Node) / 2);

    ThrowsInvalidArgument([&] { _ZN7WebCore13NodeTraversal13deepLastChildERNS_4NodeE(noAccessNode); }, EAGAIN);
    ThrowsInvalidArgument([&] { _ZN7WebCore13NodeTraversal19nextAncestorSiblingERKNS_4NodeE(noAccessNode); }, EINVAL);
    ThrowsInvalidArgument([&] { _ZN7WebCore13NodeTraversal19nextAncestorSiblingERKNS_4NodeEPS2_(noAccessNode, nullptr); }, ENOMEM);
    ThrowsInvalidArgument([&] { _ZN7WebCore13NodeTraversal4lastERKNS_13ContainerNodeE(noAccessContainer); }, EFAULT);
    ThrowsInvalidArgument([&] { _ZN7WebCore13NodeTraversal13deepLastChildERNS_4NodeE(straddlingNode); }, EAGAIN);
    ThrowsInvalidArgument([&] { _ZN7WebCore13NodeTraversal19nextAncestorSiblingERKNS_4NodeE(straddlingNode); }, EINVAL);
    ThrowsInvalidArgument([&] { _ZN7WebCore13NodeTraversal19nextAncestorSiblingERKNS_4NodeEPS2_(straddlingNode, nullptr); }, ENOMEM);
    ThrowsInvalidArgument([&] { _ZN7WebCore13NodeTraversal4lastERKNS_13ContainerNodeE(straddlingContainer); }, EFAULT);

    auto* boundaryNode = reinterpret_cast<Node*>(guardPage - sizeof(Node));
    std::memset(boundaryNode, 0, sizeof(Node));
    boundaryNode->nodeFlags = 0x40000u;
    errno = EBUSY;
    Require(_ZN7WebCore13NodeTraversal13deepLastChildERNS_4NodeE(boundaryNode) == boundaryNode);
    Require(errno == EBUSY);
    Require(_ZN7WebCore13NodeTraversal19nextAncestorSiblingERKNS_4NodeE(boundaryNode) == nullptr);
    Require(errno == EBUSY);
    Require(_ZN7WebCore13NodeTraversal19nextAncestorSiblingERKNS_4NodeEPS2_(boundaryNode, nullptr) == nullptr);
    Require(errno == EBUSY);

    boundaryNode->nodeFlags = NodeFlagIsContainerNode;
    auto* boundaryContainer = reinterpret_cast<ContainerNode*>(boundaryNode);
    ThrowsInvalidArgument([&] { _ZN7WebCore13NodeTraversal13deepLastChildERNS_4NodeE(boundaryContainer); }, EAGAIN);
    ThrowsInvalidArgument([&] { _ZN7WebCore13NodeTraversal4lastERKNS_13ContainerNodeE(boundaryContainer); }, EINVAL);
}

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

    TestGuestPointerValidation();
    return 0;
}
