#include "prx/libSceFsInternalForVsh/libSceFsInternalForVsh.hpp"
#include <cerrno>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <exception>

namespace {

void Require(bool value) {
    if (!value) {
        std::abort();
    }
}

template <typename Call>
bool Throws(Call call) {
    try {
        call();
    } catch (const std::exception&) {
        return true;
    }
    return false;
}

constexpr int InvalidArgument = static_cast<int>(0x80020016u);

template <typename T>
T Field(const std::uint8_t* opt, std::size_t offset) {
    T value{};
    std::memcpy(&value, opt + offset, sizeof(value));
    return value;
}

template <typename T>
void SetField(std::uint8_t* opt, std::size_t offset, T value) {
    std::memcpy(opt + offset, &value, sizeof(value));
}

void SetText(std::uint8_t* opt, std::size_t offset, const char* text) {
    SetField(opt, offset, reinterpret_cast<std::uint64_t>(text));
}

void CheckInitMountPprPkgOpt() {
    alignas(8) std::uint8_t opt[0x90];
    std::memset(opt, 0xa5, sizeof(opt));
    Require(sceFsInitMountPprPkgOpt(opt) == 0);
    for (std::size_t i = 0; i < 0x58; ++i) Require(opt[i] == 0);
    Require(Field<std::uint64_t>(opt, 0x58) == 0x30);
    Require(Field<std::uint64_t>(opt, 0x60) == 0);
    Require(Field<std::uint64_t>(opt, 0x68) == ~std::uint64_t{0});
    Require(Field<std::uint64_t>(opt, 0x70) == 0);
    Require(Field<std::uint32_t>(opt, 0x78) == 0x10000);
    for (std::size_t i = 0x7c; i < 0x80; ++i) Require(opt[i] == 0xa5);
    for (std::size_t i = 0x80; i < 0x90; ++i) Require(opt[i] == 0);
    Require(sceFsInitMountPprPkgOpt(nullptr) == InvalidArgument);
    Require(Throws([] { sceFsInitMountPprPkgOpt(reinterpret_cast<void*>(std::uintptr_t{0x10})); }));
}

void CheckMountPprPkg() {
    alignas(8) std::uint8_t opt[0x90];
    Require(sceFsInitMountPprPkgOpt(opt) == 0);
    int first = 7;
    int second = 7;
    Require(sceFsMountPprPkg(nullptr, &first, &second) == InvalidArgument && first == -1 && second == -1);
    first = 7;
    Require(sceFsMountPprPkg(opt, &first, nullptr) == InvalidArgument && first == -1);
    second = 7;
    Require(sceFsMountPprPkg(opt, nullptr, &second) == InvalidArgument && second == -1);
    Require(Throws([&] { sceFsMountPprPkg(opt, reinterpret_cast<int*>(std::uintptr_t{0x10}), &second); }));
    Require(Throws([&] { sceFsMountPprPkg(reinterpret_cast<const void*>(std::uintptr_t{0x10}), &first, &second); }));

    const std::uint32_t skipped[] = {0, 0x100, 0x107, 0x108, 0x10b, 0x10e, 0x201, 0xffffffffu};
    for (std::uint32_t type : skipped) {
        SetField(opt, 0x38, type);
        first = 7;
        second = 7;
        Require(sceFsMountPprPkg(opt, &first, &second) == 0x16 && first == -1 && second == -1);
    }

    const char image[] = "/data/app.pkg";
    const char mountPath[] = "/mnt/ppr";
    SetField(opt, 0x38, std::uint32_t{0x102});
    SetText(opt, 0x0, image);
    Require(sceFsMountPprPkg(opt, &first, &second) == InvalidArgument && first == -1 && second == -1);
    SetText(opt, 0x0, nullptr);
    SetText(opt, 0x80, mountPath);
    Require(sceFsMountPprPkg(opt, &first, &second) == InvalidArgument);
    SetText(opt, 0x0, image);
    Require(Throws([&] { sceFsMountPprPkg(opt, &first, &second); }));

    SetField(opt, 0x38, std::uint32_t{0x101});
    SetText(opt, 0x80, nullptr);
    Require(Throws([&] { sceFsMountPprPkg(opt, &first, &second); }));
    const char budgets[][8] = {"game", "bigapp", "system"};
    for (const char* budget : budgets) {
        SetText(opt, 0x10, budget);
        Require(sceFsMountPprPkg(opt, &first, &second) == InvalidArgument);
    }
}

int MountGamePkg(const std::uint8_t* opt, const char* mountPath) {
    return sceFsMountGamePkg(opt, mountPath, nullptr, nullptr, 0, 0);
}

void CheckGamePkgCache(std::uint8_t* opt, std::uint8_t flags, const char* budget, std::uint64_t ddr3, std::uint32_t gddr5, bool accepted) {
    const char mountPath[] = "/mnt/app";
    SetField(opt, 0x40, flags);
    SetText(opt, 0x48, budget);
    SetField(opt, 0x18, ddr3);
    SetField(opt, 0x20, gddr5);
    if (accepted) {
        Require(Throws([&] { MountGamePkg(opt, mountPath); }));
    } else {
        Require(MountGamePkg(opt, mountPath) == InvalidArgument);
    }
}

void CheckMountGamePkg() {
    alignas(8) std::uint8_t opt[0x50] = {};
    const char image[] = "/data/app.pkg";
    const char mountPath[] = "/mnt/app";
    Require(MountGamePkg(nullptr, mountPath) == InvalidArgument);
    Require(MountGamePkg(opt, nullptr) == InvalidArgument);
    Require(MountGamePkg(opt, mountPath) == InvalidArgument);
    Require(Throws([&] { MountGamePkg(reinterpret_cast<const std::uint8_t*>(std::uintptr_t{0x10}), mountPath); }));
    SetText(opt, 0x0, image);

    CheckGamePkgCache(opt, 0x00, nullptr, 0, 0, true);
    CheckGamePkgCache(opt, 0x00, nullptr, 1, 0, false);
    CheckGamePkgCache(opt, 0x00, nullptr, 0, 1, false);
    CheckGamePkgCache(opt, 0x00, "bigapp", 0, 0, true);
    CheckGamePkgCache(opt, 0x02, "game", 0, 0, true);
    CheckGamePkgCache(opt, 0x02, "game", 1, 0, false);
    CheckGamePkgCache(opt, 0x82, "game", 0, 1, false);

    CheckGamePkgCache(opt, 0x00, "game", 1, 0, true);
    CheckGamePkgCache(opt, 0x01, "game", 1, 0, true);
    CheckGamePkgCache(opt, 0x00, "game", 0, 0, false);
    CheckGamePkgCache(opt, 0x00, "game", 1, 1, false);

    CheckGamePkgCache(opt, 0x80, "game", 0, 1, true);
    CheckGamePkgCache(opt, 0xe1, "game", 0, 1, true);
    CheckGamePkgCache(opt, 0x80, "game", 1, 1, false);
    CheckGamePkgCache(opt, 0x80, "game", 0, 0, false);
}

void CheckUmountGamePkg() {
    const std::uint32_t opt = 0;
    const char path[] = "/mnt/app";
    Require(sceFsUmountGamePkg(nullptr, path, 0, 0) == InvalidArgument);
    Require(sceFsUmountGamePkg(&opt, nullptr, 0, 0) == InvalidArgument);
    Require(Throws([&] { sceFsUmountGamePkg(&opt, path, 0, 1); }));
}

void CheckUmountPprPkg() {
    alignas(8) std::uint8_t opt[0x20] = {};
    Require(sceFsUmountPprPkg(nullptr) == InvalidArgument);
    Require(sceFsUmountPprPkg(opt) == InvalidArgument);
    const char path[] = "/mnt/ppr";
    SetText(opt, 0x18, path);
    Require(sceFsUmountPprPkg(opt) == InvalidArgument);
    SetText(opt, 0x0, path);
    Require(Throws([&] { sceFsUmountPprPkg(opt); }));
    Require(Throws([] { sceFsUmountPprPkg(reinterpret_cast<const void*>(std::uintptr_t{0x10})); }));
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
    CheckInitMountPprPkgOpt();
    CheckMountPprPkg();
    CheckUmountPprPkg();
    CheckMountGamePkg();
    CheckUmountGamePkg();
    return 0;
}
