#include "prx/libc/include/general/ExportMacros.hpp"
#include "prx/libSceJsc/include/InventoryExports.hpp"
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

GuestJSString* APS5_VABI JSStringCreateWithCharacters(const std::uint16_t* characters, std::size_t count) {
    if (!characters) return NullString().release();
    if (!GuestJSStringValidLength<char16_t>(count)) throw std::length_error("JSStringCreateWithCharacters: length above the string limit");
    auto string = std::make_unique<GuestJSString>();
    string->characters.assign(characters, characters + count);
    string->characters.push_back(0);
    return string.release();
}

GuestJSString* APS5_VABI JSStringCreateWithUTF8CString(const char* text) {
    return Decode(text).release();
}

GuestJSString* APS5_VABI JSStringRetain(GuestJSString* string) {
    if (!string) throw std::invalid_argument("JSStringRetain: null string");
    string->references.fetch_add(1, std::memory_order_relaxed);
    return string;
}

void APS5_VABI JSStringRelease(GuestJSString* string) {
    if (!string) throw std::invalid_argument("JSStringRelease: null string");
    if (string->references.fetch_sub(1, std::memory_order_acq_rel) == 1) delete string;
}

std::size_t APS5_VABI JSStringGetLength(const GuestJSString* string) {
    return string ? string->characters.size() - 1 : 0;
}

const std::uint16_t* APS5_VABI JSStringGetCharactersPtr(const GuestJSString* string) {
    return string && !string->nullString ? string->characters.data() : nullptr;
}

std::size_t APS5_VABI JSStringGetMaximumUTF8CStringSize(const GuestJSString* string) {
    if (!string) throw std::invalid_argument("JSStringGetMaximumUTF8CStringSize: null string");
    const auto length = static_cast<std::uint32_t>(string->characters.size() - 1);
    return static_cast<std::uint32_t>(length * 3u + 1u);
}

