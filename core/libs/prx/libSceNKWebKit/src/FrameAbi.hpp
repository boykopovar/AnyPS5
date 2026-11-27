#pragma once

#include <cstddef>
#include <cstdint>

namespace NKWebKit::WebCoreAbi {

struct Frame;

struct WeakPtrImpl {
    std::uint32_t refCount;
    std::uint32_t padding;
    void* pointer;
};

struct FrameTree {
    Frame* thisFrame;
    WeakPtrImpl* parent;
    void* specifiedName;
    void* uniqueName;
    Frame* nextSibling;
    WeakPtrImpl* previousSibling;
    Frame* firstChild;
    WeakPtrImpl* lastChild;
    std::uint32_t scopedChildCount;
    std::uint32_t padding;
    std::uint64_t frameIDGenerator;
};

struct Frame {
    std::uint8_t leadingMembers[0x40];
    WeakPtrImpl* page;
    std::uint64_t frameID;
    FrameTree tree;
};

constexpr std::size_t FrameOffsetPage = 0x40;
constexpr std::size_t FrameOffsetTree = 0x50;
constexpr std::size_t FrameTreeOffsetParent = 0x8;
constexpr std::size_t FrameTreeOffsetNextSibling = 0x20;
constexpr std::size_t FrameTreeOffsetFirstChild = 0x30;

static_assert(sizeof(WeakPtrImpl) == 0x10);
static_assert(sizeof(FrameTree) == 0x50);
static_assert(offsetof(WeakPtrImpl, pointer) == 0x8);
static_assert(offsetof(FrameTree, parent) == FrameTreeOffsetParent);
static_assert(offsetof(FrameTree, nextSibling) == FrameTreeOffsetNextSibling);
static_assert(offsetof(FrameTree, firstChild) == FrameTreeOffsetFirstChild);
static_assert(offsetof(Frame, page) == FrameOffsetPage);
static_assert(offsetof(Frame, tree) == FrameOffsetTree);
static_assert(sizeof(Frame) == 0xa0);

}
