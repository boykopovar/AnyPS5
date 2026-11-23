/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

/*
 * Copyright (c) 2011, The WebRTC project authors. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in
 *     the documentation and/or other materials provided with the
 *     distribution.
 *
 *   * Neither the name of Google nor the names of its contributors may
 *     be used to endorse or promote products derived from this software
 *     without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/* https://webrtc.googlesource.com/src/+/8d78f5de6c27b2c793039989ea381f1428fb0100/api/video/video_frame_buffer.cc */
/* https://webrtc.googlesource.com/src/+/8d78f5de6c27b2c793039989ea381f1428fb0100/api/video/i420_buffer.cc */

#include "prx/libwebrtc/libwebrtc.hpp"

#include "libyuv/scale.h"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>

namespace {

constexpr std::size_t ReleaseSlot = 1;
constexpr std::size_t WidthSlot = 5;
constexpr std::size_t HeightSlot = 6;
constexpr std::size_t ToI420Slot = 7;
constexpr std::size_t StrideYSlot = 14;
constexpr std::size_t StrideUSlot = 15;
constexpr std::size_t StrideVSlot = 16;
constexpr std::size_t DataYSlot = 17;
constexpr std::size_t DataUSlot = 18;
constexpr std::size_t DataVSlot = 19;
constexpr int MaximumCropDimension = 32768;

using ToI420 = void** (APS5_VABI*)(void** result, void* buffer);

void* Slot(const void* object, std::size_t slot) {
    return const_cast<void*>((*static_cast<const void* const* const*>(object))[slot]);
}

template<typename TResult>
TResult Call(const void* object, std::size_t slot) {
    return reinterpret_cast<TResult (APS5_VABI*)(const void*)>(Slot(object, slot))(object);
}

struct Reference {
    void* object = nullptr;

    ~Reference() {
        if (object) Call<std::int32_t>(object, ReleaseSlot);
    }
};

}  // namespace

extern "C" {

void** APS5_VABI _ZN6webrtc16VideoFrameBuffer12CropAndScaleEiiiiii_nid_postfix(
    void** result, WebRtcVideoFrameBuffer* buffer, int offsetX, int offsetY, int cropWidth, int cropHeight, int scaledWidth, int scaledHeight) {
    if (!result) throw std::invalid_argument("VideoFrameBuffer::CropAndScale requires a result");
    if (!buffer || !buffer->vtable) throw std::invalid_argument("VideoFrameBuffer::CropAndScale requires a buffer with a vtable");
    if (scaledWidth <= 0 || scaledHeight <= 0) throw std::invalid_argument("VideoFrameBuffer::CropAndScale requires a positive scaled size");
    const int chromaStride = scaledWidth / 2 + scaledWidth % 2;
    const std::int64_t chromaRows = (std::int64_t{scaledHeight} + 1) / 2;
    if (std::int64_t{scaledWidth} * scaledHeight + 2 * std::int64_t{chromaStride} * chromaRows > std::numeric_limits<int>::max())
        throw std::invalid_argument("VideoFrameBuffer::CropAndScale requires a scaled size whose planes fit in int");
    const std::int64_t rows = cropHeight < 0 ? -std::int64_t{cropHeight} : std::int64_t{cropHeight};
    if (cropWidth <= 0 || cropWidth > MaximumCropDimension || rows == 0 || rows > MaximumCropDimension)
        throw std::invalid_argument("VideoFrameBuffer::CropAndScale requires a crop size the scaler accepts");
    Reference source;
    reinterpret_cast<ToI420>(Slot(buffer, ToI420Slot))(&source.object, buffer);
    if (!source.object) throw std::invalid_argument("VideoFrameBuffer::CropAndScale requires a buffer that converts to I420");
    const std::int64_t sourceWidth = Call<std::int32_t>(source.object, WidthSlot);
    const std::int64_t sourceHeight = Call<std::int32_t>(source.object, HeightSlot);
    if (std::int64_t{cropWidth} + offsetX > sourceWidth || rows + offsetY > sourceHeight || offsetX < 0 || offsetY < 0)
        throw std::invalid_argument("VideoFrameBuffer::CropAndScale requires a crop rectangle inside the buffer");
    const int uvOffsetX = offsetX / 2;
    const int uvOffsetY = offsetY / 2;
    offsetX = uvOffsetX * 2;
    offsetY = uvOffsetY * 2;
    const int strideY = Call<std::int32_t>(source.object, StrideYSlot);
    const int strideU = Call<std::int32_t>(source.object, StrideUSlot);
    const int strideV = Call<std::int32_t>(source.object, StrideVSlot);
    const std::uint8_t* dataY = Call<const std::uint8_t*>(source.object, DataYSlot);
    const std::uint8_t* dataU = Call<const std::uint8_t*>(source.object, DataUSlot);
    const std::uint8_t* dataV = Call<const std::uint8_t*>(source.object, DataVSlot);
    if (!dataY || !dataU || !dataV) throw std::invalid_argument("VideoFrameBuffer::CropAndScale requires a buffer with three planes");
    const std::uint8_t* planeY = dataY + std::ptrdiff_t{strideY} * offsetY + offsetX;
    const std::uint8_t* planeU = dataU + std::ptrdiff_t{strideU} * uvOffsetY + uvOffsetX;
    const std::uint8_t* planeV = dataV + std::ptrdiff_t{strideV} * uvOffsetY + uvOffsetX;
    WebRtcRefCountedI420Buffer* created = nullptr;
    _ZN6webrtc10I420Buffer6CreateEiiiii_nid_postfix(&created, scaledWidth, scaledHeight, scaledWidth, chromaStride, chromaStride);
    Reference target{created};
    WebRtcI420Buffer& destination = created->buffer;
    if (libyuv::I420Scale(planeY, strideY, planeU, strideU, planeV, strideV, cropWidth, cropHeight,
                          _ZN6webrtc10I420Buffer12MutableDataYEv_nid_postfix(&destination), destination.strideY,
                          _ZN6webrtc10I420Buffer12MutableDataUEv_nid_postfix(&destination), destination.strideU,
                          _ZN6webrtc10I420Buffer12MutableDataVEv_nid_postfix(&destination), destination.strideV,
                          destination.width, destination.height, libyuv::kFilterBox) != 0)
        throw std::runtime_error("VideoFrameBuffer::CropAndScale could not scale the planes");
    target.object = nullptr;
    *result = created;
    return result;
}

}
