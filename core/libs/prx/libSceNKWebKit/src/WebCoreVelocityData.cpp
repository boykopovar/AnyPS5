// Derived from Source/WebCore/platform/graphics/VelocityData.cpp@c55364d53f971f63a709976f168d932d558b8909
/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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

#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/include/WebCoreDumpExports.hpp"
#include "prx/libSceNKWebKit/include/WebCoreExports.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"

#include <algorithm>

#include "prx/libc/include/General.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libSceNKWebKit/src/WtfTextAbi.hpp"
#include <cstddef>
#include <stdexcept>
#include <type_traits>

namespace {

using NKWebKit::WtfAbi::TextStream;

static_assert(std::is_trivially_copyable_v<WebCore::HistoricalVelocityData>);
static_assert(std::is_trivially_copyable_v<WebCore::VelocityData>);

template<typename T>
T ReadGuestValue(const T* pointer, const char* function) {
    static_assert(std::is_trivially_copyable_v<T>);
    T value{};
    if (!GuestCopyIn_nid_no_patch(&value, pointer, sizeof(T)))
        NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    return value;
}

void RequireReadableGuestBytes(const void* pointer, std::size_t size, const char* function) {
    if (!size) return;
    if (!pointer) NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    GuestArena::GuestArenaPrepareHostRead_nid_no_patch(pointer, size);
    if (!GuestReadable_nid_no_patch(pointer, size))
        NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
}

void RequireWritableGuestBytes(void* pointer, std::size_t size, const char* function) {
    if (!size) return;
    if (!pointer) NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    bool ok = false;
    {
        const GuestArena::HostWrite guard(pointer, size);
        ok = guard.Open() && GuestWritable_nid_no_patch(pointer, size);
    }
    if (!ok) NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
}

template<typename T>
bool TryPublishGuestValue(T* pointer, const T& value, const T& previous) {
    if (GuestCopyOut_nid_no_patch(pointer, &value, sizeof(T))) return true;
    static_cast<void>(GuestCopyOut_nid_no_patch(pointer, &previous, sizeof(T)));
    return false;
}

void StartProperty(TextStream& ts, const char* name) {
    _ZN3WTF10TextStream10startGroupEv(&ts);
    _ZN3WTF10TextStreamlsEPKc(&ts, name);
    _ZN3WTF10TextStreamlsEPKc(&ts, " ");
}

void DumpProperty(TextStream& ts, const char* name, double value) {
    StartProperty(ts, name);
    _ZN3WTF10TextStreamlsEd(&ts, value);
    _ZN3WTF10TextStream8endGroupEv(&ts);
}

void DumpProperty(TextStream& ts, const char* name, float value) {
    StartProperty(ts, name);
    _ZN3WTF10TextStreamlsEf(&ts, value);
    _ZN3WTF10TextStream8endGroupEv(&ts);
}

}  // namespace

extern "C" {

WebCore::VelocityData APS5_VABI _ZN7WebCore22HistoricalVelocityData18velocityForNewDataENS_10FloatPointEdN3WTF13MonotonicTimeE(
    WebCore::HistoricalVelocityData* self, WebCore::FloatPoint newPosition, double scale, WTF::MonotonicTime timestamp) {
    if (!self) throw std::invalid_argument("WebCore::HistoricalVelocityData::velocityForNewData: null this");
    RequireReadableGuestBytes(self, sizeof(WebCore::HistoricalVelocityData), __func__);
    RequireWritableGuestBytes(self, sizeof(WebCore::HistoricalVelocityData), __func__);
    const WebCore::HistoricalVelocityData previous = ReadGuestValue(self, __func__);
    constexpr unsigned maxHistoryDepth = WebCore::HistoricalVelocityData::maxHistoryDepth;
    if (previous.m_historySize > maxHistoryDepth || previous.m_latestDataIndex >= maxHistoryDepth)
        throw std::invalid_argument("WebCore::HistoricalVelocityData::velocityForNewData: invalid history state");
    WebCore::HistoricalVelocityData updated = previous;

    auto append = [&](WebCore::FloatPoint pos, double s, WTF::MonotonicTime time) {
        updated.m_latestDataIndex = (updated.m_latestDataIndex + 1) % maxHistoryDepth;
        updated.m_positionHistory[updated.m_latestDataIndex] = {time, pos, s};
        updated.m_historySize = std::min(updated.m_historySize + 1, maxHistoryDepth);
        updated.m_lastAppendTimestamp = time;
    };

    const double filteringThreshold = 1.0 / 60;

    WebCore::VelocityData velocityData;
    if (updated.m_historySize > 0) {
        unsigned oldestDataIndex;
        unsigned distanceToLastHistoricalData = updated.m_historySize - 1;
        if (distanceToLastHistoricalData <= updated.m_latestDataIndex) {
            oldestDataIndex = updated.m_latestDataIndex - distanceToLastHistoricalData;
        } else {
            oldestDataIndex = updated.m_historySize - (distanceToLastHistoricalData - updated.m_latestDataIndex);
        }

        double timeDelta = timestamp.m_value - updated.m_positionHistory[oldestDataIndex].timestamp.m_value;
        if (timeDelta > filteringThreshold) {
            const WebCore::HistoricalVelocityData::Data& oldestData = updated.m_positionHistory[oldestDataIndex];
            velocityData = WebCore::VelocityData{
                static_cast<float>((newPosition.m_x - oldestData.position.m_x) / timeDelta),
                static_cast<float>((newPosition.m_y - oldestData.position.m_y) / timeDelta),
                static_cast<float>((scale - oldestData.scale) / timeDelta),
                timestamp};
        }
    }

    double timeSinceLastAppend = timestamp.m_value - updated.m_lastAppendTimestamp.m_value;
    if (timeSinceLastAppend > filteringThreshold) {
        append(newPosition, scale, timestamp);
    } else {
        updated.m_positionHistory[updated.m_latestDataIndex] = {timestamp, newPosition, scale};
    }

    if (!TryPublishGuestValue(self, updated, previous))
        NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
    return velocityData;
}

NKWebKit::WtfAbi::TextStream& APS5_VABI _ZN7WebCorelsERN3WTF10TextStreamERKNS_12VelocityDataE(
    NKWebKit::WtfAbi::TextStream& ts, const WebCore::VelocityData& velocityData) {
    RequireReadableGuestBytes(&ts, sizeof(TextStream), __func__);
    RequireWritableGuestBytes(&ts, sizeof(TextStream), __func__);
    const WebCore::VelocityData data = ReadGuestValue(&velocityData, __func__);
    DumpProperty(ts, "timestamp", data.lastUpdateTime.m_value);
    if (data.horizontalVelocity) {
        DumpProperty(ts, "horizontalVelocity", data.horizontalVelocity);
    }
    if (data.verticalVelocity) {
        DumpProperty(ts, "verticalVelocity", data.verticalVelocity);
    }
    if (data.scaleChangeRate) {
        DumpProperty(ts, "scaleChangeRate", data.scaleChangeRate);
    }
    return ts;
}

}