std::size_t APS5_VABI JSStringGetUTF8CString(const GuestJSString* string, char* buffer, std::size_t bufferSize) {
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

extern "C" {

int APS5_VABI JSGlobalContextSetDebuggerRunLoopWithCurrentRunLoop() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI JSGlobalContextUnsetDebuggerRunLoop() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI JSRunLoopIterateCurrentRunLoop() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI JSWeakObjectMapClear() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN10Deprecated11ScriptValueC1ERKS0_() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN10Deprecated11ScriptValueC1ERN3JSC2VMENS1_7JSValueE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN10Deprecated11ScriptValueC1Ev() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN10Deprecated11ScriptValueC2ERKS0_() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN10Deprecated11ScriptValueC2ERN3JSC2VMENS1_7JSValueE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN10Deprecated11ScriptValueC2Ev() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN10Deprecated11ScriptValueD1Ev() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN10Deprecated11ScriptValueD2Ev() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN10Deprecated11ScriptValueaSERKS0_() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN10Deprecated12ScriptObjectC1EPN3JSC14JSGlobalObjectEPNS1_8JSObjectE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN10Deprecated12ScriptObjectC2EPN3JSC14JSGlobalObjectEPNS1_8JSObjectE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN10Deprecated18ScriptFunctionCall4callEv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN10Deprecated18ScriptFunctionCallC1ERKNS_12ScriptObjectERKN3WTF6StringEPFN3JSC7JSValueEPNS8_14JSGlobalObjectES9_RKNS8_8CallDataES9_RKNS8_7ArgListERNS4_8NakedPtrINS8_9ExceptionEEEE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN10Deprecated18ScriptFunctionCallD1Ev() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN10Deprecated18ScriptFunctionCallD2Ev() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN10Deprecated25ScriptCallArgumentHandler14appendArgumentEN3JSC7JSValueE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN10Deprecated25ScriptCallArgumentHandler14appendArgumentEPKc() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN10Deprecated25ScriptCallArgumentHandler14appendArgumentERKN3WTF6StringE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN10Deprecated25ScriptCallArgumentHandler14appendArgumentEb() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN10Deprecated25ScriptCallArgumentHandler14appendArgumentEi() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN10Deprecated25ScriptCallArgumentHandler14appendArgumentEj() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN10Deprecated25ScriptCallArgumentHandler14appendArgumentEl() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN10Deprecated25ScriptCallArgumentHandler14appendArgumentEm() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN10Deprecated25ScriptCallArgumentHandler14appendArgumentEx() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN10Deprecated25ScriptCallArgumentHandlerC1EPN3JSC14JSGlobalObjectE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN10Deprecated25ScriptCallArgumentHandlerC2EPN3JSC14JSGlobalObjectE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN10Deprecated25ScriptCallArgumentHandlerD1Ev() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN10Deprecated25ScriptCallArgumentHandlerD2Ev() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN10Deprecated25ScriptCallArgumentHandlernaEm() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN10Deprecated25ScriptCallArgumentHandlernwEm() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3JSC10Identifier3addERNS_2VMEPKc() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3JSC10JSFunction11getCallDataEPNS_6JSCellE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3JSC10JSFunction16getConstructDataEPNS_6JSCellE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3JSC10loadModuleEPNS_14JSGlobalObjectERKN3WTF6StringENS_7JSValueES6_() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3JSC11RegisterSet12argumentGPRSEv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3JSC11RegisterSet14stackRegistersEv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3JSC11RegisterSet19calleeSaveRegistersEv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3JSC11RegisterSet21macroScratchRegistersEv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3JSC11RegisterSet25reservedHardwareRegistersEv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3JSC11RegisterSet7allFPRsEv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3JSC11RegisterSet7allGPRsEv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3JSC11SymbolTable14finishCreationERNS_2VME() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3JSC11VMInspector9forEachVMEON3WTF8FunctionIFNS0_13FunctorStatusERNS_2VMEEEE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3JSC11createErrorEPNS_14JSGlobalObjectENS_7JSValueERKN3WTF6StringEPFS4_S6_S6_NS_11RuntimeTypeENS_13ErrorInstance28SourceTextWhereErrorOccurredEE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3JSC12RegExpObject14deletePropertyEPNS_6JSCellEPNS_14JSGlobalObjectENS_12PropertyNameERNS_18DeletePropertySlotE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3JSC12RegExpObject14finishCreationERNS_2VME() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3JSC12RegExpObject17defineOwnPropertyEPNS_8JSObjectEPNS_14JSGlobalObjectENS_12PropertyNameERKNS_18PropertyDescriptorEb() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3JSC12StringObject10putByIndexEPNS_6JSCellEPNS_14JSGlobalObjectEjNS_7JSValueEb() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3JSC12StringObject14deletePropertyEPNS_6JSCellEPNS_14JSGlobalObjectENS_12PropertyNameERNS_18DeletePropertySlotE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3JSC12StringObject17defineOwnPropertyEPNS_8JSObjectEPNS_14JSGlobalObjectENS_12PropertyNameERKNS_18PropertyDescriptorEb() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3JSC12StringObject21deletePropertyByIndexEPNS_6JSCellEPNS_14JSGlobalObjectEj() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3JSC12StringObject25getOwnPropertySlotByIndexEPNS_8JSObjectEPNS_14JSGlobalObjectEjRNS_12PropertySlotE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3JSC12StringObject3putEPNS_6JSCellEPNS_14JSGlobalObjectENS_12PropertyNameENS_7JSValueERNS_15PutPropertySlotE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3JSC12VMEntryScopeC1ERNS_2VMEPNS_14JSGlobalObjectE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3JSC12VMEntryScopeC2ERNS_2VMEPNS_14JSGlobalObjectE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3JSC12VMEntryScopeD1Ev() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3JSC12VMEntryScopeD2Ev() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3JSC12importModuleEPNS_14JSGlobalObjectERKNS_10IdentifierENS_7JSValueES5_() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3JSC13BooleanObject14finishCreationERNS_2VME() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3JSC13JSFinalObject13visitChildrenEPNS_6JSCellERNS_11SlotVisitorE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3JSC14JSGlobalObject17defineOwnPropertyEPNS_8JSObjectEPS0_NS_12PropertyNameERKNS_18PropertyDescriptorEb() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3JSC14JSGlobalObject18getOwnPropertySlotEPNS_8JSObjectEPS0_NS_12PropertyNameERNS_12PropertySlotE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3JSC14JSGlobalObject25setRemoteDebuggingEnabledEb() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3JSC14JSGlobalObject30deprecatedCallFrameForDebuggerEv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3JSC14JSGlobalObject3putEPNS_6JSCellEPS0_NS_12PropertyNameENS_7JSValueERNS_15PutPropertySlotE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3JSC14JSGlobalObject7destroyEPNS_6JSCellE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3JSC14JSRunLoopTimer13timeUntilFireEv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3JSC14LocalAllocator16allocateSlowCaseERNS_4HeapEPNS_17GCDeferralContextENS_21AllocationFailureModeE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3JSC14ProtoCallFrame14needArityCheckEv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3JSC14ProtoCallFrame4initEPNS_9CodeBlockEPNS_14JSGlobalObjectEPNS_8JSObjectENS_7JSValueEiPS7_() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3JSC14StructureCache32emptyObjectStructureConcurrentlyEPNS_14JSGlobalObjectEPNS_8JSObjectEj() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3JSC14formatDateTimeERKN3WTF17GregorianDateTimeENS_14DateTimeFormatEb() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3JSC15ArrayBufferViewD0Ev() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3JSC15ArrayBufferViewD1Ev() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3JSC15ArrayBufferViewD2Ev() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3JSC15JSWeakObjectRef14finishCreationERNS_2VMEPNS_8JSObjectE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3JSC15JSWrapperObject13visitChildrenEPNS_6JSCellERNS_11SlotVisitorE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3JSC16InternalFunction11getCallDataEPNS_6JSCellE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3JSC16InternalFunction13visitChildrenEPNS_6JSCellERNS_11SlotVisitorE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3JSC16InternalFunction16getConstructDataEPNS_6JSCellE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3JSC16InternalFunctionC2ERNS_2VMEPNS_9StructureENS_14NativeFunctionES5_() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3JSC16IsoSubspacePerVM5forVMERNS_2VME() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3JSC16IsoSubspacePerVMC1EN3WTF8FunctionIFNS0_18SubspaceParametersERNS_2VMEEEE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3JSC16IsoSubspacePerVMC2EN3WTF8FunctionIFNS0_18SubspaceParametersERNS_2VMEEEE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3JSC17DebuggerCallFrame5scopeEv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3JSC17createJSMicrotaskERNS_2VMENS_7JSValueE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3JSC17weakClearSlowCaseERPNS_8WeakImplE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3JSC19ArrayBufferContents5clearEv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3JSC19ArrayBufferContentsC1EOS0_() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3JSC19ArrayBufferContentsC1Ev() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3JSC19ArrayBufferContentsC2EOS0_() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3JSC19ArrayBufferContentsC2Ev() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3JSC19ArrayBufferContentsD1Ev() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3JSC19ArrayBufferContentsD2Ev() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3JSC19ArrayBufferContentsaSEOS0_() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3JSC19JSSymbolTableObject14deletePropertyEPNS_6JSCellEPNS_14JSGlobalObjectENS_12PropertyNameERNS_18DeletePropertySlotE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3JSC20speculationFromValueENS_7JSValueE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3JSC23JSModuleNamespaceObject10putByIndexEPNS_6JSCellEPNS_14JSGlobalObjectEjNS_7JSValueEb() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3JSC23JSModuleNamespaceObject14deletePropertyEPNS_6JSCellEPNS_14JSGlobalObjectENS_12PropertyNameERNS_18DeletePropertySlotE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3JSC23JSModuleNamespaceObject14finishCreationEPNS_14JSGlobalObjectEPNS_20AbstractModuleRecordEON3WTF6VectorISt4pairINS_10IdentifierENS3_10ResolutionEELm0ENS5_15CrashOnOverflowELm16ENS5_10FastMallocEEE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3JSC23JSModuleNamespaceObject17defineOwnPropertyEPNS_8JSObjectEPNS_14JSGlobalObjectENS_12PropertyNameERKNS_18PropertyDescriptorEb() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3JSC23JSModuleNamespaceObject18getOwnPropertySlotEPNS_8JSObjectEPNS_14JSGlobalObjectENS_12PropertyNameERNS_12PropertySlotE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3JSC23JSModuleNamespaceObject25getOwnPropertySlotByIndexEPNS_8JSObjectEPNS_14JSGlobalObjectEjRNS_12PropertySlotE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3JSC23JSModuleNamespaceObject3putEPNS_6JSCellEPNS_14JSGlobalObjectENS_12PropertyNameENS_7JSValueERNS_15PutPropertySlotE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3JSC23createBuiltinExecutableERNS_2VMERKNS_10SourceCodeERKNS_10IdentifierENS_15ConstructorKindENS_16ConstructAbilityE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3JSC23errorMesasgeForTransferEPNS_11ArrayBufferE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3JSC23objectProtoFuncToStringEPNS_14JSGlobalObjectEPNS_9CallFrameE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3JSC25JSSegmentedVariableObject13visitChildrenEPNS_6JSCellERNS_11SlotVisitorE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3JSC25MacroAssemblerCodeRefBase11disassemblyENS_21MacroAssemblerCodePtrILN3WTF6PtrTagE3123EEEm() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3JSC27objectPrivateFuncInstanceOfEPNS_14JSGlobalObjectEPNS_9CallFrameE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3JSC29callHostFunctionAsConstructorEPNS_14JSGlobalObjectEPNS_9CallFrameE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3JSC2VM15dumpRegExpTraceEv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3JSC41DeferredStructureTransitionWatchpointFireC1ERNS_2VMEPNS_9StructureE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3JSC41DeferredStructureTransitionWatchpointFireC2ERNS_2VMEPNS_9StructureE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3JSC41DeferredStructureTransitionWatchpointFireD0Ev() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3JSC41DeferredStructureTransitionWatchpointFireD1Ev() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3JSC41DeferredStructureTransitionWatchpointFireD2Ev() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3JSC4Heap16objectTypeCountsEv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3JSC4Heap20addMarkingConstraintESt10unique_ptrINS_17MarkingConstraintESt14default_deleteIS2_EE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3JSC4Heap25protectedObjectTypeCountsEv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3JSC4Yarr10parseFlagsEN3WTF10StringViewE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3JSC4Yarr11YarrPatternC1ERKN3WTF6StringENS2_9OptionSetINS0_5FlagsEEERNS0_9ErrorCodeE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3JSC4Yarr11YarrPatternC2ERKN3WTF6StringENS2_9OptionSetINS0_5FlagsEEERNS0_9ErrorCodeE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3JSC4Yarr11byteCompileERNS0_11YarrPatternEPN3WTF20BumpPointerAllocatorERNS0_9ErrorCodeEPNS3_4LockE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3JSC4Yarr17RegularExpressionC1ERKN3WTF6StringENS0_19TextCaseSensitivityENS0_13MultilineModeENS0_11UnicodeModeE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3JSC4Yarr17RegularExpressionC2ERKN3WTF6StringENS0_19TextCaseSensitivityENS0_13MultilineModeENS0_11UnicodeModeE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3JSC4Yarr20unicodeMatchPropertyEN3WTF6StringE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3JSC4Yarr25unicodeMatchPropertyValueEN3WTF6StringES2_() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3JSC4Yarr7replaceERN3WTF6StringERKNS0_17RegularExpressionERKS2_() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3JSC4Yarr9interpretEPNS0_15BytecodePatternERKN3WTF6StringEjPj() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3JSC4callEPNS_14JSGlobalObjectENS_7JSValueERKNS_7ArgListEPKc() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3JSC4callEPNS_14JSGlobalObjectENS_7JSValueERKNS_8CallDataES2_RKNS_7ArgListERN3WTF8NakedPtrINS_9ExceptionEEE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3JSC4callEPNS_14JSGlobalObjectENS_7JSValueES2_RKNS_7ArgListEPKc() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3JSC6JSCell11analyzeHeapEPS0_RNS_12HeapAnalyzerE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3JSC6JSCell11getCallDataEPS0_() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3JSC6JSCell12dumpToStreamEPKS0_RN3WTF11PrintStreamE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3JSC6JSCell13estimatedSizeEPS0_RNS_2VME() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3JSC6JSCell16getConstructDataEPS0_() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3JSC6JSCell17customHasInstanceEPNS_8JSObjectEPNS_14JSGlobalObjectENS_7JSValueE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3JSC6JSCell7destroyEPS0_() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3JSC6RegExp12dumpToStreamEPKNS_6JSCellERN3WTF11PrintStreamE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3JSC7JSArray17defineOwnPropertyEPNS_8JSObjectEPNS_14JSGlobalObjectENS_12PropertyNameERKNS_18PropertyDescriptorEb() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3JSC7JSArray18getOwnPropertySlotEPNS_8JSObjectEPNS_14JSGlobalObjectENS_12PropertyNameERNS_12PropertySlotE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3JSC7JSProxy10putByIndexEPNS_6JSCellEPNS_14JSGlobalObjectEjNS_7JSValueEb() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3JSC7JSProxy12getPrototypeEPNS_8JSObjectEPNS_14JSGlobalObjectE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3JSC7JSProxy12isExtensibleEPNS_8JSObjectEPNS_14JSGlobalObjectE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3JSC7JSProxy12setPrototypeEPNS_8JSObjectEPNS_14JSGlobalObjectENS_7JSValueEb() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3JSC7JSProxy14deletePropertyEPNS_6JSCellEPNS_14JSGlobalObjectENS_12PropertyNameERNS_18DeletePropertySlotE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3JSC7JSProxy17defineOwnPropertyEPNS_8JSObjectEPNS_14JSGlobalObjectENS_12PropertyNameERKNS_18PropertyDescriptorEb() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3JSC7JSProxy17preventExtensionsEPNS_8JSObjectEPNS_14JSGlobalObjectE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3JSC7JSProxy18getOwnPropertySlotEPNS_8JSObjectEPNS_14JSGlobalObjectENS_12PropertyNameERNS_12PropertySlotE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3JSC7JSProxy21deletePropertyByIndexEPNS_6JSCellEPNS_14JSGlobalObjectEj() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3JSC7JSProxy25getOwnPropertySlotByIndexEPNS_8JSObjectEPNS_14JSGlobalObjectEjRNS_12PropertySlotE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3JSC7JSProxy3putEPNS_6JSCellEPNS_14JSGlobalObjectENS_12PropertyNameENS_7JSValueERNS_15PutPropertySlotE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3JSC7JSScope6toThisEPNS_6JSCellEPNS_14JSGlobalObjectENS_8ECMAModeE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3JSC7Options14dumpAllOptionsEP7__sFILENS0_9DumpLevelEPKc() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3JSC7Options9setOptionEPKc() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3JSC8FreeList14initializeBumpEPcj() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3JSC8FreeList14initializeListEPNS_8FreeCellEmj() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3JSC8JSObject10putByIndexEPNS_6JSCellEPNS_14JSGlobalObjectEjNS_7JSValueEb() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3JSC8JSObject11analyzeHeapEPNS_6JSCellERNS_12HeapAnalyzerE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3JSC8JSObject12isExtensibleEPS0_PNS_14JSGlobalObjectE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3JSC8JSObject13estimatedSizeEPNS_6JSCellERNS_2VME() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3JSC8JSObject13visitChildrenEPNS_6JSCellERNS_11SlotVisitorE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3JSC8JSObject14deletePropertyEPNS_6JSCellEPNS_14JSGlobalObjectENS_12PropertyNameERNS_18DeletePropertySlotE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3JSC8JSObject17defineOwnPropertyEPS0_PNS_14JSGlobalObjectENS_12PropertyNameERKNS_18PropertyDescriptorEb() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3JSC8JSObject17preventExtensionsEPS0_PNS_14JSGlobalObjectE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3JSC8JSObject21deletePropertyByIndexEPNS_6JSCellEPNS_14JSGlobalObjectEj() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3JSC8JSObject23putDirectNativeFunctionERNS_2VMEPNS_14JSGlobalObjectERKNS_12PropertyNameEjNS_14NativeFunctionENS_9IntrinsicEPKNS_6DOMJIT9SignatureEj() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3JSC8JSObject23putDirectNativeFunctionERNS_2VMEPNS_14JSGlobalObjectERKNS_12PropertyNameEjNS_14NativeFunctionENS_9IntrinsicEj() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3JSC8JSObject25getOwnPropertySlotByIndexEPS0_PNS_14JSGlobalObjectEjRNS_12PropertySlotE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3JSC8JSObject30putDirectNativeIntrinsicGetterERNS_2VMEPNS_14JSGlobalObjectENS_10IdentifierENS_14NativeFunctionENS_9IntrinsicEj() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3JSC8JSObject3putEPNS_6JSCellEPNS_14JSGlobalObjectENS_12PropertyNameENS_7JSValueERNS_15PutPropertySlotE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3JSC8JSObject40putDirectNativeFunctionWithoutTransitionERNS_2VMEPNS_14JSGlobalObjectERKNS_12PropertyNameEjNS_14NativeFunctionENS_9IntrinsicEj() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3JSC8JSObject47putDirectNativeIntrinsicGetterWithoutTransitionERNS_2VMEPNS_14JSGlobalObjectENS_10IdentifierENS_14NativeFunctionENS_9IntrinsicEj() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3JSC8JSObject6toThisEPNS_6JSCellEPNS_14JSGlobalObjectENS_8ECMAModeE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3JSC8evaluateEPNS_14JSGlobalObjectERKNS_10SourceCodeENS_7JSValueERN3WTF8NakedPtrINS_9ExceptionEEE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3JSC9CallFrame15isAnyWasmCalleeEv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3JSC9CodeCache5writeERNS_2VME() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3JSC9HandleSet12writeBarrierEPNS_7JSValueERKS1_() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3JSC9Integrity14auditCellFullyERNS_2VMEPNS_6JSCellE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3JSC9JSONParseEPNS_14JSGlobalObjectERKN3WTF6StringE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3JSC9Structure14sealTransitionERNS_2VMEPS0_() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3JSC9Structure16freezeTransitionERNS_2VMEPS0_() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3JSC9Structure25nonPropertyTransitionSlowERNS_2VMEPS0_NS_14TransitionKindE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3JSC9Structure3pinERKN3WTF14AbstractLockerERNS_2VMEPNS_13PropertyTableE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3JSC9constructEPNS_14JSGlobalObjectENS_7JSValueERKNS_7ArgListEPKc() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3JSC9constructEPNS_14JSGlobalObjectENS_7JSValueES2_RKNS_7ArgListEPKc() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3WTF10AtomString4initEv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3WTF10StackTrace17captureStackTraceEii() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3WTF10StackTrace8demangleEPv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3WTF10StringImpl11reverseFindEPS0_j() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3WTF10StringImpl17createFromLiteralEPKcj() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3WTF10StringImpl17createFromLiteralEPKc() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3WTF10StringImpl18simplifyWhiteSpaceEv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3WTF10StringImpl20create8BitIfPossibleEPKDs() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3WTF10StringImpl20createWithoutCopyingEPKDsj() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3WTF10StringImpl20createWithoutCopyingEPKhj() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3WTF10StringImpl23defaultWritingDirectionEPb() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3WTF10StringImpl4findEPFbDsEj() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3WTF10StringImpl4findEPS0_j() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3WTF10StringImpl4findEPS0_() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3WTF10StringImpl6createEPKh() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3WTF10StringImpl7replaceEDsPS0_() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3WTF10StringImpl7replaceEPS0_S1_() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3WTF10StringImpl7replaceEjjPS0_() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3WTF10StringImpl8endsWithEPS0_() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3WTF10StringImpl8endsWithERS0_() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3WTF10nullStringEv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3WTF10protocolIsENS_10StringViewEPKc() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3WTF11Persistence5CoderINS_10AtomStringEE6decodeERNS0_7DecoderE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3WTF11Persistence5CoderINS_6StringEE6decodeERNS0_7DecoderE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3WTF11Persistence5CoderINS_7CStringEE6decodeERNS0_7DecoderE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3WTF11Persistence5CoderISt5arrayIhLm20EEE6decodeERNS0_7DecoderE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3WTF11Persistence5CoderISt5arrayIhLm20EEE6encodeERNS0_7EncoderERKS3_() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3WTF11emptyStringEv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3WTF11setDataFileEOSt10unique_ptrINS_11PrintStreamESt14default_deleteIS1_EE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3WTF12randomNumberEv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3WTF14AtomStringImpl3addEPKDs() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3WTF14AtomStringImpl3addEPKh() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3WTF14FileSystemImpl17openTemporaryFileERKNS_6StringERiS3_() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3WTF14FileSystemImpl24pathByAppendingComponentERKNS_6StringES3_() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3WTF15FilePrintStream4openEPKcS2_() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3WTF16weakRandomUint32Ev() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3WTF17PrivateSymbolImpl16createNullSymbolEv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3WTF17TextBreakIteratorC1ENS_10StringViewENS0_4ModeERKNS_10AtomStringE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3WTF17TextBreakIteratorC2ENS_10StringViewENS0_4ModeERKNS_10AtomStringE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3WTF19initializeThreadingEv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3WTF20ObjectIdentifierBase26generateIdentifierInternalEv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3WTF20ObjectIdentifierBase36generateThreadSafeIdentifierInternalEv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3WTF21LineBreakIteratorPool10sharedPoolEv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3WTF22defaultPortForProtocolENS_10StringViewE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3WTF23openLatin1UTextProviderEPNS_15UTextWithBufferEPKhjP10UErrorCode() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3WTF24acquireLineBreakIteratorENS_10StringViewERKNS_10AtomStringEPKDsjNS_21LineBreakIteratorModeE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3WTF24releaseLineBreakIteratorEP14UBreakIterator() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3WTF25createCanonicalUUIDStringEv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3WTF29cryptographicallyRandomNumberEv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3WTF31integerToSixCharacterHashStringEj() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3WTF4SHA111computeHashERSt5arrayIhLm20EE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3WTF4SHA19hexDigestERKSt5arrayIhLm20EE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3WTF6Logger9observersEv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3WTF6String23make16BitFrom8BitSourceEPKhm() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3WTF6String23make8BitFrom16BitSourceEPKDsm() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3WTF6String6appendEDs() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3WTF6String6appendEPKDsj() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3WTF6String6appendEPKhj() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3WTF6String6appendERKS0_() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3WTF6String6appendEh() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3WTF6String6insertERKS0_j() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3WTF6String6removeEjj() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3WTF6String8truncateEj() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3WTF6StringC1ENS_12ASCIILiteralE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3WTF6StringC1EPKDs() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3WTF6StringC1EPKh() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3WTF6StringC2EPKDs() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3WTF6StringC2EPKh() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3WTF7CPUTime3getEv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3WTF7RunLoop6isMainEv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3WTF7RunLoop7iterateEv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3WTF7Unicode18convertUTF16ToUTF8EPPKDsS2_PPcS4_b() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3WTF7Unicode18convertUTF8ToUTF16EPKcS2_PPDsS3_Pb() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3WTF7Unicode19convertLatin1ToUTF8EPPKhS2_PPcS4_() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3WTF7Unicode43convertUTF8ToUTF16ReplacingInvalidSequencesEPKcS2_PPDsS3_Pb() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3WTF8Internal9appendHexEPhjmjNS_17HexConversionModeE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3WTF8JSONImpl5Value12escapeStringERNS_13StringBuilderENS_10StringViewE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3WTF8pageMaskEv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN7bmalloc11AllIsoHeapsC1ERKSt11scoped_lockIJNS_5MutexEEE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN7bmalloc11AllIsoHeapsC2ERKSt11scoped_lockIJNS_5MutexEEE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN7bmalloc11EnvironmentC1ERKSt11scoped_lockIJNS_5MutexEEE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN7bmalloc11EnvironmentC2ERKSt11scoped_lockIJNS_5MutexEEE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN7bmalloc12IsoTLSLayoutC1ERKSt11scoped_lockIJNS_5MutexEEE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN7bmalloc12IsoTLSLayoutC2ERKSt11scoped_lockIJNS_5MutexEEE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN7bmalloc9ScavengerC1ERKSt11scoped_lockIJNS_5MutexEEE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN7bmalloc9ScavengerC2ERKSt11scoped_lockIJNS_5MutexEEE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN9Inspector13AgentRegistry6appendESt10unique_ptrINS_18InspectorAgentBaseESt14default_deleteIS2_EE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN9Inspector14ConsoleMessageC1EN3JSC13MessageSourceENS1_11MessageTypeENS1_12MessageLevelEON3WTF6VectorINS5_12JSONLogValueELm0ENS5_15CrashOnOverflowELm16ENS5_10FastMallocEEEPNS1_14JSGlobalObjectEm() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN9Inspector14ConsoleMessageC1EN3JSC13MessageSourceENS1_11MessageTypeENS1_12MessageLevelERKN3WTF6StringES8_jjPNS1_14JSGlobalObjectEm() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN9Inspector14ConsoleMessageC1EN3JSC13MessageSourceENS1_11MessageTypeENS1_12MessageLevelERKN3WTF6StringEm() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN9Inspector14ConsoleMessageC2EN3JSC13MessageSourceENS1_11MessageTypeENS1_12MessageLevelEON3WTF6VectorINS5_12JSONLogValueELm0ENS5_15CrashOnOverflowELm16ENS5_10FastMallocEEEPNS1_14JSGlobalObjectEm() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN9Inspector14ConsoleMessageC2EN3JSC13MessageSourceENS1_11MessageTypeENS1_12MessageLevelERKN3WTF6StringES8_jjPNS1_14JSGlobalObjectEm() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN9Inspector14ConsoleMessageC2EN3JSC13MessageSourceENS1_11MessageTypeENS1_12MessageLevelERKN3WTF6StringEm() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN9Inspector14InjectedScriptC1EN10Deprecated12ScriptObjectEPNS_20InspectorEnvironmentE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN9Inspector14InjectedScriptC2EN10Deprecated12ScriptObjectEPNS_20InspectorEnvironmentE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN9Inspector15RemoteInspector39sendAutomaticInspectionCandidateMessageEv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN9Inspector15ScriptCallStack6createERN3WTF6VectorINS_15ScriptCallFrameELm0ENS1_15CrashOnOverflowELm16ENS1_10FastMallocEEE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN9Inspector15ScriptCallStackC1ERN3WTF6VectorINS_15ScriptCallFrameELm0ENS1_15CrashOnOverflowELm16ENS1_10FastMallocEEE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN9Inspector18InjectedScriptBase8makeCallERN10Deprecated18ScriptFunctionCallE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN9Inspector18InjectedScriptBaseC1ERKN3WTF6StringEN10Deprecated12ScriptObjectEPNS_20InspectorEnvironmentE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN9Inspector18InjectedScriptBaseC2ERKN3WTF6StringEN10Deprecated12ScriptObjectEPNS_20InspectorEnvironmentE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN9Inspector18InspectorHeapAgent27nodeForHeapObjectIdentifierERN3WTF6StringEj() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN9Inspector21InjectedScriptManager20createInjectedScriptERKN3WTF6StringEPN3JSC14JSGlobalObjectEi() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN9Inspector21InjectedScriptManager20injectedScriptSourceEv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN9Inspector21InspectorConsoleAgent19addMessageToConsoleESt10unique_ptrINS_14ConsoleMessageESt14default_deleteIS2_EE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN9Inspector22InspectorDebuggerAgent15didRunMicrotaskEv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN9Inspector22InspectorDebuggerAgent16willRunMicrotaskEv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN9Inspector22InspectorDebuggerAgent18didCancelAsyncCallENS0_13AsyncCallTypeEi() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN9Inspector22InspectorDebuggerAgent19asyncCallIdentifierENS0_13AsyncCallTypeEi() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN9Inspector22InspectorDebuggerAgent20didDispatchAsyncCallEv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN9Inspector22InspectorDebuggerAgent20didScheduleAsyncCallEPN3JSC14JSGlobalObjectENS0_13AsyncCallTypeEib() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN9Inspector22InspectorDebuggerAgent21willDispatchAsyncCallENS0_13AsyncCallTypeEi() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN9Inspector22RemoteInspectionTarget25setRemoteDebuggingAllowedEb() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN9Inspector25ConsoleFrontendDispatcher15messagesClearedEv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN9Inspector25ConsoleFrontendDispatcher25messageRepeatCountUpdatedEi() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNK10Deprecated11ScriptValue10hasNoValueEv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNK10Deprecated11ScriptValue7jsValueEv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNK3JSC11RegisterSet4dumpERN3WTF11PrintStreamE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNK3JSC14JSGlobalObject22remoteDebuggingEnabledEv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNK3JSC17DebuggerCallFrame12functionNameEv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNK3JSC17DebuggerCallFrame29deprecatedVMEntryGlobalObjectEv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNK3JSC17DebuggerCallFrame4typeEv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNK3JSC4Yarr17RegularExpression5matchERKN3WTF6StringEiPi() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNK3JSC4Yarr17RegularExpression9searchRevERKN3WTF6StringE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNK3JSC7JSValue14toThisSlowCaseEPNS_14JSGlobalObjectENS_8ECMAModeE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNK3JSC8JSObject43anyObjectInChainMayInterceptIndexedAccessesERNS_2VME() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNK3JSC8Profiler8Database4toJSEPNS_14JSGlobalObjectE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNK3JSC9ClassInfo35hasStaticSetterOrReadonlyPropertiesEv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNK3WTF10StackTrace4dumpERNS_11PrintStreamEPKc() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNK3WTF10StringImpl10startsWithEPKS0_() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNK3WTF10StringImpl10startsWithERKS0_() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNK3WTF10StringImpl10tryGetUtf8ENS_14ConversionModeE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNK3WTF10StringImpl16hasInfixEndingAtERKS0_j() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNK3WTF10StringImpl18hasInfixStartingAtERKS0_j() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNK3WTF10StringImpl18tryGetUtf8ForRangeEjjNS_14ConversionModeE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNK3WTF10StringImpl21findIgnoringASCIICaseEPKS0_j() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNK3WTF10StringImpl21findIgnoringASCIICaseERKS0_j() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNK3WTF10StringImpl21findIgnoringASCIICaseERKS0_() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNK3WTF10StringImpl25endsWithIgnoringASCIICaseEPKS0_() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNK3WTF10StringImpl25endsWithIgnoringASCIICaseERKS0_() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNK3WTF10StringImpl27startsWithIgnoringASCIICaseEPKS0_() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNK3WTF10StringImpl27startsWithIgnoringASCIICaseERKS0_() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNK3WTF10StringView10tryGetUtf8ENS_14ConversionModeE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNK3WTF10StringView16GraphemeClusters8IteratorneERKS2_() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNK3WTF10StringView8containsEPKc() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNK3WTF13StringBuilder10isAllASCIIEv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNK3WTF3URL11isLocalFileEv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNK3WTF3URL15protocolIsAboutEv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNK3WTF3URL38stringWithoutQueryOrFragmentIdentifierEv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNK3WTF3URL4portEv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNK3WTF6String10tryGetUtf8ENS_14ConversionModeE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNK3WTF6String10tryGetUtf8Ev() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNK3WTF6String15stripWhiteSpaceEv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNK3WTF6String18simplifyWhiteSpaceEv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNK3WTF6String25splitAllowingEmptyEntriesERKS0_() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNK3WTF6String33stripLeadingAndTrailingCharactersEPFbDsE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNK3WTF6String4utf8Ev() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNK3WTF6String5splitERKS0_() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNK3WTF6String9substringEjj() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNK3WTF9MediaTimeneERKS0_() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNK9Inspector15RemoteInspector18clientCapabilitiesEv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNK9Inspector18InjectedScriptBase27callFunctionWithEvalEnabledERN10Deprecated18ScriptFunctionCallE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNK9Inspector18InjectedScriptHost16savedResultAliasEv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNK9Inspector22RemoteInspectionTarget22remoteDebuggingAllowedEv() {
    NotImplemented_nid_no_patch(__func__);
}

}

extern "C" {

int APS5_VABI libSceJscInventory_TYvNAgirhfM_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("TYvNAgirhfM", libSceJscInventory_TYvNAgirhfM_nid_no_patch);

}

extern "C" {

int APS5_VABI libSceJscInventory_4AXLxzY843A_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("4AXLxzY843A", libSceJscInventory_4AXLxzY843A_nid_no_patch);

int APS5_VABI libSceJscInventory_AnZ5fEIIS40_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("AnZ5fEIIS40", libSceJscInventory_AnZ5fEIIS40_nid_no_patch);

int APS5_VABI libSceJscInventory_GtiYvkQcGr4_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("GtiYvkQcGr4", libSceJscInventory_GtiYvkQcGr4_nid_no_patch);

int APS5_VABI libSceJscInventory_RD9UWM_plus_gJiI_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("RD9UWM+gJiI", libSceJscInventory_RD9UWM_plus_gJiI_nid_no_patch);

int APS5_VABI libSceJscInventory_xu3rtZhKLb0_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("xu3rtZhKLb0", libSceJscInventory_xu3rtZhKLb0_nid_no_patch);

int APS5_VABI libSceJscInventory_1shvECo12S4_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("1shvECo12S4", libSceJscInventory_1shvECo12S4_nid_no_patch);

int APS5_VABI libSceJscInventory_8LcWf8Gi0Zk_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("8LcWf8Gi0Zk", libSceJscInventory_8LcWf8Gi0Zk_nid_no_patch);

int APS5_VABI libSceJscInventory_8R88n7E6UpI_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("8R88n7E6UpI", libSceJscInventory_8R88n7E6UpI_nid_no_patch);

int APS5_VABI libSceJscInventory_B5h48AC6Vns_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("B5h48AC6Vns", libSceJscInventory_B5h48AC6Vns_nid_no_patch);

int APS5_VABI libSceJscInventory_N_minus_rP7qvkC1s_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("N-rP7qvkC1s", libSceJscInventory_N_minus_rP7qvkC1s_nid_no_patch);

int APS5_VABI libSceJscInventory_Oc8nnt7PBhQ_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("Oc8nnt7PBhQ", libSceJscInventory_Oc8nnt7PBhQ_nid_no_patch);

int APS5_VABI libSceJscInventory_R36powg0yew_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("R36powg0yew", libSceJscInventory_R36powg0yew_nid_no_patch);

int APS5_VABI libSceJscInventory_Ri_plus_n8zTCrx4_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("Ri+n8zTCrx4", libSceJscInventory_Ri_plus_n8zTCrx4_nid_no_patch);

int APS5_VABI libSceJscInventory_SkTqPDJdrqA_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("SkTqPDJdrqA", libSceJscInventory_SkTqPDJdrqA_nid_no_patch);

int APS5_VABI libSceJscInventory_SsrlB2FLx6g_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("SsrlB2FLx6g", libSceJscInventory_SsrlB2FLx6g_nid_no_patch);

int APS5_VABI libSceJscInventory_VL5D22H8TAk_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("VL5D22H8TAk", libSceJscInventory_VL5D22H8TAk_nid_no_patch);

int APS5_VABI libSceJscInventory_WjADfj0eVFs_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("WjADfj0eVFs", libSceJscInventory_WjADfj0eVFs_nid_no_patch);

int APS5_VABI libSceJscInventory_eVnAQP7q5j8_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("eVnAQP7q5j8", libSceJscInventory_eVnAQP7q5j8_nid_no_patch);

int APS5_VABI libSceJscInventory_eZM_plus_AI68IpA_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("eZM+AI68IpA", libSceJscInventory_eZM_plus_AI68IpA_nid_no_patch);

int APS5_VABI libSceJscInventory_ecpJv6CchFE_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("ecpJv6CchFE", libSceJscInventory_ecpJv6CchFE_nid_no_patch);

int APS5_VABI libSceJscInventory_fj3HITuKDPs_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("fj3HITuKDPs", libSceJscInventory_fj3HITuKDPs_nid_no_patch);

int APS5_VABI libSceJscInventory_uF3DZ2xR1sA_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("uF3DZ2xR1sA", libSceJscInventory_uF3DZ2xR1sA_nid_no_patch);

}
