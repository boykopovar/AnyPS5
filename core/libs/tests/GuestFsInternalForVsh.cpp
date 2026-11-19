#include "prx/libSceFsInternalForVsh/libSceFsInternalForVsh.hpp"
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdlib>

namespace {

void Require(bool value) {
    if (!value) {
        std::abort();
    }
}

void CheckWorkspaceOptInit(decltype(&sceFsInitCreateWorkspaceOpt) init) {
    for (int call = 0; call < 2; ++call) {
        alignas(8) std::uint8_t opt[16];
        for (auto& byte : opt) {
            byte = 0xa5;
        }
        Require(init(opt) == 0);
        for (int i = 0; i < 4; ++i) {
            Require(opt[i] == 0);
        }
        for (int i = 4; i < 16; ++i) {
            Require(opt[i] == 0xa5);
        }
    }
}

struct OptStore {
    std::uint32_t offset;
    std::uint32_t size;
    std::uint64_t value;
};

void CheckOptInit(decltype(&sceFsInitCreatePfsSaveDataOpt) init, const OptStore* stores, std::size_t count) {
    for (int call = 0; call < 2; ++call) {
        alignas(8) std::uint8_t opt[0x100];
        std::uint8_t want[0x100];
        for (std::size_t i = 0; i < sizeof(opt); ++i) {
            opt[i] = 0xa5;
            want[i] = 0xa5;
        }
        for (std::size_t s = 0; s < count; ++s) {
            for (std::uint32_t i = 0; i < stores[s].size; ++i) {
                want[stores[s].offset + i] = i < 8 ? static_cast<std::uint8_t>(stores[s].value >> (8 * i)) : 0;
            }
        }
        Require(init(opt) == 0);
        for (std::size_t i = 0; i < sizeof(opt); ++i) {
            Require(opt[i] == want[i]);
        }
        Require(init(nullptr) == static_cast<int>(0x80020016u));
    }
}

void CheckTrophyImageError() {
    const std::uint32_t accepted[] = {0x80020005u, 0x80020016u, 0x8002002du, 0x80020060u};
    for (std::uint32_t error : accepted) {
        Require(sceFsTrophyImageError(static_cast<int>(error)) == 0);
    }
    const std::uint32_t rejected[] = {0u,          1u,          5u,          0x16u,       0x80020004u, 0x80020006u,
                                      0x80020015u, 0x80020017u, 0x8002002cu, 0x8002002eu, 0x8002005fu, 0x80020061u,
                                      0x80000000u, 0x7fffffffu, 0xffffffffu};
    for (std::uint32_t error : rejected) {
        Require(sceFsTrophyImageError(static_cast<int>(error)) == static_cast<int>(0x80020016u));
    }
}

}

