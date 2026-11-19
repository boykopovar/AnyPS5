// Derived from Source/WebCore/platform/network/BlobDataFileReference.h@e12a212a531aeddc11259e8515671cf5188e038d
/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "prx/libSceNKWebKit/src/WtfStringAbi.hpp"

#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace NKWebKit::WebCoreAbi {

struct BlobDataFileReference {
    const void* const* vtable;
    unsigned refCount;
    NKWebKit::WtfAbi::String path;
    NKWebKit::WtfAbi::String replacementPath;
    std::uint64_t size;
    double expectedModificationTime;
};

static_assert(sizeof(BlobDataFileReference) == 48);
static_assert(alignof(BlobDataFileReference) == 8);
static_assert(offsetof(BlobDataFileReference, vtable) == 0);
static_assert(offsetof(BlobDataFileReference, refCount) == 8);
static_assert(offsetof(BlobDataFileReference, path) == 16);
static_assert(offsetof(BlobDataFileReference, replacementPath) == 24);
static_assert(offsetof(BlobDataFileReference, size) == 32);
static_assert(offsetof(BlobDataFileReference, expectedModificationTime) == 40);

struct OptionalWallTime {
    bool hasValue{false};
    double value{0.0};

    OptionalWallTime() = default;
    explicit OptionalWallTime(double time) : hasValue(true), value(time) {}
    OptionalWallTime(const OptionalWallTime& other) : hasValue(other.hasValue), value(other.value) {}
    OptionalWallTime& operator=(const OptionalWallTime&) = default;
};

static_assert(sizeof(OptionalWallTime) == 16);
static_assert(offsetof(OptionalWallTime, hasValue) == 0);
static_assert(offsetof(OptionalWallTime, value) == 8);
static_assert(!std::is_trivially_copyable_v<OptionalWallTime>);

struct BlobDataFileReferenceRef {
    BlobDataFileReference* ptr;

    explicit BlobDataFileReferenceRef(BlobDataFileReference* value) : ptr(value) {}
    BlobDataFileReferenceRef(const BlobDataFileReferenceRef&) = delete;
    BlobDataFileReferenceRef& operator=(const BlobDataFileReferenceRef&) = delete;
    ~BlobDataFileReferenceRef() {}
};

static_assert(sizeof(BlobDataFileReferenceRef) == 8);

}  // namespace NKWebKit::WebCoreAbi
