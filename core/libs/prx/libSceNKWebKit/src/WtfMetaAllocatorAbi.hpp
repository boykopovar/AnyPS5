/*
 * Copyright (C) 2010, 2011 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstddef>
#include <cstdint>
#include <utility>

namespace NKWebKit::WtfAbi {

template<class NodeType, typename KeyType>
class RedBlackTree {
public:
    enum Color { Red = 1, Black };

    class Node {
        friend class RedBlackTree;

    public:
        const NodeType* successor() const {
            const Node* x = this;
            if (x->right()) return treeMinimum(x->right());
            const NodeType* y = x->parent();
            while (y && x == y->right()) {
                x = y;
                y = y->parent();
            }
            return y;
        }

        const NodeType* predecessor() const {
            const Node* x = this;
            if (x->left()) return treeMaximum(x->left());
            const NodeType* y = x->parent();
            while (y && x == y->left()) {
                x = y;
                y = y->parent();
            }
            return y;
        }

        NodeType* successor() {
            return const_cast<NodeType*>(const_cast<const Node*>(this)->successor());
        }

        NodeType* predecessor() {
            return const_cast<NodeType*>(const_cast<const Node*>(this)->predecessor());
        }

        NodeType* parent() const {
            return reinterpret_cast<NodeType*>(m_parentAndRed & ~static_cast<std::uintptr_t>(1));
        }

        NodeType* left() const {
            return m_left;
        }

        NodeType* right() const {
            return m_right;
        }

        Color color() const {
            return (m_parentAndRed & 1) ? Red : Black;
        }

        NodeType* m_left;
        NodeType* m_right;
        std::uintptr_t m_parentAndRed;

    private:
        void reset() {
            m_left = nullptr;
            m_right = nullptr;
            m_parentAndRed = 1;
        }

        void setParent(NodeType* newParent) {
            m_parentAndRed = reinterpret_cast<std::uintptr_t>(newParent) | (m_parentAndRed & 1);
        }

        void setLeft(NodeType* node) {
            m_left = node;
        }

        void setRight(NodeType* node) {
            m_right = node;
        }

        void setColor(Color value) {
            if (value == Red)
                m_parentAndRed |= 1;
            else
                m_parentAndRed &= ~static_cast<std::uintptr_t>(1);
        }
    };

    void insert(NodeType* x) {
        x->reset();
        treeInsert(x);
        x->setColor(Red);

        while (x != m_root && x->parent()->color() == Red) {
            if (x->parent() == x->parent()->parent()->left()) {
                NodeType* y = x->parent()->parent()->right();
                if (y && y->color() == Red) {
                    x->parent()->setColor(Black);
                    y->setColor(Black);
                    x->parent()->parent()->setColor(Red);
                    x = x->parent()->parent();
                } else {
                    if (x == x->parent()->right()) {
                        x = x->parent();
                        leftRotate(x);
                    }
                    x->parent()->setColor(Black);
                    x->parent()->parent()->setColor(Red);
                    rightRotate(x->parent()->parent());
                }
            } else {
                NodeType* y = x->parent()->parent()->left();
                if (y && y->color() == Red) {
                    x->parent()->setColor(Black);
                    y->setColor(Black);
                    x->parent()->parent()->setColor(Red);
                    x = x->parent()->parent();
                } else {
                    if (x == x->parent()->left()) {
                        x = x->parent();
                        rightRotate(x);
                    }
                    x->parent()->setColor(Black);
                    x->parent()->parent()->setColor(Red);
                    leftRotate(x->parent()->parent());
                }
            }
        }

        m_root->setColor(Black);
    }

    NodeType* remove(NodeType* z) {
        NodeType* y = (!z->left() || !z->right()) ? z : z->successor();
        NodeType* x = y->left() ? y->left() : y->right();

        NodeType* xParent;
        if (x) {
            x->setParent(y->parent());
            xParent = x->parent();
        } else
            xParent = y->parent();
        if (!y->parent())
            m_root = x;
        else if (y == y->parent()->left())
            y->parent()->setLeft(x);
        else
            y->parent()->setRight(x);

        if (y != z) {
            if (y->color() == Black) removeFixup(x, xParent);

            y->setParent(z->parent());
            y->setColor(z->color());
            y->setLeft(z->left());
            y->setRight(z->right());

            if (z->left()) z->left()->setParent(y);
            if (z->right()) z->right()->setParent(y);
            if (z->parent()) {
                if (z->parent()->left() == z)
                    z->parent()->setLeft(y);
                else
                    z->parent()->setRight(y);
            } else
                m_root = y;
        } else if (y->color() == Black)
            removeFixup(x, xParent);

        return z;
    }

    NodeType* findLeastGreaterThanOrEqual(const KeyType& key) const {
        NodeType* best = nullptr;
        for (NodeType* current = m_root; current;) {
            if (current->key() == key) return current;
            if (current->key() < key)
                current = current->right();
            else {
                best = current;
                current = current->left();
            }
        }
        return best;
    }

    NodeType* findGreatestLessThanOrEqual(const KeyType& key) const {
        NodeType* best = nullptr;
        for (NodeType* current = m_root; current;) {
            if (current->key() == key) return current;
            if (current->key() > key)
                current = current->left();
            else {
                best = current;
                current = current->right();
            }
        }
        return best;
    }

    NodeType* first() const {
        return m_root ? treeMinimum(m_root) : nullptr;
    }

    NodeType* m_root;

private:
    static NodeType* treeMinimum(NodeType* x) {
        while (x->left()) x = x->left();
        return x;
    }

    static NodeType* treeMaximum(NodeType* x) {
        while (x->right()) x = x->right();
        return x;
    }

    static const NodeType* treeMinimum(const NodeType* x) {
        while (x->left()) x = x->left();
        return x;
    }

    static const NodeType* treeMaximum(const NodeType* x) {
        while (x->right()) x = x->right();
        return x;
    }

    void treeInsert(NodeType* z) {
        NodeType* y = nullptr;
        NodeType* x = m_root;
        while (x) {
            y = x;
            x = (z->key() < x->key()) ? x->left() : x->right();
        }
        z->setParent(y);
        if (!y)
            m_root = z;
        else if (z->key() < y->key())
            y->setLeft(z);
        else
            y->setRight(z);
    }

    NodeType* leftRotate(NodeType* x) {
        NodeType* y = x->right();

        x->setRight(y->left());
        if (y->left()) y->left()->setParent(x);

        y->setParent(x->parent());
        if (!x->parent())
            m_root = y;
        else if (x == x->parent()->left())
            x->parent()->setLeft(y);
        else
            x->parent()->setRight(y);

        y->setLeft(x);
        x->setParent(y);
        return y;
    }

    NodeType* rightRotate(NodeType* y) {
        NodeType* x = y->left();

        y->setLeft(x->right());
        if (x->right()) x->right()->setParent(y);

        x->setParent(y->parent());
        if (!y->parent())
            m_root = x;
        else if (y == y->parent()->left())
            y->parent()->setLeft(x);
        else
            y->parent()->setRight(x);

        x->setRight(y);
        y->setParent(x);
        return x;
    }

    void removeFixup(NodeType* x, NodeType* xParent) {
        while (x != m_root && (!x || x->color() == Black)) {
            if (x == xParent->left()) {
                NodeType* w = xParent->right();
                if (w->color() == Red) {
                    w->setColor(Black);
                    xParent->setColor(Red);
                    leftRotate(xParent);
                    w = xParent->right();
                }
                if ((!w->left() || w->left()->color() == Black) && (!w->right() || w->right()->color() == Black)) {
                    w->setColor(Red);
                    x = xParent;
                    xParent = x->parent();
                } else {
                    if (!w->right() || w->right()->color() == Black) {
                        w->left()->setColor(Black);
                        w->setColor(Red);
                        rightRotate(w);
                        w = xParent->right();
                    }
                    w->setColor(xParent->color());
                    xParent->setColor(Black);
                    if (w->right()) w->right()->setColor(Black);
                    leftRotate(xParent);
                    x = m_root;
                    xParent = x->parent();
                }
            } else {
                NodeType* w = xParent->left();
                if (w->color() == Red) {
                    w->setColor(Black);
                    xParent->setColor(Red);
                    rightRotate(xParent);
                    w = xParent->left();
                }
                if ((!w->right() || w->right()->color() == Black) && (!w->left() || w->left()->color() == Black)) {
                    w->setColor(Red);
                    x = xParent;
                    xParent = x->parent();
                } else {
                    if (!w->left() || w->left()->color() == Black) {
                        w->right()->setColor(Black);
                        w->setColor(Red);
                        leftRotate(w);
                        w = xParent->left();
                    }
                    w->setColor(xParent->color());
                    xParent->setColor(Black);
                    if (w->left()) w->left()->setColor(Black);
                    rightRotate(xParent);
                    x = m_root;
                    xParent = x->parent();
                }
            }
        }
        if (x) x->setColor(Black);
    }
};

struct MetaAllocator;

struct MetaAllocatorRefCount {
    std::uint32_t refCount;
};

struct MetaAllocatorHandle : MetaAllocatorRefCount, RedBlackTree<MetaAllocatorHandle, void*>::Node {
    MetaAllocator* allocator;
    std::uintptr_t start;
    std::uintptr_t end;

    void* key() const {
        return reinterpret_cast<void*>(start);
    }

    std::size_t sizeInBytes() const {
        return end - start;
    }
};

struct MetaAllocatorFreeSpaceNode : RedBlackTree<MetaAllocatorFreeSpaceNode, std::size_t>::Node {
    std::uintptr_t start;
    std::uintptr_t end;

    std::size_t key() const {
        return end - start;
    }

    std::size_t sizeInBytes() const {
        return end - start;
    }
};

struct MetaAllocatorTracker {
    RedBlackTree<MetaAllocatorHandle, void*> allocations;
};

struct MetaAllocatorStatistics {
    std::size_t bytesAllocated;
    std::size_t bytesReserved;
    std::size_t bytesCommitted;
};

struct MetaAllocatorHandleRef {
    MetaAllocatorHandle* ptr;

    MetaAllocatorHandleRef() : ptr(nullptr) {}
    explicit MetaAllocatorHandleRef(MetaAllocatorHandle* handle) : ptr(handle) {}
    MetaAllocatorHandleRef(const MetaAllocatorHandleRef&) = delete;
    MetaAllocatorHandleRef& operator=(const MetaAllocatorHandleRef&) = delete;
    MetaAllocatorHandleRef(MetaAllocatorHandleRef&& other) : ptr(other.ptr) {
        other.ptr = nullptr;
    }
    MetaAllocatorHandleRef& operator=(MetaAllocatorHandleRef&& other) {
        MetaAllocatorHandleRef old(std::move(*this));
        ptr = other.ptr;
        other.ptr = nullptr;
        return *this;
    }
    ~MetaAllocatorHandleRef();
};

struct MetaAllocatorVtable {
    void (APS5_VABI* destructor)(MetaAllocator* self);
    void (APS5_VABI* deletingDestructor)(MetaAllocator* self);
    std::uintptr_t (APS5_VABI* allocateNewSpace)(MetaAllocator* self, std::size_t& numPages);
    void (APS5_VABI* notifyNeedPage)(MetaAllocator* self, void* page, std::size_t count);
    void (APS5_VABI* notifyPageIsFree)(MetaAllocator* self, void* page, std::size_t count);
    void (APS5_VABI* release)(MetaAllocator* self, const void* locker, MetaAllocatorHandle& handle);
};

struct MetaAllocator {
    const MetaAllocatorVtable* vptr;
    std::size_t allocationGranule;
    std::size_t pageSize;
    unsigned logAllocationGranule;
    unsigned logPageSize;
    RedBlackTree<MetaAllocatorFreeSpaceNode, std::size_t> freeSpaceSizeMap;
    void* freeSpaceStartAddressMap;
    void* freeSpaceEndAddressMap;
    void* pageOccupancyMap;
    std::size_t bytesAllocated;
    std::size_t bytesReserved;
    std::size_t bytesCommitted;
    std::uint8_t* lock;
    MetaAllocatorTracker* tracker;
};

static_assert(sizeof(RedBlackTree<MetaAllocatorHandle, void*>::Node) == 24);
static_assert(sizeof(MetaAllocatorHandle) == 56);
static_assert(sizeof(MetaAllocatorFreeSpaceNode) == 40);
static_assert(sizeof(MetaAllocatorTracker) == 8);
static_assert(sizeof(MetaAllocatorStatistics) == 24);
static_assert(sizeof(MetaAllocatorHandleRef) == 8);
static_assert(sizeof(MetaAllocator) == 104);
static_assert(offsetof(MetaAllocator, allocationGranule) == 8);
static_assert(offsetof(MetaAllocator, logAllocationGranule) == 24);
static_assert(offsetof(MetaAllocator, freeSpaceSizeMap) == 32);
static_assert(offsetof(MetaAllocator, pageOccupancyMap) == 56);
static_assert(offsetof(MetaAllocator, bytesAllocated) == 64);
static_assert(offsetof(MetaAllocator, lock) == 88);
static_assert(offsetof(MetaAllocator, tracker) == 96);

}  // namespace NKWebKit::WtfAbi
