#include "prx/libSceJsc/include/Api.hpp"
#include "prx/libc/include/General.hpp"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <memory>
#include <stdexcept>
#include <vector>



namespace {

std::unique_ptr<GuestJSString> NullString() {
    auto result = std::make_unique<GuestJSString>();
    result->nullString = true;
    result->characters.push_back(0);
    return result;
}

std::unique_ptr<GuestJSString> Decode(const char* text) {
    if (!text) return NullString();
    if (!GuestJSStringValidLength<char8_t>(std::strlen(text))) throw std::length_error("JSString: UTF-8 length above the string limit");
    auto result = std::make_unique<GuestJSString>();
    bool ascii = true;
    const auto* cursor = reinterpret_cast<const unsigned char*>(text);
    while (*cursor) {
        std::uint32_t codepoint = *cursor++;
        if (codepoint >= 0x80) ascii = false;
        unsigned following = 0;
        std::uint32_t minimum = 0;
        if (codepoint >= 0xC2 && codepoint <= 0xDF) {
            codepoint &= 0x1F;
            following = 1;
            minimum = 0x80;
        } else if (codepoint >= 0xE0 && codepoint <= 0xEF) {
            codepoint &= 0x0F;
            following = 2;
            minimum = 0x800;
        } else if (codepoint >= 0xF0 && codepoint <= 0xF4) {
            codepoint &= 7;
            following = 3;
            minimum = 0x10000;
        } else if (codepoint >= 0x80) {
            result->nullString = true;
            break;
        }
        while (following > 0) {
            --following;
            if ((*cursor & 0xC0) != 0x80) {
                result->nullString = true;
                break;
            }
            codepoint = codepoint << 6 | (*cursor++ & 0x3F);
        }
        if (result->nullString || codepoint < minimum || codepoint > 0x10FFFF || (codepoint >= 0xD800 && codepoint <= 0xDFFF)) {
            result->nullString = true;
            break;
        }
        if (codepoint > 0xFFFF) {
            const auto supplementary = codepoint - 0x10000;
            result->characters.push_back(static_cast<std::uint16_t>(0xD800 + (supplementary >> 10)));
            result->characters.push_back(static_cast<std::uint16_t>(0xDC00 + (supplementary & 0x3FF)));
        } else result->characters.push_back(static_cast<std::uint16_t>(codepoint));
    }
    if (result->nullString) result->characters.clear();
    else if (!ascii && !GuestJSStringValidLength<char16_t>(result->characters.size())) throw std::length_error("JSString: UTF-16 length above the string limit");
    result->characters.push_back(0);
    return result;
}

bool Equal(const GuestJSString* first, const GuestJSString* second) {
    if (first == second) return true;
    if (!first || !second) return false;
    return first->nullString == second->nullString && first->characters == second->characters;
}

}

