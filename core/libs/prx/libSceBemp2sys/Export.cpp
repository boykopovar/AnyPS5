#include "prx/libc/include/general/ExportMacros.hpp"
#include "prx/libSceBemp2sys/include/InventoryExports.hpp"
#include "prx/libSceBemp2sys/include/Api.hpp"
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <new>
#include <utility>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

namespace {

constexpr int SCE_BEMP2SYS_ERROR_INVALID_ARGUMENT = static_cast<int>(0x81001001);
constexpr int SCE_BEMP2SYS_ERROR_FAIL = static_cast<int>(0x81004001);
constexpr std::uint32_t PesParserMessageMax = 1;
constexpr std::uint32_t PsDecoderMessageCount = 0x13;
constexpr std::uint32_t TsDecoderMessageCount = 0x1C;
constexpr std::uint32_t TsFilterMessageMax = 0xD;

void* LogFunction = nullptr;
int LogLevel = 0;

struct MemoryManager {
    void* allocate = nullptr;
    void* deallocate = nullptr;
    void* reallocate = nullptr;
};
MemoryManager memoryManager;

template <typename T>
bool loadGuest(T& value, const void* address) {
    const int savedErrno = errno;
    const bool copied = GuestCopyIn_nid_no_patch(&value, address, sizeof(T));
    errno = savedErrno;
    return copied;
}

template <typename T>
bool storeGuest(void* address, const T& value) {
    const int savedErrno = errno;
    const bool copied = GuestCopyOut_nid_no_patch(address, &value, sizeof(T));
    errno = savedErrno;
    return copied;
}

struct PacketizerObject {
    std::uint32_t type = 0;
};

struct PesParserObject {
    std::uint32_t state0c = 0;
    bool initialized = false;
};

struct PsDecoderObject {};

struct TsDecoderObject {};

struct TsFilterObject {
    const void* firstField = nullptr;
};

bool IsValidPacketizerType(std::uint32_t type) {
    switch (type) {
    case 0x10000001u:
    case 0x10000002u:
    case 0x10000004u:
    case 0x10000008u:
    case 0x10000010u:
    case 0x10000020u:
        return true;
    default:
        return false;
    }
}

template <typename T, typename... Args>
int CreateObject(void** out, Args&&... args) {
    if (out == nullptr) return SCE_BEMP2SYS_ERROR_INVALID_ARGUMENT;
    T* obj = new (std::nothrow) T{std::forward<Args>(args)...};
    void* handle = obj;
    if (!storeGuest(out, handle)) {
        delete obj;
        return SCE_BEMP2SYS_ERROR_INVALID_ARGUMENT;
    }
    return obj != nullptr ? 0 : SCE_BEMP2SYS_ERROR_FAIL;
}

}

