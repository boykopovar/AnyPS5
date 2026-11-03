#include "prx/libkernel_sys/include/Api.hpp"
#include "prx/libkernel/include/Api.hpp"
#include "SceTypes.hpp"
#include "prx/libkernel/KernelErrors.hpp"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <set>
#include <string>
#include <vector>

static void Require(bool value) { if (!value) std::abort(); }

static std::vector<KernelModule> List(int (APS5_VABI* list)(KernelModule*, std::size_t, std::size_t*)) {
    std::vector<KernelModule> handles(256, -1);
    std::size_t count = 0;
    Require(list(handles.data(), handles.size(), &count) == 0);
    Require(count > 0 && count < handles.size());
    handles.resize(count);
    return handles;
}

static void CheckList() {
    const auto internal = List(sceKernelGetModuleListInternal);
    Require(internal == List(sceKernelGetModuleList));
    std::vector<KernelModule> shorter(internal.size(), -1);
    std::size_t untouched = 99;
    Require(sceKernelGetModuleListInternal(shorter.data(), internal.size() - 1, &untouched) == SCE_KERNEL_ERROR_ENOMEM);
    Require(untouched == 99 && shorter.back() == -1);
    Require(sceKernelGetModuleListInternal(nullptr, 0, &untouched) == SCE_KERNEL_ERROR_EFAULT);
    std::fill(shorter.begin(), shorter.end(), -1);
    Require(sceKernelGetModuleListInternal(shorter.data(), shorter.size(), nullptr) == SCE_KERNEL_ERROR_EFAULT);
    Require(shorter == internal);
}

static void CheckEachModule() {
    for (const auto handle : List(sceKernelGetModuleListInternal)) {
        ModuleInfoEx extended{};
        extended.st_size = sizeof(ModuleInfoEx);
        Require(sceKernelGetModuleInfoInternal(handle, &extended) == 0);
        ModuleInfo basic{};
        basic.st_size = sizeof(ModuleInfo);
        Require(sceKernelGetModuleInfo(handle, &basic) == 0);
        Require(extended.st_size == sizeof(ModuleInfoEx) && extended.id == handle);
        Require(std::string(extended.name) == basic.name && extended.segment_count == basic.segment_count);
        for (std::uint32_t i = 0; i < basic.segment_count; ++i)
            Require(extended.segments[i].address == basic.segments[i].address && extended.segments[i].size == basic.segments[i].size &&
                    extended.segments[i].prot == basic.segments[i].prot);
    }
}

static void CheckMatchesAddressLookup() {
    ModuleInfoEx byAddress{};
    byAddress.st_size = sizeof(ModuleInfoEx);
    Require(sceKernelGetModuleInfoFromAddr(reinterpret_cast<std::uintptr_t>(&sceKernelGetModuleInfo), 1, &byAddress) == 0);
    ModuleInfoEx byHandle{};
    byHandle.st_size = sizeof(ModuleInfoEx);
    Require(sceKernelGetModuleInfoInternal(byAddress.id, &byHandle) == 0);
    Require(byAddress.st_size == 0 && byHandle.st_size == sizeof(ModuleInfoEx));
    Require(std::memcmp(byHandle.name, byAddress.name, sizeof(ModuleInfoEx) - offsetof(ModuleInfoEx, name)) == 0);
}

static void CheckRefusals() {
    const auto handle = List(sceKernelGetModuleListInternal).front();
    for (const std::uint64_t size : {0x160, 0x1a4, 0x1b0, 0}) {
        ModuleInfoEx sized{};
        sized.st_size = size;
        sized.name[0] = 'x';
        Require(sceKernelGetModuleInfoInternal(handle, &sized) == SCE_KERNEL_ERROR_EINVAL);
        Require(sized.st_size == size && sized.name[0] == 'x');
    }
    Require(sceKernelGetModuleInfoInternal(handle, nullptr) == SCE_KERNEL_ERROR_EFAULT);
    ModuleInfoEx unknown{};
    unknown.st_size = sizeof(ModuleInfoEx);
    Require(sceKernelGetModuleInfoInternal(-1, &unknown) == SCE_KERNEL_ERROR_ESRCH && unknown.name[0] == '\0');
}

static void CheckLoadStartSysmodule() {
    int res = -1;
    Require(sceKernelLoadStartModuleForSysmodule(nullptr, 0, nullptr, 0, nullptr, &res) == static_cast<KernelModule>(SCE_KERNEL_ERROR_EFAULT));
    Require(res == 0);
    Require(sceKernelLoadStartModuleForSysmodule(nullptr, 0, nullptr, 0, nullptr, nullptr) == static_cast<KernelModule>(SCE_KERNEL_ERROR_EFAULT));
    res = -1;
    Require(sceKernelLoadStartModuleForSysmodule("/system/common/lib/nonexistent_sysmodule.sprx", 0, nullptr, 0, nullptr, &res) ==
            static_cast<KernelModule>(SCE_KERNEL_ERROR_ENOENT));
    Require(res == 0);
}

static void CheckIsModuleLoaded() {
    std::set<std::string> names;
    for (const auto handle : List(sceKernelGetModuleListInternal)) {
        ModuleInfo info{};
        info.st_size = sizeof(ModuleInfo);
        Require(sceKernelGetModuleInfo(handle, &info) == 0);
        names.insert(info.name);
    }
    const auto expected = [&](const std::string& name) { return names.count(name) ? 0 : SCE_KERNEL_ERROR_ESRCH; };
    for (const auto& name : names) {
        Require(sceKernelIsModuleLoaded(name.c_str()) == 0);
        Require(sceKernelIsModuleLoaded((name + "x").c_str()) == expected(name + "x"));
        Require(sceKernelIsModuleLoaded(name.substr(0, name.size() - 1).c_str()) == expected(name.substr(0, name.size() - 1)));
    }
    Require(sceKernelIsModuleLoaded("libSceNoSuchModule.sprx") == SCE_KERNEL_ERROR_ESRCH);
    Require(sceKernelIsModuleLoaded("") == expected(""));
}

int main() {
    CheckList();
    CheckEachModule();
    CheckMatchesAddressLookup();
    CheckRefusals();
    CheckLoadStartSysmodule();
    CheckIsModuleLoaded();
    return 0;
}
