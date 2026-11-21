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

using NKWebKit::WebCoreAbi::Frame;
using NKWebKit::WebCoreAbi::FrameTree;
using NKWebKit::WebCoreAbi::WeakPtrImpl;

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "frame tree walk check failed at line %u\n", location.line());
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

struct Forest {
    std::deque<Frame> frames;
    std::deque<WeakPtrImpl> weak;

    WeakPtrImpl* Weak(void* pointer) {
        WeakPtrImpl& impl = weak.emplace_back();
        impl = {};
        impl.refCount = 1;
        impl.pointer = pointer;
        return &impl;
    }

    Frame* Add(Frame* parent, WeakPtrImpl* page) {
        Frame& frame = frames.emplace_back();
        frame = {};
        frame.page = page;
        frame.tree.thisFrame = &frame;
        if (parent) {
            frame.tree.parent = Weak(parent);
            frame.tree.previousSibling = parent->tree.lastChild;
            if (parent->tree.lastChild) static_cast<Frame*>(parent->tree.lastChild->pointer)->tree.nextSibling = &frame;
            else parent->tree.firstChild = &frame;
            parent->tree.lastChild = Weak(&frame);
        }
        return &frame;
    }
};

void TestShape() {
    Forest forest;
    WeakPtrImpl* page = forest.Weak(reinterpret_cast<void*>(std::uintptr_t{0x1000}));
    Frame* root = forest.Add(nullptr, page);
    Frame* a = forest.Add(root, page);
    Frame* a1 = forest.Add(a, page);
    Frame* a2 = forest.Add(a, page);
    Frame* a21 = forest.Add(a2, page);
    Frame* b = forest.Add(root, page);
    Frame* c = forest.Add(root, page);
    Frame* c1 = forest.Add(c, page);

    Require(_ZNK7WebCore9FrameTree10childCountEv(&root->tree) == 3);
    Require(_ZNK7WebCore9FrameTree10childCountEv(&a->tree) == 2);
    Require(_ZNK7WebCore9FrameTree10childCountEv(&a1->tree) == 0);
    Require(_ZNK7WebCore9FrameTree10childCountEv(&c->tree) == 1);

    Require(_ZNK7WebCore9FrameTree3topEv(&root->tree) == root);
    Require(_ZNK7WebCore9FrameTree3topEv(&a21->tree) == root);
    Require(_ZNK7WebCore9FrameTree3topEv(&c1->tree) == root);

    Frame* order[] = {root, a, a1, a2, a21, b, c, c1};
    for (std::size_t i = 0; i + 1 < 8; ++i) Require(_ZNK7WebCore9FrameTree12traverseNextEPKNS_5FrameE(&order[i]->tree, nullptr) == order[i + 1]);
    Require(_ZNK7WebCore9FrameTree12traverseNextEPKNS_5FrameE(&c1->tree, nullptr) == nullptr);
    Require(_ZNK7WebCore9FrameTree12traverseNextEPKNS_5FrameE(&root->tree, root) == a);
    Require(_ZNK7WebCore9FrameTree12traverseNextEPKNS_5FrameE(&a->tree, a) == a1);
    Require(_ZNK7WebCore9FrameTree12traverseNextEPKNS_5FrameE(&a1->tree, a) == a2);
    Require(_ZNK7WebCore9FrameTree12traverseNextEPKNS_5FrameE(&a21->tree, a) == nullptr);
    Require(_ZNK7WebCore9FrameTree12traverseNextEPKNS_5FrameE(&a21->tree, a2) == nullptr);
    Require(_ZNK7WebCore9FrameTree12traverseNextEPKNS_5FrameE(&a21->tree, root) == b);
    Require(_ZNK7WebCore9FrameTree12traverseNextEPKNS_5FrameE(&a2->tree, a2) == a21);
    Require(_ZNK7WebCore9FrameTree12traverseNextEPKNS_5FrameE(&b->tree, b) == nullptr);
    Require(_ZNK7WebCore9FrameTree12traverseNextEPKNS_5FrameE(&c1->tree, c) == nullptr);
    Require(_ZNK7WebCore9FrameTree12traverseNextEPKNS_5FrameE(&c1->tree, root) == nullptr);

    Require(_ZNK7WebCore9FrameTree14isDescendantOfEPKNS_5FrameE(&a21->tree, root));
    Require(_ZNK7WebCore9FrameTree14isDescendantOfEPKNS_5FrameE(&a21->tree, a));
    Require(_ZNK7WebCore9FrameTree14isDescendantOfEPKNS_5FrameE(&a21->tree, a21));
    Require(!_ZNK7WebCore9FrameTree14isDescendantOfEPKNS_5FrameE(&a21->tree, b));
    Require(!_ZNK7WebCore9FrameTree14isDescendantOfEPKNS_5FrameE(&a->tree, a21));
    Require(!_ZNK7WebCore9FrameTree14isDescendantOfEPKNS_5FrameE(&a->tree, nullptr));
    Require(!_ZNK7WebCore9FrameTree14isDescendantOfEPKNS_5FrameE(&root->tree, c));
}

