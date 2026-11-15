#include "prx/libSceBemp2sys/libSceBemp2sys.hpp"
#include <cstddef>
#include <cstdlib>

static void Require(bool value) { if (!value) std::abort(); }

namespace {

void LogFunction(int, const char*) {}
void* Allocate(std::size_t) { return nullptr; }
void Deallocate(void*) {}
void* Reallocate(void*, std::size_t) { return nullptr; }

}

int main() {
    Require(bemp2sys_logprintf_getlevel_nid_postfix() == 0);
    for (int level = 0; level <= 3; ++level) {
        bemp2sys_logprintf_setlevel_nid_postfix(level);
        Require(bemp2sys_logprintf_getlevel_nid_postfix() == level);
    }
    bemp2sys_logprintf_setlevel_nid_postfix(4);
    Require(bemp2sys_logprintf_getlevel_nid_postfix() == 3);
    bemp2sys_logprintf_setlevel_nid_postfix(100);
    Require(bemp2sys_logprintf_getlevel_nid_postfix() == 3);
    bemp2sys_logprintf_setlevel_nid_postfix(-1);
    Require(bemp2sys_logprintf_getlevel_nid_postfix() == 3);
    bemp2sys_logprintf_setlevel_nid_postfix(1);
    Require(bemp2sys_logprintf_getlevel_nid_postfix() == 1);
    bemp2sys_logprintf_setfunc_nid_postfix(reinterpret_cast<void*>(&LogFunction));
    bemp2sys_logprintf_setfunc_nid_postfix(nullptr);
    bemp2sys_memmanager_set_nid_postfix(reinterpret_cast<void*>(&Allocate), reinterpret_cast<void*>(&Deallocate), reinterpret_cast<void*>(&Reallocate));
    bemp2sys_memmanager_set_nid_postfix(nullptr, nullptr, nullptr);
    constexpr int invalidArgument = static_cast<int>(0x81001001u);
    int object = 0;
    int data = 0;
    for (int call = 0; call < 2; ++call) {
        Require(bemp2sys_packetizer_init_nid_postfix(nullptr, &data) == invalidArgument);
        Require(bemp2sys_packetizer_init_nid_postfix(&object, nullptr) == invalidArgument);
        Require(bemp2sys_pesparser_init_nid_postfix(nullptr, &data) == invalidArgument);
        Require(bemp2sys_pesparser_init_nid_postfix(&object, nullptr) == invalidArgument);
        Require(bemp2sys_psdecoder_init_nid_postfix(nullptr, &data) == invalidArgument);
        Require(bemp2sys_psdecoder_init_nid_postfix(&object, nullptr) == invalidArgument);
        Require(bemp2sys_tsdecoder_init_nid_postfix(nullptr, &data) == invalidArgument);
        Require(bemp2sys_tsdecoder_init_nid_postfix(&object, nullptr) == invalidArgument);
        Require(bemp2sys_tsfilter_init_nid_postfix(nullptr, &data) == invalidArgument);
        Require(bemp2sys_tsfilter_init_nid_postfix(&object, nullptr) == invalidArgument);
        Require(bemp2sys_pesparser_parse_nid_postfix(nullptr, &data, &data, 4) == invalidArgument);
        Require(bemp2sys_pesparser_parse_nid_postfix(&object, nullptr, &data, 4) == invalidArgument);
        Require(bemp2sys_pesparser_parse_nid_postfix(&object, &data, nullptr, 4) == invalidArgument);
        Require(bemp2sys_pesparser_sendmessage_nid_postfix(&object, 2, &data) == invalidArgument);
        Require(bemp2sys_pesparser_sendmessage_nid_postfix(&object, 0xFFFFFFFFu, &data) == invalidArgument);
        Require(bemp2sys_pesparser_sendmessage_nid_postfix(nullptr, 0, &data) == invalidArgument);
        Require(bemp2sys_pesparser_sendmessage_nid_postfix(&object, 1, nullptr) == invalidArgument);
        Require(bemp2sys_tsfilter_sendmessage_nid_postfix(&object, 0xE, &data) == invalidArgument);
        Require(bemp2sys_tsfilter_sendmessage_nid_postfix(nullptr, 0, &data) == invalidArgument);
        Require(bemp2sys_tsfilter_sendmessage_nid_postfix(&object, 0xD, nullptr) == invalidArgument);
        Require(bemp2sys_psdecoder_sendmessage_nid_postfix(&object, 0x13, &data) == invalidArgument);
        Require(bemp2sys_psdecoder_sendmessage_nid_postfix(nullptr, 0x12, nullptr) == invalidArgument);
        Require(bemp2sys_tsdecoder_sendmessage_nid_postfix(&object, 0x1C, &data) == invalidArgument);
        Require(bemp2sys_tsdecoder_sendmessage_nid_postfix(nullptr, 0x1B, nullptr) == invalidArgument);
        Require(bemp2sys_psdecoder_readpacket_nid_postfix(nullptr, &data, &data, &data) == invalidArgument);
        Require(bemp2sys_psdecoder_readpacket_nid_postfix(&object, &data, &data, nullptr) == invalidArgument);
        Require(bemp2sys_tsdecoder_readpacket_nid_postfix(nullptr, &data, &data, &data) == invalidArgument);
        Require(bemp2sys_tsdecoder_readpacket_nid_postfix(&object, &data, &data, nullptr) == invalidArgument);
        Require(libSceBemp2sysUnknown_B1nK_plus_wb5rME_nid_no_patch(nullptr) == invalidArgument);
    }
    return 0;
}