extern "C" {

GuestJSString* APS5_VABI JSStringCreateWithCharacters_nid_postfix(const std::uint16_t* characters, std::size_t count) {
    if (!characters) return NullString().release();
    if (!GuestJSStringValidLength<char16_t>(count)) throw std::length_error("JSStringCreateWithCharacters: length above the string limit");
    auto string = std::make_unique<GuestJSString>();
    string->characters.assign(characters, characters + count);
    string->characters.push_back(0);
    return string.release();
}

GuestJSString* APS5_VABI JSStringCreateWithUTF8CString_nid_postfix(const char* text) {
    return Decode(text).release();
}

GuestJSString* APS5_VABI JSStringRetain_nid_postfix(GuestJSString* string) {
    if (!string) throw std::invalid_argument("JSStringRetain: null string");
    string->references.fetch_add(1, std::memory_order_relaxed);
    return string;
}

void APS5_VABI JSStringRelease_nid_postfix(GuestJSString* string) {
    if (!string) throw std::invalid_argument("JSStringRelease: null string");
    if (string->references.fetch_sub(1, std::memory_order_acq_rel) == 1) delete string;
}

std::size_t APS5_VABI JSStringGetLength_nid_postfix(const GuestJSString* string) {
    return string ? string->characters.size() - 1 : 0;
}

const std::uint16_t* APS5_VABI JSStringGetCharactersPtr_nid_postfix(const GuestJSString* string) {
    return string && !string->nullString ? string->characters.data() : nullptr;
}

std::size_t APS5_VABI JSStringGetMaximumUTF8CStringSize_nid_postfix(const GuestJSString* string) {
    if (!string) throw std::invalid_argument("JSStringGetMaximumUTF8CStringSize: null string");
    const auto length = static_cast<std::uint32_t>(string->characters.size() - 1);
    return static_cast<std::uint32_t>(length * 3u + 1u);
}

std::size_t APS5_VABI JSStringGetUTF8CString_nid_postfix(const GuestJSString* string, char* buffer, std::size_t bufferSize) {
    if (!string || !buffer || !bufferSize) return 0;
    std::size_t written = 0;
    const auto count = string->characters.size() - 1;
    for (std::size_t index = 0; index < count; ++index) {
        std::uint32_t codepoint = string->characters[index];
        if (codepoint >= 0xD800 && codepoint <= 0xDBFF) {
            if (index + 1 == count) break;
            const auto low = string->characters[index + 1];
            if (low < 0xDC00 || low > 0xDFFF) break;
            codepoint = 0x10000 + ((codepoint - 0xD800) << 10) + low - 0xDC00;
            ++index;
        } else if (codepoint >= 0xDC00 && codepoint <= 0xDFFF) break;
        const std::size_t bytes = codepoint < 0x80 ? 1 : codepoint < 0x800 ? 2 : codepoint < 0x10000 ? 3 : 4;
        if (bytes > bufferSize - 1 - written) break;
        if (bytes == 1) buffer[written++] = static_cast<char>(codepoint);
        else {
            const unsigned prefix = bytes == 2 ? 0xC0 : bytes == 3 ? 0xE0 : 0xF0;
            buffer[written++] = static_cast<char>(prefix | (codepoint >> (6 * (bytes - 1))));
            for (auto remaining = bytes - 1; remaining > 0; --remaining) {
                buffer[written++] = static_cast<char>(0x80 | ((codepoint >> (6 * (remaining - 1))) & 0x3F));
            }
        }
    }
    buffer[written] = '\0';
    return written + 1;
}

bool APS5_VABI JSStringIsEqual(const GuestJSString* first, const GuestJSString* second) {
    return Equal(first, second);
}

bool APS5_VABI JSStringIsEqualToUTF8CString(const GuestJSString* string, const char* text) {
    const auto other = Decode(text);
    return Equal(string, other.get());
}

int APS5_VABI _ZN3JSC7JSProxy13visitChildrenEPNS_6JSCellERNS_11SlotVisitorE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3JSC25JSSegmentedVariableObject11analyzeHeapEPNS_6JSCellERNS_12HeapAnalyzerE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN9Inspector20DOMBackendDispatcher29setBreakpointForEventListenerElON3WTF6RefPtrINS1_8JSONImpl6ObjectENS1_13DumbPtrTraitsIS4_EEEE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN9Inspector20DOMBackendDispatcher16getSearchResultsElON3WTF6RefPtrINS1_8JSONImpl6ObjectENS1_13DumbPtrTraitsIS4_EEEE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3JSC8JSObject12getPrototypeEPS0_PNS_14JSGlobalObjectE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN9Inspector24RuntimeBackendDispatcher8dispatchElRKN3WTF6StringEONS1_3RefINS1_8JSONImpl6ObjectENS1_13DumbPtrTraitsIS7_EEEE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3JSC2VM10ClientDataD1Ev() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3JSC17JSArrayBufferView14finishCreationERNS_2VME() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN9Inspector14InspectorAgentC1ERNS_12AgentContextE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3JSC17FunctionOverrides18reinstallOverridesEv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3JSC22JSFinalizationRegistry9deadCountERKN3WTF6LockerINS_10JSCellLockEEE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNK3WTF6String12toUIntStrictEPbi() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3WTF20ParallelHelperClientC1EONS_6RefPtrINS_18ParallelHelperPoolENS_13DumbPtrTraitsIS2_EEEE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3JSC11ProxyObjectC1ERNS_2VMEPNS_9StructureE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN9Inspector22InspectorDebuggerAgentnwEmPv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN7bmalloc15IsoHeapImplBasenwEmPv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3JSC17DebuggerCallFrame20positionForCallFrameERNS_2VMEPNS_9CallFrameE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3WTF24AutomaticThreadCondition6createEv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3JSC16InternalFunctionC1ERNS_2VMEPNS_9StructureENS_14NativeFunctionES5_() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3JSC22FullGCActivityCallbackD0Ev() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN9Inspector20DOMBackendDispatcher13performSearchElON3WTF6RefPtrINS1_8JSONImpl6ObjectENS1_13DumbPtrTraitsIS4_EEEE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3JSC2VM14sharedInstanceEv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN9Inspector8Protocol16InspectorHelpers24parseEnumValueFromStringINS0_7Runtime20ExecutionContextTypeEEEN3WTF8OptionalIT_EERKNS5_6StringE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN9Inspector8Protocol16InspectorHelpers24parseEnumValueFromStringINS0_4Page16CoordinateSystemEEEN3WTF8OptionalIT_EERKNS5_6StringE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN9Inspector23TargetBackendDispatcher15setPauseOnStartElON3WTF6RefPtrINS1_8JSONImpl6ObjectENS1_13DumbPtrTraitsIS4_EEEE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNK3JSC8Profiler8Database6toJSONEv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN9Inspector8Protocol16InspectorHelpers24parseEnumValueFromStringINS0_7Console14ConsoleMessage5LevelEEEN3WTF8OptionalIT_EERKNS6_6StringE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3JSC22resetSuperSamplerStateEv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3JSC7JSProxy9setTargetERNS_2VMEPNS_14JSGlobalObjectE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNK3WTF8JSONImpl9ArrayBase3endEv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN9Inspector14InspectorAgent29willDestroyFrontendAndBackendENS_16DisconnectReasonE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN9Inspector22RemoteInspectionTarget34pauseWaitingForAutomaticInspectionEv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3JSC7Options24ensureOptionsAreCoherentEv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI JSObjectHasPropertyForKey() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN9Inspector22ContentSearchUtilities11lineEndingsERKN3WTF6StringE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN9Inspector14InspectorAgentnaEm() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN9Inspector24ConsoleBackendDispatcherD0Ev() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3JSC12StringObjectC1ERNS_2VMEPNS_9StructureE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3JSC14setNeverInlineENS_7JSValueE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3JSC16getFunctionRealmERNS_2VMEPNS_8JSObjectE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3WTF10TextStreamlsERKNS0_30FormatNumberRespectingIntegersE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNK3WTF10StringImpl21findIgnoringASCIICaseEPKS0_() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3JSC9Structure26flattenDictionaryStructureERNS_2VMEPNS_8JSObjectE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN7bmalloc9Allocator14reallocateImplEPvmNS_13FailureActionE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3WTF13MetaAllocatorD1Ev() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZThn16_N9Inspector19InspectorAuditAgentD1Ev() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN9Inspector21PageBackendDispatcherD0Ev() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN9Inspector21HeapBackendDispatcherD0Ev() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN9Inspector15ScriptCallStackC2ERN3WTF6VectorINS_15ScriptCallFrameELm0ENS1_15CrashOnOverflowELm16ENS1_10FastMallocEEE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN9Inspector32TimelineBackendDispatcherHandlerC2Ev() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN9Inspector31NetworkBackendDispatcherHandler20LoadResourceCallbackC1EON3WTF3RefINS_17BackendDispatcherENS2_13DumbPtrTraitsIS4_EEEEi() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN9Inspector24NetworkBackendDispatcher28interceptRequestWithResponseElON3WTF6RefPtrINS1_8JSONImpl6ObjectENS1_13DumbPtrTraitsIS4_EEEE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3JSC12CachePayloadD2Ev() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3WTF13StringBuilder6resizeEj() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3JSC2VM29scopedArgumentsTableSpaceSlowEv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN9Inspector14ConsoleMessageC2EN3JSC13MessageSourceENS1_11MessageTypeENS1_12MessageLevelERKN3WTF6StringEONS5_3RefINS_15ScriptArgumentsENS5_13DumbPtrTraitsISA_EEEEPNS1_14JSGlobalObjectEm() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN9Inspector13AgentRegistryC1Ev() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3JSC2VM16weakSetSpaceSlowEv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3JSC17checkModuleSyntaxEPNS_14JSGlobalObjectERKNS_10SourceCodeERNS_11ParserErrorE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN9Inspector14InspectorAgentD1Ev() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN9Inspector22InspectorDebuggerAgentnaEm() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3WTF15base64URLDecodeERKNS_6StringENS_33SignedOrUnsignedCharVectorAdapterE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN9Inspector20DOMBackendDispatcher24setEventListenerDisabledElON3WTF6RefPtrINS1_8JSONImpl6ObjectENS1_13DumbPtrTraitsIS4_EEEE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN9Inspector32TimelineBackendDispatcherHandlerD1Ev() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN9Inspector22InspectorDebuggerAgent27didCreateFrontendAndBackendEPNS_14FrontendRouterEPNS_17BackendDispatcherE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3WTF7RunLoop6wakeUpEv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3WTF6StringC2ENS_12ASCIILiteralE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNK3WTF7SecondsplERKNS_24TimeWithDynamicClockTypeE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3WTF3URLC1ERKS0_RKNS_6StringEPKNS_15URLTextEncodingE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN9Inspector14InjectedScript14callFunctionOnERN3WTF6StringERKS2_S5_S5_bbRNS1_6RefPtrINS_8Protocol7Runtime12RemoteObjectENS1_13DumbPtrTraitsIS9_EEEERNS1_8OptionalIbEE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN9Inspector21InspectorConsoleAgent17addConsoleMessageESt10unique_ptrINS_14ConsoleMessageESt14default_deleteIS2_EE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN9Inspector23CanvasBackendDispatcherC2ERNS_17BackendDispatcherEPNS_30CanvasBackendDispatcherHandlerE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN9Inspector31NetworkBackendDispatcherHandler20LoadResourceCallbackD2Ev() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3WTF14FileSystemImpl12readFromFileEiPci() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNK3JSC14ProtoCallFrame9codeBlockEv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNK3WTF24TimeWithDynamicClockTypeltERKS0_() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3JSC11VMInspector9dumpValueENS_7JSValueE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3WTF7RunLoop9TimerBaseD0Ev() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN9Inspector26TimelineFrontendDispatcher13eventRecordedEN3WTF6RefPtrINS_8Protocol8Timeline13TimelineEventENS1_13DumbPtrTraitsIS5_EEEE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3JSC16setNeverOptimizeENS_7JSValueE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN9Inspector25DebuggerBackendDispatcher13setBreakpointElON3WTF6RefPtrINS1_8JSONImpl6ObjectENS1_13DumbPtrTraitsIS4_EEEE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3WTF10StringImpl8utf8ImplEPKDsjRPcmNS_14ConversionModeE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN7bmalloc9Allocator16allocateSlowCaseEmNS_13FailureActionE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI JSWeakObjectMapCreate() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3JSC27threadSpecificAssemblerDataEv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN9Inspector32TimelineBackendDispatcherHandlerD0Ev() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN9Inspector24CanvasFrontendDispatcher17recordingProgressERKN3WTF6StringENS1_6RefPtrINS1_8JSONImpl7ArrayOfINS_8Protocol9Recording5FrameEEENS1_13DumbPtrTraitsISB_EEEEi() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN9Inspector24RuntimeBackendDispatcher25enableControlFlowProfilerElON3WTF6RefPtrINS1_8JSONImpl6ObjectENS1_13DumbPtrTraitsIS4_EEEE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNK3WTF3URL5queryEv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3JSC6JSLock12DropAllLocksC2EPNS_14JSGlobalObjectE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN7bmalloc8FreeListC2Ev() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN9Inspector15RemoteInspector9setClientEPNS0_6ClientE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3JSC14JSGlobalObject13visitChildrenEPNS_6JSCellERNS_11SlotVisitorE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN9Inspector22RemoteInspectionTargetD0Ev() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3JSC12StringObject18getOwnPropertySlotEPNS_8JSObjectEPNS_14JSGlobalObjectENS_12PropertyNameERNS_12PropertySlotE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN9Inspector21InspectorConsoleAgent11startTimingEPN3JSC14JSGlobalObjectERKN3WTF6StringE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZThn16_N9Inspector21InspectorConsoleAgent13clearMessagesERN3WTF6StringE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3WTF9BitVector10filterSlowERKS0_() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN9Inspector21CSSFrontendDispatcher15styleSheetAddedEN3WTF6RefPtrINS_8Protocol3CSS19CSSStyleSheetHeaderENS1_13DumbPtrTraitsIS5_EEEE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3JSC18sanitizeStackForVMERNS_2VME() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3JSC6Config23enableRestrictedOptionsEv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI WTFLogAlwaysV() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3JSC2VM22boundFunctionSpaceSlowEv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3WTF3URL24removeFragmentIdentifierEv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN9Inspector15RemoteInspector9singletonEv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3WTF11writeIndentERNS_10TextStreamEi() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3JSC19numberOfDFGCompilesEPNS_14JSGlobalObjectEPNS_9CallFrameE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN9Inspector22PageFrontendDispatcher31frameClearedScheduledNavigationERKN3WTF6StringE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN9Inspector21HeapBackendDispatcher6createERNS_17BackendDispatcherEPNS_28HeapBackendDispatcherHandlerE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3JSC21MarkedJSValueRefArrayD2Ev() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3WTF11ThreadGroupD1Ev() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZThn24_N9Inspector18InspectorHeapAgentD0Ev() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN9Inspector32DebuggerBackendDispatcherHandlerD2Ev() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN9Inspector28DOMDebuggerBackendDispatcherD0Ev() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3WTF4Lock16unlockFairlySlowEv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN9Inspector14ConsoleMessagenwEm10NotNullTagPv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3JSC10JSFunction6createERNS_2VMEPNS_14JSGlobalObjectEiRKN3WTF6StringENS_14NativeFunctionENS_9IntrinsicES9_PKNS_6DOMJIT9SignatureE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNK9Inspector15RemoteInspector26listingForAutomationTargetERKNS_22RemoteAutomationTargetE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN9Inspector28InspectorScriptProfilerAgentD2Ev() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3WTF14FileSystemImpl15getFileDeviceIdERKNS_7CStringE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN7bmalloc3api16freeLargeVirtualEPvmNS_8HeapKindE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3JSC2VM18generatorSpaceSlowEv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNK3WTF8JSONImpl9ArrayBase6lengthEv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3JSC8Debugger17resolveBreakpointERNS_10BreakpointEPNS_14SourceProviderE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3WTF8JSONImpl9ArrayBaseD2Ev() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3JSC8JSObject12setPrototypeEPS0_PNS_14JSGlobalObjectENS_7JSValueEb() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN9Inspector25NetworkFrontendDispatcher18requestInterceptedERKN3WTF6StringENS1_6RefPtrINS_8Protocol7Network7RequestENS1_13DumbPtrTraitsIS8_EEEE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3JSC22EdenGCActivityCallback12doCollectionERNS_2VME() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3JSC8JSObject26setIndexQuicklyToUndecidedERNS_2VMEjNS_7JSValueE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3WTF11ThreadGroup16addCurrentThreadEv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI JSValueMakeString() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3WTF7RunLoop14initializeMainEv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN9Inspector22RemoteAutomationTargetC2ERKS0_() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN9Inspector15ScriptCallStack6appendERKNS_15ScriptCallFrameE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3JSC16callCustomSetterEPNS_14JSGlobalObjectENS_7JSValueEbPNS_8JSObjectES2_S2_() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNK3WTF8JSONImpl5Value8asStringERNS_6StringE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3WTF3URL11setPasswordENS_10StringViewE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNK9Inspector22RemoteAutomationTarget20remoteControlAllowedEv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3JSC12JSLockHolderC1ERNS_2VME() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNK3JSC4Yarr17RegularExpression7isValidEv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN10Deprecated18ScriptFunctionCallC2ERKNS_12ScriptObjectERKN3WTF6StringEPFN3JSC7JSValueEPNS8_14JSGlobalObjectES9_RKNS8_8CallDataES9_RKNS8_7ArgListERNS4_8NakedPtrINS8_9ExceptionEEEE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN7bmalloc8FreeListD2Ev() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZThn16_N9Inspector21InspectorRuntimeAgent20getCollectionEntriesERN3WTF6StringERKS2_PS4_PKiS8_RNS1_6RefPtrINS1_8JSONImpl7ArrayOfINS_8Protocol7Runtime15CollectionEntryEEENS1_13DumbPtrTraitsISF_EEEE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN9Inspector17ScriptDebugServer16willRunMicrotaskEv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN9Inspector22InspectorDebuggerAgent24updatePauseReasonAndDataENS_26DebuggerFrontendDispatcher6ReasonEON3WTF6RefPtrINS3_8JSONImpl6ObjectENS3_13DumbPtrTraitsIS6_EEEE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3WTF9BitVector15resizeOutOfLineEm() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3JSC18GCActivityCallback18tryCreateFullTimerEPNS_4HeapE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3JSC14SourceProviderC2ERKNS_12SourceOriginEON3WTF6StringERKNS4_12TextPositionENS_24SourceProviderSourceTypeE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN9Inspector20DOMBackendDispatcher11requestNodeElON3WTF6RefPtrINS1_8JSONImpl6ObjectENS1_13DumbPtrTraitsIS4_EEEE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3JSC17JSArrayBufferViewC2ERNS_2VMERNS0_19ConstructionContextE() {
    NotImplemented_nid_no_patch(__func__);
}

}
