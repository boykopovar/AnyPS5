/*
 * Copyright (C) 2000 Lars Knoll (knoll@kde.org)
 *           (C) 2000 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003, 2005-2008, 2017 Apple Inc. All rights reserved.
 * Copyright (C) 2006 Graham Dennis (graham.dennis@gmail.com)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */
/*
 * Derived from Source/WebCore/platform/graphics/transforms/TransformOperation.h@c55364d53f971f63a709976f168d932d558b8909
 * Derived from Source/WebCore/platform/graphics/transforms/RotateTransformOperation.h@c55364d53f971f63a709976f168d932d558b8909
 * Derived from Source/WebCore/platform/graphics/transforms/ScaleTransformOperation.h@c55364d53f971f63a709976f168d932d558b8909
 * Derived from Source/WebCore/platform/graphics/transforms/SkewTransformOperation.h@c55364d53f971f63a709976f168d932d558b8909
 * Derived from Source/WebCore/platform/graphics/transforms/MatrixTransformOperation.h@c55364d53f971f63a709976f168d932d558b8909
 */

#pragma once

#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/src/WebCoreTypes.hpp"
#include "prx/libSceNKWebKit/src/WtfTextAbi.hpp"

#include <cstddef>
#include <type_traits>
#include <utility>

namespace NKWebKit::TransformAbi {

class TransformOperationRef;

struct TransformOperationVtable {
    void (APS5_VABI* completeDestructor)(WebCore::LegacyTransformOperation*);
    void (APS5_VABI* deletingDestructor)(WebCore::LegacyTransformOperation*);
    TransformOperationRef (APS5_VABI* clone)(const WebCore::LegacyTransformOperation*);
    bool (APS5_VABI* equals)(const WebCore::LegacyTransformOperation*, const WebCore::LegacyTransformOperation&);
    bool (APS5_VABI* isIdentity)(const WebCore::LegacyTransformOperation*);
    bool (APS5_VABI* apply)(const WebCore::LegacyTransformOperation*, WebCore::TransformationMatrix&, const WebCore::FloatSize&);
    TransformOperationRef (APS5_VABI* blend)(WebCore::LegacyTransformOperation*, const WebCore::LegacyTransformOperation*, double, bool);
    bool (APS5_VABI* isAffectedByTransformOrigin)(const WebCore::LegacyTransformOperation*);
    bool (APS5_VABI* isRepresentableIn2D)(const WebCore::LegacyTransformOperation*);
    void (APS5_VABI* dump)(const WebCore::LegacyTransformOperation*, NKWebKit::WtfAbi::TextStream&);
};

struct TransformOperationVtableStorage {
    std::ptrdiff_t offsetToTop;
    const void* typeInfo;
    TransformOperationVtable dispatch;
};

inline const TransformOperationVtable& Dispatch(const WebCore::LegacyTransformOperation& operation) {
    return *static_cast<const TransformOperationVtable*>(operation.m_vptr);
}

inline void RefOperation(WebCore::LegacyTransformOperation* operation) {
    ++operation->m_refCount;
}

inline void DerefOperation(WebCore::LegacyTransformOperation* operation) {
    if (!operation) return;
    if (operation->m_refCount != 1) {
        --operation->m_refCount;
        return;
    }
    Dispatch(*operation).deletingDestructor(operation);
}

class TransformOperationRef {
public:
    explicit TransformOperationRef(WebCore::LegacyTransformOperation* operation) : m_operation(operation) {}
    TransformOperationRef(TransformOperationRef&& other) noexcept : m_operation(std::exchange(other.m_operation, nullptr)) {}
    TransformOperationRef(const TransformOperationRef&) = delete;
    TransformOperationRef& operator=(const TransformOperationRef&) = delete;
    TransformOperationRef& operator=(TransformOperationRef&&) = delete;
    ~TransformOperationRef() { DerefOperation(m_operation); }

    WebCore::LegacyTransformOperation* Get() const { return m_operation; }
    WebCore::LegacyTransformOperation* Leak() { return std::exchange(m_operation, nullptr); }

private:
    WebCore::LegacyTransformOperation* m_operation;
};

static_assert(sizeof(TransformOperationVtable) == 80);
static_assert(offsetof(TransformOperationVtable, completeDestructor) == 0);
static_assert(offsetof(TransformOperationVtable, deletingDestructor) == 8);
static_assert(offsetof(TransformOperationVtable, clone) == 16);
static_assert(offsetof(TransformOperationVtable, equals) == 24);
static_assert(offsetof(TransformOperationVtable, isIdentity) == 32);
static_assert(offsetof(TransformOperationVtable, apply) == 40);
static_assert(offsetof(TransformOperationVtable, blend) == 48);
static_assert(offsetof(TransformOperationVtable, isAffectedByTransformOrigin) == 56);
static_assert(offsetof(TransformOperationVtable, isRepresentableIn2D) == 64);
static_assert(offsetof(TransformOperationVtable, dump) == 72);
static_assert(sizeof(TransformOperationVtableStorage) == 96);
static_assert(offsetof(TransformOperationVtableStorage, dispatch) == 16);
static_assert(sizeof(TransformOperationRef) == 8);
static_assert(!std::is_trivially_copyable_v<TransformOperationRef>);

}  // namespace NKWebKit::TransformAbi
