#include "prx/libc/include/general/ExportMacros.hpp"
#include "prx/libSceBemp2sys/include/InventoryExports.hpp"
#include "prx/libSceBemp2sys/include/Api.hpp"
#include <cstddef>
#include <cstdint>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

namespace {

void* LogFunction = nullptr;
int LogLevel = 0;

struct MemoryManager {
    void* allocate = nullptr;
    void* deallocate = nullptr;
    void* reallocate = nullptr;
};
MemoryManager memoryManager;

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

int APS5_VABI bemp2sys_packetizer_create_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI bemp2sys_packetizer_destroy_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI bemp2sys_packetizer_init_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI bemp2sys_packetizer_release_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI bemp2sys_pesparser_create_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI bemp2sys_pesparser_destroy_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI bemp2sys_pesparser_init_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI bemp2sys_pesparser_parse_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI bemp2sys_pesparser_release_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI bemp2sys_pesparser_sendmessage_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI bemp2sys_psdecoder_create_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI bemp2sys_psdecoder_destroy_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI bemp2sys_psdecoder_init_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI bemp2sys_psdecoder_readpacket_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI bemp2sys_psdecoder_release_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI bemp2sys_psdecoder_sendmessage_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI bemp2sys_tsdecoder_create_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI bemp2sys_tsdecoder_destroy_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI bemp2sys_tsdecoder_init_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI bemp2sys_tsdecoder_readpacket_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI bemp2sys_tsdecoder_release_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI bemp2sys_tsdecoder_sendmessage_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI bemp2sys_tsfilter_create_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI bemp2sys_tsfilter_destroy_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI bemp2sys_tsfilter_init_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI bemp2sys_tsfilter_release_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI bemp2sys_tsfilter_sendmessage_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

}

extern "C" {

int APS5_VABI libSceBemp2sysUnknown_1L1WwRMiys8_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("1L1WwRMiys8", libSceBemp2sysUnknown_1L1WwRMiys8_nid_no_patch);

int APS5_VABI libSceBemp2sysUnknown_B1nK_plus_wb5rME_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("B1nK+wb5rME", libSceBemp2sysUnknown_B1nK_plus_wb5rME_nid_no_patch);

}
