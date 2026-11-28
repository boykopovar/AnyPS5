// Derived from Source/WebCore/platform/graphics/ImageBufferBackend.cpp@a77a158d4e2086fbe712e488ed147e8a54d44d3c
// Derived from Source/WebCore/platform/graphics/ImageBufferBackend.cpp@c55364d53f971f63a709976f168d932d558b8909
/*
 * Copyright (C) 2020 Apple Inc.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "prx/libSceNKWebKit/include/ImageBufferBackendExports.hpp"
#include "prx/libSceNKWebKit/include/WebCoreExports.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/GuestArena.hpp"

#include <cerrno>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <vector>

namespace {

bool IsWithinIntRange(float value) {
    return value > static_cast<float>(std::numeric_limits<int>::min()) && value < static_cast<float>(std::numeric_limits<int>::max());
}

bool IsValidAlpha(WebCore::AlphaPremultiplication value) {
    return value == WebCore::AlphaPremultiplication::Premultiplied || value == WebCore::AlphaPremultiplication::Unpremultiplied;
}

bool IsValidColorFormat(WebCore::ColorFormat value) {
    return value == WebCore::ColorFormat::RGBA || value == WebCore::ColorFormat::BGRA;
}

bool CopyGuestInPreservingErrno(void* destination, const void* source, std::size_t bytes) {
    const int savedErrno = errno;
    const bool ok = GuestCopyIn_nid_no_patch(destination, source, bytes);
    errno = savedErrno;
    return ok;
}

bool CopyGuestOutPreservingErrno(void* destination, const void* source, std::size_t bytes) {
    const int savedErrno = errno;
    const bool ok = GuestCopyOut_nid_no_patch(destination, source, bytes);
    errno = savedErrno;
    return ok;
}

bool IsGuestWritablePreservingErrno(void* destination, std::size_t bytes) {
    const int savedErrno = errno;
    const GuestArena::HostWrite guard(destination, bytes);
    const bool ok = guard.Open() && GuestWritable_nid_no_patch(destination, bytes);
    errno = savedErrno;
    return ok;
}

bool RowRangeFits(const void* base, std::size_t stride, std::size_t height, std::size_t rowBytes) {
    if (!base || height == 0) return false;
    const auto baseAddress = reinterpret_cast<std::uintptr_t>(base);
    const std::size_t lastRowIndex = height - 1;
    if (stride != 0 && lastRowIndex > std::numeric_limits<std::uintptr_t>::max() / stride) return false;
    const std::uintptr_t lastRowOffset = static_cast<std::uintptr_t>(lastRowIndex) * stride;
    if (baseAddress > std::numeric_limits<std::uintptr_t>::max() - lastRowOffset) return false;
    const std::uintptr_t lastRowAddress = baseAddress + lastRowOffset;
    return lastRowAddress <= std::numeric_limits<std::uintptr_t>::max() - rowBytes;
}

void CopyPremultipliedToPremultiplied(WebCore::ColorFormat srcColorFormat, const std::uint8_t* srcPixel,
                                      WebCore::ColorFormat destColorFormat, std::uint8_t* destPixel) {
    const std::uint8_t alpha = srcPixel[3];
    if (!alpha) {
        std::memset(destPixel, 0, 4);
        return;
    }
    if (srcColorFormat == destColorFormat) {
        std::memcpy(destPixel, srcPixel, 4);
        return;
    }
    destPixel[0] = srcPixel[2];
    destPixel[1] = srcPixel[1];
    destPixel[2] = srcPixel[0];
    destPixel[3] = srcPixel[3];
}

void CopyPremultipliedToUnpremultiplied(WebCore::ColorFormat srcColorFormat, const std::uint8_t* srcPixel,
                                        WebCore::ColorFormat destColorFormat, std::uint8_t* destPixel) {
    const std::uint8_t alpha = srcPixel[3];
    if (!alpha || alpha == 255) {
        CopyPremultipliedToPremultiplied(srcColorFormat, srcPixel, destColorFormat, destPixel);
        return;
    }
    if (srcColorFormat == destColorFormat) {
        destPixel[0] = static_cast<std::uint8_t>((srcPixel[0] * 255) / alpha);
        destPixel[1] = static_cast<std::uint8_t>((srcPixel[1] * 255) / alpha);
        destPixel[2] = static_cast<std::uint8_t>((srcPixel[2] * 255) / alpha);
        destPixel[3] = alpha;
        return;
    }
    destPixel[0] = static_cast<std::uint8_t>((srcPixel[2] * 255) / alpha);
    destPixel[1] = static_cast<std::uint8_t>((srcPixel[1] * 255) / alpha);
    destPixel[2] = static_cast<std::uint8_t>((srcPixel[0] * 255) / alpha);
    destPixel[3] = alpha;
}

void CopyUnpremultipliedToPremultiplied(WebCore::ColorFormat srcColorFormat, const std::uint8_t* srcPixel,
                                        WebCore::ColorFormat destColorFormat, std::uint8_t* destPixel) {
    const std::uint8_t alpha = srcPixel[3];
    if (!alpha || alpha == 255) {
        CopyPremultipliedToPremultiplied(srcColorFormat, srcPixel, destColorFormat, destPixel);
        return;
    }
    if (srcColorFormat == destColorFormat) {
        destPixel[0] = static_cast<std::uint8_t>((srcPixel[0] * alpha + 254) / 255);
        destPixel[1] = static_cast<std::uint8_t>((srcPixel[1] * alpha + 254) / 255);
        destPixel[2] = static_cast<std::uint8_t>((srcPixel[2] * alpha + 254) / 255);
        destPixel[3] = alpha;
        return;
    }
    destPixel[0] = static_cast<std::uint8_t>((srcPixel[2] * alpha + 254) / 255);
    destPixel[1] = static_cast<std::uint8_t>((srcPixel[1] * alpha + 254) / 255);
    destPixel[2] = static_cast<std::uint8_t>((srcPixel[0] * alpha + 254) / 255);
    destPixel[3] = alpha;
}

void CopyUnpremultipliedToUnpremultiplied(WebCore::ColorFormat srcColorFormat, const std::uint8_t* srcPixel,
                                          WebCore::ColorFormat destColorFormat, std::uint8_t* destPixel) {
    if (srcColorFormat == destColorFormat) {
        std::memcpy(destPixel, srcPixel, 4);
        return;
    }
    destPixel[0] = srcPixel[2];
    destPixel[1] = srcPixel[1];
    destPixel[2] = srcPixel[0];
    destPixel[3] = srcPixel[3];
}

template <void (*CopyFunctor)(WebCore::ColorFormat, const std::uint8_t*, WebCore::ColorFormat, std::uint8_t*)>
void CopyImagePixelsUnaccelerated(WebCore::ColorFormat srcColorFormat, std::size_t srcBytesPerRow, const std::uint8_t* srcRows,
                                  WebCore::ColorFormat destColorFormat, std::size_t destBytesPerRow, std::uint8_t* destRows,
                                  std::size_t rowBytes, std::size_t height,
                                  std::vector<std::uint8_t>& srcRow, std::vector<std::uint8_t>& destRow,
                                  const char* caller) {
    for (std::size_t y = 0; y < height; ++y) {
        const std::uint8_t* const guestSrc = srcRows + y * srcBytesPerRow;
        std::uint8_t* const guestDest = destRows + y * destBytesPerRow;
        if (!CopyGuestInPreservingErrno(srcRow.data(), guestSrc, rowBytes)) {
            NotImplemented_nid_no_patch(caller);
        }
        for (std::size_t x = 0; x < rowBytes; x += 4) {
            CopyFunctor(srcColorFormat, srcRow.data() + x, destColorFormat, destRow.data() + x);
        }
        if (!CopyGuestOutPreservingErrno(guestDest, destRow.data(), rowBytes)) {
            NotImplemented_nid_no_patch(caller);
        }
    }
}

} // namespace

extern "C" {

WebCore::IntSize APS5_VABI _ZN7WebCore18ImageBufferBackend20calculateBackendSizeERKNS_9FloatSizeEf(
    const WebCore::FloatSize& size, float resolutionScale) {
    WebCore::FloatSize scaledSize{std::ceil(resolutionScale * size.m_width), std::ceil(resolutionScale * size.m_height)};
    if (scaledSize.m_width <= 0.0f || scaledSize.m_height <= 0.0f) return {};
    if (!IsWithinIntRange(scaledSize.m_width) || !IsWithinIntRange(scaledSize.m_height)) return {};
    WebCore::IntSize backendSize;
    _ZN7WebCore7IntSizeC1ERKNS_9FloatSizeE(&backendSize, scaledSize);
    std::uint64_t bytesPerRow = 4ull * static_cast<unsigned>(backendSize.m_width);
    if (bytesPerRow > std::numeric_limits<unsigned>::max()) return {};
    if (static_cast<unsigned>(backendSize.m_height) * bytesPerRow > std::numeric_limits<unsigned>::max()) return {};
    return backendSize;
}

void APS5_VABI _ZNK7WebCore18ImageBufferBackend15copyImagePixelsENS_22AlphaPremultiplicationENS_11ColorFormatEjPhS1_S2_jS3_RKNS_7IntSizeE(
    const WebCore::ImageBufferBackend* self,
    WebCore::AlphaPremultiplication srcAlphaFormat,
    WebCore::ColorFormat srcColorFormat,
    unsigned srcBytesPerRow,
    std::uint8_t* srcRows,
    WebCore::AlphaPremultiplication destAlphaFormat,
    WebCore::ColorFormat destColorFormat,
    unsigned destBytesPerRow,
    std::uint8_t* destRows,
    const WebCore::IntSize* size) {
    static_cast<void>(self);
    if (!IsValidAlpha(srcAlphaFormat) || !IsValidAlpha(destAlphaFormat) ||
        !IsValidColorFormat(srcColorFormat) || !IsValidColorFormat(destColorFormat)) {
        NotImplemented_nid_no_patch(__func__);
    }
    WebCore::IntSize guestSize{};
    if (!size || !CopyGuestInPreservingErrno(&guestSize, size, sizeof(guestSize))) {
        NotImplemented_nid_no_patch(__func__);
    }
    if (guestSize.m_height <= 0) return;
    if (guestSize.m_width < 0) NotImplemented_nid_no_patch(__func__);
    if (guestSize.m_width == 0) return;

    const std::size_t width = static_cast<std::size_t>(guestSize.m_width);
    const std::size_t height = static_cast<std::size_t>(guestSize.m_height);
    if (width > std::numeric_limits<std::size_t>::max() / 4u) {
        NotImplemented_nid_no_patch(__func__);
    }
    const std::size_t rowBytes = width * 4u;
    const std::size_t srcStride = srcBytesPerRow;
    const std::size_t destStride = destBytesPerRow;
    if (!RowRangeFits(srcRows, srcStride, height, rowBytes) ||
        !RowRangeFits(destRows, destStride, height, rowBytes)) {
        NotImplemented_nid_no_patch(__func__);
    }

    std::vector<std::uint8_t> srcRow(rowBytes);
    std::vector<std::uint8_t> destRow(rowBytes);
    for (std::size_t y = 0; y < height; ++y) {
        if (!CopyGuestInPreservingErrno(srcRow.data(), srcRows + y * srcStride, rowBytes) ||
            !IsGuestWritablePreservingErrno(destRows + y * destStride, rowBytes)) {
            NotImplemented_nid_no_patch(__func__);
        }
    }

    if (srcAlphaFormat == destAlphaFormat) {
        if (srcAlphaFormat == WebCore::AlphaPremultiplication::Premultiplied) {
            CopyImagePixelsUnaccelerated<CopyPremultipliedToPremultiplied>(
                srcColorFormat, srcStride, srcRows, destColorFormat, destStride, destRows, rowBytes, height, srcRow, destRow, __func__);
        } else {
            CopyImagePixelsUnaccelerated<CopyUnpremultipliedToUnpremultiplied>(
                srcColorFormat, srcStride, srcRows, destColorFormat, destStride, destRows, rowBytes, height, srcRow, destRow, __func__);
        }
        return;
    }

    if (destAlphaFormat == WebCore::AlphaPremultiplication::Unpremultiplied) {
        CopyImagePixelsUnaccelerated<CopyPremultipliedToUnpremultiplied>(
            srcColorFormat, srcStride, srcRows, destColorFormat, destStride, destRows, rowBytes, height, srcRow, destRow, __func__);
        return;
    }

    CopyImagePixelsUnaccelerated<CopyUnpremultipliedToPremultiplied>(
        srcColorFormat, srcStride, srcRows, destColorFormat, destStride, destRows, rowBytes, height, srcRow, destRow, __func__);
}

}
