#include "prx/libc/include/general/ExportMacros.hpp"
#include "prx/libSceNpCommon/include/InventoryExports.hpp"
#include "prx/libSceNpCommon/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/Api.hpp"

extern "C" {

static_assert(sizeof(std::uintptr_t) == 8);

[[noreturn]] static void APS5_VABI NpJsonNumberDestructor(void*) { NotImplemented_nid_no_patch(__func__); }
[[noreturn]] static void APS5_VABI NpJsonNumberDeletingDestructor(void*) { NotImplemented_nid_no_patch(__func__); }
std::uintptr_t _ZTVN3sce2np10JsonNumberE_nid_postfix[] {
    0, 0,
    reinterpret_cast<std::uintptr_t>(&NpJsonNumberDestructor),
    reinterpret_cast<std::uintptr_t>(&NpJsonNumberDeletingDestructor),
    reinterpret_cast<std::uintptr_t>(&_ZN3sce2np10JsonNumber5ClearEv),
    reinterpret_cast<std::uintptr_t>(&_ZNK3sce2np10JsonNumber10CloneValueEP16SceNpAllocatorEx),
};

[[noreturn]] static void APS5_VABI NpJsonObjectDestructor(void*) { NotImplemented_nid_no_patch(__func__); }
[[noreturn]] static void APS5_VABI NpJsonObjectDeletingDestructor(void*) { NotImplemented_nid_no_patch(__func__); }
std::uintptr_t _ZTVN3sce2np10JsonObjectE_nid_postfix[] {
    0, 0,
    reinterpret_cast<std::uintptr_t>(&NpJsonObjectDestructor),
    reinterpret_cast<std::uintptr_t>(&NpJsonObjectDeletingDestructor),
    reinterpret_cast<std::uintptr_t>(&_ZN3sce2np10JsonObject5ClearEv),
    reinterpret_cast<std::uintptr_t>(&_ZNK3sce2np10JsonObject10CloneValueEP16SceNpAllocatorEx),
};

[[noreturn]] static void APS5_VABI NpJsonStringDestructor(void*) { NotImplemented_nid_no_patch(__func__); }
[[noreturn]] static void APS5_VABI NpJsonStringDeletingDestructor(void*) { NotImplemented_nid_no_patch(__func__); }
std::uintptr_t _ZTVN3sce2np10JsonStringE_nid_postfix[] {
    0, 0,
    reinterpret_cast<std::uintptr_t>(&NpJsonStringDestructor),
    reinterpret_cast<std::uintptr_t>(&NpJsonStringDeletingDestructor),
    reinterpret_cast<std::uintptr_t>(&_ZN3sce2np10JsonString5ClearEv),
    reinterpret_cast<std::uintptr_t>(&_ZNK3sce2np10JsonString10CloneValueEP16SceNpAllocatorEx),
};

[[noreturn]] static void APS5_VABI NpJsonBoolDestructor(void*) { NotImplemented_nid_no_patch(__func__); }
[[noreturn]] static void APS5_VABI NpJsonBoolDeletingDestructor(void*) { NotImplemented_nid_no_patch(__func__); }
std::uintptr_t _ZTVN3sce2np8JsonBoolE_nid_postfix[] {
    0, 0,
    reinterpret_cast<std::uintptr_t>(&NpJsonBoolDestructor),
    reinterpret_cast<std::uintptr_t>(&NpJsonBoolDeletingDestructor),
    reinterpret_cast<std::uintptr_t>(&_ZN3sce2np8JsonBool5ClearEv),
    reinterpret_cast<std::uintptr_t>(&_ZNK3sce2np8JsonBool10CloneValueEP16SceNpAllocatorEx),
};

[[noreturn]] static void APS5_VABI NpJsonNullDestructor(void*) { NotImplemented_nid_no_patch(__func__); }
[[noreturn]] static void APS5_VABI NpJsonNullDeletingDestructor(void*) { NotImplemented_nid_no_patch(__func__); }
std::uintptr_t _ZTVN3sce2np8JsonNullE_nid_postfix[] {
    0, 0,
    reinterpret_cast<std::uintptr_t>(&NpJsonNullDestructor),
    reinterpret_cast<std::uintptr_t>(&NpJsonNullDeletingDestructor),
    reinterpret_cast<std::uintptr_t>(&_ZN3sce2np8JsonNull5ClearEv),
    reinterpret_cast<std::uintptr_t>(&_ZNK3sce2np8JsonNull10CloneValueEP16SceNpAllocatorEx),
};

[[noreturn]] static void APS5_VABI NpJsonArrayDestructor(void*) { NotImplemented_nid_no_patch(__func__); }
[[noreturn]] static void APS5_VABI NpJsonArrayDeletingDestructor(void*) { NotImplemented_nid_no_patch(__func__); }
std::uintptr_t _ZTVN3sce2np9JsonArrayE_nid_postfix[] {
    0, 0,
    reinterpret_cast<std::uintptr_t>(&NpJsonArrayDestructor),
    reinterpret_cast<std::uintptr_t>(&NpJsonArrayDeletingDestructor),
    reinterpret_cast<std::uintptr_t>(&_ZN3sce2np9JsonArray5ClearEv),
    reinterpret_cast<std::uintptr_t>(&_ZNK3sce2np9JsonArray10CloneValueEP16SceNpAllocatorEx),
};


std::uintptr_t _ZTVN3sce2np9JsonValueE_nid_postfix[] {
    0, 0,
    reinterpret_cast<std::uintptr_t>(&_ZN3sce2np9JsonValueD1Ev),
    reinterpret_cast<std::uintptr_t>(&_ZN3sce2np9JsonValueD0Ev),
    reinterpret_cast<std::uintptr_t>(&__cxa_pure_virtual_nid_postfix),
    reinterpret_cast<std::uintptr_t>(&__cxa_pure_virtual_nid_postfix),
};

std::uintptr_t _ZTVN3sce2np12SelectorItemE_nid_postfix[] {
    0, 0,
    reinterpret_cast<std::uintptr_t>(&_ZN3sce2np12SelectorItemD1Ev),
    reinterpret_cast<std::uintptr_t>(&_ZN3sce2np12SelectorItemD0Ev),
    reinterpret_cast<std::uintptr_t>(&__cxa_pure_virtual_nid_postfix),
    reinterpret_cast<std::uintptr_t>(&__cxa_pure_virtual_nid_postfix),
};


static int APS5_VABI NpMsgPackMapImplType(const void*) { return 0x2003; }
std::uintptr_t _ZTVN3sce2np8msg_pack14MsgPackMapImplE_nid_postfix[] {
    0, 0,
    reinterpret_cast<std::uintptr_t>(&_ZN3sce2np8msg_pack14MsgPackMapImplD1Ev),
    reinterpret_cast<std::uintptr_t>(&_ZN3sce2np8msg_pack14MsgPackMapImplD0Ev),
    reinterpret_cast<std::uintptr_t>(&NpMsgPackMapImplType),
    reinterpret_cast<std::uintptr_t>(&_ZNK3sce2np8msg_pack14MsgPackMapImpl6EqualsERKNS1_16MsgPackValueImplE),
    reinterpret_cast<std::uintptr_t>(&_ZNK3sce2np8msg_pack14MsgPackMapImpl18GetMessagePackSizeEv),
    reinterpret_cast<std::uintptr_t>(&_ZNK3sce2np8msg_pack14MsgPackMapImpl12ToJsonStringEPcmPmi),
};

static int APS5_VABI NpMsgPackArrayImplType(const void*) { return 0x2002; }
std::uintptr_t _ZTVN3sce2np8msg_pack16MsgPackArrayImplE_nid_postfix[] {
    0, 0,
    reinterpret_cast<std::uintptr_t>(&_ZN3sce2np8msg_pack16MsgPackArrayImplD1Ev),
    reinterpret_cast<std::uintptr_t>(&_ZN3sce2np8msg_pack16MsgPackArrayImplD0Ev),
    reinterpret_cast<std::uintptr_t>(&NpMsgPackArrayImplType),
    reinterpret_cast<std::uintptr_t>(&_ZNK3sce2np8msg_pack16MsgPackArrayImpl6EqualsERKNS1_16MsgPackValueImplE),
    reinterpret_cast<std::uintptr_t>(&_ZNK3sce2np8msg_pack16MsgPackArrayImpl18GetMessagePackSizeEv),
    reinterpret_cast<std::uintptr_t>(&_ZNK3sce2np8msg_pack16MsgPackArrayImpl12ToJsonStringEPcmPmi),
};

static int APS5_VABI NpMsgPackBinaryImplType(const void*) { return 0x2004; }
std::uintptr_t _ZTVN3sce2np8msg_pack17MsgPackBinaryImplE_nid_postfix[] {
    0, 0,
    reinterpret_cast<std::uintptr_t>(&_ZN3sce2np8msg_pack17MsgPackBinaryImplD1Ev),
    reinterpret_cast<std::uintptr_t>(&_ZN3sce2np8msg_pack17MsgPackBinaryImplD0Ev),
    reinterpret_cast<std::uintptr_t>(&NpMsgPackBinaryImplType),
    reinterpret_cast<std::uintptr_t>(&_ZNK3sce2np8msg_pack17MsgPackBinaryImpl6EqualsERKNS1_16MsgPackValueImplE),
    reinterpret_cast<std::uintptr_t>(&_ZNK3sce2np8msg_pack17MsgPackBinaryImpl18GetMessagePackSizeEv),
    reinterpret_cast<std::uintptr_t>(&_ZNK3sce2np8msg_pack17MsgPackBinaryImpl12ToJsonStringEPcmPmi),
};

static int APS5_VABI NpMsgPackStringImplType(const void*) { return 0x2001; }
std::uintptr_t _ZTVN3sce2np8msg_pack17MsgPackStringImplE_nid_postfix[] {
    0, 0,
    reinterpret_cast<std::uintptr_t>(&_ZN3sce2np8msg_pack17MsgPackStringImplD1Ev),
    reinterpret_cast<std::uintptr_t>(&_ZN3sce2np8msg_pack17MsgPackStringImplD0Ev),
    reinterpret_cast<std::uintptr_t>(&NpMsgPackStringImplType),
    reinterpret_cast<std::uintptr_t>(&_ZNK3sce2np8msg_pack17MsgPackStringImpl6EqualsERKNS1_16MsgPackValueImplE),
    reinterpret_cast<std::uintptr_t>(&_ZNK3sce2np8msg_pack17MsgPackStringImpl18GetMessagePackSizeEv),
    reinterpret_cast<std::uintptr_t>(&_ZNK3sce2np8msg_pack17MsgPackStringImpl12ToJsonStringEPcmPmi),
};


std::uintptr_t _ZTVN3sce2np8SelectorE_nid_postfix[] {
    0, 0,
    reinterpret_cast<std::uintptr_t>(&_ZN3sce2np8SelectorD2Ev),
    reinterpret_cast<std::uintptr_t>(&_ZN3sce2np8SelectorD0Ev),
};

std::uintptr_t _ZTVN3sce2np20BufferedStreamReaderE_nid_postfix[] {
    0, 0,
    reinterpret_cast<std::uintptr_t>(&_ZN3sce2np20BufferedStreamReaderD1Ev),
    reinterpret_cast<std::uintptr_t>(&_ZN3sce2np20BufferedStreamReaderD0Ev),
    reinterpret_cast<std::uintptr_t>(&_ZN3sce2np20BufferedStreamReader4ReadEPNS0_6HandleEPvmPm),
};

std::uintptr_t _ZTVN3sce2np20BufferedStreamWriterE_nid_postfix[] {
    0, 0,
    reinterpret_cast<std::uintptr_t>(&_ZN3sce2np20BufferedStreamWriterD1Ev),
    reinterpret_cast<std::uintptr_t>(&_ZN3sce2np20BufferedStreamWriterD0Ev),
    reinterpret_cast<std::uintptr_t>(&_ZN3sce2np20BufferedStreamWriter5WriteEPNS0_6HandleEPKvmPm),
};

std::uintptr_t _ZTVN3sce2np8msg_pack10MsgPackDocE_nid_postfix[] {
    0, 0,
    reinterpret_cast<std::uintptr_t>(&_ZN3sce2np8msg_pack10MsgPackDocD1Ev),
    reinterpret_cast<std::uintptr_t>(&_ZN3sce2np8msg_pack10MsgPackDocD0Ev),
};

std::uintptr_t _ZTVN3sce2np8msg_pack13MsgPackParserE_nid_postfix[] {
    0, 0,
    reinterpret_cast<std::uintptr_t>(&_ZN3sce2np8msg_pack13MsgPackParserD1Ev),
    reinterpret_cast<std::uintptr_t>(&_ZN3sce2np8msg_pack13MsgPackParserD0Ev),
};

int APS5_VABI _ZN3sce2np10CancelLock3EndEPKciS3_() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np10CancelLock5BeginEPNS0_6HandleEPKciS5_() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np10CancelLockC1Ev() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np10CancelLockD1Ev() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np10CancelLockD2Ev() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np10Cancelable10IsCanceledEv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np10Cancelable11CheckCancelEPKciS3_() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np10Cancelable12UnlockCancelEPKciS3_() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np10Cancelable13SetCancelableEb() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np10Cancelable14SetupSubCancelEPS1_PKciS4_() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np10Cancelable16CleanupSubCancelEPS1_() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np10Cancelable4InitEv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np10Cancelable6CancelEij() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np10CancelableD0Ev() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np10CancelableD2Ev() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np10EventQueue10ClearAbortEt() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np10EventQueue10TryDequeueEPvm() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np10EventQueue4InitEPKcmm() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np10EventQueue4dtorEv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np10EventQueue5AbortEt() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np10EventQueue7DequeueEPvmj() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np10EventQueue7DestroyEv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np10EventQueueC1EP16SceNpAllocatorEx() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np10EventQueueD1Ev() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np10EventQueueD2Ev() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np10JsonNumber5ClearEv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np10JsonNumber6SetNumEPKc() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np10JsonNumber6SetNumEi() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np10JsonNumber6SetNumEj() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np10JsonNumber6SetNumEl() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np10JsonNumber6SetNumEm() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np10JsonObject12SetFieldBoolEPKcbPPNS0_8JsonBoolE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np10JsonObject13GetFieldArrayEPKc() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np10JsonObject13GetFieldArrayEiPPKc() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np10JsonObject13SetFieldArrayEPKcPPNS0_9JsonArrayE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np10JsonObject13SetFieldArrayEPKcRKNS0_9JsonArrayEPPS4_() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np10JsonObject14GetFieldObjectEPKc() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np10JsonObject14GetFieldObjectEiPPKc() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np10JsonObject14SetFieldNumberEPKciPPNS0_10JsonNumberE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np10JsonObject14SetFieldNumberEPKcjPPNS0_10JsonNumberE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np10JsonObject14SetFieldNumberEPKclPPNS0_10JsonNumberE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np10JsonObject14SetFieldNumberEPKcmPPNS0_10JsonNumberE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np10JsonObject14SetFieldObjectEPKcPPS1_() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np10JsonObject14SetFieldObjectEPKcRKS1_PPS1_() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np10JsonObject14SetFieldStringEPKcS3_PPNS0_10JsonStringE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np10JsonObject16DeleteFieldValueEPKc() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np10JsonObject5ClearEv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np10JsonParser17JsonParseCallbackEP14JsonParseInfo2ijjPKcPK13JsonValueElm2iPv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np10JsonParser17jsonParseCallbackEP14JsonParseInfo2ijjPKcPK13JsonValueElm2i() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np10JsonParser4InitEPK7JsonDefPNS1_12EventHandlerE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np10JsonParser5ParseEPKcm() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np10JsonParser5ParseEPNS0_6HandleEPNS0_12StreamReaderEPNS0_9StreamCtxE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np10JsonParserC1EP16SceNpAllocatorEx() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np10JsonParserC2EP16SceNpAllocatorEx() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np10JsonParserD0Ev() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np10JsonParserD1Ev() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np10JsonParserD2Ev() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np10JsonString5ClearEv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np10JsonString6SetStrEPKc() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np10MemoryFile11GetFileSizeEPl() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np10MemoryFile11WriteToFileEPNS0_6HandleEPNS0_4FileE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np10MemoryFile12ExtendBufferEv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np10MemoryFile12ReadFromFileEPNS0_6HandleEPNS0_4FileE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np10MemoryFile4OpenEPKNS1_9OpenParamE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np10MemoryFile4ReadEPNS0_6HandleEPvmPm() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np10MemoryFile4SeekEliPl() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np10MemoryFile4SyncEv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np10MemoryFile5CloseEv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np10MemoryFile5WriteEPNS0_6HandleEPKvmPm() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np10MemoryFileC1EP16SceNpAllocatorEx() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np10MemoryFileC2EP16SceNpAllocatorEx() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np10MemoryFileD2Ev() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np10NpCommSign5ClearEv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np10NpCommSign7SetDataEPKhm() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np10NpCommSign8BuildHexERKS1_Pcmb() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np10NpCommSign8ParseHexEPS1_PKcm() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np10NpCommSignC1ERK27SceNpCommunicationSignature() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np10NpCommSignC1ERKS1_() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np10NpCommSignC1Ev() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np10NpCommSignC2ERK27SceNpCommunicationSignature() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np10NpCommSignC2ERKS1_() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np10NpCommSignC2Ev() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np10NpCommSignD0Ev() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np10NpCommSignD1Ev() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np10NpCommSignD2Ev() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np12HttpTemplate16SetSCEOnlyOptionEv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np12HttpTemplate4InitEiPKcib() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np12HttpTemplateC1Ev() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np12HttpTemplateD0Ev() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np12HttpTemplateD2Ev() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np12SelectorItem4InitEPKc() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np12SelectorItem6NotifyEv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np12SelectorItem9GetEventsEv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np12SelectorItemC2Ev() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np12SelectorItemD0Ev() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np12SelectorItemD1Ev() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("H74u5owPMbY", _ZN3sce2np12SelectorItemD1Ev);

int APS5_VABI _ZN3sce2np12StreamBuffer10GetBufSizeEv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np12StreamBuffer11GetCapacityEv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np12StreamBuffer11GetDataSizeEv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np12StreamBuffer14CheckinForReadEm() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np12StreamBuffer15CheckinForWriteEm() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np12StreamBuffer15CheckoutForReadEPm() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np12StreamBuffer16CheckoutForWriteEPmb() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np12StreamBuffer4ReadEPvm() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np12StreamBuffer5ResetEv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np12StreamBuffer5WriteEPKvmb() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np12StreamBuffer6IsFullEv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np12StreamBuffer7FreeBufEv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np12StreamBuffer7IsEmptyEv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np12StreamBuffer8AllocBufEm() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np12StreamBufferC1EP16SceNpAllocatorEx() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np12StreamBufferC2EP16SceNpAllocatorEx() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np12StreamBufferD1Ev() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np12StreamBufferD2Ev() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np12StreamBufferixEi() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np12WorkerThread10ThreadMainEv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np12WorkerThreadC1EPNS0_9WorkQueueE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np12WorkerThreadC2EPNS0_9WorkQueueE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np12WorkerThreadD1Ev() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np12WorkerThreadD2Ev() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np13JsonDocParser5ParseEPKcm() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np13JsonDocParser5ParseEPNS0_6HandleEPNS0_12StreamReaderEPNS0_9StreamCtxE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np13JsonDocParser9GetResultEPPNS0_10JsonObjectE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np13JsonDocParser9GetResultEPPNS0_9JsonValueE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np13JsonDocParserC1EP16SceNpAllocatorExPK7JsonDef() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np13JsonDocParserC2EP16SceNpAllocatorExPK7JsonDef() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np13JsonDocParserD0Ev() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np13JsonDocParserD1Ev() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np13JsonDocParserD2Ev() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np13LocalFilePathC1EP16SceNpAllocatorEx() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np13LocalFilePathC2EP16SceNpAllocatorEx() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np13LocalFilePathD0Ev() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np13LocalFilePathD1Ev() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np13LocalFilePathD2Ev() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np14CalloutContext4InitEPKNS1_5ParamE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np14CalloutContext4InitEPKcimm() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np14CalloutContext7DestroyEv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np14CalloutContextC1Ev() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np14CalloutContextC2Ev() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np14CalloutContextD2Ev() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np14JsonDocBuilder16EscapeJsonStringEPKcPcmPm() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np14JsonDocBuilder23EscapeJsonStringBufSizeEPKc() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np14JsonDocBuilder5BuildEPNS0_6HandleEPNS0_12StreamWriterEPNS0_9StreamCtxE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np14JsonDocBuilder5BuildEPcmPm() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np14JsonDocBuilderD0Ev() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np14JsonDocBuilderD1Ev() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np14JsonDocBuilderD2Ev() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np15CancelableScope3EndEiPKciS3_() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np15CancelableScopeC2Ev() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np15CancelableScopeD2Ev() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np15StreamInstaller21SetFileSizeWorkAroundEl() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np15StreamInstaller4InitEPNS0_12StreamReaderEPNS0_12StreamWriterEPNS0_9StreamCtxE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np15StreamInstaller4ReadEPNS0_6HandleEPvmPm() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np15StreamInstaller7InstallEPNS0_6HandleE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np15StreamInstallerC1EP16SceNpAllocatorEx() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np15StreamInstallerC2EP16SceNpAllocatorEx() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np15StreamInstallerD0Ev() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np15StreamInstallerD1Ev() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np15StreamInstallerD2Ev() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np16StreamReadBuffer17FillBufFromStreamEPNS0_6HandleEPNS0_12StreamReaderEmPm() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np16StreamReadBufferC1EP16SceNpAllocatorEx() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np16StreamReadBufferC2EP16SceNpAllocatorEx() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np17StreamWriteBuffer13FlushToStreamEPNS0_6HandleEPNS0_12StreamWriterEPm() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np17StreamWriteBuffer7FillBufEPKvmPm() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np17StreamWriteBufferC1EP16SceNpAllocatorEx() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np17StreamWriteBufferC2EP16SceNpAllocatorEx() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np17StreamWriteBufferD1Ev() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np17StreamWriteBufferD2Ev() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np18HttpConnectionPool23CloseAllIdleConnectionsEv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np18HttpConnectionPool4InitEi() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np18HttpConnectionPool7DestroyEv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np18HttpConnectionPoolC1EP16SceNpAllocatorEx() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np18HttpConnectionPoolC2EP16SceNpAllocatorEx() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np18HttpConnectionPoolD0Ev() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np18HttpConnectionPoolD1Ev() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np18HttpConnectionPoolD2Ev() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np20BufferedStreamReader4OpenEPNS0_12StreamReaderEm() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np20BufferedStreamReader4ReadEPNS0_6HandleEPvmPm() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np20BufferedStreamReader5CloseEv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np20BufferedStreamReaderC1EP16SceNpAllocatorEx() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np20BufferedStreamReaderC2EP16SceNpAllocatorEx() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np20BufferedStreamReaderD0Ev() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np20BufferedStreamWriter4OpenEPNS0_12StreamWriterEm() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np20BufferedStreamWriter5CloseEPNS0_6HandleE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np20BufferedStreamWriter5FlushEPNS0_6HandleE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np20BufferedStreamWriter5WriteEPNS0_6HandleEPKvmPm() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np20BufferedStreamWriterC1EP16SceNpAllocatorEx() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np20BufferedStreamWriterC2EP16SceNpAllocatorEx() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np20BufferedStreamWriterD0Ev() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np20BufferedStreamWriterD1Ev() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("MeAc+ooYzaI", _ZN3sce2np20BufferedStreamWriterD1Ev);

int APS5_VABI _ZN3sce2np3Dir14MkdirRecursiveEPKctP16SceNpAllocatorEx() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np3Dir15CreateDirectoryEPNS0_13LocalFilePathEt() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np3Dir15RemoveRecursiveEPKcP16SceNpAllocatorEx() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np3Dir16ConvertErrorCodeEi() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np3Dir4OpenEPKcj() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np3Dir4ReadEPNS0_6DirEntEPb() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np3Dir5CloseEv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np3Dir5MkdirEPKctP16SceNpAllocatorEx() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np3Dir6RemoveEPKc() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np3Dir8IsOpenedEv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np3DirC1EP16SceNpAllocatorEx() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np3DirC2EP16SceNpAllocatorEx() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np3DirD0Ev() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np3DirD1Ev() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np3DirD2Ev() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np3ipc10IpmiClient10DisconnectEv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np3ipc10IpmiClient16invokeSyncMethodEjPKvmPvPmm() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np3ipc10IpmiClient4InitEPKNS2_6ConfigE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np3ipc10IpmiClient4ctorEv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np3ipc10IpmiClient4dtorEv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np3ipc10IpmiClient7DestroyEv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np3ipc10IpmiClientC1Ev() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np3ipc10IpmiClientC2Ev() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np3ipc10IpmiClientD0Ev() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np3ipc10IpmiClientD1Ev() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np3ipc10IpmiClientD2Ev() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np3ipc13ServiceClientC1EPNS1_17ServiceIpmiClientEPKNS1_17ServiceClientInfoE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np3ipc13ServiceClientC2EPNS1_17ServiceIpmiClientEPKNS1_17ServiceClientInfoE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np3ipc17ServiceIpmiClient10EndRequestEii() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np3ipc17ServiceIpmiClient11InitServiceEi() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np3ipc17ServiceIpmiClient11WaitRequestEiij() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np3ipc17ServiceIpmiClient11findServiceEi() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np3ipc17ServiceIpmiClient12BeginRequestEii() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np3ipc17ServiceIpmiClient13DeleteRequestEii() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np3ipc17ServiceIpmiClient13IsServiceInitEi() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np3ipc17ServiceIpmiClient13PollEventFlagEijmjPm() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np3ipc17ServiceIpmiClient13WaitEventFlagEijmjPmj() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np3ipc17ServiceIpmiClient14PollEventQueueEiPvm() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np3ipc17ServiceIpmiClient15CancelEventFlagEijm() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np3ipc17ServiceIpmiClient16RegisterServicesEPKNS1_17ServiceClientInfoE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np3ipc17ServiceIpmiClient17invokeInitServiceEi() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np3ipc17ServiceIpmiClient17invokeTermServiceEi() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np3ipc17ServiceIpmiClient18EndRequestForAsyncEii() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np3ipc17ServiceIpmiClient19WaitRequestForAsyncEiij() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np3ipc17ServiceIpmiClient20AbortRequestForAsyncEii() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np3ipc17ServiceIpmiClient20BeginRequestForAsyncEiiPN4IPMI6Client12EventNotifeeE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np3ipc17ServiceIpmiClient4InitEPNS2_6ConfigE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np3ipc17ServiceIpmiClient4ctorEv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np3ipc17ServiceIpmiClient4dtorEv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np3ipc17ServiceIpmiClient7DestroyEv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np3ipc17ServiceIpmiClientC1Ev() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np3ipc17ServiceIpmiClientC2Ev() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np3ipc17ServiceIpmiClientD0Ev() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np3ipc17ServiceIpmiClientD1Ev() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np3ipc17ServiceIpmiClientD2Ev() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np4File8MoveDataEPNS0_6HandleElllPvm() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np4Path10AppendStrsEiP13__va_list_tag() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np4Path10AppendStrsEiz() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np4Path11BuildAppendEPcmcPKcm() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np4Path12AddDelimiterEPcmc() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np4Path14CheckBuildSizeEcPKcm() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np4Path14RemoveBasenameEPcmc() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np4Path14RemoveBasenameEv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np4Path20CheckBuildAppendSizeEPccPKcm() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np4Path22CheckDelimiterRequiredEPKcc() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np4Path5BuildEPcmcPKcm() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np4Path5ClearEv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np4Path6SetFmtEPKcz() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np4Path6SetStrEPKc() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np4Path6SetStrEPKcm() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np4Path7SetStrsEiP13__va_list_tag() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np4Path7SetStrsEiz() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np4Path9AppendStrEPKcm() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np4Path9AppendStrEPKc() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np4Path9ExpandBufEm() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np4Path9ExpandBufEv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np4PathC1EP16SceNpAllocatorExc() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np4PathC2EP16SceNpAllocatorExc() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np4PathD0Ev() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np4PathD1Ev() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np4Time12ParseRFC3339EPKc() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np5NpEnv8GetNpEnvEPS1_() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np6Handle10CancelImplEi() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np6Handle7DestroyEv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np6HandleC1Ev() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np6HandleC2Ev() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np6HandleD0Ev() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np6HandleD2Ev() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np6Thread12DoThreadMainEv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np6Thread4InitEPKNS1_5ParamE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np6Thread4JoinEPi() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np6Thread4ctorEv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np6Thread5StartEv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np6Thread7DestroyEv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np6Thread9EntryFuncEPv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np6Thread9GetResultEv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np6Thread9IsRunningEv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np6ThreadC2Ev() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np6ThreadD0Ev() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np6ThreadD2Ev() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np7Callout5StartEjPNS1_7HandlerE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np7Callout5StartEmPNS1_7HandlerE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np7Callout9IsStartedEv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np7CalloutC1EPNS0_14CalloutContextE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np7CalloutC2EPNS0_14CalloutContextE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np7CalloutD0Ev() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np7CalloutD1Ev() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np7CalloutD2Ev() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np7HttpUri5BuildEPKS1_PcmPmj() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np7HttpUri5BuildEPKcS3_PKNS1_10QueryParamEiPcmPm() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np7HttpUri5ParseEPS1_PKc() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np7HttpUriC1EP16SceNpAllocatorEx() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np7HttpUriC2EP16SceNpAllocatorEx() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np7HttpUriD0Ev() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np7HttpUriD2Ev() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np8HttpFile4OpenEPNS0_6HandleERKNS0_12HttpTemplateEPKcS8_tS8_PKNS0_9HttpTrans12TimeoutParamE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np8HttpFile4OpenEPNS0_6HandleERKNS0_12HttpTemplateEPNS0_18HttpConnectionPoolEPKcSA_tSA_PKNS0_9HttpTrans12TimeoutParamE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np8HttpFile4ReadEPNS0_6HandleEPvmPm() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np8HttpFile5CloseEv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np8HttpFileC1EP16SceNpAllocatorEx() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np8HttpFileC2EP16SceNpAllocatorEx() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np8HttpFileD0Ev() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np8HttpFileD1Ev() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np8HttpFileD2Ev() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np8JsonBool7SetBoolEb() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np8JsonFile4OpenEPKcit() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np8JsonFile5CloseEv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np8JsonFile8ReadJsonEPNS0_6HandleEPPNS0_10JsonObjectE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np8JsonFile9WriteJsonEPNS0_6HandleERKNS0_10JsonObjectE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np8JsonFileC1EP16SceNpAllocatorExPK7JsonDef() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np8JsonFileC2EP16SceNpAllocatorExPK7JsonDef() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np8JsonFileD0Ev() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np8JsonFileD1Ev() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np8Selector10UnregisterEPNS0_12SelectorItemE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np8Selector12GetFirstItemEv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np8Selector13GetEventsImplEPNS1_5EventEi() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np8Selector13UnregisterAllEv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np8Selector4FindEi() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np8Selector4InitEPKc() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np8Selector4PollEPNS1_5EventEiPi() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np8Selector4WaitEPNS1_5EventEiPij() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np8Selector6NotifyEPNS0_12SelectorItemE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np8Selector8RegisterEPNS0_12SelectorItemEj() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np8SelectorC1EP16SceNpAllocatorEx() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np8SelectorC2EP16SceNpAllocatorEx() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np8SelectorD0Ev() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np8WorkItem11SetFinishedEi() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np8WorkItem14FinishCallbackEv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np8WorkItem15RemoveFromQueueEv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np8WorkItem9BindQueueEPNS0_9WorkQueueEi() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np8WorkItemC2EPKc() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np8WorkItemD1Ev() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np8WorkItemD2Ev() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np8msg_pack10MsgPackDoc3AddERKNS1_12MsgPackValueE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np8msg_pack10MsgPackDoc5ClearEv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np8msg_pack10MsgPackDocD0Ev() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np8msg_pack10MsgPackDocD1Ev() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("cVxiXMcEG2s", _ZN3sce2np8msg_pack10MsgPackDocD1Ev);

int APS5_VABI _ZN3sce2np8msg_pack10MsgPackMap10IsValidKeyERKNS1_12MsgPackValueE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np8msg_pack10MsgPackMap12PutAndGetPtrERKNS1_12MsgPackValueES5_PPS3_() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np8msg_pack10MsgPackMap3PutERKNS1_12MsgPackValueES5_() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np8msg_pack10MsgPackMap6CreateEP14SceNpAllocatorPS2_() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np8msg_pack10MsgPackMap6RemoveERKNS1_12MsgPackValueEPS3_() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np8msg_pack10MsgPackMap8AllocateEP14SceNpAllocator() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np8msg_pack12MsgPackArray12AddAndGetPtrERKNS1_12MsgPackValueEPPS3_() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np8msg_pack12MsgPackArray3AddERKNS1_12MsgPackValueE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np8msg_pack12MsgPackArray3AddEjRKNS1_12MsgPackValueE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np8msg_pack12MsgPackArray6CreateEP14SceNpAllocatorPS2_() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np8msg_pack12MsgPackArray6RemoveEjPNS1_12MsgPackValueE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np8msg_pack12MsgPackArray8AllocateEP14SceNpAllocator() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np8msg_pack12MsgPackValue9IsDerivedENS1_11MsgPackTypeES3_() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np8msg_pack12MsgPackValueC1ENS1_11MsgPackTypeE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np8msg_pack12MsgPackValueC1ERKS2_() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np8msg_pack12MsgPackValueC1Ev() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np8msg_pack12MsgPackValueC2ENS1_11MsgPackTypeE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np8msg_pack12MsgPackValueC2ERKS2_() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np8msg_pack12MsgPackValueC2Ev() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np8msg_pack12MsgPackValueD1Ev() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np8msg_pack12MsgPackValueD2Ev() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np8msg_pack13MsgPackBinary6CreateEP14SceNpAllocatorPS2_() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np8msg_pack13MsgPackBinary8AllocateEP14SceNpAllocator() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np8msg_pack13MsgPackBinary8SetValueEPKvm() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np8msg_pack13MsgPackPacker10InitializeEPNS0_12StreamWriterEPNS0_9StreamCtxE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np8msg_pack13MsgPackPacker10PackBinaryEPNS0_6HandleEPKvm() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np8msg_pack13MsgPackPacker10PackStringEPNS0_6HandleEPKcm() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np8msg_pack13MsgPackPacker10PackUInt32EPNS0_6HandleEj() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np8msg_pack13MsgPackPacker10PackUInt64EPNS0_6HandleEm() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np8msg_pack13MsgPackPacker11PackFloat32EPNS0_6HandleEf() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np8msg_pack13MsgPackPacker11PackFloat64EPNS0_6HandleEd() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np8msg_pack13MsgPackPacker5WriteEPNS0_6HandleEPKvm() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np8msg_pack13MsgPackPacker6Write1EPNS0_6HandleEh() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np8msg_pack13MsgPackPacker6Write2EPNS0_6HandleEt() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np8msg_pack13MsgPackPacker6Write4EPNS0_6HandleEj() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np8msg_pack13MsgPackPacker6Write8EPNS0_6HandleEm() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np8msg_pack13MsgPackPacker7PackMapEPNS0_6HandleEm() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np8msg_pack13MsgPackPacker8FinalizeEv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np8msg_pack13MsgPackPacker8PackBoolEPNS0_6HandleEb() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np8msg_pack13MsgPackPacker9PackArrayEPNS0_6HandleEm() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np8msg_pack13MsgPackPacker9PackInt32EPNS0_6HandleEi() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np8msg_pack13MsgPackPacker9PackInt64EPNS0_6HandleEl() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np8msg_pack13MsgPackParser10ParseArrayEPNS0_6HandleEPNS1_15MsgPackUnpackerEmPNS1_12MsgPackValueE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np8msg_pack13MsgPackParser10ParseInt32EPNS0_6HandleEPNS1_15MsgPackUnpackerEPNS1_12MsgPackValueE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np8msg_pack13MsgPackParser10ParseInt64EPNS0_6HandleEPNS1_15MsgPackUnpackerEPNS1_12MsgPackValueE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np8msg_pack13MsgPackParser11ParseBinaryEPNS0_6HandleEPNS1_15MsgPackUnpackerEmPNS1_12MsgPackValueE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np8msg_pack13MsgPackParser11ParseStringEPNS0_6HandleEPNS1_15MsgPackUnpackerEmPNS1_12MsgPackValueE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np8msg_pack13MsgPackParser11ParseUInt32EPNS0_6HandleEPNS1_15MsgPackUnpackerEPNS1_12MsgPackValueE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np8msg_pack13MsgPackParser11ParseUInt64EPNS0_6HandleEPNS1_15MsgPackUnpackerEPNS1_12MsgPackValueE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np8msg_pack13MsgPackParser12ParseFloat32EPNS0_6HandleEPNS1_15MsgPackUnpackerEPNS1_12MsgPackValueE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np8msg_pack13MsgPackParser12ParseFloat64EPNS0_6HandleEPNS1_15MsgPackUnpackerEPNS1_12MsgPackValueE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np8msg_pack13MsgPackParser5ParseEPKvm() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np8msg_pack13MsgPackParser5ParseEPNS0_6HandleEPNS0_12StreamReaderEPNS0_9StreamCtxE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np8msg_pack13MsgPackParser8ParseMapEPNS0_6HandleEPNS1_15MsgPackUnpackerEmPNS1_12MsgPackValueE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np8msg_pack13MsgPackParser9GetResultEPPNS1_10MsgPackDocE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np8msg_pack13MsgPackParser9ParseBoolEPNS0_6HandleEPNS1_15MsgPackUnpackerEPNS1_12MsgPackValueE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np8msg_pack13MsgPackParser9ParseImplEPNS0_6HandleEPNS1_15MsgPackUnpackerEPNS1_12MsgPackValueE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np8msg_pack13MsgPackParserD0Ev() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np8msg_pack13MsgPackParserD1Ev() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("O9+ZlqCjPxE", _ZN3sce2np8msg_pack13MsgPackParserD1Ev);

int APS5_VABI _ZN3sce2np8msg_pack13MsgPackString6CreateEP14SceNpAllocatorPS2_() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np8msg_pack13MsgPackString8AllocateEP14SceNpAllocator() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np8msg_pack13MsgPackString8SetValueEPKc() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np8msg_pack14MsgPackBuilder5BuildEPNS0_6HandleEPNS0_12StreamWriterEPNS0_9StreamCtxE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np8msg_pack14MsgPackBuilder5BuildEPvmPm() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np8msg_pack14MsgPackBuilder9BuildImplEPNS0_6HandleEPNS1_13MsgPackPackerERKNS1_12MsgPackValueE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np8msg_pack14MsgPackMapImpl12PutAndGetPtrERKNS1_12MsgPackValueES5_PPS3_() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np8msg_pack14MsgPackMapImpl3PutERKNS1_12MsgPackValueES5_() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np8msg_pack14MsgPackMapImpl6RemoveERKNS1_12MsgPackValueEPS3_() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np8msg_pack14MsgPackMapImplC1EP14SceNpAllocator() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np8msg_pack14MsgPackMapImplC2EP14SceNpAllocator() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np8msg_pack14MsgPackMapImplD0Ev() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np8msg_pack14MsgPackMapImplD1Ev() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("AdHs9XUPQOg", _ZN3sce2np8msg_pack14MsgPackMapImplD1Ev);

int APS5_VABI _ZN3sce2np8msg_pack15MsgPackUnpacker10InitializeEPNS0_12StreamReaderEPNS0_9StreamCtxE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np8msg_pack15MsgPackUnpacker10UnpackBoolEPNS0_6HandleEPb() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np8msg_pack15MsgPackUnpacker11UnpackInt32EPNS0_6HandleEPi() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np8msg_pack15MsgPackUnpacker11UnpackInt64EPNS0_6HandleEPl() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np8msg_pack15MsgPackUnpacker12UnpackBinaryEPNS0_6HandleEPvm() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np8msg_pack15MsgPackUnpacker12UnpackFormatEPNS0_6HandleEPNS1_11MsgPackTypeEPm() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np8msg_pack15MsgPackUnpacker12UnpackStringEPNS0_6HandleEPcm() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np8msg_pack15MsgPackUnpacker12UnpackUInt32EPNS0_6HandleEPj() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np8msg_pack15MsgPackUnpacker12UnpackUInt64EPNS0_6HandleEPm() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np8msg_pack15MsgPackUnpacker13UnpackFloat32EPNS0_6HandleEPf() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np8msg_pack15MsgPackUnpacker13UnpackFloat64EPNS0_6HandleEPd() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np8msg_pack15MsgPackUnpacker17InitInternalStateEv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np8msg_pack15MsgPackUnpacker22OnUnpackValueCompletedEv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np8msg_pack15MsgPackUnpacker4ReadEPNS0_6HandleEPvmPm() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np8msg_pack15MsgPackUnpacker5Read1EPNS0_6HandleEPh() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np8msg_pack15MsgPackUnpacker5Read2EPNS0_6HandleEPt() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np8msg_pack15MsgPackUnpacker5Read4EPNS0_6HandleEPj() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np8msg_pack15MsgPackUnpacker5Read8EPNS0_6HandleEPm() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np8msg_pack15MsgPackUnpacker8FinalizeEv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np8msg_pack16MsgPackArrayImpl16AddLastAndGetPtrERKNS1_12MsgPackValueEPPS3_() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np8msg_pack16MsgPackArrayImpl3AddEjRKNS1_12MsgPackValueE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np8msg_pack16MsgPackArrayImpl6RemoveEjPNS1_12MsgPackValueE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np8msg_pack16MsgPackArrayImpl7AddLastERKNS1_12MsgPackValueE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np8msg_pack16MsgPackArrayImpl8AddFirstERKNS1_12MsgPackValueE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np8msg_pack16MsgPackArrayImplC1EP14SceNpAllocator() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np8msg_pack16MsgPackArrayImplC2EP14SceNpAllocator() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np8msg_pack16MsgPackArrayImplD0Ev() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np8msg_pack16MsgPackArrayImplD1Ev() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("iuilWJsw1OA", _ZN3sce2np8msg_pack16MsgPackArrayImplD1Ev);

int APS5_VABI _ZN3sce2np8msg_pack16MsgPackValueImpl6AddRefEv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np8msg_pack16MsgPackValueImpl7ReleaseEv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np8msg_pack17MsgPackBinaryImpl8SetValueEPKvm() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np8msg_pack17MsgPackBinaryImplD0Ev() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np8msg_pack17MsgPackBinaryImplD1Ev() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("0cyGJtj6Mos", _ZN3sce2np8msg_pack17MsgPackBinaryImplD1Ev);

int APS5_VABI _ZN3sce2np8msg_pack17MsgPackStringImpl8SetValueEPKc() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np8msg_pack17MsgPackStringImplD0Ev() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np8msg_pack17MsgPackStringImplD1Ev() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("72TLahYlJI4", _ZN3sce2np8msg_pack17MsgPackStringImplD1Ev);

int APS5_VABI _ZN3sce2np9HttpTrans10SetTimeoutEPKNS1_12TimeoutParamE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np9HttpTrans11SendRequestEPNS0_6HandleEPKvm() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np9HttpTrans12RecvResponseEPNS0_6HandleEPvmPm() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np9HttpTrans12SkipResponseEPNS0_6HandleE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np9HttpTrans16AddRequestHeaderEPKcS3_() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np9HttpTrans16SetRequestHeaderEPKcS3_() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np9HttpTrans17GetResponseHeaderEPNS0_6HandleEPKcPvmPm() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np9HttpTrans21SetRequestContentTypeEPKc() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np9HttpTrans23SetRequestContentLengthEm() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np9HttpTrans4InitERKNS0_12HttpTemplateEPNS0_18HttpConnectionPoolEiPKcS8_tS8_m() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np9HttpTrans4InitERKNS0_12HttpTemplateEPNS0_18HttpConnectionPoolEiPKcm() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np9HttpTrans4ReadEPNS0_6HandleEPvmPm() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np9HttpTrans5WriteEPNS0_6HandleEPKvmPm() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np9HttpTrans7DestroyEv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np9HttpTransC2EP16SceNpAllocatorEx() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np9HttpTransD0Ev() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np9HttpTransD1Ev() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np9JsonArray11AddItemBoolEbPPNS0_8JsonBoolE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np9JsonArray12AddItemArrayERKS1_PPS1_() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np9JsonArray13AddItemNumberEiPPNS0_10JsonNumberE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np9JsonArray13AddItemNumberEjPPNS0_10JsonNumberE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np9JsonArray13AddItemNumberElPPNS0_10JsonNumberE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np9JsonArray13AddItemNumberEmPPNS0_10JsonNumberE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np9JsonArray13AddItemObjectEPPNS0_10JsonObjectE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np9JsonArray13AddItemObjectERKNS0_10JsonObjectEPPS2_() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np9JsonArray13AddItemStringEPKcPPNS0_10JsonStringE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np9JsonArray5ClearEv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np9JsonValue12AddItemValueEPS1_() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np9JsonValue12GetItemValueEi() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np9JsonValue13GetFieldValueEPKc() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np9JsonValue13GetFieldValueEiPPKc() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np9JsonValue13SetFieldValueEPKcPS1_() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np9JsonValueC2EP16SceNpAllocatorExNS1_9ValueTypeE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np9JsonValueD0Ev() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np9JsonValueD1Ev() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("FfSNfBmn+K8", _ZN3sce2np9JsonValueD1Ev);

int APS5_VABI _ZN3sce2np9LocalFile11GetFileSizeEPl() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np9LocalFile11GetFileStatEPKcPNS1_8FileStatE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np9LocalFile16ConvertErrorCodeEi() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np9LocalFile4OpenEPKcit() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np9LocalFile4ReadEPNS0_6HandleEPvmPm() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np9LocalFile4SeekEliPl() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np9LocalFile4SyncEv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np9LocalFile5WriteEPNS0_6HandleEPKvmPm() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np9LocalFile6ExistsEPKcPbS4_() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np9LocalFile6RenameEPKcS3_() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np9LocalFile8CopyFileEPNS0_6HandleEPKcS5_itP16SceNpAllocatorEx() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np9LocalFile8ReadFileEPNS0_6HandleEPKcPvPm() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np9LocalFileC2Ev() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np9LocalFileD1Ev() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np9LocalFileD2Ev() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np9WorkQueue16WorkItemFinishedEPNS0_8WorkItemEi() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np9WorkQueue17ProcFinishedItemsENS0_14WorkItemStatusE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np9WorkQueue18RemoveFinishedItemEPNS0_8WorkItemE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np9WorkQueue18WaitForPendingItemEPPNS0_8WorkItemEPb() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np9WorkQueue4InitEPKNS0_6Thread5ParamE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np9WorkQueue4InitEPKcimm() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np9WorkQueue4StopEv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np9WorkQueue4dtorEv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np9WorkQueue5StartEv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np9WorkQueue6CancelEii() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np9WorkQueue6IsInitEv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np9WorkQueue7DestroyEv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np9WorkQueue7EnqueueEiPNS0_8WorkItemE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np9WorkQueue9CancelAllEi() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np9WorkQueue9IsRunningEv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np9WorkQueueC1Ev() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np9WorkQueueC2Ev() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np9WorkQueueD0Ev() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2np9WorkQueueD1Ev() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2npeqERK27SceNpCommunicationSignatureRKNS0_10NpCommSignE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2npeqERKNS0_10NpCommSignERK27SceNpCommunicationSignature() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2npeqERKNS0_10NpCommSignES3_() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2npneERK27SceNpCommunicationSignatureRKNS0_10NpCommSignE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2npneERKNS0_10NpCommSignERK27SceNpCommunicationSignature() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN3sce2npneERKNS0_10NpCommSignES3_() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNK3sce2np10Cancelable6IsInitEv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNK3sce2np10EventQueue7IsEmptyEv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNK3sce2np10JsonNumber10CloneValueEP16SceNpAllocatorEx() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNK3sce2np10JsonNumber5CloneEP16SceNpAllocatorEx() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNK3sce2np10JsonNumber6GetNumEPi() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNK3sce2np10JsonNumber6GetNumEPj() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNK3sce2np10JsonNumber6GetNumEPl() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNK3sce2np10JsonNumber6GetNumEPm() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNK3sce2np10JsonNumber9GetNumStrEv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNK3sce2np10JsonObject10CloneValueEP16SceNpAllocatorEx() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNK3sce2np10JsonObject12GetFieldBoolEPKc() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNK3sce2np10JsonObject12GetFieldBoolEiPPKc() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNK3sce2np10JsonObject13GetFieldArrayEPKc() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNK3sce2np10JsonObject13GetFieldArrayEiPPKc() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNK3sce2np10JsonObject13GetFieldCountEv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNK3sce2np10JsonObject14GetFieldNumberEPKcPi() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNK3sce2np10JsonObject14GetFieldNumberEPKcPj() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNK3sce2np10JsonObject14GetFieldNumberEPKcPl() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNK3sce2np10JsonObject14GetFieldNumberEPKcPm() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNK3sce2np10JsonObject14GetFieldNumberEPKc() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNK3sce2np10JsonObject14GetFieldNumberEiPPKc() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNK3sce2np10JsonObject14GetFieldObjectEPKc() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNK3sce2np10JsonObject14GetFieldObjectEiPPKc() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNK3sce2np10JsonObject14GetFieldStringEPKcPcm() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNK3sce2np10JsonObject14GetFieldStringEPKc() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNK3sce2np10JsonObject14GetFieldStringEiPPKc() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNK3sce2np10JsonObject5CloneEP16SceNpAllocatorEx() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNK3sce2np10JsonString10CloneValueEP16SceNpAllocatorEx() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNK3sce2np10JsonString5CloneEP16SceNpAllocatorEx() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNK3sce2np10JsonString6GetStrEv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNK3sce2np10MemoryFile8IsOpenedEv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNK3sce2np10NpCommSign6EqualsEPhm() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNK3sce2np10NpCommSign6EqualsERK27SceNpCommunicationSignature() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNK3sce2np10NpCommSign6EqualsERKS1_() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNK3sce2np10NpCommSign7GetDataEPhm() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNK3sce2np10NpCommSign7IsEmptyEv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNK3sce2np12HttpTemplate6IsInitEv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNK3sce2np18HttpConnectionPool6IsInitEv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNK3sce2np20BufferedStreamReader8IsOpenedEv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNK3sce2np20BufferedStreamWriter8IsOpenedEv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNK3sce2np3ipc10IpmiClient6IsInitEv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNK3sce2np3ipc17ServiceIpmiClient6IsInitEv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNK3sce2np4Path6GetStrEv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNK3sce2np4Path7IsEmptyEv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNK3sce2np6Handle6IsInitEv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNK3sce2np6Thread6IsInitEv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNK3sce2np8HttpFile8IsOpenedEv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNK3sce2np8JsonBool10CloneValueEP16SceNpAllocatorEx() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNK3sce2np8JsonBool5CloneEP16SceNpAllocatorEx() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNK3sce2np8JsonBool7GetBoolEv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNK3sce2np8JsonFile8IsOpenedEv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNK3sce2np8JsonNull10CloneValueEP16SceNpAllocatorEx() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNK3sce2np8JsonNull5CloneEP16SceNpAllocatorEx() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNK3sce2np8msg_pack10MsgPackDoc18GetMessagePackSizeEv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNK3sce2np8msg_pack10MsgPackMap11ContainsKeyERKNS1_12MsgPackValueE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNK3sce2np8msg_pack10MsgPackMap3GetERKNS1_12MsgPackValueEPS3_() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNK3sce2np8msg_pack10MsgPackMap6GetPtrERKNS1_12MsgPackValueEPPS3_() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNK3sce2np8msg_pack10MsgPackMap7GetSizeEv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNK3sce2np8msg_pack10MsgPackMap8IteratorEv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNK3sce2np8msg_pack10MsgPackMap9CloneDeepEP14SceNpAllocatorPNS1_12MsgPackValueE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNK3sce2np8msg_pack12MsgPackArray3GetEjPNS1_12MsgPackValueE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNK3sce2np8msg_pack12MsgPackArray6GetPtrEjPPNS1_12MsgPackValueE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNK3sce2np8msg_pack12MsgPackArray7GetSizeEv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNK3sce2np8msg_pack12MsgPackArray8IteratorEv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNK3sce2np8msg_pack12MsgPackArray9CloneDeepEP14SceNpAllocatorPNS1_12MsgPackValueE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNK3sce2np8msg_pack12MsgPackValue12ToJsonStringEPcmPmi() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNK3sce2np8msg_pack12MsgPackValue13IsDerivedFromENS1_11MsgPackTypeE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNK3sce2np8msg_pack12MsgPackValue13IsDerivedFromERKS2_() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNK3sce2np8msg_pack12MsgPackValue16IsRefDerivedFromERKS2_() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNK3sce2np8msg_pack12MsgPackValue18GetMessagePackSizeEv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNK3sce2np8msg_pack12MsgPackValue5CloneEP14SceNpAllocatorPS2_() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNK3sce2np8msg_pack12MsgPackValue6EqualsERKS2_() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNK3sce2np8msg_pack13MsgPackBinary8GetValueEPPKvPm() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNK3sce2np8msg_pack13MsgPackBinary9CloneDeepEP14SceNpAllocatorPNS1_12MsgPackValueE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNK3sce2np8msg_pack13MsgPackString8GetValueEv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNK3sce2np8msg_pack13MsgPackString9CloneDeepEP14SceNpAllocatorPNS1_12MsgPackValueE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNK3sce2np8msg_pack14MsgPackMapImpl11ContainsKeyERKNS1_12MsgPackValueE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNK3sce2np8msg_pack14MsgPackMapImpl12ToJsonStringEPcmPmi() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNK3sce2np8msg_pack14MsgPackMapImpl18GetMessagePackSizeEv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNK3sce2np8msg_pack14MsgPackMapImpl3GetERKNS1_12MsgPackValueEPS3_() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNK3sce2np8msg_pack14MsgPackMapImpl6EqualsERKNS1_16MsgPackValueImplE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNK3sce2np8msg_pack14MsgPackMapImpl6GetPtrERKNS1_12MsgPackValueEPPS3_() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNK3sce2np8msg_pack14MsgPackMapImpl8GetEntryERKNS1_12MsgPackValueE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNK3sce2np8msg_pack16MsgPackArrayImpl12ToJsonStringEPcmPmi() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNK3sce2np8msg_pack16MsgPackArrayImpl14CreateNewEntryERKNS1_12MsgPackValueEPPNS2_5EntryE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNK3sce2np8msg_pack16MsgPackArrayImpl18GetMessagePackSizeEv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNK3sce2np8msg_pack16MsgPackArrayImpl3GetEjPNS1_12MsgPackValueE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNK3sce2np8msg_pack16MsgPackArrayImpl6EqualsERKNS1_16MsgPackValueImplE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNK3sce2np8msg_pack16MsgPackArrayImpl6GetPtrEjPPNS1_12MsgPackValueE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNK3sce2np8msg_pack16MsgPackArrayImpl8GetEntryEjPPNS2_5EntryE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNK3sce2np8msg_pack17MsgPackBinaryImpl12ToJsonStringEPcmPmi() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNK3sce2np8msg_pack17MsgPackBinaryImpl18GetMessagePackSizeEv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNK3sce2np8msg_pack17MsgPackBinaryImpl6EqualsERKNS1_16MsgPackValueImplE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNK3sce2np8msg_pack17MsgPackBinaryImpl8GetValueEPPKvPm() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNK3sce2np8msg_pack17MsgPackStringImpl12ToJsonStringEPcmPmi() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNK3sce2np8msg_pack17MsgPackStringImpl18GetMessagePackSizeEv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNK3sce2np8msg_pack17MsgPackStringImpl6EqualsERKNS1_16MsgPackValueImplE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNK3sce2np9HttpTrans6IsInitEv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNK3sce2np9JsonArray10CloneValueEP16SceNpAllocatorEx() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNK3sce2np9JsonArray11GetItemBoolEi() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNK3sce2np9JsonArray12GetItemArrayEi() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNK3sce2np9JsonArray12GetItemCountEv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNK3sce2np9JsonArray13GetItemNumberEi() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNK3sce2np9JsonArray13GetItemObjectEi() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNK3sce2np9JsonArray13GetItemStringEi() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNK3sce2np9JsonArray5CloneEP16SceNpAllocatorEx() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNK3sce2np9JsonValue11GetJsonBoolEv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNK3sce2np9JsonValue11GetJsonNullEv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNK3sce2np9JsonValue12GetItemValueEi() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNK3sce2np9JsonValue12GetJsonArrayEv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNK3sce2np9JsonValue13GetFieldValueEPKc() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNK3sce2np9JsonValue13GetFieldValueEiPPKc() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNK3sce2np9JsonValue13GetJsonNumberEv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNK3sce2np9JsonValue13GetJsonObjectEv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNK3sce2np9JsonValue13GetJsonStringEv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNK3sce2np9LocalFile8IsOpenedEv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZThn16_N3sce2np10MemoryFile5WriteEPNS0_6HandleEPKvmPm() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZThn16_N3sce2np10MemoryFileD0Ev() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZThn16_N3sce2np9HttpTransD1Ev() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZThn16_N3sce2np9LocalFile5WriteEPNS0_6HandleEPKvmPm() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZThn16_N3sce2np9LocalFileD0Ev() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZThn16_N3sce2np9LocalFileD1Ev() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZThn8_N3sce2np10MemoryFileD0Ev() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZThn8_N3sce2np10MemoryFileD1Ev() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZThn8_N3sce2np6Handle10CancelImplEi() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZThn8_N3sce2np6HandleD0Ev() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZThn8_N3sce2np9HttpTrans4ReadEPNS0_6HandleEPvmPm() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZThn8_N3sce2np9HttpTransD0Ev() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZThn8_N3sce2np9HttpTransD1Ev() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZThn8_N3sce2np9LocalFile4ReadEPNS0_6HandleEPvmPm() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZThn8_N3sce2np9LocalFileD0Ev() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _sceNpAllocatorFree() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _sceNpAllocatorRealloc() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _sceNpAllocatorStrdup() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _sceNpAllocatorStrndup() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _sceNpFree() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _sceNpHeapFree() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _sceNpHeapRealloc() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _sceNpHeapStrdup() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _sceNpHeapStrndup() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _sceNpMalloc() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _sceNpMsgPackStrUtilIsValidUtf8StringWithSize() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _sceNpMsgPackStrUtilIsValidUtf8String() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _sceNpRealloc() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceNpJsonParse2ForNpExt() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceNpJsonParse2ForNpExtInit() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceNpJwsHs256Decode() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceNpJwsHs256Encode() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceNpJwsHs256GetDecodeSize() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceNpJwsHs256GetEncodeSize() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceNpUtilNpIdToJid() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceNpUtilNumChars() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceNpUtilSerializeJid() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceNpUtilWebApiCheckResponseHeaderAndShowNotification() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceNpUtilWebApiEnableNpDebug() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceNpUtilXmlGetOneChar() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceNpXmlParseInit() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI libSceNpCommonUnknown_5FuxkbSbLtk_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("5FuxkbSbLtk", libSceNpCommonUnknown_5FuxkbSbLtk_nid_no_patch);

int APS5_VABI libSceNpCommonUnknown_IMWAoxm9nAw_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("IMWAoxm9nAw", libSceNpCommonUnknown_IMWAoxm9nAw_nid_no_patch);

int APS5_VABI libSceNpCommonUnknown_NOTv_minus_472yf4_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("NOTv-472yf4", libSceNpCommonUnknown_NOTv_minus_472yf4_nid_no_patch);

int APS5_VABI libSceNpCommonUnknown_NnxHmyZODbk_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("NnxHmyZODbk", libSceNpCommonUnknown_NnxHmyZODbk_nid_no_patch);

int APS5_VABI libSceNpCommonUnknown_WF2l_minus_GUIlrw_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("WF2l-GUIlrw", libSceNpCommonUnknown_WF2l_minus_GUIlrw_nid_no_patch);

int APS5_VABI libSceNpCommonUnknown_WaBg_plus_f4DGk8_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("WaBg+f4DGk8", libSceNpCommonUnknown_WaBg_plus_f4DGk8_nid_no_patch);

int APS5_VABI libSceNpCommonUnknown_qlYEhYUyESQ_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("qlYEhYUyESQ", libSceNpCommonUnknown_qlYEhYUyESQ_nid_no_patch);

int APS5_VABI libSceNpCommonUnknown_wnXVM2Zy49s_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("wnXVM2Zy49s", libSceNpCommonUnknown_wnXVM2Zy49s_nid_no_patch);

int APS5_VABI libSceNpCommonUnknown_xJecuUi348c_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("xJecuUi348c", libSceNpCommonUnknown_xJecuUi348c_nid_no_patch);

}
