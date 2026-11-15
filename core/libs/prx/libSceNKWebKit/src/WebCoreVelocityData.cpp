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

namespace {

using NKWebKit::WtfAbi::TextStream;

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
    constexpr unsigned maxHistoryDepth = WebCore::HistoricalVelocityData::maxHistoryDepth;

    auto append = [&](WebCore::FloatPoint newPosition, double scale, WTF::MonotonicTime timestamp) {
        self->m_latestDataIndex = (self->m_latestDataIndex + 1) % maxHistoryDepth;
        self->m_positionHistory[self->m_latestDataIndex] = {timestamp, newPosition, scale};
        self->m_historySize = std::min(self->m_historySize + 1, maxHistoryDepth);
        self->m_lastAppendTimestamp = timestamp;
    };

    const double filteringThreshold = 1.0 / 60;

    WebCore::VelocityData velocityData;
    if (self->m_historySize > 0) {
        unsigned oldestDataIndex;
        unsigned distanceToLastHistoricalData = self->m_historySize - 1;
        if (distanceToLastHistoricalData <= self->m_latestDataIndex) {
            oldestDataIndex = self->m_latestDataIndex - distanceToLastHistoricalData;
        } else {
            oldestDataIndex = self->m_historySize - (distanceToLastHistoricalData - self->m_latestDataIndex);
        }

        double timeDelta = timestamp.m_value - self->m_positionHistory[oldestDataIndex].timestamp.m_value;
        if (timeDelta > filteringThreshold) {
            WebCore::HistoricalVelocityData::Data& oldestData = self->m_positionHistory[oldestDataIndex];
            velocityData = WebCore::VelocityData{
                static_cast<float>((newPosition.m_x - oldestData.position.m_x) / timeDelta),
                static_cast<float>((newPosition.m_y - oldestData.position.m_y) / timeDelta),
                static_cast<float>((scale - oldestData.scale) / timeDelta),
                timestamp};
        }
    }

    double timeSinceLastAppend = timestamp.m_value - self->m_lastAppendTimestamp.m_value;
    if (timeSinceLastAppend > filteringThreshold) {
        append(newPosition, scale, timestamp);
    } else {
        self->m_positionHistory[self->m_latestDataIndex] = {timestamp, newPosition, scale};
    }

    return velocityData;
}

NKWebKit::WtfAbi::TextStream& APS5_VABI _ZN7WebCorelsERN3WTF10TextStreamERKNS_12VelocityDataE(
    NKWebKit::WtfAbi::TextStream& ts, const WebCore::VelocityData& velocityData) {
    DumpProperty(ts, "timestamp", velocityData.lastUpdateTime.m_value);
    if (velocityData.horizontalVelocity) {
        DumpProperty(ts, "horizontalVelocity", velocityData.horizontalVelocity);
    }
    if (velocityData.verticalVelocity) {
        DumpProperty(ts, "verticalVelocity", velocityData.verticalVelocity);
    }
    if (velocityData.scaleChangeRate) {
        DumpProperty(ts, "scaleChangeRate", velocityData.scaleChangeRate);
    }
    return ts;
}

}