int main() {
    Require(sceFsErriCtl() == static_cast<int>(0x8002002d));
    Require(sceFsErriGetState() == static_cast<int>(0x8002002d));
    CheckWorkspaceOptInit(sceFsInitCreateWorkspaceOpt);
    CheckWorkspaceOptInit(sceFsInitDeleteWorkspaceOpt);
    CheckWorkspaceOptInit(sceFsInitUmountWorkspaceOpt);
    const decltype(&sceFsInitCreateWorkspaceOpt) inits[] = {
        sceFsInitCreateWorkspaceOpt,
        sceFsInitDeleteWorkspaceOpt,
        sceFsInitUmountWorkspaceOpt,
    };
    for (auto init : inits) {
        errno = 71;
        Require(init(nullptr) == static_cast<int>(0x80020016u));
        Require(errno == 71);
        Require(init(reinterpret_cast<void*>(std::uintptr_t{0x10})) == static_cast<int>(0x80020016u));
        Require(errno == 71);
    }
    for (int call = 0; call < 2; ++call) {
        Require(libSceFsInternalForVshUnknown_0XlyVxRNeIw_nid_no_patch() == 0);
        Require(libSceFsInternalForVshUnknown_W8CmA_plus_wZ9_plus_w_nid_no_patch() == 0);
        Require(libSceFsInternalForVshUnknown_lxyJjALNmp4_nid_no_patch() == 0);
    }
    CheckTrophyImageError();
    {
        const OptStore stores[] = {{0x0, 4, 0x8000}, {0x4, 2, 0x0}};
        CheckOptInit(sceFsInitCreatePfsSaveDataOpt, stores, sizeof(stores) / sizeof(stores[0]));
    }
    {
        const OptStore stores[] = {{0x0, 4, 0x8000}, {0x4, 2, 0x0}};
        CheckOptInit(sceFsInitCreatePfsTrophyDataOpt, stores, sizeof(stores) / sizeof(stores[0]));
    }
    {
        const OptStore stores[] = {{0x44, 4, 0x0}, {0x48, 8, 0x0}, {0x0, 32, 0x0}, {0x20, 4, 0x0}, {0x28, 16, 0x0}, {0x31, 16, 0x0}};
        CheckOptInit(sceFsInitMountGamePkgOpt, stores, sizeof(stores) / sizeof(stores[0]));
    }
    CheckOptInit(sceFsInitUmountDownloadDataOpt, nullptr, 0);
    {
        const OptStore stores[] = {{0x0, 4, 0x1}};
        CheckOptInit(sceFsInitUmountGamePkgOpt, stores, sizeof(stores) / sizeof(stores[0]));
    }
    CheckOptInit(sceFsInitUmountLwfsCompatOpt, nullptr, 0);
    CheckOptInit(sceFsInitUmountLwfsOpt, nullptr, 0);
    {
        const OptStore stores[] = {{0x0, 8, 0x0}, {0x8, 8, 0xffffffff00000000}, {0x10, 4, 0xffffffff}, {0x18, 8, 0x0}};
        CheckOptInit(sceFsInitUmountPprPkgOpt, stores, sizeof(stores) / sizeof(stores[0]));
    }
    CheckOptInit(sceFsInitUmountSaveDataOpt, nullptr, 0);
    CheckOptInit(sceFsInitUmountTrophyDataOpt, nullptr, 0);
    {
        const OptStore stores[] = {{0x0, 4, 0x10000}, {0x4, 2, 0x0}};
        CheckOptInit(sceFsInitCreatePprPfsSaveDataOpt, stores, sizeof(stores) / sizeof(stores[0]));
    }
    CheckOptInit(libSceFsInternalForVshUnknown_6Bku_plus_Im1Pew_nid_no_patch, nullptr, 0);
    CheckOptInit(libSceFsInternalForVshUnknown_GNeCtmqY_minus_58_nid_no_patch, nullptr, 0);
    CheckOptInit(libSceFsInternalForVshUnknown_KrZEB_plus_TsZ5k_nid_no_patch, nullptr, 0);
    {
        const OptStore stores[] = {{0x0, 4, 0x10000}, {0x4, 2, 0x0}};
        CheckOptInit(libSceFsInternalForVshUnknown_LrpsS5hGlyY_nid_no_patch, stores, sizeof(stores) / sizeof(stores[0]));
    }
    CheckOptInit(libSceFsInternalForVshUnknown_MQs7RB_minus_KfYM_nid_no_patch, nullptr, 0);
    {
        const OptStore stores[] = {{0x0, 4, 0x10000}, {0x4, 2, 0x0}};
        CheckOptInit(libSceFsInternalForVshUnknown_T3LiDkpiS7s_nid_no_patch, stores, sizeof(stores) / sizeof(stores[0]));
    }
    {
        const OptStore stores[] = {{0x0, 4, 0x10000}, {0x4, 2, 0x0}};
        CheckOptInit(libSceFsInternalForVshUnknown_cUyUz68FKBc_nid_no_patch, stores, sizeof(stores) / sizeof(stores[0]));
    }
    CheckOptInit(libSceFsInternalForVshUnknown_k_minus_WaWvM74jI_nid_no_patch, nullptr, 0);
    return 0;
}