void TestPages() {
    Forest forest;
    WeakPtrImpl* pageOne = forest.Weak(reinterpret_cast<void*>(std::uintptr_t{0x1000}));
    WeakPtrImpl* pageTwo = forest.Weak(reinterpret_cast<void*>(std::uintptr_t{0x2000}));
    WeakPtrImpl* pageOneAgain = forest.Weak(reinterpret_cast<void*>(std::uintptr_t{0x1000}));
    WeakPtrImpl* clearedPage = forest.Weak(nullptr);
    Frame* root = forest.Add(nullptr, pageOne);
    Frame* sameImpl = forest.Add(root, pageOne);
    Frame* otherImplSamePage = forest.Add(root, pageOneAgain);
    Frame* otherPage = forest.Add(root, pageTwo);
    Frame* noPage = forest.Add(root, nullptr);
    Frame* clearedFrame = forest.Add(root, clearedPage);

    Require(_ZNK7WebCore9FrameTree14isDescendantOfEPKNS_5FrameE(&sameImpl->tree, root));
    Require(_ZNK7WebCore9FrameTree14isDescendantOfEPKNS_5FrameE(&otherImplSamePage->tree, root));
    Require(!_ZNK7WebCore9FrameTree14isDescendantOfEPKNS_5FrameE(&otherPage->tree, root));
    Require(!_ZNK7WebCore9FrameTree14isDescendantOfEPKNS_5FrameE(&noPage->tree, root));
    Require(_ZNK7WebCore9FrameTree14isDescendantOfEPKNS_5FrameE(&noPage->tree, clearedFrame) == false);
    Require(_ZNK7WebCore9FrameTree14isDescendantOfEPKNS_5FrameE(&noPage->tree, noPage));
    Require(_ZNK7WebCore9FrameTree14isDescendantOfEPKNS_5FrameE(&clearedFrame->tree, clearedFrame));
}

void TestDetachedFrames() {
    Forest forest;
    Frame* root = forest.Add(nullptr, nullptr);
    Frame* child = forest.Add(root, nullptr);
    child->tree.parent = forest.Weak(nullptr);
    Require(_ZNK7WebCore9FrameTree3topEv(&child->tree) == child);
    Require(_ZNK7WebCore9FrameTree12traverseNextEPKNS_5FrameE(&child->tree, nullptr) == nullptr);
    Require(_ZNK7WebCore9FrameTree12traverseNextEPKNS_5FrameE(&child->tree, root) == nullptr);

    FrameTree orphan{};
    Require(_ZNK7WebCore9FrameTree3topEv(&orphan) == nullptr);
    Require(_ZNK7WebCore9FrameTree10childCountEv(&orphan) == 0);
    Require(_ZNK7WebCore9FrameTree12traverseNextEPKNS_5FrameE(&orphan, nullptr) == nullptr);
}

void TestGuestPointerValidation() {
    auto* unmappedTree = reinterpret_cast<FrameTree*>(std::uintptr_t{0x1});
    auto* unmappedFrame = reinterpret_cast<Frame*>(std::uintptr_t{0x1});

    ThrowsInvalidArgument([] { _ZNK7WebCore9FrameTree10childCountEv(nullptr); }, EAGAIN);
    ThrowsInvalidArgument([&] { _ZNK7WebCore9FrameTree10childCountEv(unmappedTree); }, EINVAL);
    ThrowsInvalidArgument([] { _ZNK7WebCore9FrameTree3topEv(nullptr); }, ENOMEM);
    ThrowsInvalidArgument([&] { _ZNK7WebCore9FrameTree3topEv(unmappedTree); }, EFAULT);
    ThrowsInvalidArgument([] { _ZNK7WebCore9FrameTree12traverseNextEPKNS_5FrameE(nullptr, nullptr); }, EAGAIN);
    ThrowsInvalidArgument([&] { _ZNK7WebCore9FrameTree12traverseNextEPKNS_5FrameE(unmappedTree, nullptr); }, EINVAL);
    ThrowsInvalidArgument([&] { _ZNK7WebCore9FrameTree14isDescendantOfEPKNS_5FrameE(nullptr, unmappedFrame); }, ENOMEM);
    ThrowsInvalidArgument([&] { _ZNK7WebCore9FrameTree14isDescendantOfEPKNS_5FrameE(unmappedTree, unmappedFrame); }, EFAULT);

    Forest forest;
    Frame* root = forest.Add(nullptr, nullptr);
    root->tree.firstChild = unmappedFrame;
    ThrowsInvalidArgument([&] { _ZNK7WebCore9FrameTree10childCountEv(&root->tree); }, EAGAIN);
    Require(_ZNK7WebCore9FrameTree12traverseNextEPKNS_5FrameE(&root->tree, nullptr) == unmappedFrame);

    Frame* child = forest.Add(nullptr, nullptr);
    child->tree.parent = forest.Weak(unmappedFrame);
    ThrowsInvalidArgument([&] { _ZNK7WebCore9FrameTree3topEv(&child->tree); }, EINVAL);

    Frame* weakBroken = forest.Add(nullptr, nullptr);
    weakBroken->tree.parent = reinterpret_cast<WeakPtrImpl*>(std::uintptr_t{0x1});
    ThrowsInvalidArgument([&] { _ZNK7WebCore9FrameTree3topEv(&weakBroken->tree); }, ENOMEM);
    Frame* ancestor = forest.Add(nullptr, nullptr);
    ThrowsInvalidArgument([&] { _ZNK7WebCore9FrameTree14isDescendantOfEPKNS_5FrameE(&weakBroken->tree, ancestor); }, EFAULT);
}

}

int main() {
    TestShape();
    TestPages();
    TestDetachedFrames();
    TestGuestPointerValidation();
    return 0;
}
