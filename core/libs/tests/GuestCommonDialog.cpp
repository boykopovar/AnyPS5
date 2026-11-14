#include "prx/libSceCommonDialog/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceCommonDialog/CommonDialog.hpp"
#include "prx/libSceCommonDialog/include/DialogUse.hpp"
#include "prx/libSceCommonDialog/include/Exports.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace {

void Require(bool value) {
    if (!value) std::abort();
}

constexpr int kErrAlreadySystemInitialized = static_cast<int>(0x80B80002);
constexpr int kResult80B8000E = static_cast<int>(0x80B8000Eu);

class GuardedBefore {
public:
    unsigned char* data = nullptr;
    std::size_t size = 0;
    GuardedBefore() {
#ifdef _WIN32
        SYSTEM_INFO info{};
        GetSystemInfo(&info);
        size = info.dwPageSize;
        data = static_cast<unsigned char*>(VirtualAlloc(nullptr, size * 2, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
        Require(data != nullptr);
        DWORD previous = 0;
        Require(VirtualProtect(data, size, PAGE_NOACCESS, &previous) != 0);
#else
        const auto pageSize = sysconf(_SC_PAGESIZE);
        Require(pageSize > 0);
        size = static_cast<std::size_t>(pageSize);
        auto* mapping = mmap(nullptr, size * 2, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        Require(mapping != MAP_FAILED);
        data = static_cast<unsigned char*>(mapping);
        Require(mprotect(data, size, PROT_NONE) == 0);
#endif
    }
    ~GuardedBefore() {
#ifdef _WIN32
        VirtualFree(data, 0, MEM_RELEASE);
#else
        munmap(data, size * 2);
#endif
    }
    GuardedBefore(const GuardedBefore&) = delete;
    GuardedBefore& operator=(const GuardedBefore&) = delete;
};

static_assert(COMMON_DIALOG_OK == 0);
static_assert(COMMON_DIALOG_STATUS_NONE == 0 && COMMON_DIALOG_STATUS_INITIALIZED == 1 && COMMON_DIALOG_STATUS_RUNNING == 2 && COMMON_DIALOG_STATUS_FINISHED == 3);
static_assert(COMMON_DIALOG_RESULT_OK == 0 && COMMON_DIALOG_RESULT_USER_CANCELED == 1);
static_assert(COMMON_DIALOG_ERROR_NOT_SYSTEM_INITIALIZED == static_cast<int>(0x80B80001u));
static_assert(COMMON_DIALOG_ERROR_ALREADY_SYSTEM_INITIALIZED == static_cast<int>(0x80B80002u));
static_assert(COMMON_DIALOG_ERROR_NOT_INITIALIZED == static_cast<int>(0x80B80003u));
static_assert(COMMON_DIALOG_ERROR_ALREADY_INITIALIZED == static_cast<int>(0x80B80004u));
static_assert(COMMON_DIALOG_ERROR_NOT_FINISHED == static_cast<int>(0x80B80005u));
static_assert(COMMON_DIALOG_ERROR_INVALID_STATE == static_cast<int>(0x80B80006u));
static_assert(COMMON_DIALOG_ERROR_INVALID_ARGUMENT == static_cast<int>(0x80B8000Au) && COMMON_DIALOG_ERROR_PARAMETER == static_cast<int>(0x80B8000Au));
static_assert(COMMON_DIALOG_ERROR_NOT_RUNNING == static_cast<int>(0x80B8000Bu));
static_assert(COMMON_DIALOG_ERROR_ARG_NULL == static_cast<int>(0x80B8000Du));

}

int main() {
    Require(!sceCommonDialogIsUsed());
    Require(!CommonDialogSystemInitialized_nid_no_patch());
    Require(sceCommonDialogInitialize() == COMMON_DIALOG_OK);
    Require(CommonDialogSystemInitialized_nid_no_patch());
    Require(!sceCommonDialogIsUsed());
    Require(sceCommonDialogInitialize() == kErrAlreadySystemInitialized);
    Require(sceCommonDialogInitialize() == kErrAlreadySystemInitialized);
    Require(!sceCommonDialogIsUsed());
    Require(CommonDialogAcquire_nid_no_patch() && sceCommonDialogIsUsed());
    Require(!CommonDialogAcquire_nid_no_patch() && sceCommonDialogIsUsed());
    CommonDialogRelease_nid_no_patch();
    Require(!sceCommonDialogIsUsed());
    Require(CommonDialogAcquire_nid_no_patch());
    CommonDialogRelease_nid_no_patch();

    CommonDialogBaseParam base{};
    base.size = sizeof(base);
    base.magic = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(&base)) - 1060003575u;
    Require(ValidateCommonDialogBaseParam(base));
    base.size = 0;
    Require(!ValidateCommonDialogBaseParam(base));
    base.size = sizeof(base) - 1;
    Require(!ValidateCommonDialogBaseParam(base));
    base.size = sizeof(base) + 1;
    Require(!ValidateCommonDialogBaseParam(base));
    base.size = sizeof(base);
    base.magic = 0;
    Require(!ValidateCommonDialogBaseParam(base));
    base.magic = (static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(&base)) - 1060003575u) ^ 1u;
    Require(!ValidateCommonDialogBaseParam(base));
    base.magic = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(&base)) - 1060003575u;
    base.reserved[0] = 1;
    Require(!ValidateCommonDialogBaseParam(base));
    base.reserved[0] = 0;
    base.reserved[1] = 0x80;
    Require(!ValidateCommonDialogBaseParam(base));
    base.reserved[1] = 0;
    base.reserved[35] = 1;
    Require(!ValidateCommonDialogBaseParam(base));
    base.reserved[35] = 0;
    Require(ValidateCommonDialogBaseParam(base));

    Require(libSceCommonDialogUnknown_afLdI6i0lQw_nid_no_patch(&base) == COMMON_DIALOG_OK);
    base.size = sizeof(base) + 1;
    Require(libSceCommonDialogUnknown_afLdI6i0lQw_nid_no_patch(&base) == COMMON_DIALOG_ERROR_INVALID_ARGUMENT);
    base.size = sizeof(base);
    base.magic ^= 1u;
    Require(libSceCommonDialogUnknown_afLdI6i0lQw_nid_no_patch(&base) == COMMON_DIALOG_ERROR_INVALID_ARGUMENT);
    base.magic ^= 1u;
    base.reserved[20] = 1;
    Require(libSceCommonDialogUnknown_afLdI6i0lQw_nid_no_patch(&base) == COMMON_DIALOG_ERROR_INVALID_ARGUMENT);
    base.reserved[20] = 0;
    Require(libSceCommonDialogUnknown_afLdI6i0lQw_nid_no_patch(&base) == COMMON_DIALOG_OK);

    GuardedBefore guarded;
    unsigned char* straddling = guarded.data + guarded.size - 0x10;
    const std::uint32_t wrongMagic = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(straddling)) - 1060003575u + 1u;
    std::memcpy(straddling + offsetof(CommonDialogBaseParam, magic), &wrongMagic, sizeof(wrongMagic));
    Require(libSceCommonDialogUnknown_afLdI6i0lQw_nid_no_patch(straddling) == COMMON_DIALOG_ERROR_INVALID_ARGUMENT);

    Require(libSceCommonDialogUnknown_D_minus_V35OhFeIM_nid_no_patch() == 0);
    Require(libSceCommonDialogUnknown_p9TTq4bLdFU_nid_no_patch() == 0);

    std::array<std::uint64_t, 6> client{};
    client[0] = 0x1122334455667788u;
    client[2] = 0x99u;
    Require(_ZNK3sce16CommonDialogUtil6Client8getAppIdEv(client.data()) == -1);
    Require(_ZNK3sce16CommonDialogUtil6Client14getClientStateEv(client.data()) == -1);
    Require(_ZN3sce16CommonDialogUtil6Client15launchCmnDialogEv(client.data()) == -1);
    Require(_ZN3sce16CommonDialogUtil6Client11updateStateEv(client.data()) == kResult80B8000E);
    std::array<unsigned char, 16> finish{};
    Require(_ZNK3sce16CommonDialogUtil6Client13getFinishDataEPvm(client.data(), finish.data(), finish.size()) == kResult80B8000E);
    Require(_ZN3sce16CommonDialogUtil6Client11closeModuleEv(client.data()) == kResult80B8000E);
    Require(_ZNK3sce16CommonDialogUtil6Client9getResultEv(client.data()) == kResult80B8000E);
    Require(_ZNK3sce16CommonDialogUtil6Client19isInitializedStatusEv(client.data()) == 0);
    Require(_ZNK3sce16CommonDialogUtil6Client8isFinishEv(client.data()) == 0);
    Require(_ZNK3sce16CommonDialogUtil6Client10isCloseReqEv(client.data()) == 0);
    Require(libSceCommonDialogUnknown_Zaa6fhiKQDE_nid_no_patch(client.data()) == kResult80B8000E);
    Require(libSceCommonDialogUnknown_SDpCfY9uB0g_nid_no_patch(client.data()) == kResult80B8000E);
    Require(libSceCommonDialogUnknown_mVRnPerBcK0_nid_no_patch(client.data()) == kResult80B8000E);
    Require(libSceCommonDialogUnknown_yxjgDvqUbGQ_nid_no_patch(client.data()) == kResult80B8000E);
    Require(libSceCommonDialogUnknown_aUS4PgJye98_nid_no_patch(client.data()) == 0);
    Require(client[0] == 0x1122334455667788u && client[1] == 0 && client[2] == 0x99u);
    Require(finish == std::array<unsigned char, 16>{});
}
