#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <source_location>
#include <stdexcept>
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#endif

namespace {

using NKWebKit::WebCoreAbi::ContainerNode;
using NKWebKit::WebCoreAbi::Node;
using NKWebKit::WebCoreAbi::Position;

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "position anchor check failed at line %u\n", location.line());
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

Position Garbage() {
    Position position;
    std::memset(&position, 0xa5, sizeof(position));
    return position;
}

void TestAnchorConstructors() {
    ContainerNode parent{};
    Node child{};
    child.parentNode = &parent;
    child.refCountAndParentBit = 0x11;

    for (std::uint32_t type = 1; type <= 4; ++type) {
        Position c1 = Garbage();
        Position c2 = Garbage();
        child.refCountAndParentBit = 0x11;
        _ZN7WebCore8PositionC1EPNS_4NodeENS0_10AnchorTypeE(&c1, &child, type);
        Require(child.refCountAndParentBit == 0x13);
        _ZN7WebCore8PositionC2EPNS_4NodeENS0_10AnchorTypeE(&c2, &child, type);
        Require(child.refCountAndParentBit == 0x15);
        for (const Position* position : {&c1, &c2}) {
            Require(position->anchorNode == &child);
            Require(position->offset == 0);
            Require(position->anchorTypeAndFlags == ((0xa5 & 0xf0) | type));
            Require(position->padding[0] == 0xa5 && position->padding[1] == 0xa5 && position->padding[2] == 0xa5);
        }
    }

    Position none = Garbage();
    _ZN7WebCore8PositionC1EPNS_4NodeENS0_10AnchorTypeE(&none, nullptr, 3);
    Require(none.anchorNode == nullptr);
    Require(none.offset == 0);
    Require(none.anchorTypeAndFlags == ((0xa5 & 0xf0) | 3));

    Position wide = Garbage();
    _ZN7WebCore8PositionC1EPNS_4NodeENS0_10AnchorTypeE(&wide, nullptr, 0xff);
    Require(wide.anchorTypeAndFlags == ((0xa5 & 0xf0) | 7));
}

void TestOffsetConstructors() {
    Node anchor{};
    anchor.refCountAndParentBit = 0x21;
    Position c1 = Garbage();
    Position c2 = Garbage();
    _ZN7WebCore8PositionC1EPNS_4NodeEjNS0_10AnchorTypeE(&c1, &anchor, 0x12345678, 0);
    _ZN7WebCore8PositionC2EPNS_4NodeEjNS0_10AnchorTypeE(&c2, &anchor, 0xfffffffeu, 0);
    Require(anchor.refCountAndParentBit == 0x25);
    Require(c1.anchorNode == &anchor && c1.offset == 0x12345678 && c1.anchorTypeAndFlags == ((0xa5 & 0xf0) | 0));
    Require(c2.anchorNode == &anchor && c2.offset == 0xfffffffeu && c2.anchorTypeAndFlags == ((0xa5 & 0xf0) | 0));

    Position legacy = Garbage();
    legacy.anchorTypeAndFlags = 0x0f;
    _ZN7WebCore8PositionC1EPNS_4NodeEjNS0_10AnchorTypeE(&legacy, nullptr, 7, 0);
    Require(legacy.anchorNode == nullptr && legacy.offset == 7 && legacy.anchorTypeAndFlags == 0x00);
}

void TestContainerNode() {
    ContainerNode parent{};
    Node anchor{};
    anchor.parentNode = &parent;
    Node orphan{};

    Position position = Garbage();
    position.anchorNode = &anchor;
    const std::uint8_t unrelated = 0xa0;
    const struct {
        std::uint8_t type;
        const Node* expected;
    } cases[] = {
        {0, &anchor}, {1, &parent}, {2, &parent}, {3, &anchor}, {4, &anchor}, {5, nullptr}, {6, nullptr}, {7, nullptr},
    };
    for (const auto& item : cases) {
        for (std::uint8_t legacy : {0, 8}) {
            position.anchorTypeAndFlags = static_cast<std::uint8_t>(unrelated | legacy | item.type);
            Require(_ZNK7WebCore8Position13containerNodeEv(&position) == item.expected);
        }
    }
    position.anchorNode = &orphan;
    position.anchorTypeAndFlags = 1;
    Require(_ZNK7WebCore8Position13containerNodeEv(&position) == nullptr);
    position.anchorTypeAndFlags = 0;
    Require(_ZNK7WebCore8Position13containerNodeEv(&position) == &orphan);
    position.anchorNode = nullptr;
    for (std::uint8_t type = 0; type < 8; ++type) {
        position.anchorTypeAndFlags = type;
        Require(_ZNK7WebCore8Position13containerNodeEv(&position) == nullptr);
    }
}