extern "C" {

APS5_DUMMY_FUN
int APS5_VABI bemp2sys_logprintf_getlevel_nid_postfix(void) {
    return LogLevel;
}

void APS5_VABI bemp2sys_logprintf_setfunc_nid_postfix(void* function) {
    LogFunction = function;
}

void APS5_VABI bemp2sys_logprintf_setlevel_nid_postfix(int level) {
    if (static_cast<unsigned int>(level) <= 3u) LogLevel = level;
}

void APS5_VABI bemp2sys_memmanager_set_nid_postfix(void* allocate, void* deallocate, void* reallocate) {
    memoryManager = {allocate, deallocate, reallocate};
}

int APS5_VABI bemp2sys_packetizer_create_nid_postfix(void** packetizer, std::uint32_t type) {
    if (packetizer == nullptr) return SCE_BEMP2SYS_ERROR_INVALID_ARGUMENT;
    if (!IsValidPacketizerType(type)) {
        void* nullHandle = nullptr;
        if (!storeGuest(packetizer, nullHandle)) return SCE_BEMP2SYS_ERROR_INVALID_ARGUMENT;
        return SCE_BEMP2SYS_ERROR_FAIL;
    }
    return CreateObject<PacketizerObject>(packetizer, type);
}

void APS5_VABI bemp2sys_packetizer_destroy_nid_postfix(void* packetizer) {
    if (packetizer != nullptr) delete static_cast<PacketizerObject*>(packetizer);
}

int APS5_VABI bemp2sys_packetizer_init_nid_postfix(void* packetizer, const void* param) {
    if (packetizer == nullptr || param == nullptr) return SCE_BEMP2SYS_ERROR_INVALID_ARGUMENT;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

void APS5_VABI bemp2sys_packetizer_release_nid_postfix(void* packetizer) {
    if (packetizer == nullptr) return;
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI bemp2sys_pesparser_create_nid_postfix(void** parser) {
    return CreateObject<PesParserObject>(parser);
}

void APS5_VABI bemp2sys_pesparser_destroy_nid_postfix(void* parser) {
    if (parser != nullptr) delete static_cast<PesParserObject*>(parser);
}

int APS5_VABI bemp2sys_pesparser_init_nid_postfix(void* parser, const void* param) {
    if (parser == nullptr || param == nullptr) return SCE_BEMP2SYS_ERROR_INVALID_ARGUMENT;
    auto* state = static_cast<PesParserObject*>(parser);
    state->state0c = 0;
    state->initialized = true;
    return 0;
}

int APS5_VABI bemp2sys_pesparser_parse_nid_postfix(void* parser, const void* first, const void* second, std::uint32_t) {
    if (parser == nullptr || first == nullptr || second == nullptr) return SCE_BEMP2SYS_ERROR_INVALID_ARGUMENT;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

void APS5_VABI bemp2sys_pesparser_release_nid_postfix(void* parser) {
    if (parser == nullptr) return;
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI bemp2sys_pesparser_sendmessage_nid_postfix(void* parser, std::uint32_t message, void* data) {
    if (message > PesParserMessageMax || parser == nullptr || data == nullptr) return SCE_BEMP2SYS_ERROR_INVALID_ARGUMENT;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI bemp2sys_psdecoder_create_nid_postfix(void** decoder) {
    return CreateObject<PsDecoderObject>(decoder);
}

void APS5_VABI bemp2sys_psdecoder_destroy_nid_postfix(void* decoder) {
    if (decoder != nullptr) delete static_cast<PsDecoderObject*>(decoder);
}

int APS5_VABI bemp2sys_psdecoder_init_nid_postfix(void* decoder, const void* param) {
    if (decoder == nullptr || param == nullptr) return SCE_BEMP2SYS_ERROR_INVALID_ARGUMENT;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI bemp2sys_psdecoder_readpacket_nid_postfix(void* decoder, void*, void*, void* packet) {
    if (packet == nullptr || decoder == nullptr) return SCE_BEMP2SYS_ERROR_INVALID_ARGUMENT;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

void APS5_VABI bemp2sys_psdecoder_release_nid_postfix(void) {}

int APS5_VABI bemp2sys_psdecoder_sendmessage_nid_postfix(void* decoder, std::uint32_t message, void* data) {
    if (data == nullptr || message >= PsDecoderMessageCount) return SCE_BEMP2SYS_ERROR_INVALID_ARGUMENT;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI bemp2sys_tsdecoder_create_nid_postfix(void** decoder) {
    return CreateObject<TsDecoderObject>(decoder);
}

void APS5_VABI bemp2sys_tsdecoder_destroy_nid_postfix(void* decoder) {
    if (decoder != nullptr) delete static_cast<TsDecoderObject*>(decoder);
}

int APS5_VABI bemp2sys_tsdecoder_init_nid_postfix(void* decoder, const void* param) {
    if (decoder == nullptr || param == nullptr) return SCE_BEMP2SYS_ERROR_INVALID_ARGUMENT;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI bemp2sys_tsdecoder_readpacket_nid_postfix(void* decoder, void*, void*, void* packet) {
    if (packet == nullptr || decoder == nullptr) return SCE_BEMP2SYS_ERROR_INVALID_ARGUMENT;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

void APS5_VABI bemp2sys_tsdecoder_release_nid_postfix(void) {}

int APS5_VABI bemp2sys_tsdecoder_sendmessage_nid_postfix(void* decoder, std::uint32_t message, void* data) {
    if (data == nullptr || message >= TsDecoderMessageCount) return SCE_BEMP2SYS_ERROR_INVALID_ARGUMENT;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI bemp2sys_tsfilter_create_nid_postfix(void** filter) {
    return CreateObject<TsFilterObject>(filter);
}

void APS5_VABI bemp2sys_tsfilter_destroy_nid_postfix(void* filter) {
    if (filter != nullptr) delete static_cast<TsFilterObject*>(filter);
}

int APS5_VABI bemp2sys_tsfilter_init_nid_postfix(void* filter, const void* param) {
    if (filter == nullptr || param == nullptr) return SCE_BEMP2SYS_ERROR_INVALID_ARGUMENT;
    const void* firstField = nullptr;
    if (!loadGuest(firstField, param)) return SCE_BEMP2SYS_ERROR_INVALID_ARGUMENT;
    if (firstField == nullptr) return SCE_BEMP2SYS_ERROR_FAIL;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

void APS5_VABI bemp2sys_tsfilter_release_nid_postfix(void* filter) {
    if (filter == nullptr) return;
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI bemp2sys_tsfilter_sendmessage_nid_postfix(void* filter, std::uint32_t message, void* data) {
    if (message > TsFilterMessageMax || filter == nullptr || data == nullptr) return SCE_BEMP2SYS_ERROR_INVALID_ARGUMENT;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

}

extern "C" {

int APS5_VABI libSceBemp2sysUnknown_1L1WwRMiys8_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("1L1WwRMiys8", libSceBemp2sysUnknown_1L1WwRMiys8_nid_no_patch);

int APS5_VABI libSceBemp2sysUnknown_B1nK_plus_wb5rME_nid_no_patch(void* object) {
    if (object == nullptr) return SCE_BEMP2SYS_ERROR_INVALID_ARGUMENT;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("B1nK+wb5rME", libSceBemp2sysUnknown_B1nK_plus_wb5rME_nid_no_patch);

}
