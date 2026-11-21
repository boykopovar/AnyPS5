#include "prx/libSceBemp2sys/libSceBemp2sys.hpp"
#include <cstddef>
#include <cstdint>
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
    constexpr int failError = static_cast<int>(0x81004001u);
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

    Require(bemp2sys_packetizer_create_nid_postfix(nullptr, 0x10000001u) == invalidArgument);
    Require(bemp2sys_pesparser_create_nid_postfix(nullptr) == invalidArgument);
    Require(bemp2sys_psdecoder_create_nid_postfix(nullptr) == invalidArgument);
    Require(bemp2sys_tsdecoder_create_nid_postfix(nullptr) == invalidArgument);
    Require(bemp2sys_tsfilter_create_nid_postfix(nullptr) == invalidArgument);

    void* const sentinel = reinterpret_cast<void*>(static_cast<std::uintptr_t>(0xA5A5A5A5A5A5A5A5ull));
    const std::uint32_t invalidTypes[] = {
        0u,
        0x10000000u,
        0x10000003u,
        0x10000005u,
        0x10000021u,
        0x10000040u,
        0xFFFFFFFFu,
    };
    for (const std::uint32_t invalidType : invalidTypes) {
        void* handle = sentinel;
        Require(bemp2sys_packetizer_create_nid_postfix(&handle, invalidType) == failError);
        Require(handle == nullptr);
    }

    const std::uint32_t validTypes[] = {
        0x10000001u,
        0x10000002u,
        0x10000004u,
        0x10000008u,
        0x10000010u,
        0x10000020u,
    };
    for (const std::uint32_t validType : validTypes) {
        void* handle = sentinel;
        Require(bemp2sys_packetizer_create_nid_postfix(&handle, validType) == 0);
        Require(handle != nullptr && handle != sentinel);
        bemp2sys_packetizer_destroy_nid_postfix(handle);
    }

    void* psdecoder = sentinel;
    Require(bemp2sys_psdecoder_create_nid_postfix(&psdecoder) == 0);
    Require(psdecoder != nullptr && psdecoder != sentinel);
    bemp2sys_psdecoder_release_nid_postfix();
    bemp2sys_psdecoder_destroy_nid_postfix(psdecoder);

    void* tsdecoder = sentinel;
    Require(bemp2sys_tsdecoder_create_nid_postfix(&tsdecoder) == 0);
    Require(tsdecoder != nullptr && tsdecoder != sentinel);
    bemp2sys_tsdecoder_release_nid_postfix();
    bemp2sys_tsdecoder_destroy_nid_postfix(tsdecoder);

    void* pesparser = sentinel;
    Require(bemp2sys_pesparser_create_nid_postfix(&pesparser) == 0);
    Require(pesparser != nullptr && pesparser != sentinel);
    Require(bemp2sys_pesparser_init_nid_postfix(pesparser, &data) == 0);
    Require(bemp2sys_pesparser_init_nid_postfix(pesparser, &data) == 0);
    bemp2sys_pesparser_destroy_nid_postfix(pesparser);

    void* tsfilter = sentinel;
    Require(bemp2sys_tsfilter_create_nid_postfix(&tsfilter) == 0);
    Require(tsfilter != nullptr && tsfilter != sentinel);
    void* nullFirstField = nullptr;
    Require(bemp2sys_tsfilter_init_nid_postfix(tsfilter, &nullFirstField) == failError);
    bemp2sys_tsfilter_destroy_nid_postfix(tsfilter);

    bemp2sys_packetizer_destroy_nid_postfix(nullptr);
    bemp2sys_pesparser_destroy_nid_postfix(nullptr);
    bemp2sys_psdecoder_destroy_nid_postfix(nullptr);
    bemp2sys_tsdecoder_destroy_nid_postfix(nullptr);
    bemp2sys_tsfilter_destroy_nid_postfix(nullptr);
    bemp2sys_packetizer_release_nid_postfix(nullptr);
    bemp2sys_pesparser_release_nid_postfix(nullptr);
    bemp2sys_tsfilter_release_nid_postfix(nullptr);

    return 0;
}
