// Derived from InspectorBackendDispatchers.cpp generated from https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/JavaScriptCore/inspector/protocol by https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/JavaScriptCore/inspector/scripts/codegen/generate_cpp_backend_dispatcher_implementation.py
// Derived from https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/JavaScriptCore/inspector/InspectorBackendDispatcher.cpp
/*
 * Copyright (C) 2013-2020 Apple Inc. All rights reserved.
 * Copyright (C) 2011 The Chromium Authors. All rights reserved.
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

#include "prx/libSceNKWebKit/include/InspectorDomainDispatcherExports.hpp"
#include "prx/libSceNKWebKit/include/InspectorFrontendRouterExports.hpp"
#include "prx/libSceNKWebKit/include/JsonExports.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"
#include "prx/libSceNKWebKit/src/WtfStringAbi.hpp"
#include "prx/libc/include/Api.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <initializer_list>
#include <iterator>
#include <stdexcept>
#include <string>
#include <utility>

namespace {

using NKWebKit::InspectorAbi::BackendDispatcher;
using NKWebKit::InspectorAbi::CallbackBase;
using NKWebKit::InspectorAbi::CommonErrorCode;
using NKWebKit::InspectorAbi::DomainBackendDispatcher;
using NKWebKit::InspectorAbi::DomainBackendDispatcherRef;
using NKWebKit::InspectorAbi::SupplementalBackendDispatcher;
using NKWebKit::InspectorAbi::SupplementalBackendDispatcherVtable;
using NKWebKit::JsonAbi::Object;
using NKWebKit::JsonAbi::Ref;
using NKWebKit::JsonAbi::RefPtr;
using NKWebKit::JsonAbi::Value;
using NKWebKit::JsonAbi::ValueType;
using NKWebKit::WtfAbi::String;
using NKWebKit::WtfAbi::StringImpl;

enum class ParamKind : unsigned char {
    String,
    Integer,
    Boolean,
    OptionalBoolean,
    OptionalInteger,
    RefObject,
    RefPtrObject,
    RefArray,
    RefPtrArray,
    Enum,
    OptionalEnum,
    Callback,
};

enum class Getter : unsigned char {
    Boolean,
    Integer,
    String,
    Object,
    Array,
};

enum class OutKind : unsigned char {
    String,
    Integer,
    Boolean,
    Double,
    RefValue,
    RefPtrValue,
    OptionalBoolean,
    OptionalInteger,
    Enum,
};

struct EnumValue {
    unsigned short value;
    const char* name;
};

struct EnumList {
    const EnumValue* values;
    std::size_t count;
};

struct ParamSpec {
    const char* key;
    ParamKind kind;
    Getter getter;
    bool required;
    int enumList;
};

struct OutSpec {
    const char* key;
    OutKind kind;
    bool conditional;
    int enumList;
};

struct CommandSpec {
    const char* name;
    unsigned slot;
    const ParamSpec* params;
    std::size_t paramCount;
    const OutSpec* outs;
    std::size_t outCount;
    bool async;
};

struct DomainSpec {
    const char* domain;
    const char* dispatch;
    const char* constructor;
    const char* destructor;
    const CommandSpec* commands;
    std::size_t commandCount;
};

const EnumValue EnumValues0[] = {{71, "xml"}, {72, "javascript"}, {73, "network"}, {74, "console-api"}, {75, "storage"}, {76, "appcache"}, {77, "rendering"}, {78, "css"}, {79, "security"}, {80, "content-blocker"}, {81, "media"}, {82, "mediasource"}, {83, "webrtc"}, {84, "itp-debug"}, {85, "private-click-measurement"}, {86, "payment-request"}, {87, "other"}};
const EnumValue EnumValues1[] = {{88, "off"}, {89, "basic"}, {90, "verbose"}};
const EnumValue EnumValues2[] = {{58, "observed"}, {59, "all"}};
const EnumValue EnumValues3[] = {{131, "subtree-modified"}, {132, "attribute-modified"}, {133, "node-removed"}};
const EnumValue EnumValues4[] = {{134, "animation-frame"}, {135, "interval"}, {136, "listener"}, {137, "timeout"}};
const EnumValue EnumValues5[] = {{188, "request"}, {189, "response"}};
const EnumValue EnumValues6[] = {{190, "General"}, {191, "AccessControl"}, {192, "Cancellation"}, {193, "Timeout"}};
const EnumValue EnumValues7[] = {{194, "PrivateClickMeasurementDebugModeEnabled"}, {195, "AuthorAndUserStylesEnabled"}, {196, "ICECandidateFilteringEnabled"}, {197, "ITPDebugModeEnabled"}, {198, "ImagesEnabled"}, {199, "MediaCaptureRequiresSecureConnection"}, {200, "MockCaptureDevicesEnabled"}, {201, "NeedsSiteSpecificQuirks"}, {202, "ScriptEnabled"}, {203, "ShowDebugBorders"}, {204, "ShowRepaintCounter"}, {205, "WebSecurityEnabled"}};
const EnumValue EnumValues8[] = {{206, "PrefersReducedMotion"}, {207, "PrefersContrast"}, {208, "PrefersColorScheme"}};
const EnumValue EnumValues9[] = {{209, "NoPreference"}, {210, "Reduce"}, {211, "More"}, {212, "Light"}, {213, "Dark"}};
const EnumValue EnumValues10[] = {{214, "Viewport"}, {215, "Page"}};
const EnumValue EnumValues11[] = {{9, "none"}, {243, "irrecoverable"}, {244, "unterminated-literal"}, {245, "recoverable"}};

const EnumList EnumLists[] = {
    {EnumValues0, 17},
    {EnumValues1, 3},
    {EnumValues2, 2},
    {EnumValues3, 3},
    {EnumValues4, 4},
    {EnumValues5, 2},
    {EnumValues6, 4},
    {EnumValues7, 12},
    {EnumValues8, 3},
    {EnumValues9, 5},
    {EnumValues10, 2},
    {EnumValues11, 4},
};

const ParamSpec AnimationRequestEffectTargetParams[] = {
    {"animationId", ParamKind::String, Getter::String, true, -1},
};
const OutSpec AnimationRequestEffectTargetOuts[] = {
    {"effectTarget", OutKind::RefValue, false, -1},
};
const ParamSpec AnimationResolveAnimationParams[] = {
    {"animationId", ParamKind::String, Getter::String, true, -1},
    {"objectGroup", ParamKind::String, Getter::String, false, -1},
};
const OutSpec AnimationResolveAnimationOuts[] = {
    {"object", OutKind::RefValue, false, -1},
};

const CommandSpec AnimationCommands[] = {
    {"enable", 0, nullptr, 0, nullptr, 0, false},
    {"disable", 1, nullptr, 0, nullptr, 0, false},
    {"requestEffectTarget", 2, AnimationRequestEffectTargetParams, 1, AnimationRequestEffectTargetOuts, 1, false},
    {"resolveAnimation", 3, AnimationResolveAnimationParams, 2, AnimationResolveAnimationOuts, 1, false},
    {"startTracking", 4, nullptr, 0, nullptr, 0, false},
    {"stopTracking", 5, nullptr, 0, nullptr, 0, false},
};

const OutSpec ApplicationCacheGetFramesWithManifestsOuts[] = {
    {"frameIds", OutKind::RefValue, false, -1},
};
const ParamSpec ApplicationCacheGetManifestForFrameParams[] = {
    {"frameId", ParamKind::String, Getter::String, true, -1},
};
const OutSpec ApplicationCacheGetManifestForFrameOuts[] = {
    {"manifestURL", OutKind::String, false, -1},
};
const ParamSpec ApplicationCacheGetApplicationCacheForFrameParams[] = {
    {"frameId", ParamKind::String, Getter::String, true, -1},
};
const OutSpec ApplicationCacheGetApplicationCacheForFrameOuts[] = {
    {"applicationCache", OutKind::RefValue, false, -1},
};

const CommandSpec ApplicationCacheCommands[] = {
    {"getFramesWithManifests", 0, nullptr, 0, ApplicationCacheGetFramesWithManifestsOuts, 1, false},
    {"enable", 1, nullptr, 0, nullptr, 0, false},
    {"disable", 2, nullptr, 0, nullptr, 0, false},
    {"getManifestForFrame", 3, ApplicationCacheGetManifestForFrameParams, 1, ApplicationCacheGetManifestForFrameOuts, 1, false},
    {"getApplicationCacheForFrame", 4, ApplicationCacheGetApplicationCacheForFrameParams, 1, ApplicationCacheGetApplicationCacheForFrameOuts, 1, false},
};

const ParamSpec AuditSetupParams[] = {
    {"contextId", ParamKind::OptionalInteger, Getter::Integer, false, -1},
};
const ParamSpec AuditRunParams[] = {
    {"test", ParamKind::String, Getter::String, true, -1},
    {"contextId", ParamKind::OptionalInteger, Getter::Integer, false, -1},
};
const OutSpec AuditRunOuts[] = {
    {"result", OutKind::RefValue, false, -1},
    {"wasThrown", OutKind::OptionalBoolean, true, -1},
};

const CommandSpec AuditCommands[] = {
    {"setup", 0, AuditSetupParams, 1, nullptr, 0, false},
    {"run", 1, AuditRunParams, 2, AuditRunOuts, 2, false},
    {"teardown", 2, nullptr, 0, nullptr, 0, false},
};


const CommandSpec BrowserCommands[] = {
    {"enable", 0, nullptr, 0, nullptr, 0, false},
    {"disable", 1, nullptr, 0, nullptr, 0, false},
};

const ParamSpec CanvasRequestNodeParams[] = {
    {"canvasId", ParamKind::String, Getter::String, true, -1},
};
const OutSpec CanvasRequestNodeOuts[] = {
    {"nodeId", OutKind::Integer, false, -1},
};
const ParamSpec CanvasRequestContentParams[] = {
    {"canvasId", ParamKind::String, Getter::String, true, -1},
};
const OutSpec CanvasRequestContentOuts[] = {
    {"content", OutKind::String, false, -1},
};
const ParamSpec CanvasRequestClientNodesParams[] = {
    {"canvasId", ParamKind::String, Getter::String, true, -1},
};
const OutSpec CanvasRequestClientNodesOuts[] = {
    {"clientNodeIds", OutKind::RefValue, false, -1},
};
const ParamSpec CanvasResolveContextParams[] = {
    {"canvasId", ParamKind::String, Getter::String, true, -1},
    {"objectGroup", ParamKind::String, Getter::String, false, -1},
};
const OutSpec CanvasResolveContextOuts[] = {
    {"object", OutKind::RefValue, false, -1},
};
const ParamSpec CanvasSetRecordingAutoCaptureFrameCountParams[] = {
    {"count", ParamKind::Integer, Getter::Integer, true, -1},
};
const ParamSpec CanvasStartRecordingParams[] = {
    {"canvasId", ParamKind::String, Getter::String, true, -1},
    {"frameCount", ParamKind::OptionalInteger, Getter::Integer, false, -1},
    {"memoryLimit", ParamKind::OptionalInteger, Getter::Integer, false, -1},
};
const ParamSpec CanvasStopRecordingParams[] = {
    {"canvasId", ParamKind::String, Getter::String, true, -1},
};

const CommandSpec CanvasCommands[] = {
    {"enable", 0, nullptr, 0, nullptr, 0, false},
    {"disable", 1, nullptr, 0, nullptr, 0, false},
    {"requestNode", 2, CanvasRequestNodeParams, 1, CanvasRequestNodeOuts, 1, false},
    {"requestContent", 3, CanvasRequestContentParams, 1, CanvasRequestContentOuts, 1, false},
    {"requestClientNodes", 4, CanvasRequestClientNodesParams, 1, CanvasRequestClientNodesOuts, 1, false},
    {"resolveContext", 5, CanvasResolveContextParams, 2, CanvasResolveContextOuts, 1, false},
    {"setRecordingAutoCaptureFrameCount", 6, CanvasSetRecordingAutoCaptureFrameCountParams, 1, nullptr, 0, false},
    {"startRecording", 7, CanvasStartRecordingParams, 3, nullptr, 0, false},
    {"stopRecording", 8, CanvasStopRecordingParams, 1, nullptr, 0, false},
};

const OutSpec ConsoleGetLoggingChannelsOuts[] = {
    {"channels", OutKind::RefValue, false, -1},
};
const ParamSpec ConsoleSetLoggingChannelLevelParams[] = {
    {"source", ParamKind::Enum, Getter::String, true, 0},
    {"level", ParamKind::Enum, Getter::String, true, 1},
};

const CommandSpec ConsoleCommands[] = {
    {"enable", 0, nullptr, 0, nullptr, 0, false},
    {"disable", 1, nullptr, 0, nullptr, 0, false},
    {"clearMessages", 2, nullptr, 0, nullptr, 0, false},
    {"getLoggingChannels", 3, nullptr, 0, ConsoleGetLoggingChannelsOuts, 1, false},
    {"setLoggingChannelLevel", 4, ConsoleSetLoggingChannelLevelParams, 2, nullptr, 0, false},
};

const ParamSpec CSSGetMatchedStylesForNodeParams[] = {
    {"nodeId", ParamKind::Integer, Getter::Integer, true, -1},
    {"includePseudo", ParamKind::OptionalBoolean, Getter::Boolean, false, -1},
    {"includeInherited", ParamKind::OptionalBoolean, Getter::Boolean, false, -1},
};
const OutSpec CSSGetMatchedStylesForNodeOuts[] = {
    {"matchedCSSRules", OutKind::RefPtrValue, true, -1},
    {"pseudoElements", OutKind::RefPtrValue, true, -1},
    {"inherited", OutKind::RefPtrValue, true, -1},
};
const ParamSpec CSSGetInlineStylesForNodeParams[] = {
    {"nodeId", ParamKind::Integer, Getter::Integer, true, -1},
};
const OutSpec CSSGetInlineStylesForNodeOuts[] = {
    {"inlineStyle", OutKind::RefPtrValue, true, -1},
    {"attributesStyle", OutKind::RefPtrValue, true, -1},
};
const ParamSpec CSSGetComputedStyleForNodeParams[] = {
    {"nodeId", ParamKind::Integer, Getter::Integer, true, -1},
};
const OutSpec CSSGetComputedStyleForNodeOuts[] = {
    {"computedStyle", OutKind::RefValue, false, -1},
};
const ParamSpec CSSGetFontDataForNodeParams[] = {
    {"nodeId", ParamKind::Integer, Getter::Integer, true, -1},
};
const OutSpec CSSGetFontDataForNodeOuts[] = {
    {"primaryFont", OutKind::RefValue, false, -1},
};
const OutSpec CSSGetAllStyleSheetsOuts[] = {
    {"headers", OutKind::RefValue, false, -1},
};
const ParamSpec CSSGetStyleSheetParams[] = {
    {"styleSheetId", ParamKind::String, Getter::String, true, -1},
};
const OutSpec CSSGetStyleSheetOuts[] = {
    {"styleSheet", OutKind::RefValue, false, -1},
};
const ParamSpec CSSGetStyleSheetTextParams[] = {
    {"styleSheetId", ParamKind::String, Getter::String, true, -1},
};
const OutSpec CSSGetStyleSheetTextOuts[] = {
    {"text", OutKind::String, false, -1},
};
const ParamSpec CSSSetStyleSheetTextParams[] = {
    {"styleSheetId", ParamKind::String, Getter::String, true, -1},
    {"text", ParamKind::String, Getter::String, true, -1},
};
const ParamSpec CSSSetStyleTextParams[] = {
    {"styleId", ParamKind::RefObject, Getter::Object, true, -1},
    {"text", ParamKind::String, Getter::String, true, -1},
};
const OutSpec CSSSetStyleTextOuts[] = {
    {"style", OutKind::RefValue, false, -1},
};
const ParamSpec CSSSetRuleSelectorParams[] = {
    {"ruleId", ParamKind::RefObject, Getter::Object, true, -1},
    {"selector", ParamKind::String, Getter::String, true, -1},
};
const OutSpec CSSSetRuleSelectorOuts[] = {
    {"rule", OutKind::RefValue, false, -1},
};
const ParamSpec CSSSetGroupingHeaderTextParams[] = {
    {"ruleId", ParamKind::RefObject, Getter::Object, true, -1},
    {"headerText", ParamKind::String, Getter::String, true, -1},
};
const OutSpec CSSSetGroupingHeaderTextOuts[] = {
    {"grouping", OutKind::RefValue, false, -1},
};
const ParamSpec CSSCreateStyleSheetParams[] = {
    {"frameId", ParamKind::String, Getter::String, true, -1},
};
const OutSpec CSSCreateStyleSheetOuts[] = {
    {"styleSheetId", OutKind::String, false, -1},
};
const ParamSpec CSSAddRuleParams[] = {
    {"styleSheetId", ParamKind::String, Getter::String, true, -1},
    {"selector", ParamKind::String, Getter::String, true, -1},
};
const OutSpec CSSAddRuleOuts[] = {
    {"rule", OutKind::RefValue, false, -1},
};
const OutSpec CSSGetSupportedCSSPropertiesOuts[] = {
    {"cssProperties", OutKind::RefValue, false, -1},
};
const OutSpec CSSGetSupportedSystemFontFamilyNamesOuts[] = {
    {"fontFamilyNames", OutKind::RefValue, false, -1},
};
const ParamSpec CSSForcePseudoStateParams[] = {
    {"nodeId", ParamKind::Integer, Getter::Integer, true, -1},
    {"forcedPseudoClasses", ParamKind::RefArray, Getter::Array, true, -1},
};
const ParamSpec CSSSetLayoutContextTypeChangedModeParams[] = {
    {"mode", ParamKind::Enum, Getter::String, true, 2},
};

const CommandSpec CSSCommands[] = {
    {"enable", 0, nullptr, 0, nullptr, 0, false},
    {"disable", 1, nullptr, 0, nullptr, 0, false},
    {"getMatchedStylesForNode", 2, CSSGetMatchedStylesForNodeParams, 3, CSSGetMatchedStylesForNodeOuts, 3, false},
    {"getInlineStylesForNode", 3, CSSGetInlineStylesForNodeParams, 1, CSSGetInlineStylesForNodeOuts, 2, false},
    {"getComputedStyleForNode", 4, CSSGetComputedStyleForNodeParams, 1, CSSGetComputedStyleForNodeOuts, 1, false},
    {"getFontDataForNode", 5, CSSGetFontDataForNodeParams, 1, CSSGetFontDataForNodeOuts, 1, false},
    {"getAllStyleSheets", 6, nullptr, 0, CSSGetAllStyleSheetsOuts, 1, false},
    {"getStyleSheet", 7, CSSGetStyleSheetParams, 1, CSSGetStyleSheetOuts, 1, false},
    {"getStyleSheetText", 8, CSSGetStyleSheetTextParams, 1, CSSGetStyleSheetTextOuts, 1, false},
    {"setStyleSheetText", 9, CSSSetStyleSheetTextParams, 2, nullptr, 0, false},
    {"setStyleText", 10, CSSSetStyleTextParams, 2, CSSSetStyleTextOuts, 1, false},
    {"setRuleSelector", 11, CSSSetRuleSelectorParams, 2, CSSSetRuleSelectorOuts, 1, false},
    {"setGroupingHeaderText", 12, CSSSetGroupingHeaderTextParams, 2, CSSSetGroupingHeaderTextOuts, 1, false},
    {"createStyleSheet", 13, CSSCreateStyleSheetParams, 1, CSSCreateStyleSheetOuts, 1, false},
    {"addRule", 14, CSSAddRuleParams, 2, CSSAddRuleOuts, 1, false},
    {"getSupportedCSSProperties", 15, nullptr, 0, CSSGetSupportedCSSPropertiesOuts, 1, false},
    {"getSupportedSystemFontFamilyNames", 16, nullptr, 0, CSSGetSupportedSystemFontFamilyNamesOuts, 1, false},
    {"forcePseudoState", 17, CSSForcePseudoStateParams, 2, nullptr, 0, false},
    {"setLayoutContextTypeChangedMode", 18, CSSSetLayoutContextTypeChangedModeParams, 1, nullptr, 0, false},
};

const ParamSpec DatabaseGetDatabaseTableNamesParams[] = {
    {"databaseId", ParamKind::String, Getter::String, true, -1},
};
const OutSpec DatabaseGetDatabaseTableNamesOuts[] = {
    {"tableNames", OutKind::RefValue, false, -1},
};
const ParamSpec DatabaseExecuteSQLParams[] = {
    {"databaseId", ParamKind::String, Getter::String, true, -1},
    {"query", ParamKind::String, Getter::String, true, -1},
    {nullptr, ParamKind::Callback, Getter::Object, false, -1},
};

const CommandSpec DatabaseCommands[] = {
    {"enable", 0, nullptr, 0, nullptr, 0, false},
    {"disable", 1, nullptr, 0, nullptr, 0, false},
    {"getDatabaseTableNames", 2, DatabaseGetDatabaseTableNamesParams, 1, DatabaseGetDatabaseTableNamesOuts, 1, false},
    {"executeSQL", 3, DatabaseExecuteSQLParams, 3, nullptr, 0, true},
};

const ParamSpec DebuggerSetAsyncStackTraceDepthParams[] = {
    {"depth", ParamKind::Integer, Getter::Integer, true, -1},
};
const ParamSpec DebuggerSetBreakpointsActiveParams[] = {
    {"active", ParamKind::Boolean, Getter::Boolean, true, -1},
};
const ParamSpec DebuggerSetBreakpointByUrlParams[] = {
    {"lineNumber", ParamKind::Integer, Getter::Integer, true, -1},
    {"url", ParamKind::String, Getter::String, false, -1},
    {"urlRegex", ParamKind::String, Getter::String, false, -1},
    {"columnNumber", ParamKind::OptionalInteger, Getter::Integer, false, -1},
    {"options", ParamKind::RefPtrObject, Getter::Object, false, -1},
};
const OutSpec DebuggerSetBreakpointByUrlOuts[] = {
    {"breakpointId", OutKind::String, false, -1},
    {"locations", OutKind::RefValue, false, -1},
};
const ParamSpec DebuggerSetBreakpointParams[] = {
    {"location", ParamKind::RefObject, Getter::Object, true, -1},
    {"options", ParamKind::RefPtrObject, Getter::Object, false, -1},
};
const OutSpec DebuggerSetBreakpointOuts[] = {
    {"breakpointId", OutKind::String, false, -1},
    {"actualLocation", OutKind::RefValue, false, -1},
};
const ParamSpec DebuggerRemoveBreakpointParams[] = {
    {"breakpointId", ParamKind::String, Getter::String, true, -1},
};
const ParamSpec DebuggerAddSymbolicBreakpointParams[] = {
    {"symbol", ParamKind::String, Getter::String, true, -1},
    {"caseSensitive", ParamKind::OptionalBoolean, Getter::Boolean, false, -1},
    {"isRegex", ParamKind::OptionalBoolean, Getter::Boolean, false, -1},
    {"options", ParamKind::RefPtrObject, Getter::Object, false, -1},
};
const ParamSpec DebuggerRemoveSymbolicBreakpointParams[] = {
    {"symbol", ParamKind::String, Getter::String, true, -1},
    {"caseSensitive", ParamKind::OptionalBoolean, Getter::Boolean, false, -1},
    {"isRegex", ParamKind::OptionalBoolean, Getter::Boolean, false, -1},
};
const ParamSpec DebuggerContinueToLocationParams[] = {
    {"location", ParamKind::RefObject, Getter::Object, true, -1},
};
const ParamSpec DebuggerSearchInContentParams[] = {
    {"scriptId", ParamKind::String, Getter::String, true, -1},
    {"query", ParamKind::String, Getter::String, true, -1},
    {"caseSensitive", ParamKind::OptionalBoolean, Getter::Boolean, false, -1},
    {"isRegex", ParamKind::OptionalBoolean, Getter::Boolean, false, -1},
};
const OutSpec DebuggerSearchInContentOuts[] = {
    {"result", OutKind::RefValue, false, -1},
};
const ParamSpec DebuggerGetScriptSourceParams[] = {
    {"scriptId", ParamKind::String, Getter::String, true, -1},
};
const OutSpec DebuggerGetScriptSourceOuts[] = {
    {"scriptSource", OutKind::String, false, -1},
};
const ParamSpec DebuggerGetFunctionDetailsParams[] = {
    {"functionId", ParamKind::String, Getter::String, true, -1},
};
const OutSpec DebuggerGetFunctionDetailsOuts[] = {
    {"details", OutKind::RefValue, false, -1},
};
const ParamSpec DebuggerGetBreakpointLocationsParams[] = {
    {"start", ParamKind::RefObject, Getter::Object, true, -1},
    {"end", ParamKind::RefObject, Getter::Object, true, -1},
};
const OutSpec DebuggerGetBreakpointLocationsOuts[] = {
    {"locations", OutKind::RefValue, false, -1},
};
const ParamSpec DebuggerSetPauseOnDebuggerStatementsParams[] = {
    {"enabled", ParamKind::Boolean, Getter::Boolean, true, -1},
    {"options", ParamKind::RefPtrObject, Getter::Object, false, -1},
};
const ParamSpec DebuggerSetPauseOnExceptionsParams[] = {
    {"state", ParamKind::String, Getter::String, true, -1},
    {"options", ParamKind::RefPtrObject, Getter::Object, false, -1},
};
const ParamSpec DebuggerSetPauseOnAssertionsParams[] = {
    {"enabled", ParamKind::Boolean, Getter::Boolean, true, -1},
    {"options", ParamKind::RefPtrObject, Getter::Object, false, -1},
};
const ParamSpec DebuggerSetPauseOnMicrotasksParams[] = {
    {"enabled", ParamKind::Boolean, Getter::Boolean, true, -1},
    {"options", ParamKind::RefPtrObject, Getter::Object, false, -1},
};
const ParamSpec DebuggerSetPauseForInternalScriptsParams[] = {
    {"shouldPause", ParamKind::Boolean, Getter::Boolean, true, -1},
};
const ParamSpec DebuggerEvaluateOnCallFrameParams[] = {
    {"callFrameId", ParamKind::String, Getter::String, true, -1},
    {"expression", ParamKind::String, Getter::String, true, -1},
    {"objectGroup", ParamKind::String, Getter::String, false, -1},
    {"includeCommandLineAPI", ParamKind::OptionalBoolean, Getter::Boolean, false, -1},
    {"doNotPauseOnExceptionsAndMuteConsole", ParamKind::OptionalBoolean, Getter::Boolean, false, -1},
    {"returnByValue", ParamKind::OptionalBoolean, Getter::Boolean, false, -1},
    {"generatePreview", ParamKind::OptionalBoolean, Getter::Boolean, false, -1},
    {"saveResult", ParamKind::OptionalBoolean, Getter::Boolean, false, -1},
    {"emulateUserGesture", ParamKind::OptionalBoolean, Getter::Boolean, false, -1},
};
const OutSpec DebuggerEvaluateOnCallFrameOuts[] = {
    {"result", OutKind::RefValue, false, -1},
    {"wasThrown", OutKind::OptionalBoolean, true, -1},
    {"savedResultIndex", OutKind::OptionalInteger, true, -1},
};
const ParamSpec DebuggerSetShouldBlackboxURLParams[] = {
    {"url", ParamKind::String, Getter::String, true, -1},
    {"shouldBlackbox", ParamKind::Boolean, Getter::Boolean, true, -1},
    {"caseSensitive", ParamKind::OptionalBoolean, Getter::Boolean, false, -1},
    {"isRegex", ParamKind::OptionalBoolean, Getter::Boolean, false, -1},
};
const ParamSpec DebuggerSetBlackboxBreakpointEvaluationsParams[] = {
    {"blackboxBreakpointEvaluations", ParamKind::Boolean, Getter::Boolean, true, -1},
};

const CommandSpec DebuggerCommands[] = {
    {"enable", 0, nullptr, 0, nullptr, 0, false},
    {"disable", 1, nullptr, 0, nullptr, 0, false},
    {"setAsyncStackTraceDepth", 2, DebuggerSetAsyncStackTraceDepthParams, 1, nullptr, 0, false},
    {"setBreakpointsActive", 3, DebuggerSetBreakpointsActiveParams, 1, nullptr, 0, false},
    {"setBreakpointByUrl", 4, DebuggerSetBreakpointByUrlParams, 5, DebuggerSetBreakpointByUrlOuts, 2, false},
    {"setBreakpoint", 5, DebuggerSetBreakpointParams, 2, DebuggerSetBreakpointOuts, 2, false},
    {"removeBreakpoint", 6, DebuggerRemoveBreakpointParams, 1, nullptr, 0, false},
    {"addSymbolicBreakpoint", 7, DebuggerAddSymbolicBreakpointParams, 4, nullptr, 0, false},
    {"removeSymbolicBreakpoint", 8, DebuggerRemoveSymbolicBreakpointParams, 3, nullptr, 0, false},
    {"continueUntilNextRunLoop", 9, nullptr, 0, nullptr, 0, false},
    {"continueToLocation", 10, DebuggerContinueToLocationParams, 1, nullptr, 0, false},
    {"stepNext", 11, nullptr, 0, nullptr, 0, false},
    {"stepOver", 12, nullptr, 0, nullptr, 0, false},
    {"stepInto", 13, nullptr, 0, nullptr, 0, false},
    {"stepOut", 14, nullptr, 0, nullptr, 0, false},
    {"pause", 15, nullptr, 0, nullptr, 0, false},
    {"resume", 16, nullptr, 0, nullptr, 0, false},
    {"searchInContent", 17, DebuggerSearchInContentParams, 4, DebuggerSearchInContentOuts, 1, false},
    {"getScriptSource", 18, DebuggerGetScriptSourceParams, 1, DebuggerGetScriptSourceOuts, 1, false},
    {"getFunctionDetails", 19, DebuggerGetFunctionDetailsParams, 1, DebuggerGetFunctionDetailsOuts, 1, false},
    {"getBreakpointLocations", 20, DebuggerGetBreakpointLocationsParams, 2, DebuggerGetBreakpointLocationsOuts, 1, false},
    {"setPauseOnDebuggerStatements", 21, DebuggerSetPauseOnDebuggerStatementsParams, 2, nullptr, 0, false},
    {"setPauseOnExceptions", 22, DebuggerSetPauseOnExceptionsParams, 2, nullptr, 0, false},
    {"setPauseOnAssertions", 23, DebuggerSetPauseOnAssertionsParams, 2, nullptr, 0, false},
    {"setPauseOnMicrotasks", 24, DebuggerSetPauseOnMicrotasksParams, 2, nullptr, 0, false},
    {"setPauseForInternalScripts", 25, DebuggerSetPauseForInternalScriptsParams, 1, nullptr, 0, false},
    {"evaluateOnCallFrame", 26, DebuggerEvaluateOnCallFrameParams, 9, DebuggerEvaluateOnCallFrameOuts, 3, false},
    {"setShouldBlackboxURL", 27, DebuggerSetShouldBlackboxURLParams, 4, nullptr, 0, false},
    {"setBlackboxBreakpointEvaluations", 28, DebuggerSetBlackboxBreakpointEvaluationsParams, 1, nullptr, 0, false},
};

const OutSpec DOMGetDocumentOuts[] = {
    {"root", OutKind::RefValue, false, -1},
};
const ParamSpec DOMRequestChildNodesParams[] = {
    {"nodeId", ParamKind::Integer, Getter::Integer, true, -1},
    {"depth", ParamKind::OptionalInteger, Getter::Integer, false, -1},
};
const ParamSpec DOMQuerySelectorParams[] = {
    {"nodeId", ParamKind::Integer, Getter::Integer, true, -1},
    {"selector", ParamKind::String, Getter::String, true, -1},
};
const OutSpec DOMQuerySelectorOuts[] = {
    {"nodeId", OutKind::OptionalInteger, true, -1},
};
const ParamSpec DOMQuerySelectorAllParams[] = {
    {"nodeId", ParamKind::Integer, Getter::Integer, true, -1},
    {"selector", ParamKind::String, Getter::String, true, -1},
};
const OutSpec DOMQuerySelectorAllOuts[] = {
    {"nodeIds", OutKind::RefValue, false, -1},
};
const ParamSpec DOMSetNodeNameParams[] = {
    {"nodeId", ParamKind::Integer, Getter::Integer, true, -1},
    {"name", ParamKind::String, Getter::String, true, -1},
};
const OutSpec DOMSetNodeNameOuts[] = {
    {"nodeId", OutKind::Integer, false, -1},
};
const ParamSpec DOMSetNodeValueParams[] = {
    {"nodeId", ParamKind::Integer, Getter::Integer, true, -1},
    {"value", ParamKind::String, Getter::String, true, -1},
};
const ParamSpec DOMRemoveNodeParams[] = {
    {"nodeId", ParamKind::Integer, Getter::Integer, true, -1},
};
const ParamSpec DOMSetAttributeValueParams[] = {
    {"nodeId", ParamKind::Integer, Getter::Integer, true, -1},
    {"name", ParamKind::String, Getter::String, true, -1},
    {"value", ParamKind::String, Getter::String, true, -1},
};
const ParamSpec DOMSetAttributesAsTextParams[] = {
    {"nodeId", ParamKind::Integer, Getter::Integer, true, -1},
    {"text", ParamKind::String, Getter::String, true, -1},
    {"name", ParamKind::String, Getter::String, false, -1},
};
const ParamSpec DOMRemoveAttributeParams[] = {
    {"nodeId", ParamKind::Integer, Getter::Integer, true, -1},
    {"name", ParamKind::String, Getter::String, true, -1},
};
const OutSpec DOMGetSupportedEventNamesOuts[] = {
    {"eventNames", OutKind::RefValue, false, -1},
};
const ParamSpec DOMGetEventListenersForNodeParams[] = {
    {"nodeId", ParamKind::Integer, Getter::Integer, true, -1},
    {"includeAncestors", ParamKind::OptionalBoolean, Getter::Boolean, false, -1},
};
const OutSpec DOMGetEventListenersForNodeOuts[] = {
    {"listeners", OutKind::RefValue, false, -1},
};
const ParamSpec DOMSetEventListenerDisabledParams[] = {
    {"eventListenerId", ParamKind::Integer, Getter::Integer, true, -1},
    {"disabled", ParamKind::Boolean, Getter::Boolean, true, -1},
};
const ParamSpec DOMSetBreakpointForEventListenerParams[] = {
    {"eventListenerId", ParamKind::Integer, Getter::Integer, true, -1},
    {"options", ParamKind::RefPtrObject, Getter::Object, false, -1},
};
const ParamSpec DOMRemoveBreakpointForEventListenerParams[] = {
    {"eventListenerId", ParamKind::Integer, Getter::Integer, true, -1},
};
const ParamSpec DOMGetAccessibilityPropertiesForNodeParams[] = {
    {"nodeId", ParamKind::Integer, Getter::Integer, true, -1},
};
const OutSpec DOMGetAccessibilityPropertiesForNodeOuts[] = {
    {"properties", OutKind::RefValue, false, -1},
};
const ParamSpec DOMGetOuterHTMLParams[] = {
    {"nodeId", ParamKind::Integer, Getter::Integer, true, -1},
};
const OutSpec DOMGetOuterHTMLOuts[] = {
    {"outerHTML", OutKind::String, false, -1},
};
const ParamSpec DOMSetOuterHTMLParams[] = {
    {"nodeId", ParamKind::Integer, Getter::Integer, true, -1},
    {"outerHTML", ParamKind::String, Getter::String, true, -1},
};
const ParamSpec DOMInsertAdjacentHTMLParams[] = {
    {"nodeId", ParamKind::Integer, Getter::Integer, true, -1},
    {"position", ParamKind::String, Getter::String, true, -1},
    {"html", ParamKind::String, Getter::String, true, -1},
};
const ParamSpec DOMPerformSearchParams[] = {
    {"query", ParamKind::String, Getter::String, true, -1},
    {"nodeIds", ParamKind::RefPtrArray, Getter::Array, false, -1},
    {"caseSensitive", ParamKind::OptionalBoolean, Getter::Boolean, false, -1},
};
const OutSpec DOMPerformSearchOuts[] = {
    {"searchId", OutKind::String, false, -1},
    {"resultCount", OutKind::Integer, false, -1},
};
const ParamSpec DOMGetSearchResultsParams[] = {
    {"searchId", ParamKind::String, Getter::String, true, -1},
    {"fromIndex", ParamKind::Integer, Getter::Integer, true, -1},
    {"toIndex", ParamKind::Integer, Getter::Integer, true, -1},
};
const OutSpec DOMGetSearchResultsOuts[] = {
    {"nodeIds", OutKind::RefValue, false, -1},
};
const ParamSpec DOMDiscardSearchResultsParams[] = {
    {"searchId", ParamKind::String, Getter::String, true, -1},
};
const ParamSpec DOMRequestNodeParams[] = {
    {"objectId", ParamKind::String, Getter::String, true, -1},
};
const OutSpec DOMRequestNodeOuts[] = {
    {"nodeId", OutKind::Integer, false, -1},
};
const ParamSpec DOMSetInspectModeEnabledParams[] = {
    {"enabled", ParamKind::Boolean, Getter::Boolean, true, -1},
    {"highlightConfig", ParamKind::RefPtrObject, Getter::Object, false, -1},
    {"gridOverlayConfig", ParamKind::RefPtrObject, Getter::Object, false, -1},
    {"flexOverlayConfig", ParamKind::RefPtrObject, Getter::Object, false, -1},
    {"showRulers", ParamKind::OptionalBoolean, Getter::Boolean, false, -1},
};
const ParamSpec DOMHighlightRectParams[] = {
    {"x", ParamKind::Integer, Getter::Integer, true, -1},
    {"y", ParamKind::Integer, Getter::Integer, true, -1},
    {"width", ParamKind::Integer, Getter::Integer, true, -1},
    {"height", ParamKind::Integer, Getter::Integer, true, -1},
    {"color", ParamKind::RefPtrObject, Getter::Object, false, -1},
    {"outlineColor", ParamKind::RefPtrObject, Getter::Object, false, -1},
    {"usePageCoordinates", ParamKind::OptionalBoolean, Getter::Boolean, false, -1},
};
const ParamSpec DOMHighlightQuadParams[] = {
    {"quad", ParamKind::RefArray, Getter::Array, true, -1},
    {"color", ParamKind::RefPtrObject, Getter::Object, false, -1},
    {"outlineColor", ParamKind::RefPtrObject, Getter::Object, false, -1},
    {"usePageCoordinates", ParamKind::OptionalBoolean, Getter::Boolean, false, -1},
};
const ParamSpec DOMHighlightSelectorParams[] = {
    {"selectorString", ParamKind::String, Getter::String, true, -1},
    {"frameId", ParamKind::String, Getter::String, false, -1},
    {"highlightConfig", ParamKind::RefObject, Getter::Object, true, -1},
    {"gridOverlayConfig", ParamKind::RefPtrObject, Getter::Object, false, -1},
    {"flexOverlayConfig", ParamKind::RefPtrObject, Getter::Object, false, -1},
    {"showRulers", ParamKind::OptionalBoolean, Getter::Boolean, false, -1},
};
const ParamSpec DOMHighlightNodeParams[] = {
    {"nodeId", ParamKind::OptionalInteger, Getter::Integer, false, -1},
    {"objectId", ParamKind::String, Getter::String, false, -1},
    {"highlightConfig", ParamKind::RefObject, Getter::Object, true, -1},
    {"gridOverlayConfig", ParamKind::RefPtrObject, Getter::Object, false, -1},
    {"flexOverlayConfig", ParamKind::RefPtrObject, Getter::Object, false, -1},
    {"showRulers", ParamKind::OptionalBoolean, Getter::Boolean, false, -1},
};
const ParamSpec DOMHighlightNodeListParams[] = {
    {"nodeIds", ParamKind::RefArray, Getter::Array, true, -1},
    {"highlightConfig", ParamKind::RefObject, Getter::Object, true, -1},
    {"gridOverlayConfig", ParamKind::RefPtrObject, Getter::Object, false, -1},
    {"flexOverlayConfig", ParamKind::RefPtrObject, Getter::Object, false, -1},
    {"showRulers", ParamKind::OptionalBoolean, Getter::Boolean, false, -1},
};
const ParamSpec DOMHighlightFrameParams[] = {
    {"frameId", ParamKind::String, Getter::String, true, -1},
    {"contentColor", ParamKind::RefPtrObject, Getter::Object, false, -1},
    {"contentOutlineColor", ParamKind::RefPtrObject, Getter::Object, false, -1},
};
const ParamSpec DOMShowGridOverlayParams[] = {
    {"nodeId", ParamKind::Integer, Getter::Integer, true, -1},
    {"gridOverlayConfig", ParamKind::RefObject, Getter::Object, true, -1},
};
const ParamSpec DOMHideGridOverlayParams[] = {
    {"nodeId", ParamKind::OptionalInteger, Getter::Integer, false, -1},
};
const ParamSpec DOMShowFlexOverlayParams[] = {
    {"nodeId", ParamKind::Integer, Getter::Integer, true, -1},
    {"flexOverlayConfig", ParamKind::RefObject, Getter::Object, true, -1},
};
const ParamSpec DOMHideFlexOverlayParams[] = {
    {"nodeId", ParamKind::OptionalInteger, Getter::Integer, false, -1},
};
const ParamSpec DOMPushNodeByPathToFrontendParams[] = {
    {"path", ParamKind::String, Getter::String, true, -1},
};
const OutSpec DOMPushNodeByPathToFrontendOuts[] = {
    {"nodeId", OutKind::Integer, false, -1},
};
const ParamSpec DOMResolveNodeParams[] = {
    {"nodeId", ParamKind::Integer, Getter::Integer, true, -1},
    {"objectGroup", ParamKind::String, Getter::String, false, -1},
};
const OutSpec DOMResolveNodeOuts[] = {
    {"object", OutKind::RefValue, false, -1},
};
const ParamSpec DOMGetAttributesParams[] = {
    {"nodeId", ParamKind::Integer, Getter::Integer, true, -1},
};
const OutSpec DOMGetAttributesOuts[] = {
    {"attributes", OutKind::RefValue, false, -1},
};
const ParamSpec DOMMoveToParams[] = {
    {"nodeId", ParamKind::Integer, Getter::Integer, true, -1},
    {"targetNodeId", ParamKind::Integer, Getter::Integer, true, -1},
    {"insertBeforeNodeId", ParamKind::OptionalInteger, Getter::Integer, false, -1},
};
const OutSpec DOMMoveToOuts[] = {
    {"nodeId", OutKind::Integer, false, -1},
};
const ParamSpec DOMFocusParams[] = {
    {"nodeId", ParamKind::Integer, Getter::Integer, true, -1},
};
const ParamSpec DOMSetInspectedNodeParams[] = {
    {"nodeId", ParamKind::Integer, Getter::Integer, true, -1},
};
const ParamSpec DOMSetAllowEditingUserAgentShadowTreesParams[] = {
    {"allow", ParamKind::Boolean, Getter::Boolean, true, -1},
};

const CommandSpec DOMCommands[] = {
    {"getDocument", 0, nullptr, 0, DOMGetDocumentOuts, 1, false},
    {"requestChildNodes", 1, DOMRequestChildNodesParams, 2, nullptr, 0, false},
    {"querySelector", 2, DOMQuerySelectorParams, 2, DOMQuerySelectorOuts, 1, false},
    {"querySelectorAll", 3, DOMQuerySelectorAllParams, 2, DOMQuerySelectorAllOuts, 1, false},
    {"setNodeName", 4, DOMSetNodeNameParams, 2, DOMSetNodeNameOuts, 1, false},
    {"setNodeValue", 5, DOMSetNodeValueParams, 2, nullptr, 0, false},
    {"removeNode", 6, DOMRemoveNodeParams, 1, nullptr, 0, false},
    {"setAttributeValue", 7, DOMSetAttributeValueParams, 3, nullptr, 0, false},
    {"setAttributesAsText", 8, DOMSetAttributesAsTextParams, 3, nullptr, 0, false},
    {"removeAttribute", 9, DOMRemoveAttributeParams, 2, nullptr, 0, false},
    {"getSupportedEventNames", 10, nullptr, 0, DOMGetSupportedEventNamesOuts, 1, false},
    {"getEventListenersForNode", 11, DOMGetEventListenersForNodeParams, 2, DOMGetEventListenersForNodeOuts, 1, false},
    {"setEventListenerDisabled", 12, DOMSetEventListenerDisabledParams, 2, nullptr, 0, false},
    {"setBreakpointForEventListener", 13, DOMSetBreakpointForEventListenerParams, 2, nullptr, 0, false},
    {"removeBreakpointForEventListener", 14, DOMRemoveBreakpointForEventListenerParams, 1, nullptr, 0, false},
    {"getAccessibilityPropertiesForNode", 15, DOMGetAccessibilityPropertiesForNodeParams, 1, DOMGetAccessibilityPropertiesForNodeOuts, 1, false},
    {"getOuterHTML", 16, DOMGetOuterHTMLParams, 1, DOMGetOuterHTMLOuts, 1, false},
    {"setOuterHTML", 17, DOMSetOuterHTMLParams, 2, nullptr, 0, false},
    {"insertAdjacentHTML", 18, DOMInsertAdjacentHTMLParams, 3, nullptr, 0, false},
    {"performSearch", 19, DOMPerformSearchParams, 3, DOMPerformSearchOuts, 2, false},
    {"getSearchResults", 20, DOMGetSearchResultsParams, 3, DOMGetSearchResultsOuts, 1, false},
    {"discardSearchResults", 21, DOMDiscardSearchResultsParams, 1, nullptr, 0, false},
    {"requestNode", 22, DOMRequestNodeParams, 1, DOMRequestNodeOuts, 1, false},
    {"setInspectModeEnabled", 23, DOMSetInspectModeEnabledParams, 5, nullptr, 0, false},
    {"highlightRect", 24, DOMHighlightRectParams, 7, nullptr, 0, false},
    {"highlightQuad", 25, DOMHighlightQuadParams, 4, nullptr, 0, false},
    {"highlightSelector", 26, DOMHighlightSelectorParams, 6, nullptr, 0, false},
    {"highlightNode", 27, DOMHighlightNodeParams, 6, nullptr, 0, false},
    {"highlightNodeList", 28, DOMHighlightNodeListParams, 5, nullptr, 0, false},
    {"hideHighlight", 29, nullptr, 0, nullptr, 0, false},
    {"highlightFrame", 30, DOMHighlightFrameParams, 3, nullptr, 0, false},
    {"showGridOverlay", 31, DOMShowGridOverlayParams, 2, nullptr, 0, false},
    {"hideGridOverlay", 32, DOMHideGridOverlayParams, 1, nullptr, 0, false},
    {"showFlexOverlay", 33, DOMShowFlexOverlayParams, 2, nullptr, 0, false},
    {"hideFlexOverlay", 34, DOMHideFlexOverlayParams, 1, nullptr, 0, false},
    {"pushNodeByPathToFrontend", 35, DOMPushNodeByPathToFrontendParams, 1, DOMPushNodeByPathToFrontendOuts, 1, false},
    {"resolveNode", 36, DOMResolveNodeParams, 2, DOMResolveNodeOuts, 1, false},
    {"getAttributes", 37, DOMGetAttributesParams, 1, DOMGetAttributesOuts, 1, false},
    {"moveTo", 38, DOMMoveToParams, 3, DOMMoveToOuts, 1, false},
    {"undo", 39, nullptr, 0, nullptr, 0, false},
    {"redo", 40, nullptr, 0, nullptr, 0, false},
    {"markUndoableState", 41, nullptr, 0, nullptr, 0, false},
    {"focus", 42, DOMFocusParams, 1, nullptr, 0, false},
    {"setInspectedNode", 43, DOMSetInspectedNodeParams, 1, nullptr, 0, false},
    {"setAllowEditingUserAgentShadowTrees", 44, DOMSetAllowEditingUserAgentShadowTreesParams, 1, nullptr, 0, false},
};

const ParamSpec DOMDebuggerSetDOMBreakpointParams[] = {
    {"nodeId", ParamKind::Integer, Getter::Integer, true, -1},
    {"type", ParamKind::Enum, Getter::String, true, 3},
    {"options", ParamKind::RefPtrObject, Getter::Object, false, -1},
};
const ParamSpec DOMDebuggerRemoveDOMBreakpointParams[] = {
    {"nodeId", ParamKind::Integer, Getter::Integer, true, -1},
    {"type", ParamKind::Enum, Getter::String, true, 3},
};
const ParamSpec DOMDebuggerSetEventBreakpointParams[] = {
    {"breakpointType", ParamKind::Enum, Getter::String, true, 4},
    {"eventName", ParamKind::String, Getter::String, false, -1},
    {"caseSensitive", ParamKind::OptionalBoolean, Getter::Boolean, false, -1},
    {"isRegex", ParamKind::OptionalBoolean, Getter::Boolean, false, -1},
    {"options", ParamKind::RefPtrObject, Getter::Object, false, -1},
};
const ParamSpec DOMDebuggerRemoveEventBreakpointParams[] = {
    {"breakpointType", ParamKind::Enum, Getter::String, true, 4},
    {"eventName", ParamKind::String, Getter::String, false, -1},
    {"caseSensitive", ParamKind::OptionalBoolean, Getter::Boolean, false, -1},
    {"isRegex", ParamKind::OptionalBoolean, Getter::Boolean, false, -1},
};
const ParamSpec DOMDebuggerSetURLBreakpointParams[] = {
    {"url", ParamKind::String, Getter::String, true, -1},
    {"isRegex", ParamKind::OptionalBoolean, Getter::Boolean, false, -1},
    {"options", ParamKind::RefPtrObject, Getter::Object, false, -1},
};
const ParamSpec DOMDebuggerRemoveURLBreakpointParams[] = {
    {"url", ParamKind::String, Getter::String, true, -1},
    {"isRegex", ParamKind::OptionalBoolean, Getter::Boolean, false, -1},
};

const CommandSpec DOMDebuggerCommands[] = {
    {"setDOMBreakpoint", 0, DOMDebuggerSetDOMBreakpointParams, 3, nullptr, 0, false},
    {"removeDOMBreakpoint", 1, DOMDebuggerRemoveDOMBreakpointParams, 2, nullptr, 0, false},
    {"setEventBreakpoint", 2, DOMDebuggerSetEventBreakpointParams, 5, nullptr, 0, false},
    {"removeEventBreakpoint", 3, DOMDebuggerRemoveEventBreakpointParams, 4, nullptr, 0, false},
    {"setURLBreakpoint", 4, DOMDebuggerSetURLBreakpointParams, 3, nullptr, 0, false},
    {"removeURLBreakpoint", 5, DOMDebuggerRemoveURLBreakpointParams, 2, nullptr, 0, false},
};

const ParamSpec DOMStorageGetDOMStorageItemsParams[] = {
    {"storageId", ParamKind::RefObject, Getter::Object, true, -1},
};
const OutSpec DOMStorageGetDOMStorageItemsOuts[] = {
    {"entries", OutKind::RefValue, false, -1},
};
const ParamSpec DOMStorageSetDOMStorageItemParams[] = {
    {"storageId", ParamKind::RefObject, Getter::Object, true, -1},
    {"key", ParamKind::String, Getter::String, true, -1},
    {"value", ParamKind::String, Getter::String, true, -1},
};
const ParamSpec DOMStorageRemoveDOMStorageItemParams[] = {
    {"storageId", ParamKind::RefObject, Getter::Object, true, -1},
    {"key", ParamKind::String, Getter::String, true, -1},
};
const ParamSpec DOMStorageClearDOMStorageItemsParams[] = {
    {"storageId", ParamKind::RefObject, Getter::Object, true, -1},
};

const CommandSpec DOMStorageCommands[] = {
    {"enable", 0, nullptr, 0, nullptr, 0, false},
    {"disable", 1, nullptr, 0, nullptr, 0, false},
    {"getDOMStorageItems", 2, DOMStorageGetDOMStorageItemsParams, 1, DOMStorageGetDOMStorageItemsOuts, 1, false},
    {"setDOMStorageItem", 3, DOMStorageSetDOMStorageItemParams, 3, nullptr, 0, false},
    {"removeDOMStorageItem", 4, DOMStorageRemoveDOMStorageItemParams, 2, nullptr, 0, false},
    {"clearDOMStorageItems", 5, DOMStorageClearDOMStorageItemsParams, 1, nullptr, 0, false},
};

const OutSpec HeapSnapshotOuts[] = {
    {"timestamp", OutKind::Double, false, -1},
    {"snapshotData", OutKind::String, false, -1},
};
const ParamSpec HeapGetPreviewParams[] = {
    {"heapObjectId", ParamKind::Integer, Getter::Integer, true, -1},
};
const OutSpec HeapGetPreviewOuts[] = {
    {"string", OutKind::String, true, -1},
    {"functionDetails", OutKind::RefPtrValue, true, -1},
    {"preview", OutKind::RefPtrValue, true, -1},
};
const ParamSpec HeapGetRemoteObjectParams[] = {
    {"heapObjectId", ParamKind::Integer, Getter::Integer, true, -1},
    {"objectGroup", ParamKind::String, Getter::String, false, -1},
};
const OutSpec HeapGetRemoteObjectOuts[] = {
    {"result", OutKind::RefValue, false, -1},
};

const CommandSpec HeapCommands[] = {
    {"enable", 0, nullptr, 0, nullptr, 0, false},
    {"disable", 1, nullptr, 0, nullptr, 0, false},
    {"gc", 2, nullptr, 0, nullptr, 0, false},
    {"snapshot", 3, nullptr, 0, HeapSnapshotOuts, 2, false},
    {"startTracking", 4, nullptr, 0, nullptr, 0, false},
    {"stopTracking", 5, nullptr, 0, nullptr, 0, false},
    {"getPreview", 6, HeapGetPreviewParams, 1, HeapGetPreviewOuts, 3, false},
    {"getRemoteObject", 7, HeapGetRemoteObjectParams, 2, HeapGetRemoteObjectOuts, 1, false},
};


const CommandSpec InspectorCommands[] = {
    {"enable", 0, nullptr, 0, nullptr, 0, false},
    {"disable", 1, nullptr, 0, nullptr, 0, false},
    {"initialized", 2, nullptr, 0, nullptr, 0, false},
};

const ParamSpec LayerTreeLayersForNodeParams[] = {
    {"nodeId", ParamKind::Integer, Getter::Integer, true, -1},
};
const OutSpec LayerTreeLayersForNodeOuts[] = {
    {"layers", OutKind::RefValue, false, -1},
};
const ParamSpec LayerTreeReasonsForCompositingLayerParams[] = {
    {"layerId", ParamKind::String, Getter::String, true, -1},
};
const OutSpec LayerTreeReasonsForCompositingLayerOuts[] = {
    {"compositingReasons", OutKind::RefValue, false, -1},
};

const CommandSpec LayerTreeCommands[] = {
    {"enable", 0, nullptr, 0, nullptr, 0, false},
    {"disable", 1, nullptr, 0, nullptr, 0, false},
    {"layersForNode", 2, LayerTreeLayersForNodeParams, 1, LayerTreeLayersForNodeOuts, 1, false},
    {"reasonsForCompositingLayer", 3, LayerTreeReasonsForCompositingLayerParams, 1, LayerTreeReasonsForCompositingLayerOuts, 1, false},
};

const ParamSpec NetworkSetExtraHTTPHeadersParams[] = {
    {"headers", ParamKind::RefObject, Getter::Object, true, -1},
};
const ParamSpec NetworkGetResponseBodyParams[] = {
    {"requestId", ParamKind::String, Getter::String, true, -1},
};
const OutSpec NetworkGetResponseBodyOuts[] = {
    {"body", OutKind::String, false, -1},
    {"base64Encoded", OutKind::Boolean, false, -1},
};
const ParamSpec NetworkSetResourceCachingDisabledParams[] = {
    {"disabled", ParamKind::Boolean, Getter::Boolean, true, -1},
};
const ParamSpec NetworkLoadResourceParams[] = {
    {"frameId", ParamKind::String, Getter::String, true, -1},
    {"url", ParamKind::String, Getter::String, true, -1},
    {nullptr, ParamKind::Callback, Getter::Object, false, -1},
};
const ParamSpec NetworkGetSerializedCertificateParams[] = {
    {"requestId", ParamKind::String, Getter::String, true, -1},
};
const OutSpec NetworkGetSerializedCertificateOuts[] = {
    {"serializedCertificate", OutKind::String, false, -1},
};
const ParamSpec NetworkResolveWebSocketParams[] = {
    {"requestId", ParamKind::String, Getter::String, true, -1},
    {"objectGroup", ParamKind::String, Getter::String, false, -1},
};
const OutSpec NetworkResolveWebSocketOuts[] = {
    {"object", OutKind::RefValue, false, -1},
};
const ParamSpec NetworkSetInterceptionEnabledParams[] = {
    {"enabled", ParamKind::Boolean, Getter::Boolean, true, -1},
};
const ParamSpec NetworkAddInterceptionParams[] = {
    {"url", ParamKind::String, Getter::String, true, -1},
    {"stage", ParamKind::Enum, Getter::String, true, 5},
    {"caseSensitive", ParamKind::OptionalBoolean, Getter::Boolean, false, -1},
    {"isRegex", ParamKind::OptionalBoolean, Getter::Boolean, false, -1},
};
const ParamSpec NetworkRemoveInterceptionParams[] = {
    {"url", ParamKind::String, Getter::String, true, -1},
    {"stage", ParamKind::Enum, Getter::String, true, 5},
    {"caseSensitive", ParamKind::OptionalBoolean, Getter::Boolean, false, -1},
    {"isRegex", ParamKind::OptionalBoolean, Getter::Boolean, false, -1},
};
const ParamSpec NetworkInterceptContinueParams[] = {
    {"requestId", ParamKind::String, Getter::String, true, -1},
    {"stage", ParamKind::Enum, Getter::String, true, 5},
};
const ParamSpec NetworkInterceptWithRequestParams[] = {
    {"requestId", ParamKind::String, Getter::String, true, -1},
    {"url", ParamKind::String, Getter::String, false, -1},
    {"method", ParamKind::String, Getter::String, false, -1},
    {"headers", ParamKind::RefPtrObject, Getter::Object, false, -1},
    {"postData", ParamKind::String, Getter::String, false, -1},
};
const ParamSpec NetworkInterceptWithResponseParams[] = {
    {"requestId", ParamKind::String, Getter::String, true, -1},
    {"content", ParamKind::String, Getter::String, true, -1},
    {"base64Encoded", ParamKind::Boolean, Getter::Boolean, true, -1},
    {"mimeType", ParamKind::String, Getter::String, false, -1},
    {"status", ParamKind::OptionalInteger, Getter::Integer, false, -1},
    {"statusText", ParamKind::String, Getter::String, false, -1},
    {"headers", ParamKind::RefPtrObject, Getter::Object, false, -1},
};
const ParamSpec NetworkInterceptRequestWithResponseParams[] = {
    {"requestId", ParamKind::String, Getter::String, true, -1},
    {"content", ParamKind::String, Getter::String, true, -1},
    {"base64Encoded", ParamKind::Boolean, Getter::Boolean, true, -1},
    {"mimeType", ParamKind::String, Getter::String, true, -1},
    {"status", ParamKind::Integer, Getter::Integer, true, -1},
    {"statusText", ParamKind::String, Getter::String, true, -1},
    {"headers", ParamKind::RefObject, Getter::Object, true, -1},
};
const ParamSpec NetworkInterceptRequestWithErrorParams[] = {
    {"requestId", ParamKind::String, Getter::String, true, -1},
    {"errorType", ParamKind::Enum, Getter::String, true, 6},
};

const CommandSpec NetworkCommands[] = {
    {"enable", 0, nullptr, 0, nullptr, 0, false},
    {"disable", 1, nullptr, 0, nullptr, 0, false},
    {"setExtraHTTPHeaders", 2, NetworkSetExtraHTTPHeadersParams, 1, nullptr, 0, false},
    {"getResponseBody", 3, NetworkGetResponseBodyParams, 1, NetworkGetResponseBodyOuts, 2, false},
    {"setResourceCachingDisabled", 4, NetworkSetResourceCachingDisabledParams, 1, nullptr, 0, false},
    {"loadResource", 5, NetworkLoadResourceParams, 3, nullptr, 0, true},
    {"getSerializedCertificate", 6, NetworkGetSerializedCertificateParams, 1, NetworkGetSerializedCertificateOuts, 1, false},
    {"resolveWebSocket", 7, NetworkResolveWebSocketParams, 2, NetworkResolveWebSocketOuts, 1, false},
    {"setInterceptionEnabled", 8, NetworkSetInterceptionEnabledParams, 1, nullptr, 0, false},
    {"addInterception", 9, NetworkAddInterceptionParams, 4, nullptr, 0, false},
    {"removeInterception", 10, NetworkRemoveInterceptionParams, 4, nullptr, 0, false},
    {"interceptContinue", 11, NetworkInterceptContinueParams, 2, nullptr, 0, false},
    {"interceptWithRequest", 12, NetworkInterceptWithRequestParams, 5, nullptr, 0, false},
    {"interceptWithResponse", 13, NetworkInterceptWithResponseParams, 7, nullptr, 0, false},
    {"interceptRequestWithResponse", 14, NetworkInterceptRequestWithResponseParams, 7, nullptr, 0, false},
    {"interceptRequestWithError", 15, NetworkInterceptRequestWithErrorParams, 2, nullptr, 0, false},
};

const ParamSpec PageReloadParams[] = {
    {"ignoreCache", ParamKind::OptionalBoolean, Getter::Boolean, false, -1},
    {"revalidateAllResources", ParamKind::OptionalBoolean, Getter::Boolean, false, -1},
};
const ParamSpec PageNavigateParams[] = {
    {"url", ParamKind::String, Getter::String, true, -1},
};
const ParamSpec PageOverrideUserAgentParams[] = {
    {"value", ParamKind::String, Getter::String, false, -1},
};
const ParamSpec PageOverrideSettingParams[] = {
    {"setting", ParamKind::Enum, Getter::String, true, 7},
    {"value", ParamKind::OptionalBoolean, Getter::Boolean, false, -1},
};
const ParamSpec PageOverrideUserPreferenceParams[] = {
    {"name", ParamKind::Enum, Getter::String, true, 8},
    {"value", ParamKind::OptionalEnum, Getter::String, false, 9},
};
const OutSpec PageGetCookiesOuts[] = {
    {"cookies", OutKind::RefValue, false, -1},
};
const ParamSpec PageSetCookieParams[] = {
    {"cookie", ParamKind::RefObject, Getter::Object, true, -1},
};
const ParamSpec PageDeleteCookieParams[] = {
    {"cookieName", ParamKind::String, Getter::String, true, -1},
    {"url", ParamKind::String, Getter::String, true, -1},
};
const OutSpec PageGetResourceTreeOuts[] = {
    {"frameTree", OutKind::RefValue, false, -1},
};
const ParamSpec PageGetResourceContentParams[] = {
    {"frameId", ParamKind::String, Getter::String, true, -1},
    {"url", ParamKind::String, Getter::String, true, -1},
};
const OutSpec PageGetResourceContentOuts[] = {
    {"content", OutKind::String, false, -1},
    {"base64Encoded", OutKind::Boolean, false, -1},
};
const ParamSpec PageSetBootstrapScriptParams[] = {
    {"source", ParamKind::String, Getter::String, false, -1},
};
const ParamSpec PageSearchInResourceParams[] = {
    {"frameId", ParamKind::String, Getter::String, true, -1},
    {"url", ParamKind::String, Getter::String, true, -1},
    {"query", ParamKind::String, Getter::String, true, -1},
    {"caseSensitive", ParamKind::OptionalBoolean, Getter::Boolean, false, -1},
    {"isRegex", ParamKind::OptionalBoolean, Getter::Boolean, false, -1},
    {"requestId", ParamKind::String, Getter::String, false, -1},
};
const OutSpec PageSearchInResourceOuts[] = {
    {"result", OutKind::RefValue, false, -1},
};
const ParamSpec PageSearchInResourcesParams[] = {
    {"text", ParamKind::String, Getter::String, true, -1},
    {"caseSensitive", ParamKind::OptionalBoolean, Getter::Boolean, false, -1},
    {"isRegex", ParamKind::OptionalBoolean, Getter::Boolean, false, -1},
};
const OutSpec PageSearchInResourcesOuts[] = {
    {"result", OutKind::RefValue, false, -1},
};
const ParamSpec PageSetShowRulersParams[] = {
    {"result", ParamKind::Boolean, Getter::Boolean, true, -1},
};
const ParamSpec PageSetShowPaintRectsParams[] = {
    {"result", ParamKind::Boolean, Getter::Boolean, true, -1},
};
const ParamSpec PageSetEmulatedMediaParams[] = {
    {"media", ParamKind::String, Getter::String, true, -1},
};
const ParamSpec PageSnapshotNodeParams[] = {
    {"nodeId", ParamKind::Integer, Getter::Integer, true, -1},
};
const OutSpec PageSnapshotNodeOuts[] = {
    {"dataURL", OutKind::String, false, -1},
};
const ParamSpec PageSnapshotRectParams[] = {
    {"x", ParamKind::Integer, Getter::Integer, true, -1},
    {"y", ParamKind::Integer, Getter::Integer, true, -1},
    {"width", ParamKind::Integer, Getter::Integer, true, -1},
    {"height", ParamKind::Integer, Getter::Integer, true, -1},
    {"coordinateSystem", ParamKind::Enum, Getter::String, true, 10},
};
const OutSpec PageSnapshotRectOuts[] = {
    {"dataURL", OutKind::String, false, -1},
};
const ParamSpec PageSetScreenSizeOverrideParams[] = {
    {"width", ParamKind::OptionalInteger, Getter::Integer, false, -1},
    {"height", ParamKind::OptionalInteger, Getter::Integer, false, -1},
};

const CommandSpec PageCommands[] = {
    {"enable", 0, nullptr, 0, nullptr, 0, false},
    {"disable", 1, nullptr, 0, nullptr, 0, false},
    {"reload", 2, PageReloadParams, 2, nullptr, 0, false},
    {"navigate", 3, PageNavigateParams, 1, nullptr, 0, false},
    {"overrideUserAgent", 4, PageOverrideUserAgentParams, 1, nullptr, 0, false},
    {"overrideSetting", 5, PageOverrideSettingParams, 2, nullptr, 0, false},
    {"overrideUserPreference", 6, PageOverrideUserPreferenceParams, 2, nullptr, 0, false},
    {"getCookies", 7, nullptr, 0, PageGetCookiesOuts, 1, false},
    {"setCookie", 8, PageSetCookieParams, 1, nullptr, 0, false},
    {"deleteCookie", 9, PageDeleteCookieParams, 2, nullptr, 0, false},
    {"getResourceTree", 10, nullptr, 0, PageGetResourceTreeOuts, 1, false},
    {"getResourceContent", 11, PageGetResourceContentParams, 2, PageGetResourceContentOuts, 2, false},
    {"setBootstrapScript", 12, PageSetBootstrapScriptParams, 1, nullptr, 0, false},
    {"searchInResource", 13, PageSearchInResourceParams, 6, PageSearchInResourceOuts, 1, false},
    {"searchInResources", 14, PageSearchInResourcesParams, 3, PageSearchInResourcesOuts, 1, false},
    {"setShowRulers", 15, PageSetShowRulersParams, 1, nullptr, 0, false},
    {"setShowPaintRects", 16, PageSetShowPaintRectsParams, 1, nullptr, 0, false},
    {"setEmulatedMedia", 17, PageSetEmulatedMediaParams, 1, nullptr, 0, false},
    {"snapshotNode", 18, PageSnapshotNodeParams, 1, PageSnapshotNodeOuts, 1, false},
    {"snapshotRect", 19, PageSnapshotRectParams, 5, PageSnapshotRectOuts, 1, false},
    {"setScreenSizeOverride", 20, PageSetScreenSizeOverrideParams, 2, nullptr, 0, false},
};

const ParamSpec RuntimeParseParams[] = {
    {"source", ParamKind::String, Getter::String, true, -1},
};
const OutSpec RuntimeParseOuts[] = {
    {"result", OutKind::Enum, false, 11},
    {"message", OutKind::String, true, -1},
    {"range", OutKind::RefPtrValue, true, -1},
};
const ParamSpec RuntimeEvaluateParams[] = {
    {"expression", ParamKind::String, Getter::String, true, -1},
    {"objectGroup", ParamKind::String, Getter::String, false, -1},
    {"includeCommandLineAPI", ParamKind::OptionalBoolean, Getter::Boolean, false, -1},
    {"doNotPauseOnExceptionsAndMuteConsole", ParamKind::OptionalBoolean, Getter::Boolean, false, -1},
    {"contextId", ParamKind::OptionalInteger, Getter::Integer, false, -1},
    {"returnByValue", ParamKind::OptionalBoolean, Getter::Boolean, false, -1},
    {"generatePreview", ParamKind::OptionalBoolean, Getter::Boolean, false, -1},
    {"saveResult", ParamKind::OptionalBoolean, Getter::Boolean, false, -1},
    {"emulateUserGesture", ParamKind::OptionalBoolean, Getter::Boolean, false, -1},
};
const OutSpec RuntimeEvaluateOuts[] = {
    {"result", OutKind::RefValue, false, -1},
    {"wasThrown", OutKind::OptionalBoolean, true, -1},
    {"savedResultIndex", OutKind::OptionalInteger, true, -1},
};
const ParamSpec RuntimeAwaitPromiseParams[] = {
    {"promiseObjectId", ParamKind::String, Getter::String, true, -1},
    {"returnByValue", ParamKind::OptionalBoolean, Getter::Boolean, false, -1},
    {"generatePreview", ParamKind::OptionalBoolean, Getter::Boolean, false, -1},
    {"saveResult", ParamKind::OptionalBoolean, Getter::Boolean, false, -1},
    {nullptr, ParamKind::Callback, Getter::Object, false, -1},
};
const ParamSpec RuntimeCallFunctionOnParams[] = {
    {"objectId", ParamKind::String, Getter::String, true, -1},
    {"functionDeclaration", ParamKind::String, Getter::String, true, -1},
    {"arguments", ParamKind::RefPtrArray, Getter::Array, false, -1},
    {"doNotPauseOnExceptionsAndMuteConsole", ParamKind::OptionalBoolean, Getter::Boolean, false, -1},
    {"returnByValue", ParamKind::OptionalBoolean, Getter::Boolean, false, -1},
    {"generatePreview", ParamKind::OptionalBoolean, Getter::Boolean, false, -1},
    {"emulateUserGesture", ParamKind::OptionalBoolean, Getter::Boolean, false, -1},
};
const OutSpec RuntimeCallFunctionOnOuts[] = {
    {"result", OutKind::RefValue, false, -1},
    {"wasThrown", OutKind::OptionalBoolean, true, -1},
};
const ParamSpec RuntimeGetPreviewParams[] = {
    {"objectId", ParamKind::String, Getter::String, true, -1},
};
const OutSpec RuntimeGetPreviewOuts[] = {
    {"preview", OutKind::RefValue, false, -1},
};
const ParamSpec RuntimeGetPropertiesParams[] = {
    {"objectId", ParamKind::String, Getter::String, true, -1},
    {"ownProperties", ParamKind::OptionalBoolean, Getter::Boolean, false, -1},
    {"fetchStart", ParamKind::OptionalInteger, Getter::Integer, false, -1},
    {"fetchCount", ParamKind::OptionalInteger, Getter::Integer, false, -1},
    {"generatePreview", ParamKind::OptionalBoolean, Getter::Boolean, false, -1},
};
const OutSpec RuntimeGetPropertiesOuts[] = {
    {"properties", OutKind::RefValue, false, -1},
    {"internalProperties", OutKind::RefPtrValue, true, -1},
};
const ParamSpec RuntimeGetDisplayablePropertiesParams[] = {
    {"objectId", ParamKind::String, Getter::String, true, -1},
    {"fetchStart", ParamKind::OptionalInteger, Getter::Integer, false, -1},
    {"fetchCount", ParamKind::OptionalInteger, Getter::Integer, false, -1},
    {"generatePreview", ParamKind::OptionalBoolean, Getter::Boolean, false, -1},
};
const OutSpec RuntimeGetDisplayablePropertiesOuts[] = {
    {"properties", OutKind::RefValue, false, -1},
    {"internalProperties", OutKind::RefPtrValue, true, -1},
};
const ParamSpec RuntimeGetCollectionEntriesParams[] = {
    {"objectId", ParamKind::String, Getter::String, true, -1},
    {"objectGroup", ParamKind::String, Getter::String, false, -1},
    {"fetchStart", ParamKind::OptionalInteger, Getter::Integer, false, -1},
    {"fetchCount", ParamKind::OptionalInteger, Getter::Integer, false, -1},
};
const OutSpec RuntimeGetCollectionEntriesOuts[] = {
    {"entries", OutKind::RefValue, false, -1},
};
const ParamSpec RuntimeSaveResultParams[] = {
    {"value", ParamKind::RefObject, Getter::Object, true, -1},
    {"contextId", ParamKind::OptionalInteger, Getter::Integer, false, -1},
};
const OutSpec RuntimeSaveResultOuts[] = {
    {"savedResultIndex", OutKind::OptionalInteger, true, -1},
};
const ParamSpec RuntimeSetSavedResultAliasParams[] = {
    {"alias", ParamKind::String, Getter::String, false, -1},
};
const ParamSpec RuntimeReleaseObjectParams[] = {
    {"objectId", ParamKind::String, Getter::String, true, -1},
};
const ParamSpec RuntimeReleaseObjectGroupParams[] = {
    {"objectGroup", ParamKind::String, Getter::String, true, -1},
};
const ParamSpec RuntimeGetRuntimeTypesForVariablesAtOffsetsParams[] = {
    {"locations", ParamKind::RefArray, Getter::Array, true, -1},
};
const OutSpec RuntimeGetRuntimeTypesForVariablesAtOffsetsOuts[] = {
    {"types", OutKind::RefValue, false, -1},
};
const ParamSpec RuntimeGetBasicBlocksParams[] = {
    {"sourceID", ParamKind::String, Getter::String, true, -1},
};
const OutSpec RuntimeGetBasicBlocksOuts[] = {
    {"basicBlocks", OutKind::RefValue, false, -1},
};

const CommandSpec RuntimeCommands[] = {
    {"parse", 0, RuntimeParseParams, 1, RuntimeParseOuts, 3, false},
    {"evaluate", 1, RuntimeEvaluateParams, 9, RuntimeEvaluateOuts, 3, false},
    {"awaitPromise", 2, RuntimeAwaitPromiseParams, 5, nullptr, 0, true},
    {"callFunctionOn", 3, RuntimeCallFunctionOnParams, 7, RuntimeCallFunctionOnOuts, 2, false},
    {"getPreview", 4, RuntimeGetPreviewParams, 1, RuntimeGetPreviewOuts, 1, false},
    {"getProperties", 5, RuntimeGetPropertiesParams, 5, RuntimeGetPropertiesOuts, 2, false},
    {"getDisplayableProperties", 6, RuntimeGetDisplayablePropertiesParams, 4, RuntimeGetDisplayablePropertiesOuts, 2, false},
    {"getCollectionEntries", 7, RuntimeGetCollectionEntriesParams, 4, RuntimeGetCollectionEntriesOuts, 1, false},
    {"saveResult", 8, RuntimeSaveResultParams, 2, RuntimeSaveResultOuts, 1, false},
    {"setSavedResultAlias", 9, RuntimeSetSavedResultAliasParams, 1, nullptr, 0, false},
    {"releaseObject", 10, RuntimeReleaseObjectParams, 1, nullptr, 0, false},
    {"releaseObjectGroup", 11, RuntimeReleaseObjectGroupParams, 1, nullptr, 0, false},
    {"enable", 12, nullptr, 0, nullptr, 0, false},
    {"disable", 13, nullptr, 0, nullptr, 0, false},
    {"getRuntimeTypesForVariablesAtOffsets", 14, RuntimeGetRuntimeTypesForVariablesAtOffsetsParams, 1, RuntimeGetRuntimeTypesForVariablesAtOffsetsOuts, 1, false},
    {"enableTypeProfiler", 15, nullptr, 0, nullptr, 0, false},
    {"disableTypeProfiler", 16, nullptr, 0, nullptr, 0, false},
    {"enableControlFlowProfiler", 17, nullptr, 0, nullptr, 0, false},
    {"disableControlFlowProfiler", 18, nullptr, 0, nullptr, 0, false},
    {"getBasicBlocks", 19, RuntimeGetBasicBlocksParams, 1, RuntimeGetBasicBlocksOuts, 1, false},
};

const ParamSpec ScriptProfilerStartTrackingParams[] = {
    {"includeSamples", ParamKind::OptionalBoolean, Getter::Boolean, false, -1},
};

const CommandSpec ScriptProfilerCommands[] = {
    {"startTracking", 0, ScriptProfilerStartTrackingParams, 1, nullptr, 0, false},
    {"stopTracking", 1, nullptr, 0, nullptr, 0, false},
};

const ParamSpec TargetSetPauseOnStartParams[] = {
    {"pauseOnStart", ParamKind::Boolean, Getter::Boolean, true, -1},
};
const ParamSpec TargetResumeParams[] = {
    {"targetId", ParamKind::String, Getter::String, true, -1},
};
const ParamSpec TargetSendMessageToTargetParams[] = {
    {"targetId", ParamKind::String, Getter::String, true, -1},
    {"message", ParamKind::String, Getter::String, true, -1},
};

const CommandSpec TargetCommands[] = {
    {"setPauseOnStart", 0, TargetSetPauseOnStartParams, 1, nullptr, 0, false},
    {"resume", 1, TargetResumeParams, 1, nullptr, 0, false},
    {"sendMessageToTarget", 2, TargetSendMessageToTargetParams, 2, nullptr, 0, false},
};

const ParamSpec TimelineStartParams[] = {
    {"maxCallStackDepth", ParamKind::OptionalInteger, Getter::Integer, false, -1},
};
const ParamSpec TimelineSetAutoCaptureEnabledParams[] = {
    {"enabled", ParamKind::Boolean, Getter::Boolean, true, -1},
};
const ParamSpec TimelineSetInstrumentsParams[] = {
    {"instruments", ParamKind::RefArray, Getter::Array, true, -1},
};

const CommandSpec TimelineCommands[] = {
    {"enable", 0, nullptr, 0, nullptr, 0, false},
    {"disable", 1, nullptr, 0, nullptr, 0, false},
    {"start", 2, TimelineStartParams, 1, nullptr, 0, false},
    {"stop", 3, nullptr, 0, nullptr, 0, false},
    {"setAutoCaptureEnabled", 4, TimelineSetAutoCaptureEnabledParams, 1, nullptr, 0, false},
    {"setInstruments", 5, TimelineSetInstrumentsParams, 1, nullptr, 0, false},
};

const ParamSpec WorkerInitializedParams[] = {
    {"workerId", ParamKind::String, Getter::String, true, -1},
};
const ParamSpec WorkerSendMessageToWorkerParams[] = {
    {"workerId", ParamKind::String, Getter::String, true, -1},
    {"message", ParamKind::String, Getter::String, true, -1},
};

const CommandSpec WorkerCommands[] = {
    {"enable", 0, nullptr, 0, nullptr, 0, false},
    {"disable", 1, nullptr, 0, nullptr, 0, false},
    {"initialized", 2, WorkerInitializedParams, 1, nullptr, 0, false},
    {"sendMessageToWorker", 3, WorkerSendMessageToWorkerParams, 2, nullptr, 0, false},
};

const DomainSpec Domains[] = {
    {"Animation", "Inspector::AnimationBackendDispatcher::dispatch", "Inspector::AnimationBackendDispatcher::AnimationBackendDispatcher", "Inspector::AnimationBackendDispatcher::~AnimationBackendDispatcher", AnimationCommands, std::size(AnimationCommands)},
    {"ApplicationCache", "Inspector::ApplicationCacheBackendDispatcher::dispatch", "Inspector::ApplicationCacheBackendDispatcher::ApplicationCacheBackendDispatcher", "Inspector::ApplicationCacheBackendDispatcher::~ApplicationCacheBackendDispatcher", ApplicationCacheCommands, std::size(ApplicationCacheCommands)},
    {"Audit", "Inspector::AuditBackendDispatcher::dispatch", "Inspector::AuditBackendDispatcher::AuditBackendDispatcher", "Inspector::AuditBackendDispatcher::~AuditBackendDispatcher", AuditCommands, std::size(AuditCommands)},
    {"Browser", "Inspector::BrowserBackendDispatcher::dispatch", "Inspector::BrowserBackendDispatcher::BrowserBackendDispatcher", "Inspector::BrowserBackendDispatcher::~BrowserBackendDispatcher", BrowserCommands, std::size(BrowserCommands)},
    {"Canvas", "Inspector::CanvasBackendDispatcher::dispatch", "Inspector::CanvasBackendDispatcher::CanvasBackendDispatcher", "Inspector::CanvasBackendDispatcher::~CanvasBackendDispatcher", CanvasCommands, std::size(CanvasCommands)},
    {"Console", "Inspector::ConsoleBackendDispatcher::dispatch", "Inspector::ConsoleBackendDispatcher::ConsoleBackendDispatcher", "Inspector::ConsoleBackendDispatcher::~ConsoleBackendDispatcher", ConsoleCommands, std::size(ConsoleCommands)},
    {"CSS", "Inspector::CSSBackendDispatcher::dispatch", "Inspector::CSSBackendDispatcher::CSSBackendDispatcher", "Inspector::CSSBackendDispatcher::~CSSBackendDispatcher", CSSCommands, std::size(CSSCommands)},
    {"Database", "Inspector::DatabaseBackendDispatcher::dispatch", "Inspector::DatabaseBackendDispatcher::DatabaseBackendDispatcher", "Inspector::DatabaseBackendDispatcher::~DatabaseBackendDispatcher", DatabaseCommands, std::size(DatabaseCommands)},
    {"Debugger", "Inspector::DebuggerBackendDispatcher::dispatch", "Inspector::DebuggerBackendDispatcher::DebuggerBackendDispatcher", "Inspector::DebuggerBackendDispatcher::~DebuggerBackendDispatcher", DebuggerCommands, std::size(DebuggerCommands)},
    {"DOM", "Inspector::DOMBackendDispatcher::dispatch", "Inspector::DOMBackendDispatcher::DOMBackendDispatcher", "Inspector::DOMBackendDispatcher::~DOMBackendDispatcher", DOMCommands, std::size(DOMCommands)},
    {"DOMDebugger", "Inspector::DOMDebuggerBackendDispatcher::dispatch", "Inspector::DOMDebuggerBackendDispatcher::DOMDebuggerBackendDispatcher", "Inspector::DOMDebuggerBackendDispatcher::~DOMDebuggerBackendDispatcher", DOMDebuggerCommands, std::size(DOMDebuggerCommands)},
    {"DOMStorage", "Inspector::DOMStorageBackendDispatcher::dispatch", "Inspector::DOMStorageBackendDispatcher::DOMStorageBackendDispatcher", "Inspector::DOMStorageBackendDispatcher::~DOMStorageBackendDispatcher", DOMStorageCommands, std::size(DOMStorageCommands)},
    {"Heap", "Inspector::HeapBackendDispatcher::dispatch", "Inspector::HeapBackendDispatcher::HeapBackendDispatcher", "Inspector::HeapBackendDispatcher::~HeapBackendDispatcher", HeapCommands, std::size(HeapCommands)},
    {"Inspector", "Inspector::InspectorBackendDispatcher::dispatch", "Inspector::InspectorBackendDispatcher::InspectorBackendDispatcher", "Inspector::InspectorBackendDispatcher::~InspectorBackendDispatcher", InspectorCommands, std::size(InspectorCommands)},
    {"LayerTree", "Inspector::LayerTreeBackendDispatcher::dispatch", "Inspector::LayerTreeBackendDispatcher::LayerTreeBackendDispatcher", "Inspector::LayerTreeBackendDispatcher::~LayerTreeBackendDispatcher", LayerTreeCommands, std::size(LayerTreeCommands)},
    {"Network", "Inspector::NetworkBackendDispatcher::dispatch", "Inspector::NetworkBackendDispatcher::NetworkBackendDispatcher", "Inspector::NetworkBackendDispatcher::~NetworkBackendDispatcher", NetworkCommands, std::size(NetworkCommands)},
    {"Page", "Inspector::PageBackendDispatcher::dispatch", "Inspector::PageBackendDispatcher::PageBackendDispatcher", "Inspector::PageBackendDispatcher::~PageBackendDispatcher", PageCommands, std::size(PageCommands)},
    {"Runtime", "Inspector::RuntimeBackendDispatcher::dispatch", "Inspector::RuntimeBackendDispatcher::RuntimeBackendDispatcher", "Inspector::RuntimeBackendDispatcher::~RuntimeBackendDispatcher", RuntimeCommands, std::size(RuntimeCommands)},
    {"ScriptProfiler", "Inspector::ScriptProfilerBackendDispatcher::dispatch", "Inspector::ScriptProfilerBackendDispatcher::ScriptProfilerBackendDispatcher", "Inspector::ScriptProfilerBackendDispatcher::~ScriptProfilerBackendDispatcher", ScriptProfilerCommands, std::size(ScriptProfilerCommands)},
    {"Target", "Inspector::TargetBackendDispatcher::dispatch", "Inspector::TargetBackendDispatcher::TargetBackendDispatcher", "Inspector::TargetBackendDispatcher::~TargetBackendDispatcher", TargetCommands, std::size(TargetCommands)},
    {"Timeline", "Inspector::TimelineBackendDispatcher::dispatch", "Inspector::TimelineBackendDispatcher::TimelineBackendDispatcher", "Inspector::TimelineBackendDispatcher::~TimelineBackendDispatcher", TimelineCommands, std::size(TimelineCommands)},
    {"Worker", "Inspector::WorkerBackendDispatcher::dispatch", "Inspector::WorkerBackendDispatcher::WorkerBackendDispatcher", "Inspector::WorkerBackendDispatcher::~WorkerBackendDispatcher", WorkerCommands, std::size(WorkerCommands)},
};

constexpr std::size_t DomainCount = std::size(Domains);
constexpr std::size_t MaximumArguments = 12;
constexpr std::size_t ResultBufferSize = 128;

struct DomainVtable {
    std::ptrdiff_t offsetToTop;
    const void* typeInfo;
    SupplementalBackendDispatcherVtable functions;
};

struct OptionalBoolean {
    bool value;
    bool hasValue;
};

struct OptionalInteger {
    int value;
    bool hasValue;
};

static_assert(sizeof(OptionalBoolean) == 2 && alignof(OptionalBoolean) == 1);
static_assert(sizeof(OptionalInteger) == 8 && alignof(OptionalInteger) == 4);

using HandlerCall = void (APS5_VABI*)(std::uintptr_t, std::uintptr_t, std::uintptr_t, std::uintptr_t, std::uintptr_t, std::uintptr_t, std::uintptr_t,
    std::uintptr_t, std::uintptr_t, std::uintptr_t, std::uintptr_t, std::uintptr_t);

SupplementalBackendDispatcher* Base(DomainBackendDispatcher* dispatcher) {
    return reinterpret_cast<SupplementalBackendDispatcher*>(dispatcher);
}

DomainBackendDispatcher& Checked(DomainBackendDispatcher* dispatcher, const char* function) {
    if (!dispatcher) throw std::invalid_argument(std::string(function) + " requires an object");
    return *dispatcher;
}

String Latin1(const std::u16string& text, bool is8Bit) {
    void* characters = nullptr;
    String result(NKWebKit::WtfAbi::CreateUninitialized(static_cast<unsigned>(text.size()), is8Bit, characters));
    for (std::size_t i = 0; i < text.size(); ++i) {
        if (is8Bit) static_cast<unsigned char*>(characters)[i] = static_cast<unsigned char>(text[i]);
        else static_cast<char16_t*>(characters)[i] = text[i];
    }
    return result;
}

String Ascii(const char* text) {
    std::u16string characters;
    while (*text) characters.push_back(static_cast<unsigned char>(*text++));
    return Latin1(characters, true);
}

bool Is8Bit(const String& string) {
    return !string.impl || (string.impl->hashAndFlags & (1U << 2));
}

void AppendString(std::u16string& output, const String& string) {
    if (!string.impl) return;
    const auto view = NKWebKit::WtfAbi::ViewOf(string.impl);
    for (unsigned i = 0; i < string.impl->length; ++i) output.push_back(NKWebKit::WtfAbi::CodeUnitAt(view, i));
}

struct Piece {
    const char* ascii;
    const String* string;
    Piece(const char* text) : ascii(text), string(nullptr) {}
    Piece(const String& text) : ascii(nullptr), string(&text) {}
};

String Concatenate(std::initializer_list<Piece> pieces) {
    std::u16string text;
    bool is8Bit = true;
    for (const Piece& piece : pieces) {
        if (piece.string) {
            AppendString(text, *piece.string);
            is8Bit = is8Bit && Is8Bit(*piece.string);
        } else {
            for (const char* c = piece.ascii; *c; ++c) text.push_back(static_cast<unsigned char>(*c));
        }
    }
    return Latin1(text, is8Bit);
}

bool EqualsAscii(const String& string, const char* text) {
    const std::size_t length = std::strlen(text);
    if (!string.impl || string.impl->length != length) return false;
    const auto view = NKWebKit::WtfAbi::ViewOf(string.impl);
    for (std::size_t i = 0; i < length; ++i)
        if (NKWebKit::WtfAbi::CodeUnitAt(view, static_cast<unsigned>(i)) != static_cast<unsigned char>(text[i])) return false;
    return true;
}

void Report(BackendDispatcher& backend, CommonErrorCode code, const String& message) {
    _ZN9Inspector17BackendDispatcher19reportProtocolErrorENS0_15CommonErrorCodeERKN3WTF6StringE(&backend, code, &message);
}

const char* TypeName(Getter getter) {
    switch (getter) {
    case Getter::Boolean: return "Boolean";
    case Getter::Integer: return "Integer";
    case Getter::String: return "String";
    case Getter::Object: return "Object";
    case Getter::Array: return "Array";
    }
    return "";
}

bool HasType(const Value& value, Getter getter) {
    switch (getter) {
    case Getter::Boolean: return value.valueType == ValueType::Boolean;
    case Getter::Integer: return value.valueType == ValueType::Double || value.valueType == ValueType::Integer;
    case Getter::String: return value.valueType == ValueType::String;
    case Getter::Object: return value.valueType == ValueType::Object;
    case Getter::Array: return value.valueType == ValueType::Array;
    }
    return false;
}

Value* GetProperty(BackendDispatcher& backend, Object* parameters, const ParamSpec& param) {
    const String key = Ascii(param.key);
    const String type = Ascii(TypeName(param.getter));
    if (!parameters) {
        if (param.required) Report(backend, CommonErrorCode::InvalidParams, Concatenate({"'params' object must contain required parameter '", key, "' with type '", type, "'."}));
        return nullptr;
    }
    Value* value = NKWebKit::JsonAbi::FindValue(*parameters, key);
    if (!value) {
        if (param.required) Report(backend, CommonErrorCode::InvalidParams, Concatenate({"Parameter '", key, "' with type '", type, "' was not found."}));
        return nullptr;
    }
    if (!HasType(*value, param.getter)) {
        Report(backend, CommonErrorCode::InvalidParams, Concatenate({"Parameter '", key, "' has wrong type. It must be '", type, "'."}));
        return nullptr;
    }
    return value;
}

int ToInteger(double number) {
    if (number >= -2147483648.0 && number < 2147483648.0) return static_cast<int>(number);
    return INT32_MIN;
}

int FindEnum(int list, const String& string) {
    if (!string.impl) return -1;
    const EnumList& values = EnumLists[list];
    for (std::size_t i = 0; i < values.count; ++i)
        if (EqualsAscii(string, values.values[i].name)) return values.values[i].value;
    return -1;
}

const char* EnumName(int list, unsigned value) {
    const EnumList& values = EnumLists[list];
    for (std::size_t i = 0; i < values.count; ++i)
        if (values.values[i].value == value) return values.values[i].name;
    throw std::out_of_range("Inspector protocol enum value is outside its type");
}

struct Argument {
    String string;
    Value* value = nullptr;
    CallbackBase* callback = nullptr;
    OptionalBoolean optionalBoolean{};
    OptionalInteger optionalInteger{};
    int integer = 0;
    bool boolean = false;
    ~Argument() {
        NKWebKit::JsonAbi::Deref(std::exchange(value, nullptr));
        DerefCallback(std::exchange(callback, nullptr));
    }
    static void DerefCallback(CallbackBase* callback) {
        if (!callback) return;
        if (callback->refCount != 1) {
            --callback->refCount;
            return;
        }
        _ZN9Inspector17BackendDispatcher12CallbackBaseD2Ev(callback);
        _ZN3WTF8fastFreeEPv(callback);
    }
};

std::size_t AlignUp(std::size_t value, std::size_t alignment) {
    return (value + alignment - 1) / alignment * alignment;
}

void OutLayout(OutKind kind, std::size_t& size, std::size_t& alignment) {
    switch (kind) {
    case OutKind::String:
    case OutKind::Double:
    case OutKind::RefValue:
    case OutKind::RefPtrValue: size = 8; alignment = 8; return;
    case OutKind::Integer:
    case OutKind::Enum: size = 4; alignment = 4; return;
    case OutKind::Boolean: size = 1; alignment = 1; return;
    case OutKind::OptionalBoolean: size = 2; alignment = 1; return;
    case OutKind::OptionalInteger: size = 8; alignment = 4; return;
    }
}

void SendResponse(BackendDispatcher& backend, std::int64_t requestId, Ref<Object>&& result) {
    Ref<Object> message = _ZN3WTF8JSONImpl6Object6createEv();
    NKWebKit::JsonAbi::SetValue(*message, Ascii("result"), Ref<Value>::Adopt(result.Leak()));
    NKWebKit::JsonAbi::SetValue(*message, Ascii("id"), _ZN3WTF8JSONImpl5Value6createEi(static_cast<int>(requestId)));
    const String text = NKWebKit::JsonAbi::ToJSONString(*message);
    _ZNK9Inspector14FrontendRouter12sendResponseERKN3WTF6StringE(backend.frontendRouter, &text);
}

void ReadOuts(const CommandSpec& command, unsigned char* buffer, Object& result) {
    std::size_t offset = 0;
    for (std::size_t i = 0; i < command.outCount; ++i) {
        const OutSpec& out = command.outs[i];
        std::size_t size = 0;
        std::size_t alignment = 1;
        OutLayout(out.kind, size, alignment);
        offset = AlignUp(offset, alignment);
        unsigned char* field = buffer + offset;
        offset += size;
        const String key = Ascii(out.key);
        switch (out.kind) {
        case OutKind::String: {
            String value(std::exchange(*reinterpret_cast<StringImpl**>(field), nullptr));
            if (out.conditional && !value.impl) break;
            NKWebKit::JsonAbi::SetValue(result, key, _ZN3WTF8JSONImpl5Value6createERKNS_6StringE(value));
            break;
        }
        case OutKind::Integer: {
            int value;
            std::memcpy(&value, field, sizeof(value));
            NKWebKit::JsonAbi::SetValue(result, key, _ZN3WTF8JSONImpl5Value6createEi(value));
            break;
        }
        case OutKind::Boolean:
            NKWebKit::JsonAbi::SetValue(result, key, _ZN3WTF8JSONImpl5Value6createEb(*field != 0));
            break;
        case OutKind::Double: {
            double value;
            std::memcpy(&value, field, sizeof(value));
            NKWebKit::JsonAbi::SetValue(result, key, _ZN3WTF8JSONImpl5Value6createEd(value));
            break;
        }
        case OutKind::RefValue:
        case OutKind::RefPtrValue: {
            Value* value = std::exchange(*reinterpret_cast<Value**>(field), nullptr);
            if (!value) {
                if (out.kind == OutKind::RefValue) throw std::invalid_argument("Inspector backend handler returned a null reference");
                break;
            }
            NKWebKit::JsonAbi::SetValue(result, key, Ref<Value>::Adopt(value));
            break;
        }
        case OutKind::OptionalBoolean: {
            const auto* value = reinterpret_cast<const OptionalBoolean*>(field);
            if (value->hasValue) NKWebKit::JsonAbi::SetValue(result, key, _ZN3WTF8JSONImpl5Value6createEb(value->value));
            break;
        }
        case OutKind::OptionalInteger: {
            OptionalInteger value;
            std::memcpy(&value, field, sizeof(value));
            if (value.hasValue) NKWebKit::JsonAbi::SetValue(result, key, _ZN3WTF8JSONImpl5Value6createEi(value.value));
            break;
        }
        case OutKind::Enum: {
            int value;
            std::memcpy(&value, field, sizeof(value));
            NKWebKit::JsonAbi::SetValue(result, key, _ZN3WTF8JSONImpl5Value6createERKNS_6StringE(Ascii(EnumName(out.enumList, static_cast<unsigned>(value)))));
            break;
        }
        }
    }
}

void ReleaseOuts(const CommandSpec& command, unsigned char* buffer) {
    std::size_t offset = 0;
    for (std::size_t i = 0; i < command.outCount; ++i) {
        std::size_t size = 0;
        std::size_t alignment = 1;
        OutLayout(command.outs[i].kind, size, alignment);
        offset = AlignUp(offset, alignment);
        unsigned char* field = buffer + offset;
        offset += size;
        if (command.outs[i].kind == OutKind::String) NKWebKit::WtfAbi::Release(std::exchange(*reinterpret_cast<StringImpl**>(field), nullptr));
        if (command.outs[i].kind == OutKind::RefValue || command.outs[i].kind == OutKind::RefPtrValue) NKWebKit::JsonAbi::Deref(std::exchange(*reinterpret_cast<Value**>(field), nullptr));
    }
}

std::size_t HasValueOffset(const CommandSpec& command) {
    std::size_t offset = 0;
    std::size_t tupleAlignment = 1;
    for (std::size_t i = 0; i < command.outCount; ++i) {
        std::size_t size = 0;
        std::size_t alignment = 1;
        OutLayout(command.outs[i].kind, size, alignment);
        offset = AlignUp(offset, alignment) + size;
        tupleAlignment = std::max(tupleAlignment, alignment);
    }
    const std::size_t storageAlignment = std::max<std::size_t>(tupleAlignment, alignof(StringImpl*));
    return AlignUp(std::max<std::size_t>(AlignUp(offset, tupleAlignment), sizeof(StringImpl*)), storageAlignment);
}

void RunCommand(const DomainSpec& domain, const CommandSpec& command, DomainBackendDispatcher& dispatcher, std::int64_t requestId, Object* parameters) {
    BackendDispatcher& backend = *dispatcher.backendDispatcher;
    std::array<Argument, MaximumArguments> arguments;
    std::array<Value*, MaximumArguments> found{};
    for (std::size_t i = 0; i < command.paramCount; ++i) {
        if (command.params[i].kind != ParamKind::Callback) found[i] = GetProperty(backend, parameters, command.params[i]);
    }
    if (_ZNK9Inspector17BackendDispatcher17hasProtocolErrorsEv(&backend)) {
        const String domainName = Ascii(domain.domain);
        const String commandName = Ascii(command.name);
        Report(backend, CommonErrorCode::InvalidParams, Concatenate({"Some arguments of method '", domainName, ".", commandName, "' can't be processed"}));
        return;
    }
    for (std::size_t i = 0; i < command.paramCount; ++i) {
        const ParamSpec& param = command.params[i];
        Argument& argument = arguments[i];
        Value* value = found[i];
        switch (param.kind) {
        case ParamKind::String:
            if (value) {
                argument.string.impl = value->data.string;
                if (argument.string.impl) argument.string.impl->refCount += 2;
            }
            break;
        case ParamKind::Integer:
            argument.integer = ToInteger(value->data.number);
            break;
        case ParamKind::Boolean:
            argument.boolean = value->data.boolean;
            break;
        case ParamKind::OptionalBoolean:
            if (value) argument.optionalBoolean = {value->data.boolean, true};
            break;
        case ParamKind::OptionalInteger:
            if (value) argument.optionalInteger = {ToInteger(value->data.number), true};
            break;
        case ParamKind::RefObject:
        case ParamKind::RefPtrObject:
        case ParamKind::RefArray:
        case ParamKind::RefPtrArray:
            if (value) {
                ++value->refCount;
                argument.value = value;
            }
            break;
        case ParamKind::Enum:
        case ParamKind::OptionalEnum: {
            String text;
            if (value) {
                text.impl = value->data.string;
                if (text.impl) text.impl->refCount += 2;
            }
            const int parsed = FindEnum(param.enumList, text);
            if (param.kind == ParamKind::OptionalEnum) {
                if (parsed >= 0) argument.optionalInteger = {parsed, true};
                break;
            }
            if (parsed < 0) {
                const String key = Ascii(param.key);
                Report(backend, CommonErrorCode::ServerError, Concatenate({"Unknown ", key, ": ", text}));
                return;
            }
            argument.integer = parsed;
            break;
        }
        case ParamKind::Callback: {
            auto* callback = static_cast<CallbackBase*>(_ZN3WTF10fastMallocEm(sizeof(CallbackBase)));
            NKWebKit::InspectorAbi::RefBackendDispatcher(backend);
            *callback = CallbackBase{1, {}, &backend, static_cast<int>(requestId), false, {}};
            argument.callback = callback;
            break;
        }
        }
    }
    void* agent = dispatcher.agent;
    if (!agent) throw std::invalid_argument(std::string(domain.dispatch) + " requires a handler");
    const auto* vtable = *static_cast<void* const* const*>(agent);
    alignas(16) unsigned char result[ResultBufferSize] = {};
    std::array<std::uintptr_t, MaximumArguments> registers{};
    std::size_t count = 0;
    if (!command.async) registers[count++] = reinterpret_cast<std::uintptr_t>(result);
    registers[count++] = reinterpret_cast<std::uintptr_t>(agent);
    for (std::size_t i = 0; i < command.paramCount; ++i) {
        Argument& argument = arguments[i];
        switch (command.params[i].kind) {
        case ParamKind::String: registers[count++] = reinterpret_cast<std::uintptr_t>(&argument.string); break;
        case ParamKind::Integer:
        case ParamKind::Enum: registers[count++] = static_cast<std::uintptr_t>(static_cast<std::intptr_t>(argument.integer)); break;
        case ParamKind::Boolean: registers[count++] = argument.boolean ? 1U : 0U; break;
        case ParamKind::OptionalBoolean: registers[count++] = reinterpret_cast<std::uintptr_t>(&argument.optionalBoolean); break;
        case ParamKind::OptionalInteger:
        case ParamKind::OptionalEnum: registers[count++] = reinterpret_cast<std::uintptr_t>(&argument.optionalInteger); break;
        case ParamKind::RefObject:
        case ParamKind::RefPtrObject:
        case ParamKind::RefArray:
        case ParamKind::RefPtrArray: registers[count++] = reinterpret_cast<std::uintptr_t>(&argument.value); break;
        case ParamKind::Callback: registers[count++] = reinterpret_cast<std::uintptr_t>(&argument.callback); break;
        }
    }
    const auto call = reinterpret_cast<HandlerCall>(vtable[command.slot]);
    call(registers[0], registers[1], registers[2], registers[3], registers[4], registers[5], registers[6], registers[7], registers[8], registers[9],
        registers[10], registers[11]);
    if (command.async) return;
    const std::size_t hasValueOffset = HasValueOffset(command);
    if (!result[hasValueOffset]) {
        String error(std::exchange(*reinterpret_cast<StringImpl**>(result), nullptr));
        Report(backend, CommonErrorCode::ServerError, error);
        return;
    }
    Ref<Object> response = _ZN3WTF8JSONImpl6Object6createEv();
    try {
        ReadOuts(command, result, *response);
    } catch (...) {
        ReleaseOuts(command, result);
        throw;
    }
    SendResponse(backend, requestId, std::move(response));
}

void Dispatch(std::size_t domainIndex, SupplementalBackendDispatcher* self, std::int64_t requestId, const String* method, Object** message) {
    const DomainSpec& domain = Domains[domainIndex];
    auto& dispatcher = Checked(reinterpret_cast<DomainBackendDispatcher*>(self), domain.dispatch);
    if (!method || !message || !*message) throw std::invalid_argument(std::string(domain.dispatch) + " requires a method and a message");
    ++dispatcher.refCount;
    struct Protector {
        DomainBackendDispatcher* dispatcher;
        ~Protector() { NKWebKit::InspectorAbi::DerefDomainBackendDispatcher(dispatcher); }
    } protect{&dispatcher};
    RefPtr<Object> parameters;
    if (Value* value = NKWebKit::JsonAbi::FindValue(**message, Ascii("params")); value && value->valueType == ValueType::Object)
        parameters = RefPtr<Object>(static_cast<Object*>(value));
    for (std::size_t i = 0; i < domain.commandCount; ++i) {
        if (EqualsAscii(*method, domain.commands[i].name)) {
            RunCommand(domain, domain.commands[i], dispatcher, requestId, parameters.Get());
            return;
        }
    }
    const String domainName = Ascii(domain.domain);
    Report(*dispatcher.backendDispatcher, CommonErrorCode::MethodNotFound, Concatenate({"'", domainName, ".", *method, "' was not found"}));
}

void Destroy(std::size_t domain, DomainBackendDispatcher* self) {
    _ZN9Inspector29SupplementalBackendDispatcherD2Ev(Base(&Checked(self, Domains[domain].destructor)));
}

void Delete(std::size_t domain, DomainBackendDispatcher* self) {
    Destroy(domain, self);
    _ZN3WTF8fastFreeEPv(self);
}

template<std::size_t Domain> void APS5_VABI CompleteDestructor(SupplementalBackendDispatcher* self) {
    Destroy(Domain, reinterpret_cast<DomainBackendDispatcher*>(self));
}

template<std::size_t Domain> void APS5_VABI DeletingDestructor(SupplementalBackendDispatcher* self) {
    Delete(Domain, reinterpret_cast<DomainBackendDispatcher*>(self));
}

template<std::size_t Domain> void APS5_VABI DispatchSlot(SupplementalBackendDispatcher* self, std::int64_t requestId, const String* method, Object** message) {
    Dispatch(Domain, self, requestId, method, message);
}

template<std::size_t... Domain> std::array<DomainVtable, sizeof...(Domain)> MakeVtables(std::index_sequence<Domain...>) {
    return {{{0, nullptr, {&CompleteDestructor<Domain>, &DeletingDestructor<Domain>, &DispatchSlot<Domain>}}...}};
}

const std::array<DomainVtable, DomainCount> Vtables = MakeVtables(std::make_index_sequence<DomainCount>());

void Construct(std::size_t domain, DomainBackendDispatcher* self, BackendDispatcher* backendDispatcher, void* agent) {
    auto& dispatcher = Checked(self, Domains[domain].constructor);
    if (!backendDispatcher) throw std::invalid_argument(std::string(Domains[domain].constructor) + " requires a backend dispatcher");
    _ZN9Inspector29SupplementalBackendDispatcherC2ERNS_17BackendDispatcherE(Base(&dispatcher), backendDispatcher);
    dispatcher.vptr = &Vtables[domain].functions;
    dispatcher.agent = agent;
    const String name = Ascii(Domains[domain].domain);
    _ZN9Inspector17BackendDispatcher27registerDispatcherForDomainERKN3WTF6StringEPNS_29SupplementalBackendDispatcherE(dispatcher.backendDispatcher, &name, Base(&dispatcher));
}

DomainBackendDispatcherRef Create(std::size_t domain, BackendDispatcher* backendDispatcher, void* agent) {
    if (!backendDispatcher) throw std::invalid_argument(std::string(Domains[domain].constructor) + " requires a backend dispatcher");
    auto* dispatcher = static_cast<DomainBackendDispatcher*>(_ZN3WTF10fastMallocEm(sizeof(DomainBackendDispatcher)));
    try {
        Construct(domain, dispatcher, backendDispatcher, agent);
    } catch (...) {
        _ZN3WTF8fastFreeEPv(dispatcher);
        throw;
    }
    return DomainBackendDispatcherRef(dispatcher);
}

}  // namespace

namespace NKWebKit::InspectorAbi {

void DerefDomainBackendDispatcher(DomainBackendDispatcher* dispatcher) {
    if (!dispatcher) return;
    if (dispatcher->refCount != 1) {
        --dispatcher->refCount;
        return;
    }
    dispatcher->vptr->deletingDestructor(Base(dispatcher));
}

DomainBackendDispatcherRef::~DomainBackendDispatcherRef() {
    DerefDomainBackendDispatcher(ptr);
}

}  // namespace NKWebKit::InspectorAbi

extern "C" {

DomainBackendDispatcherRef APS5_VABI _ZN9Inspector26AnimationBackendDispatcher6createERNS_17BackendDispatcherEPNS_33AnimationBackendDispatcherHandlerE(BackendDispatcher* backendDispatcher, void* agent) {
    return Create(0, backendDispatcher, agent);
}

void APS5_VABI _ZN9Inspector26AnimationBackendDispatcherC1ERNS_17BackendDispatcherEPNS_33AnimationBackendDispatcherHandlerE(DomainBackendDispatcher* self, BackendDispatcher* backendDispatcher, void* agent) {
    Construct(0, self, backendDispatcher, agent);
}

void APS5_VABI _ZN9Inspector26AnimationBackendDispatcherC2ERNS_17BackendDispatcherEPNS_33AnimationBackendDispatcherHandlerE(DomainBackendDispatcher* self, BackendDispatcher* backendDispatcher, void* agent) {
    Construct(0, self, backendDispatcher, agent);
}

void APS5_VABI _ZN9Inspector26AnimationBackendDispatcherD0Ev(DomainBackendDispatcher* self) {
    Delete(0, self);
}

DomainBackendDispatcherRef APS5_VABI _ZN9Inspector33ApplicationCacheBackendDispatcher6createERNS_17BackendDispatcherEPNS_40ApplicationCacheBackendDispatcherHandlerE(BackendDispatcher* backendDispatcher, void* agent) {
    return Create(1, backendDispatcher, agent);
}

void APS5_VABI _ZN9Inspector33ApplicationCacheBackendDispatcherC1ERNS_17BackendDispatcherEPNS_40ApplicationCacheBackendDispatcherHandlerE(DomainBackendDispatcher* self, BackendDispatcher* backendDispatcher, void* agent) {
    Construct(1, self, backendDispatcher, agent);
}

void APS5_VABI _ZN9Inspector33ApplicationCacheBackendDispatcherC2ERNS_17BackendDispatcherEPNS_40ApplicationCacheBackendDispatcherHandlerE(DomainBackendDispatcher* self, BackendDispatcher* backendDispatcher, void* agent) {
    Construct(1, self, backendDispatcher, agent);
}

void APS5_VABI _ZN9Inspector33ApplicationCacheBackendDispatcherD0Ev(DomainBackendDispatcher* self) {
    Delete(1, self);
}

DomainBackendDispatcherRef APS5_VABI _ZN9Inspector22AuditBackendDispatcher6createERNS_17BackendDispatcherEPNS_29AuditBackendDispatcherHandlerE(BackendDispatcher* backendDispatcher, void* agent) {
    return Create(2, backendDispatcher, agent);
}

void APS5_VABI _ZN9Inspector22AuditBackendDispatcherC1ERNS_17BackendDispatcherEPNS_29AuditBackendDispatcherHandlerE(DomainBackendDispatcher* self, BackendDispatcher* backendDispatcher, void* agent) {
    Construct(2, self, backendDispatcher, agent);
}

void APS5_VABI _ZN9Inspector22AuditBackendDispatcherC2ERNS_17BackendDispatcherEPNS_29AuditBackendDispatcherHandlerE(DomainBackendDispatcher* self, BackendDispatcher* backendDispatcher, void* agent) {
    Construct(2, self, backendDispatcher, agent);
}

void APS5_VABI _ZN9Inspector22AuditBackendDispatcherD0Ev(DomainBackendDispatcher* self) {
    Delete(2, self);
}

DomainBackendDispatcherRef APS5_VABI _ZN9Inspector24BrowserBackendDispatcher6createERNS_17BackendDispatcherEPNS_31BrowserBackendDispatcherHandlerE(BackendDispatcher* backendDispatcher, void* agent) {
    return Create(3, backendDispatcher, agent);
}

void APS5_VABI _ZN9Inspector24BrowserBackendDispatcherC1ERNS_17BackendDispatcherEPNS_31BrowserBackendDispatcherHandlerE(DomainBackendDispatcher* self, BackendDispatcher* backendDispatcher, void* agent) {
    Construct(3, self, backendDispatcher, agent);
}

void APS5_VABI _ZN9Inspector24BrowserBackendDispatcherC2ERNS_17BackendDispatcherEPNS_31BrowserBackendDispatcherHandlerE(DomainBackendDispatcher* self, BackendDispatcher* backendDispatcher, void* agent) {
    Construct(3, self, backendDispatcher, agent);
}

void APS5_VABI _ZN9Inspector24BrowserBackendDispatcherD0Ev(DomainBackendDispatcher* self) {
    Delete(3, self);
}

DomainBackendDispatcherRef APS5_VABI _ZN9Inspector23CanvasBackendDispatcher6createERNS_17BackendDispatcherEPNS_30CanvasBackendDispatcherHandlerE(BackendDispatcher* backendDispatcher, void* agent) {
    return Create(4, backendDispatcher, agent);
}

void APS5_VABI _ZN9Inspector23CanvasBackendDispatcherC1ERNS_17BackendDispatcherEPNS_30CanvasBackendDispatcherHandlerE(DomainBackendDispatcher* self, BackendDispatcher* backendDispatcher, void* agent) {
    Construct(4, self, backendDispatcher, agent);
}

void APS5_VABI _ZN9Inspector23CanvasBackendDispatcherD0Ev(DomainBackendDispatcher* self) {
    Delete(4, self);
}

DomainBackendDispatcherRef APS5_VABI _ZN9Inspector24ConsoleBackendDispatcher6createERNS_17BackendDispatcherEPNS_31ConsoleBackendDispatcherHandlerE(BackendDispatcher* backendDispatcher, void* agent) {
    return Create(5, backendDispatcher, agent);
}

void APS5_VABI _ZN9Inspector24ConsoleBackendDispatcherC1ERNS_17BackendDispatcherEPNS_31ConsoleBackendDispatcherHandlerE(DomainBackendDispatcher* self, BackendDispatcher* backendDispatcher, void* agent) {
    Construct(5, self, backendDispatcher, agent);
}

void APS5_VABI _ZN9Inspector24ConsoleBackendDispatcherC2ERNS_17BackendDispatcherEPNS_31ConsoleBackendDispatcherHandlerE(DomainBackendDispatcher* self, BackendDispatcher* backendDispatcher, void* agent) {
    Construct(5, self, backendDispatcher, agent);
}

DomainBackendDispatcherRef APS5_VABI _ZN9Inspector20CSSBackendDispatcher6createERNS_17BackendDispatcherEPNS_27CSSBackendDispatcherHandlerE(BackendDispatcher* backendDispatcher, void* agent) {
    return Create(6, backendDispatcher, agent);
}

void APS5_VABI _ZN9Inspector20CSSBackendDispatcherC1ERNS_17BackendDispatcherEPNS_27CSSBackendDispatcherHandlerE(DomainBackendDispatcher* self, BackendDispatcher* backendDispatcher, void* agent) {
    Construct(6, self, backendDispatcher, agent);
}

void APS5_VABI _ZN9Inspector20CSSBackendDispatcherC2ERNS_17BackendDispatcherEPNS_27CSSBackendDispatcherHandlerE(DomainBackendDispatcher* self, BackendDispatcher* backendDispatcher, void* agent) {
    Construct(6, self, backendDispatcher, agent);
}

void APS5_VABI _ZN9Inspector20CSSBackendDispatcherD0Ev(DomainBackendDispatcher* self) {
    Delete(6, self);
}

DomainBackendDispatcherRef APS5_VABI _ZN9Inspector25DatabaseBackendDispatcher6createERNS_17BackendDispatcherEPNS_32DatabaseBackendDispatcherHandlerE(BackendDispatcher* backendDispatcher, void* agent) {
    return Create(7, backendDispatcher, agent);
}

void APS5_VABI _ZN9Inspector25DatabaseBackendDispatcherC1ERNS_17BackendDispatcherEPNS_32DatabaseBackendDispatcherHandlerE(DomainBackendDispatcher* self, BackendDispatcher* backendDispatcher, void* agent) {
    Construct(7, self, backendDispatcher, agent);
}

void APS5_VABI _ZN9Inspector25DatabaseBackendDispatcherC2ERNS_17BackendDispatcherEPNS_32DatabaseBackendDispatcherHandlerE(DomainBackendDispatcher* self, BackendDispatcher* backendDispatcher, void* agent) {
    Construct(7, self, backendDispatcher, agent);
}

void APS5_VABI _ZN9Inspector25DatabaseBackendDispatcherD0Ev(DomainBackendDispatcher* self) {
    Delete(7, self);
}

DomainBackendDispatcherRef APS5_VABI _ZN9Inspector25DebuggerBackendDispatcher6createERNS_17BackendDispatcherEPNS_32DebuggerBackendDispatcherHandlerE(BackendDispatcher* backendDispatcher, void* agent) {
    return Create(8, backendDispatcher, agent);
}

void APS5_VABI _ZN9Inspector25DebuggerBackendDispatcherC1ERNS_17BackendDispatcherEPNS_32DebuggerBackendDispatcherHandlerE(DomainBackendDispatcher* self, BackendDispatcher* backendDispatcher, void* agent) {
    Construct(8, self, backendDispatcher, agent);
}

void APS5_VABI _ZN9Inspector25DebuggerBackendDispatcherC2ERNS_17BackendDispatcherEPNS_32DebuggerBackendDispatcherHandlerE(DomainBackendDispatcher* self, BackendDispatcher* backendDispatcher, void* agent) {
    Construct(8, self, backendDispatcher, agent);
}

void APS5_VABI _ZN9Inspector25DebuggerBackendDispatcherD0Ev(DomainBackendDispatcher* self) {
    Delete(8, self);
}

DomainBackendDispatcherRef APS5_VABI _ZN9Inspector20DOMBackendDispatcher6createERNS_17BackendDispatcherEPNS_27DOMBackendDispatcherHandlerE(BackendDispatcher* backendDispatcher, void* agent) {
    return Create(9, backendDispatcher, agent);
}

void APS5_VABI _ZN9Inspector20DOMBackendDispatcherC1ERNS_17BackendDispatcherEPNS_27DOMBackendDispatcherHandlerE(DomainBackendDispatcher* self, BackendDispatcher* backendDispatcher, void* agent) {
    Construct(9, self, backendDispatcher, agent);
}

void APS5_VABI _ZN9Inspector20DOMBackendDispatcherC2ERNS_17BackendDispatcherEPNS_27DOMBackendDispatcherHandlerE(DomainBackendDispatcher* self, BackendDispatcher* backendDispatcher, void* agent) {
    Construct(9, self, backendDispatcher, agent);
}

void APS5_VABI _ZN9Inspector20DOMBackendDispatcherD0Ev(DomainBackendDispatcher* self) {
    Delete(9, self);
}

DomainBackendDispatcherRef APS5_VABI _ZN9Inspector28DOMDebuggerBackendDispatcher6createERNS_17BackendDispatcherEPNS_35DOMDebuggerBackendDispatcherHandlerE(BackendDispatcher* backendDispatcher, void* agent) {
    return Create(10, backendDispatcher, agent);
}

void APS5_VABI _ZN9Inspector28DOMDebuggerBackendDispatcherC1ERNS_17BackendDispatcherEPNS_35DOMDebuggerBackendDispatcherHandlerE(DomainBackendDispatcher* self, BackendDispatcher* backendDispatcher, void* agent) {
    Construct(10, self, backendDispatcher, agent);
}

void APS5_VABI _ZN9Inspector28DOMDebuggerBackendDispatcherC2ERNS_17BackendDispatcherEPNS_35DOMDebuggerBackendDispatcherHandlerE(DomainBackendDispatcher* self, BackendDispatcher* backendDispatcher, void* agent) {
    Construct(10, self, backendDispatcher, agent);
}

DomainBackendDispatcherRef APS5_VABI _ZN9Inspector27DOMStorageBackendDispatcher6createERNS_17BackendDispatcherEPNS_34DOMStorageBackendDispatcherHandlerE(BackendDispatcher* backendDispatcher, void* agent) {
    return Create(11, backendDispatcher, agent);
}

void APS5_VABI _ZN9Inspector27DOMStorageBackendDispatcherC1ERNS_17BackendDispatcherEPNS_34DOMStorageBackendDispatcherHandlerE(DomainBackendDispatcher* self, BackendDispatcher* backendDispatcher, void* agent) {
    Construct(11, self, backendDispatcher, agent);
}

void APS5_VABI _ZN9Inspector27DOMStorageBackendDispatcherC2ERNS_17BackendDispatcherEPNS_34DOMStorageBackendDispatcherHandlerE(DomainBackendDispatcher* self, BackendDispatcher* backendDispatcher, void* agent) {
    Construct(11, self, backendDispatcher, agent);
}

void APS5_VABI _ZN9Inspector27DOMStorageBackendDispatcherD0Ev(DomainBackendDispatcher* self) {
    Delete(11, self);
}

void APS5_VABI _ZN9Inspector21HeapBackendDispatcherC1ERNS_17BackendDispatcherEPNS_28HeapBackendDispatcherHandlerE(DomainBackendDispatcher* self, BackendDispatcher* backendDispatcher, void* agent) {
    Construct(12, self, backendDispatcher, agent);
}

void APS5_VABI _ZN9Inspector21HeapBackendDispatcherC2ERNS_17BackendDispatcherEPNS_28HeapBackendDispatcherHandlerE(DomainBackendDispatcher* self, BackendDispatcher* backendDispatcher, void* agent) {
    Construct(12, self, backendDispatcher, agent);
}

DomainBackendDispatcherRef APS5_VABI _ZN9Inspector26InspectorBackendDispatcher6createERNS_17BackendDispatcherEPNS_33InspectorBackendDispatcherHandlerE(BackendDispatcher* backendDispatcher, void* agent) {
    return Create(13, backendDispatcher, agent);
}

void APS5_VABI _ZN9Inspector26InspectorBackendDispatcherC1ERNS_17BackendDispatcherEPNS_33InspectorBackendDispatcherHandlerE(DomainBackendDispatcher* self, BackendDispatcher* backendDispatcher, void* agent) {
    Construct(13, self, backendDispatcher, agent);
}

void APS5_VABI _ZN9Inspector26InspectorBackendDispatcherC2ERNS_17BackendDispatcherEPNS_33InspectorBackendDispatcherHandlerE(DomainBackendDispatcher* self, BackendDispatcher* backendDispatcher, void* agent) {
    Construct(13, self, backendDispatcher, agent);
}

void APS5_VABI _ZN9Inspector26InspectorBackendDispatcherD0Ev(DomainBackendDispatcher* self) {
    Delete(13, self);
}

DomainBackendDispatcherRef APS5_VABI _ZN9Inspector26LayerTreeBackendDispatcher6createERNS_17BackendDispatcherEPNS_33LayerTreeBackendDispatcherHandlerE(BackendDispatcher* backendDispatcher, void* agent) {
    return Create(14, backendDispatcher, agent);
}

void APS5_VABI _ZN9Inspector26LayerTreeBackendDispatcherC1ERNS_17BackendDispatcherEPNS_33LayerTreeBackendDispatcherHandlerE(DomainBackendDispatcher* self, BackendDispatcher* backendDispatcher, void* agent) {
    Construct(14, self, backendDispatcher, agent);
}

void APS5_VABI _ZN9Inspector26LayerTreeBackendDispatcherC2ERNS_17BackendDispatcherEPNS_33LayerTreeBackendDispatcherHandlerE(DomainBackendDispatcher* self, BackendDispatcher* backendDispatcher, void* agent) {
    Construct(14, self, backendDispatcher, agent);
}

void APS5_VABI _ZN9Inspector26LayerTreeBackendDispatcherD0Ev(DomainBackendDispatcher* self) {
    Delete(14, self);
}

DomainBackendDispatcherRef APS5_VABI _ZN9Inspector24NetworkBackendDispatcher6createERNS_17BackendDispatcherEPNS_31NetworkBackendDispatcherHandlerE(BackendDispatcher* backendDispatcher, void* agent) {
    return Create(15, backendDispatcher, agent);
}

void APS5_VABI _ZN9Inspector24NetworkBackendDispatcherC1ERNS_17BackendDispatcherEPNS_31NetworkBackendDispatcherHandlerE(DomainBackendDispatcher* self, BackendDispatcher* backendDispatcher, void* agent) {
    Construct(15, self, backendDispatcher, agent);
}

void APS5_VABI _ZN9Inspector24NetworkBackendDispatcherC2ERNS_17BackendDispatcherEPNS_31NetworkBackendDispatcherHandlerE(DomainBackendDispatcher* self, BackendDispatcher* backendDispatcher, void* agent) {
    Construct(15, self, backendDispatcher, agent);
}

void APS5_VABI _ZN9Inspector24NetworkBackendDispatcherD0Ev(DomainBackendDispatcher* self) {
    Delete(15, self);
}

DomainBackendDispatcherRef APS5_VABI _ZN9Inspector21PageBackendDispatcher6createERNS_17BackendDispatcherEPNS_28PageBackendDispatcherHandlerE(BackendDispatcher* backendDispatcher, void* agent) {
    return Create(16, backendDispatcher, agent);
}

void APS5_VABI _ZN9Inspector21PageBackendDispatcherC1ERNS_17BackendDispatcherEPNS_28PageBackendDispatcherHandlerE(DomainBackendDispatcher* self, BackendDispatcher* backendDispatcher, void* agent) {
    Construct(16, self, backendDispatcher, agent);
}

void APS5_VABI _ZN9Inspector21PageBackendDispatcherC2ERNS_17BackendDispatcherEPNS_28PageBackendDispatcherHandlerE(DomainBackendDispatcher* self, BackendDispatcher* backendDispatcher, void* agent) {
    Construct(16, self, backendDispatcher, agent);
}

DomainBackendDispatcherRef APS5_VABI _ZN9Inspector24RuntimeBackendDispatcher6createERNS_17BackendDispatcherEPNS_31RuntimeBackendDispatcherHandlerE(BackendDispatcher* backendDispatcher, void* agent) {
    return Create(17, backendDispatcher, agent);
}

void APS5_VABI _ZN9Inspector24RuntimeBackendDispatcherC1ERNS_17BackendDispatcherEPNS_31RuntimeBackendDispatcherHandlerE(DomainBackendDispatcher* self, BackendDispatcher* backendDispatcher, void* agent) {
    Construct(17, self, backendDispatcher, agent);
}

void APS5_VABI _ZN9Inspector24RuntimeBackendDispatcherC2ERNS_17BackendDispatcherEPNS_31RuntimeBackendDispatcherHandlerE(DomainBackendDispatcher* self, BackendDispatcher* backendDispatcher, void* agent) {
    Construct(17, self, backendDispatcher, agent);
}

void APS5_VABI _ZN9Inspector24RuntimeBackendDispatcherD0Ev(DomainBackendDispatcher* self) {
    Delete(17, self);
}

DomainBackendDispatcherRef APS5_VABI _ZN9Inspector31ScriptProfilerBackendDispatcher6createERNS_17BackendDispatcherEPNS_38ScriptProfilerBackendDispatcherHandlerE(BackendDispatcher* backendDispatcher, void* agent) {
    return Create(18, backendDispatcher, agent);
}

void APS5_VABI _ZN9Inspector31ScriptProfilerBackendDispatcherC1ERNS_17BackendDispatcherEPNS_38ScriptProfilerBackendDispatcherHandlerE(DomainBackendDispatcher* self, BackendDispatcher* backendDispatcher, void* agent) {
    Construct(18, self, backendDispatcher, agent);
}

void APS5_VABI _ZN9Inspector31ScriptProfilerBackendDispatcherC2ERNS_17BackendDispatcherEPNS_38ScriptProfilerBackendDispatcherHandlerE(DomainBackendDispatcher* self, BackendDispatcher* backendDispatcher, void* agent) {
    Construct(18, self, backendDispatcher, agent);
}

void APS5_VABI _ZN9Inspector31ScriptProfilerBackendDispatcherD0Ev(DomainBackendDispatcher* self) {
    Delete(18, self);
}

DomainBackendDispatcherRef APS5_VABI _ZN9Inspector23TargetBackendDispatcher6createERNS_17BackendDispatcherEPNS_30TargetBackendDispatcherHandlerE(BackendDispatcher* backendDispatcher, void* agent) {
    return Create(19, backendDispatcher, agent);
}

void APS5_VABI _ZN9Inspector23TargetBackendDispatcherC1ERNS_17BackendDispatcherEPNS_30TargetBackendDispatcherHandlerE(DomainBackendDispatcher* self, BackendDispatcher* backendDispatcher, void* agent) {
    Construct(19, self, backendDispatcher, agent);
}

void APS5_VABI _ZN9Inspector23TargetBackendDispatcherC2ERNS_17BackendDispatcherEPNS_30TargetBackendDispatcherHandlerE(DomainBackendDispatcher* self, BackendDispatcher* backendDispatcher, void* agent) {
    Construct(19, self, backendDispatcher, agent);
}

void APS5_VABI _ZN9Inspector23TargetBackendDispatcherD0Ev(DomainBackendDispatcher* self) {
    Delete(19, self);
}

DomainBackendDispatcherRef APS5_VABI _ZN9Inspector25TimelineBackendDispatcher6createERNS_17BackendDispatcherEPNS_32TimelineBackendDispatcherHandlerE(BackendDispatcher* backendDispatcher, void* agent) {
    return Create(20, backendDispatcher, agent);
}

void APS5_VABI _ZN9Inspector25TimelineBackendDispatcherC1ERNS_17BackendDispatcherEPNS_32TimelineBackendDispatcherHandlerE(DomainBackendDispatcher* self, BackendDispatcher* backendDispatcher, void* agent) {
    Construct(20, self, backendDispatcher, agent);
}

void APS5_VABI _ZN9Inspector25TimelineBackendDispatcherC2ERNS_17BackendDispatcherEPNS_32TimelineBackendDispatcherHandlerE(DomainBackendDispatcher* self, BackendDispatcher* backendDispatcher, void* agent) {
    Construct(20, self, backendDispatcher, agent);
}

void APS5_VABI _ZN9Inspector25TimelineBackendDispatcherD0Ev(DomainBackendDispatcher* self) {
    Delete(20, self);
}

DomainBackendDispatcherRef APS5_VABI _ZN9Inspector23WorkerBackendDispatcher6createERNS_17BackendDispatcherEPNS_30WorkerBackendDispatcherHandlerE(BackendDispatcher* backendDispatcher, void* agent) {
    return Create(21, backendDispatcher, agent);
}

void APS5_VABI _ZN9Inspector23WorkerBackendDispatcherC1ERNS_17BackendDispatcherEPNS_30WorkerBackendDispatcherHandlerE(DomainBackendDispatcher* self, BackendDispatcher* backendDispatcher, void* agent) {
    Construct(21, self, backendDispatcher, agent);
}

void APS5_VABI _ZN9Inspector23WorkerBackendDispatcherC2ERNS_17BackendDispatcherEPNS_30WorkerBackendDispatcherHandlerE(DomainBackendDispatcher* self, BackendDispatcher* backendDispatcher, void* agent) {
    Construct(21, self, backendDispatcher, agent);
}

void APS5_VABI _ZN9Inspector23WorkerBackendDispatcherD0Ev(DomainBackendDispatcher* self) {
    Delete(21, self);
}
}
