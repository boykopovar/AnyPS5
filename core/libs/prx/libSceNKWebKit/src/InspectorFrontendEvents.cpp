// Derived from InspectorFrontendDispatchers.cpp generated from https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/JavaScriptCore/inspector/protocol by https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/JavaScriptCore/inspector/scripts/codegen/generate_cpp_frontend_dispatcher_implementation.py
// Derived from https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WTF/wtf/JSONValues.cpp
// Derived from https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WTF/wtf/text/StringBuilderJSON.cpp
/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 * Copyright (C) 2013-2016 Apple Inc. All rights reserved.
 * Copyright (C) 2014 University of Washington. All rights reserved.
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
/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 * Copyright (C) 2014 University of Washington. All rights reserved.
 * Copyright (C) 2017-2019 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
/*
 * Copyright (C) 2010-2018 Apple Inc. All rights reserved.
 * Copyright (C) 2012 Google Inc. All rights reserved.
 * Copyright (C) 2017 Yusuke Suzuki <utatane.tea@gmail.com>. All rights reserved.
 * Copyright (C) 2017 Mozilla Foundation. All rights reserved.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "prx/libSceNKWebKit/include/InspectorFrontendEventExports.hpp"
#include "prx/libSceNKWebKit/include/InspectorFrontendRouterExports.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"
#include "prx/libSceNKWebKit/src/WtfTextAbi.hpp"

#include <cmath>
#include <cstddef>
#include <initializer_list>
#include <stdexcept>
#include <string>

namespace {

using NKWebKit::InspectorAbi::FrontendDispatcher;
using NKWebKit::WtfAbi::String;
using NKWebKit::WtfAbi::StringImpl;

struct EnumValues {
    int first;
    std::initializer_list<const char*> names;
};

const EnumValues CustomElementStateValues = {112, {"builtin", "custom", "waiting", "failed"}};
const EnumValues InitiatorValues = {222, {"frontend", "console", "auto-capture"}};

char LowercaseHexDigit(unsigned value) {
    return "0123456789abcdef"[value & 0xF];
}

class EventMessage {
public:
    EventMessage(const FrontendDispatcher* dispatcher, const char* method) : m_dispatcher(dispatcher) {
        if (!dispatcher) throw std::invalid_argument("Inspector frontend dispatcher event requires a dispatcher");
        m_output.push_back(u'{');
        AppendKey("method");
        AppendQuotedAscii(method);
    }

    void AddString(const char* key, const NKWebKit::WtfAbi::String* value) {
        if (!value) throw std::invalid_argument("Inspector frontend dispatcher event requires a string argument");
        AppendParameterKey(key);
        AppendQuotedString(value->impl);
    }

    void AddInteger(const char* key, int value) {
        AppendParameterKey(key);
        AppendNumber(static_cast<double>(value));
    }

    void AddDouble(const char* key, double value) {
        AppendParameterKey(key);
        AppendNumber(value);
    }

    void AddBoolean(const char* key, bool value) {
        AppendParameterKey(key);
        AppendAscii(value ? "true" : "false");
    }

    void AddEnum(const char* key, int value, const EnumValues& values) {
        const int index = value - values.first;
        if (index < 0 || index >= static_cast<int>(values.names.size())) throw std::out_of_range("Inspector protocol enum value is out of range");
        AppendParameterKey(key);
        AppendQuotedAscii(values.names.begin()[index]);
    }

    void Send() {
        if (m_hasParameters) m_output.push_back(u'}');
        m_output.push_back(u'}');
        NKWebKit::WtfAbi::String message = m_is8Bit ? MakeLatin1() : _ZN3WTF10StringImpl6createEPKDsj(m_output.data(), static_cast<unsigned>(m_output.size()));
        _ZNK9Inspector14FrontendRouter9sendEventERKN3WTF6StringE(m_dispatcher->frontendRouter, &message);
    }

private:
    NKWebKit::WtfAbi::String MakeLatin1() const {
        std::string bytes(m_output.begin(), m_output.end());
        return _ZN3WTF10StringImpl6createEPKhj(reinterpret_cast<const unsigned char*>(bytes.data()), static_cast<unsigned>(bytes.size()));
    }

    void AppendAscii(const char* text) {
        while (*text) m_output.push_back(static_cast<unsigned char>(*text++));
    }

    void AppendQuotedAscii(const char* text) {
        m_output.push_back(u'"');
        while (*text) AppendEscaped(static_cast<unsigned char>(*text++), nullptr, nullptr);
        m_output.push_back(u'"');
    }

    void AppendKey(const char* key) {
        AppendQuotedAscii(key);
        m_output.push_back(u':');
    }

    void AppendParameterKey(const char* key) {
        if (!m_hasParameters) {
            m_output.push_back(u',');
            AppendKey("params");
            m_output.push_back(u'{');
            m_hasParameters = true;
        } else {
            m_output.push_back(u',');
        }
        AppendKey(key);
    }

    void AppendNumber(double value) {
        if (!std::isfinite(value)) {
            AppendAscii("null");
            return;
        }
        AppendAscii(NKWebKit::WtfAbi::NumberToString(value).c_str());
    }

    void AppendEscaped(char16_t character, const char16_t** cursor, const char16_t* end) {
        if (character <= 0xFF) {
            char escaped = 0;
            switch (character) {
            case '\b': escaped = 'b'; break;
            case '\t': escaped = 't'; break;
            case '\n': escaped = 'n'; break;
            case '\f': escaped = 'f'; break;
            case '\r': escaped = 'r'; break;
            case '"': escaped = '"'; break;
            case '\\': escaped = '\\'; break;
            default:
                if (character < 0x20) escaped = 'u';
                break;
            }
            if (!escaped) {
                m_output.push_back(character);
                return;
            }
            m_output.push_back(u'\\');
            m_output.push_back(static_cast<char16_t>(escaped));
            if (escaped == 'u') {
                m_output.push_back(u'0');
                m_output.push_back(u'0');
                m_output.push_back(static_cast<char16_t>(LowercaseHexDigit(character >> 4)));
                m_output.push_back(static_cast<char16_t>(LowercaseHexDigit(character)));
            }
            return;
        }
        const bool isSurrogate = (character & 0xF800) == 0xD800;
        if (!isSurrogate) {
            m_output.push_back(character);
            return;
        }
        const bool isLead = (character & 0xFC00) == 0xD800;
        if (isLead && cursor && *cursor + 1 != end && ((*cursor)[1] & 0xFC00) == 0xDC00) {
            m_output.push_back(character);
            m_output.push_back((*cursor)[1]);
            ++*cursor;
            return;
        }
        m_output.push_back(u'\\');
        m_output.push_back(u'u');
        for (int shift = 12; shift >= 0; shift -= 4) m_output.push_back(static_cast<char16_t>(LowercaseHexDigit(character >> shift)));
    }

    void AppendQuotedString(const StringImpl* string) {
        m_output.push_back(u'"');
        if (string) {
            if (string->hashAndFlags & 4) {
                const auto* characters = static_cast<const unsigned char*>(string->data);
                for (unsigned i = 0; i < string->length; ++i) AppendEscaped(characters[i], nullptr, nullptr);
            } else {
                m_is8Bit = false;
                const auto* characters = static_cast<const char16_t*>(string->data);
                const char16_t* end = characters + string->length;
                for (const char16_t* cursor = characters; cursor != end; ++cursor) AppendEscaped(*cursor, &cursor, end);
            }
        }
        m_output.push_back(u'"');
    }

    const FrontendDispatcher* m_dispatcher;
    std::u16string m_output;
    bool m_hasParameters = false;
    bool m_is8Bit = true;
};

}  // namespace

extern "C" {

void APS5_VABI _ZN9Inspector21CSSFrontendDispatcher17styleSheetChangedERKN3WTF6StringE(const FrontendDispatcher* self, const NKWebKit::WtfAbi::String* styleSheetId) {
    EventMessage event(self, "CSS.styleSheetChanged");
    event.AddString("styleSheetId", styleSheetId);
    event.Send();
}

void APS5_VABI _ZN9Inspector21CSSFrontendDispatcher17styleSheetRemovedERKN3WTF6StringE(const FrontendDispatcher* self, const NKWebKit::WtfAbi::String* styleSheetId) {
    EventMessage event(self, "CSS.styleSheetRemoved");
    event.AddString("styleSheetId", styleSheetId);
    event.Send();
}

void APS5_VABI _ZN9Inspector21CSSFrontendDispatcher23mediaQueryResultChangedEv(const FrontendDispatcher* self) {
    EventMessage event(self, "CSS.mediaQueryResultChanged");
    event.Send();
}

void APS5_VABI _ZN9Inspector21DOMFrontendDispatcher15documentUpdatedEv(const FrontendDispatcher* self) {
    EventMessage event(self, "DOM.documentUpdated");
    event.Send();
}

void APS5_VABI _ZN9Inspector21DOMFrontendDispatcher16attributeRemovedEiRKN3WTF6StringE(const FrontendDispatcher* self, int nodeId, const NKWebKit::WtfAbi::String* name) {
    EventMessage event(self, "DOM.attributeRemoved");
    event.AddInteger("nodeId", nodeId);
    event.AddString("name", name);
    event.Send();
}

void APS5_VABI _ZN9Inspector21DOMFrontendDispatcher16childNodeRemovedEii(const FrontendDispatcher* self, int parentNodeId, int nodeId) {
    EventMessage event(self, "DOM.childNodeRemoved");
    event.AddInteger("parentNodeId", parentNodeId);
    event.AddInteger("nodeId", nodeId);
    event.Send();
}

void APS5_VABI _ZN9Inspector21DOMFrontendDispatcher16shadowRootPoppedEii(const FrontendDispatcher* self, int hostId, int rootId) {
    EventMessage event(self, "DOM.shadowRootPopped");
    event.AddInteger("hostId", hostId);
    event.AddInteger("rootId", rootId);
    event.Send();
}

void APS5_VABI _ZN9Inspector21DOMFrontendDispatcher17attributeModifiedEiRKN3WTF6StringES4_(const FrontendDispatcher* self, int nodeId, const NKWebKit::WtfAbi::String* name, const NKWebKit::WtfAbi::String* value) {
    EventMessage event(self, "DOM.attributeModified");
    event.AddInteger("nodeId", nodeId);
    event.AddString("name", name);
    event.AddString("value", value);
    event.Send();
}

void APS5_VABI _ZN9Inspector21DOMFrontendDispatcher19didAddEventListenerEi(const FrontendDispatcher* self, int nodeId) {
    EventMessage event(self, "DOM.didAddEventListener");
    event.AddInteger("nodeId", nodeId);
    event.Send();
}

void APS5_VABI _ZN9Inspector21DOMFrontendDispatcher20pseudoElementRemovedEii(const FrontendDispatcher* self, int parentId, int pseudoElementId) {
    EventMessage event(self, "DOM.pseudoElementRemoved");
    event.AddInteger("parentId", parentId);
    event.AddInteger("pseudoElementId", pseudoElementId);
    event.Send();
}

void APS5_VABI _ZN9Inspector21DOMFrontendDispatcher21characterDataModifiedEiRKN3WTF6StringE(const FrontendDispatcher* self, int nodeId, const NKWebKit::WtfAbi::String* characterData) {
    EventMessage event(self, "DOM.characterDataModified");
    event.AddInteger("nodeId", nodeId);
    event.AddString("characterData", characterData);
    event.Send();
}

void APS5_VABI _ZN9Inspector21DOMFrontendDispatcher21childNodeCountUpdatedEii(const FrontendDispatcher* self, int nodeId, int childNodeCount) {
    EventMessage event(self, "DOM.childNodeCountUpdated");
    event.AddInteger("nodeId", nodeId);
    event.AddInteger("childNodeCount", childNodeCount);
    event.Send();
}

void APS5_VABI _ZN9Inspector21DOMFrontendDispatcher23willRemoveEventListenerEi(const FrontendDispatcher* self, int nodeId) {
    EventMessage event(self, "DOM.willRemoveEventListener");
    event.AddInteger("nodeId", nodeId);
    event.Send();
}

void APS5_VABI _ZN9Inspector21DOMFrontendDispatcher25customElementStateChangedEiNS_8Protocol3DOM18CustomElementStateE(const FrontendDispatcher* self, int nodeId, int customElementState) {
    EventMessage event(self, "DOM.customElementStateChanged");
    event.AddInteger("nodeId", nodeId);
    event.AddEnum("customElementState", customElementState, CustomElementStateValues);
    event.Send();
}

void APS5_VABI _ZN9Inspector21DOMFrontendDispatcher34powerEfficientPlaybackStateChangedEidb(const FrontendDispatcher* self, int nodeId, double timestamp, bool isPowerEfficient) {
    EventMessage event(self, "DOM.powerEfficientPlaybackStateChanged");
    event.AddInteger("nodeId", nodeId);
    event.AddDouble("timestamp", timestamp);
    event.AddBoolean("isPowerEfficient", isPowerEfficient);
    event.Send();
}

void APS5_VABI _ZN9Inspector21DOMFrontendDispatcher7inspectEi(const FrontendDispatcher* self, int nodeId) {
    EventMessage event(self, "DOM.inspect");
    event.AddInteger("nodeId", nodeId);
    event.Send();
}

void APS5_VABI _ZN9Inspector22HeapFrontendDispatcher16trackingCompleteEdRKN3WTF6StringE(const FrontendDispatcher* self, double timestamp, const NKWebKit::WtfAbi::String* snapshotData) {
    EventMessage event(self, "Heap.trackingComplete");
    event.AddDouble("timestamp", timestamp);
    event.AddString("snapshotData", snapshotData);
    event.Send();
}

void APS5_VABI _ZN9Inspector22PageFrontendDispatcher13frameDetachedERKN3WTF6StringE(const FrontendDispatcher* self, const NKWebKit::WtfAbi::String* frameId) {
    EventMessage event(self, "Page.frameDetached");
    event.AddString("frameId", frameId);
    event.Send();
}

void APS5_VABI _ZN9Inspector22PageFrontendDispatcher14loadEventFiredEd(const FrontendDispatcher* self, double timestamp) {
    EventMessage event(self, "Page.loadEventFired");
    event.AddDouble("timestamp", timestamp);
    event.Send();
}

void APS5_VABI _ZN9Inspector22PageFrontendDispatcher19frameStartedLoadingERKN3WTF6StringE(const FrontendDispatcher* self, const NKWebKit::WtfAbi::String* frameId) {
    EventMessage event(self, "Page.frameStartedLoading");
    event.AddString("frameId", frameId);
    event.Send();
}

void APS5_VABI _ZN9Inspector22PageFrontendDispatcher19frameStoppedLoadingERKN3WTF6StringE(const FrontendDispatcher* self, const NKWebKit::WtfAbi::String* frameId) {
    EventMessage event(self, "Page.frameStoppedLoading");
    event.AddString("frameId", frameId);
    event.Send();
}

void APS5_VABI _ZN9Inspector22PageFrontendDispatcher20domContentEventFiredEd(const FrontendDispatcher* self, double timestamp) {
    EventMessage event(self, "Page.domContentEventFired");
    event.AddDouble("timestamp", timestamp);
    event.Send();
}

void APS5_VABI _ZN9Inspector22PageFrontendDispatcher24frameScheduledNavigationERKN3WTF6StringEd(const FrontendDispatcher* self, const NKWebKit::WtfAbi::String* frameId, double delay) {
    EventMessage event(self, "Page.frameScheduledNavigation");
    event.AddString("frameId", frameId);
    event.AddDouble("delay", delay);
    event.Send();
}

void APS5_VABI _ZN9Inspector24CanvasFrontendDispatcher13canvasRemovedERKN3WTF6StringE(const FrontendDispatcher* self, const NKWebKit::WtfAbi::String* canvasId) {
    EventMessage event(self, "Canvas.canvasRemoved");
    event.AddString("canvasId", canvasId);
    event.Send();
}

void APS5_VABI _ZN9Inspector24CanvasFrontendDispatcher16extensionEnabledERKN3WTF6StringES4_(const FrontendDispatcher* self, const NKWebKit::WtfAbi::String* canvasId, const NKWebKit::WtfAbi::String* extension) {
    EventMessage event(self, "Canvas.extensionEnabled");
    event.AddString("canvasId", canvasId);
    event.AddString("extension", extension);
    event.Send();
}

void APS5_VABI _ZN9Inspector24CanvasFrontendDispatcher16recordingStartedERKN3WTF6StringENS_8Protocol9Recording9InitiatorE(const FrontendDispatcher* self, const NKWebKit::WtfAbi::String* canvasId, int initiator) {
    EventMessage event(self, "Canvas.recordingStarted");
    event.AddString("canvasId", canvasId);
    event.AddEnum("initiator", initiator, InitiatorValues);
    event.Send();
}

void APS5_VABI _ZN9Inspector24CanvasFrontendDispatcher18clientNodesChangedERKN3WTF6StringE(const FrontendDispatcher* self, const NKWebKit::WtfAbi::String* canvasId) {
    EventMessage event(self, "Canvas.clientNodesChanged");
    event.AddString("canvasId", canvasId);
    event.Send();
}

void APS5_VABI _ZN9Inspector24CanvasFrontendDispatcher19canvasMemoryChangedERKN3WTF6StringEd(const FrontendDispatcher* self, const NKWebKit::WtfAbi::String* canvasId, double memoryCost) {
    EventMessage event(self, "Canvas.canvasMemoryChanged");
    event.AddString("canvasId", canvasId);
    event.AddDouble("memoryCost", memoryCost);
    event.Send();
}

void APS5_VABI _ZN9Inspector24TargetFrontendDispatcher15targetDestroyedERKN3WTF6StringE(const FrontendDispatcher* self, const NKWebKit::WtfAbi::String* targetId) {
    EventMessage event(self, "Target.targetDestroyed");
    event.AddString("targetId", targetId);
    event.Send();
}

void APS5_VABI _ZN9Inspector24TargetFrontendDispatcher25dispatchMessageFromTargetERKN3WTF6StringES4_(const FrontendDispatcher* self, const NKWebKit::WtfAbi::String* targetId, const NKWebKit::WtfAbi::String* message) {
    EventMessage event(self, "Target.dispatchMessageFromTarget");
    event.AddString("targetId", targetId);
    event.AddString("message", message);
    event.Send();
}

void APS5_VABI _ZN9Inspector24TargetFrontendDispatcher26didCommitProvisionalTargetERKN3WTF6StringES4_(const FrontendDispatcher* self, const NKWebKit::WtfAbi::String* oldTargetId, const NKWebKit::WtfAbi::String* newTargetId) {
    EventMessage event(self, "Target.didCommitProvisionalTarget");
    event.AddString("oldTargetId", oldTargetId);
    event.AddString("newTargetId", newTargetId);
    event.Send();
}

void APS5_VABI _ZN9Inspector24WorkerFrontendDispatcher13workerCreatedERKN3WTF6StringES4_S4_(const FrontendDispatcher* self, const NKWebKit::WtfAbi::String* workerId, const NKWebKit::WtfAbi::String* url, const NKWebKit::WtfAbi::String* name) {
    EventMessage event(self, "Worker.workerCreated");
    event.AddString("workerId", workerId);
    event.AddString("url", url);
    event.AddString("name", name);
    event.Send();
}

void APS5_VABI _ZN9Inspector24WorkerFrontendDispatcher16workerTerminatedERKN3WTF6StringE(const FrontendDispatcher* self, const NKWebKit::WtfAbi::String* workerId) {
    EventMessage event(self, "Worker.workerTerminated");
    event.AddString("workerId", workerId);
    event.Send();
}

void APS5_VABI _ZN9Inspector24WorkerFrontendDispatcher25dispatchMessageFromWorkerERKN3WTF6StringES4_(const FrontendDispatcher* self, const NKWebKit::WtfAbi::String* workerId, const NKWebKit::WtfAbi::String* message) {
    EventMessage event(self, "Worker.dispatchMessageFromWorker");
    event.AddString("workerId", workerId);
    event.AddString("message", message);
    event.Send();
}

void APS5_VABI _ZN9Inspector25NetworkFrontendDispatcher12dataReceivedERKN3WTF6StringEdii(const FrontendDispatcher* self, const NKWebKit::WtfAbi::String* requestId, double timestamp, int dataLength, int encodedDataLength) {
    EventMessage event(self, "Network.dataReceived");
    event.AddString("requestId", requestId);
    event.AddDouble("timestamp", timestamp);
    event.AddInteger("dataLength", dataLength);
    event.AddInteger("encodedDataLength", encodedDataLength);
    event.Send();
}

void APS5_VABI _ZN9Inspector25NetworkFrontendDispatcher15webSocketClosedERKN3WTF6StringEd(const FrontendDispatcher* self, const NKWebKit::WtfAbi::String* requestId, double timestamp) {
    EventMessage event(self, "Network.webSocketClosed");
    event.AddString("requestId", requestId);
    event.AddDouble("timestamp", timestamp);
    event.Send();
}

void APS5_VABI _ZN9Inspector25NetworkFrontendDispatcher16webSocketCreatedERKN3WTF6StringES4_(const FrontendDispatcher* self, const NKWebKit::WtfAbi::String* requestId, const NKWebKit::WtfAbi::String* url) {
    EventMessage event(self, "Network.webSocketCreated");
    event.AddString("requestId", requestId);
    event.AddString("url", url);
    event.Send();
}

void APS5_VABI _ZN9Inspector25NetworkFrontendDispatcher19webSocketFrameErrorERKN3WTF6StringEdS4_(const FrontendDispatcher* self, const NKWebKit::WtfAbi::String* requestId, double timestamp, const NKWebKit::WtfAbi::String* errorMessage) {
    EventMessage event(self, "Network.webSocketFrameError");
    event.AddString("requestId", requestId);
    event.AddDouble("timestamp", timestamp);
    event.AddString("errorMessage", errorMessage);
    event.Send();
}

void APS5_VABI _ZN9Inspector26DebuggerFrontendDispatcher19globalObjectClearedEv(const FrontendDispatcher* self) {
    EventMessage event(self, "Debugger.globalObjectCleared");
    event.Send();
}

void APS5_VABI _ZN9Inspector26DebuggerFrontendDispatcher19scriptFailedToParseERKN3WTF6StringES4_iiS4_(const FrontendDispatcher* self, const NKWebKit::WtfAbi::String* url, const NKWebKit::WtfAbi::String* scriptSource, int startLine, int errorLine, const NKWebKit::WtfAbi::String* errorMessage) {
    EventMessage event(self, "Debugger.scriptFailedToParse");
    event.AddString("url", url);
    event.AddString("scriptSource", scriptSource);
    event.AddInteger("startLine", startLine);
    event.AddInteger("errorLine", errorLine);
    event.AddString("errorMessage", errorMessage);
    event.Send();
}

void APS5_VABI _ZN9Inspector26DebuggerFrontendDispatcher25playBreakpointActionSoundEi(const FrontendDispatcher* self, int breakpointActionId) {
    EventMessage event(self, "Debugger.playBreakpointActionSound");
    event.AddInteger("breakpointActionId", breakpointActionId);
    event.Send();
}

void APS5_VABI _ZN9Inspector26DebuggerFrontendDispatcher7resumedEv(const FrontendDispatcher* self) {
    EventMessage event(self, "Debugger.resumed");
    event.Send();
}

void APS5_VABI _ZN9Inspector26TimelineFrontendDispatcher16recordingStartedEd(const FrontendDispatcher* self, double startTime) {
    EventMessage event(self, "Timeline.recordingStarted");
    event.AddDouble("startTime", startTime);
    event.Send();
}

void APS5_VABI _ZN9Inspector26TimelineFrontendDispatcher16recordingStoppedEd(const FrontendDispatcher* self, double endTime) {
    EventMessage event(self, "Timeline.recordingStopped");
    event.AddDouble("endTime", endTime);
    event.Send();
}

void APS5_VABI _ZN9Inspector26TimelineFrontendDispatcher18autoCaptureStartedEv(const FrontendDispatcher* self) {
    EventMessage event(self, "Timeline.autoCaptureStarted");
    event.Send();
}

void APS5_VABI _ZN9Inspector27AnimationFrontendDispatcher13targetChangedERKN3WTF6StringE(const FrontendDispatcher* self, const NKWebKit::WtfAbi::String* animationId) {
    EventMessage event(self, "Animation.targetChanged");
    event.AddString("animationId", animationId);
    event.Send();
}

void APS5_VABI _ZN9Inspector27AnimationFrontendDispatcher13trackingStartEd(const FrontendDispatcher* self, double timestamp) {
    EventMessage event(self, "Animation.trackingStart");
    event.AddDouble("timestamp", timestamp);
    event.Send();
}

void APS5_VABI _ZN9Inspector27AnimationFrontendDispatcher16trackingCompleteEd(const FrontendDispatcher* self, double timestamp) {
    EventMessage event(self, "Animation.trackingComplete");
    event.AddDouble("timestamp", timestamp);
    event.Send();
}

void APS5_VABI _ZN9Inspector27AnimationFrontendDispatcher18animationDestroyedERKN3WTF6StringE(const FrontendDispatcher* self, const NKWebKit::WtfAbi::String* animationId) {
    EventMessage event(self, "Animation.animationDestroyed");
    event.AddString("animationId", animationId);
    event.Send();
}

void APS5_VABI _ZN9Inspector27InspectorFrontendDispatcher25evaluateForTestInFrontendERKN3WTF6StringE(const FrontendDispatcher* self, const NKWebKit::WtfAbi::String* script) {
    EventMessage event(self, "Inspector.evaluateForTestInFrontend");
    event.AddString("script", script);
    event.Send();
}

void APS5_VABI _ZN9Inspector27LayerTreeFrontendDispatcher18layerTreeDidChangeEv(const FrontendDispatcher* self) {
    EventMessage event(self, "LayerTree.layerTreeDidChange");
    event.Send();
}

void APS5_VABI _ZN9Inspector32ScriptProfilerFrontendDispatcher13trackingStartEd(const FrontendDispatcher* self, double timestamp) {
    EventMessage event(self, "ScriptProfiler.trackingStart");
    event.AddDouble("timestamp", timestamp);
    event.Send();
}

void APS5_VABI _ZN9Inspector34ApplicationCacheFrontendDispatcher19networkStateUpdatedEb(const FrontendDispatcher* self, bool isNowOnline) {
    EventMessage event(self, "ApplicationCache.networkStateUpdated");
    event.AddBoolean("isNowOnline", isNowOnline);
    event.Send();
}

void APS5_VABI _ZN9Inspector34ApplicationCacheFrontendDispatcher29applicationCacheStatusUpdatedERKN3WTF6StringES4_i(const FrontendDispatcher* self, const NKWebKit::WtfAbi::String* frameId, const NKWebKit::WtfAbi::String* manifestURL, int status) {
    EventMessage event(self, "ApplicationCache.applicationCacheStatusUpdated");
    event.AddString("frameId", frameId);
    event.AddString("manifestURL", manifestURL);
    event.AddInteger("status", status);
    event.Send();
}
}