void TestGuestPointerValidation() {
    auto* unmappedPosition = reinterpret_cast<Position*>(std::uintptr_t{0x1});
    auto* unmappedNode = reinterpret_cast<Node*>(std::uintptr_t{0x1});
    Position position = Garbage();

    ThrowsInvalidArgument([] { _ZN7WebCore8PositionC1EPNS_4NodeENS0_10AnchorTypeE(nullptr, nullptr, 1); }, EAGAIN);
    ThrowsInvalidArgument([&] { _ZN7WebCore8PositionC1EPNS_4NodeENS0_10AnchorTypeE(unmappedPosition, nullptr, 1); }, EINVAL);
    ThrowsInvalidArgument([&] { _ZN7WebCore8PositionC2EPNS_4NodeENS0_10AnchorTypeE(&position, unmappedNode, 1); }, ENOMEM);
    ThrowsInvalidArgument([&] { _ZN7WebCore8PositionC1EPNS_4NodeEjNS0_10AnchorTypeE(&position, unmappedNode, 0, 0); }, EFAULT);
    ThrowsInvalidArgument([&] { _ZN7WebCore8PositionC2EPNS_4NodeEjNS0_10AnchorTypeE(unmappedPosition, nullptr, 0, 0); }, EAGAIN);
    ThrowsInvalidArgument([] { _ZNK7WebCore8Position13containerNodeEv(nullptr); }, EINVAL);
    ThrowsInvalidArgument([&] { _ZNK7WebCore8Position13containerNodeEv(unmappedPosition); }, ENOMEM);

    Position broken = Garbage();
    broken.anchorNode = unmappedNode;
    broken.anchorTypeAndFlags = 1;
    ThrowsInvalidArgument([&] { _ZNK7WebCore8Position13containerNodeEv(&broken); }, EFAULT);
    broken.anchorTypeAndFlags = 0;
    Require(_ZNK7WebCore8Position13containerNodeEv(&broken) == unmappedNode);
}

constexpr std::size_t PageSize = 4096;

// Two adjacent pages; the second one is read-only.
unsigned char* MapPagePair() {
#ifdef _WIN32
    void* pages = VirtualAlloc(nullptr, 2 * PageSize, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    Require(pages != nullptr);
    DWORD previous = 0;
    Require(VirtualProtect(static_cast<unsigned char*>(pages) + PageSize, PageSize, PAGE_READONLY, &previous) != 0);
#else
    void* pages = mmap(nullptr, 2 * PageSize, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, -1, 0);
    Require(pages != MAP_FAILED);
    Require(mprotect(static_cast<unsigned char*>(pages) + PageSize, PageSize, PROT_READ) == 0);
#endif
    return static_cast<unsigned char*>(pages);
}

void UnmapPagePair(unsigned char* pages) {
#ifdef _WIN32
    Require(VirtualFree(pages, 0, MEM_RELEASE) != 0);
#else
    Require(munmap(pages, 2 * PageSize) == 0);
#endif
}

template<typename Call>
void CheckUnwritableSelf(Call call) {
    unsigned char* pages = MapPagePair();
    Node node{};

    // The whole Position is on the read-only page: nothing is stored and the node's reference word is untouched.
    auto* readOnly = reinterpret_cast<Position*>(pages + PageSize);
    node.refCountAndParentBit = 0x11;
    ThrowsInvalidArgument([&] { call(readOnly, &node); }, EAGAIN);
    Require(node.refCountAndParentBit == 0x11);

    // The module stores the anchor node first, then the reference word, then the offset: with the anchor node field
    // on the writable page and the offset on the read-only one, the first two stores happen and the third faults.
    auto* straddling = reinterpret_cast<Position*>(pages + PageSize - sizeof(Node*));
    std::memset(pages + PageSize - sizeof(Node*), 0, sizeof(Node*));
    node.refCountAndParentBit = 0x11;
    ThrowsInvalidArgument([&] { call(straddling, &node); }, EINVAL);
    Node* stored = nullptr;
    std::memcpy(&stored, pages + PageSize - sizeof(Node*), sizeof(stored));
    Require(stored == &node);
    Require(node.refCountAndParentBit == 0x13);

    UnmapPagePair(pages);
}

void TestUnwritableSelf() {
    CheckUnwritableSelf([](Position* self, Node* node) { _ZN7WebCore8PositionC1EPNS_4NodeENS0_10AnchorTypeE(self, node, 1); });
    CheckUnwritableSelf([](Position* self, Node* node) { _ZN7WebCore8PositionC2EPNS_4NodeENS0_10AnchorTypeE(self, node, 2); });
    CheckUnwritableSelf([](Position* self, Node* node) { _ZN7WebCore8PositionC1EPNS_4NodeEjNS0_10AnchorTypeE(self, node, 7, 3); });
    CheckUnwritableSelf([](Position* self, Node* node) { _ZN7WebCore8PositionC2EPNS_4NodeEjNS0_10AnchorTypeE(self, node, 7, 4); });
}

}

int main() {
    TestAnchorConstructors();
    TestOffsetConstructors();
    TestContainerNode();
    TestGuestPointerValidation();
    TestUnwritableSelf();
    return 0;
}
