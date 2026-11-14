#include "prx/libkernel_sys/include/AdditionalExports.hpp"
#include "prx/libkernel_sys/include/Api.hpp"
#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/FilesystemError.hpp"
#include "prx/libkernel/include/Api.hpp"

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include "prx/libkernel/Time/include/Time.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libkernel/KernelErrors.hpp"
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstring>

#include <atomic>
#include <cstdint>

namespace {

std::atomic<std::uint8_t> g_flag1WXQlxMpnNA{0};

}

extern "C" {

int APS5_VABI __cfi_slowpath_diag_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI __cfi_slowpath_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI __freeze_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI __sceKernelGetGPI_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI __sys_debug_init_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI __sys_dl_get_info_2_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI __sys_dl_get_info_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI __sys_dl_get_list_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI __sys_dl_get_metadata_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI __sys_dynlib_get_info2_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI __sys_dynlib_get_info_for_libdbg_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI __sys_dynlib_get_list2_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI __sys_dynlib_get_list_for_libdbg_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI __sys_dynlib_load_prx_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI __sys_dynlib_unload_prx_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI __sys_get_proc_type_info_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

std::uint64_t APS5_VABI __sys_is_development_mode_nid_postfix() {
    return 0;
}

int APS5_VABI __sys_kqueueex_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI __sys_namedobj_create_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI __sys_namedobj_delete_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI __sys_nanosleep_nid_postfix(const KernelTimespec* rqtp, KernelTimespec* rmtp) {
    int oldCancelState = 0;
    scePthreadSetcancelstate(1, &oldCancelState);
    const int result = sceKernelNanosleep(rqtp, rmtp);
    scePthreadSetcancelstate(oldCancelState, nullptr);
    if (result == 0) return 0;
    *__error_nid_postfix() = result & 0xFFFF;
    return -1;
}

int APS5_VABI __sys_netabort_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI __sys_netcontrol_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI __sys_netgetiflist_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI __sys_netgetsockinfo_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI __sys_opmc_disable_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI __sys_opmc_enable_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI __sys_opmc_get_ctr_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI __sys_opmc_get_hw_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI __sys_opmc_set_ctl_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI __sys_opmc_set_ctr_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI __sys_opmc_set_hw_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI __sys_osem_close_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI __sys_osem_open_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI __sys_randomized_path_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI __sys_rdup_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI __sys_regmgr_call_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI __sys_resume_internal_hdd_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI __sys_resume_process_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI __sys_set_uevt_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI __sys_socketclose_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI __sys_suspend_process_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI __sys_test_debug_rwmem_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI __sys_workaround8849_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

void APS5_VABI __ubsan_handle_cfi_check_fail_abort_nid_postfix(Ubsan::CfiCheckFailData* data, std::uint64_t value, std::uint64_t validVtable) {
    Ubsan::ReportCfiCheckFail(data, value, validVtable, true);
}

void APS5_VABI __ubsan_handle_cfi_check_fail_nid_postfix(Ubsan::CfiCheckFailData* data, std::uint64_t value, std::uint64_t validVtable) {
    Ubsan::ReportCfiCheckFail(data, value, validVtable, false);
}

int GuestDup2_nid_no_patch(int from, int to) {
    return dup2_nid_postfix(from, to);
}

int APS5_VABI _dup2_nid_postfix(int from, int to) {
    return GuestDup2_nid_no_patch(from, to);
}

int GuestExecve_nid_no_patch(const char* path, char* const* argv, char* const* envp) {
    constexpr int GuestEfault = 14;
    constexpr int GuestEacces = 13;
    constexpr int GuestEnoexec = 8;
    constexpr int GuestE2big = 7;
    constexpr int GuestEnoent = 2;
    constexpr int GuestEnametoolong = 63;
    constexpr std::size_t GuestPathMax = 1024;
    constexpr std::size_t GuestArgMax = 262144;
    const auto fail = [](int error) {
        *__error_nid_postfix() = error;
        return -1;
    };
    const auto gather = [](const char* guest, std::size_t maxLength, std::string& copy) -> int {
        for (std::size_t read = 0; read < maxLength;) {
            const auto address = reinterpret_cast<std::uintptr_t>(guest) + read;
            const std::size_t boundary = 4096 - address % 4096;
            const std::size_t bytes = boundary < maxLength - read ? boundary : maxLength - read;
            char buffer[4096];
            if (!GuestCopyIn_nid_no_patch(buffer, reinterpret_cast<const void*>(address), bytes)) return GuestEfault;
            const auto end = static_cast<const char*>(std::memchr(buffer, 0, bytes));
            if (end != nullptr) {
                copy.append(buffer, static_cast<std::size_t>(end - buffer));
                return 0;
            }
            copy.append(buffer, bytes);
            read += bytes;
        }
        return GuestEnametoolong;
    };
    if (argv == nullptr) return fail(GuestEfault);
    std::string fname;
    if (path != nullptr) {
        const int error = gather(path, GuestPathMax, fname);
        if (error != 0) return fail(error);
    }
    std::string text;
    std::vector<std::string> arguments;
    std::vector<std::string> environment;
    std::size_t stringspace = GuestArgMax;
    for (char* const* slot = argv;; ++slot) {
        char* argument = nullptr;
        if (!GuestCopyIn_nid_no_patch(&argument, slot, sizeof(argument))) return fail(GuestEfault);
        if (argument == nullptr) break;
        text.clear();
        const int error = gather(argument, stringspace, text);
        if (error != 0) return fail(error == GuestEnametoolong ? GuestE2big : error);
        stringspace -= text.size() + 1;
        arguments.push_back(std::move(text));
    }
    if (envp != nullptr) {
        for (char* const* slot = envp;; ++slot) {
            char* variable = nullptr;
            if (!GuestCopyIn_nid_no_patch(&variable, slot, sizeof(variable))) return fail(GuestEfault);
            if (variable == nullptr) break;
            text.clear();
            const int error = gather(variable, stringspace, text);
            if (error != 0) return fail(error == GuestEnametoolong ? GuestE2big : error);
            stringspace -= text.size() + 1;
            environment.push_back(std::move(text));
        }
    }
    if (path == nullptr) NotImplemented_nid_no_patch(__func__);
    if (fname.empty()) return fail(GuestEnoent);
    std::filesystem::path native;
    if (const int error = ResolvePathError_nid_no_patch(fname.c_str(), native)) return fail(error);
    std::error_code statusError;
    const auto status = std::filesystem::status(native, statusError);
    if (status.type() == std::filesystem::file_type::not_found) return fail(NotFoundError_nid_no_patch(native));
    if (statusError) return fail(FilesystemError(statusError));
#ifdef _WIN32
    const bool executable = std::filesystem::is_regular_file(status);
#else
    constexpr std::filesystem::perms execute = std::filesystem::perms::owner_exec | std::filesystem::perms::group_exec | std::filesystem::perms::others_exec;
    const bool executable = (status.permissions() & execute) != std::filesystem::perms::none && std::filesystem::is_regular_file(status);
#endif
    if (!executable) return fail(GuestEacces);
    std::error_code sizeError;
    const auto size = std::filesystem::file_size(native, sizeError);
    if (sizeError) return fail(FilesystemError(sizeError));
    if (size == 0) return fail(GuestEnoexec);
    if (!ExecutePermission_nid_no_patch(native)) return fail(GuestEacces);
    std::ifstream image(native, std::ios::binary);
    if (!image) return fail(GuestEacces);
    unsigned char header[4]{};
    image.read(reinterpret_cast<char*>(header), sizeof(header));
    if (image.gcount() < static_cast<std::streamsize>(sizeof(header))) return fail(GuestEnoexec);
    if (header[0] == '#' && header[1] == '!') {
        NotImplemented_nid_no_patch(__func__);
    }
    constexpr unsigned char ElfMagic[] = {0x7f, 0x45, 0x4c, 0x46};
    constexpr unsigned char Ps4SelfMagic[] = {0x4f, 0x15, 0x3d, 0x1d};
    constexpr unsigned char Ps5SelfMagic[] = {0x54, 0x14, 0xf5, 0xee};
    if (std::memcmp(header, ElfMagic, sizeof(ElfMagic)) == 0 ||
        std::memcmp(header, Ps4SelfMagic, sizeof(Ps4SelfMagic)) == 0 ||
        std::memcmp(header, Ps5SelfMagic, sizeof(Ps5SelfMagic)) == 0) {
        std::vector<char*> argumentPointers;
        for (auto& argument : arguments) argumentPointers.push_back(argument.data());
        argumentPointers.push_back(nullptr);
        std::vector<char*> environmentPointers;
        for (auto& variable : environment) environmentPointers.push_back(variable.data());
        environmentPointers.push_back(nullptr);
        return execve_nid_postfix(fname.c_str(), argumentPointers.data(), environmentPointers.data());
    }
    return fail(GuestEnoexec);
}

int APS5_VABI _execve_nid_postfix(const char* path, char* const* argv, char* const* envp) {
    return GuestExecve_nid_no_patch(path, argv, envp);
}

int GuestExecvpe_nid_no_patch(const char* name, char* const* argv, char* const* envp) {
    constexpr int GuestEacces = 13;
    constexpr int GuestEnoent = 2;
    constexpr int GuestE2big = 7;
    constexpr int GuestEnoexec = 8;
    constexpr int GuestEnomem = 12;
    constexpr int GuestEtxtbsy = 26;
    constexpr int GuestEloop = 62;
    constexpr int GuestEnametoolong = 63;
    constexpr int GuestEnotdir = 20;
    constexpr int GuestEfault = 14;
    constexpr std::size_t GuestCandidateMax = 1024;
    const std::string_view DefaultSearchPath = "/sbin:/bin:/usr/sbin:/usr/bin:/usr/local/sbin:/usr/local/bin";
    const std::string_view ShellPath = "/bin/sh";
    const auto fail = [](int error) {
        *__error_nid_postfix() = error;
        return -1;
    };
    std::string searchPath;
    if (!CopyGuestEnvironment_nid_no_patch("PATH", searchPath)) searchPath = std::string(DefaultSearchPath);
    std::string nameText;
    if (name != nullptr) {
        for (std::size_t read = 0; read < GuestCandidateMax;) {
            const auto address = reinterpret_cast<std::uintptr_t>(name) + read;
            const std::size_t boundary = 4096 - address % 4096;
            const std::size_t bytes = std::min(boundary, GuestCandidateMax - read);
            char buffer[4096];
            if (!GuestCopyIn_nid_no_patch(buffer, reinterpret_cast<const void*>(address), bytes)) return fail(GuestEfault);
            const auto end = static_cast<const char*>(std::memchr(buffer, 0, bytes));
            if (end != nullptr) {
                nameText.append(buffer, static_cast<std::size_t>(end - buffer));
                break;
            }
            nameText.append(buffer, bytes);
            read += bytes;
        }
    } else {
        return fail(GuestEfault);
    }
    if (nameText.size() == GuestCandidateMax) return fail(GuestEnoent);
    bool accessFailure = false;
    const auto ladder = [&](const std::string& candidate) {
        GuestExecve_nid_no_patch(candidate.c_str(), argv, envp);
        const int refusal = *__error_nid_postfix();
        switch (refusal) {
        case GuestE2big:
        case GuestEnomem:
        case GuestEtxtbsy:
            return true;
        case GuestEloop:
        case GuestEnametoolong:
        case GuestEnoent:
        case GuestEnotdir:
            return false;
        case GuestEnoexec: {
            std::size_t count = 0;
            for (;;) {
                char* entry = nullptr;
                if (!GuestCopyIn_nid_no_patch(&entry, argv + count, sizeof(entry))) {
                    fail(GuestEfault);
                    return true;
                }
                if (entry == nullptr) break;
                ++count;
            }
            const std::string shell("sh");
            std::vector<char*> shellArgs;
            shellArgs.reserve(count + 2);
            shellArgs.push_back(const_cast<char*>(shell.c_str()));
            shellArgs.push_back(const_cast<char*>(candidate.c_str()));
            for (std::size_t index = 1; index < count; ++index) {
                char* next = nullptr;
                if (!GuestCopyIn_nid_no_patch(&next, argv + index, sizeof(next))) {
                    fail(GuestEfault);
                    return true;
                }
                shellArgs.push_back(next);
            }
            shellArgs.push_back(nullptr);
            GuestExecve_nid_no_patch(ShellPath.data(), shellArgs.data(), envp);
            return true;
        }
        default: {
            std::filesystem::path native;
            if (const int resolved = ResolvePathError_nid_no_patch(candidate.c_str(), native)) return false;
            std::error_code lookupError;
            const auto status = std::filesystem::status(native, lookupError);
            if (lookupError || status.type() == std::filesystem::file_type::not_found) return false;
            if (refusal == GuestEacces) {
                accessFailure = true;
                return false;
            }
            return true;
        }
        }
    };
    if (nameText.find('/') != std::string::npos) {
        if (ladder(nameText)) return -1;
        return fail(accessFailure ? GuestEacces : GuestEnoent);
    }
    if (nameText.empty()) return fail(GuestEnoent);
    std::size_t offset = 0;
    for (bool finished = false; !finished;) {
        const auto separator = searchPath.find(':', offset);
        finished = separator == std::string::npos;
        const std::string_view element(searchPath.data() + offset, finished ? searchPath.size() - offset : separator - offset);
        offset = finished ? searchPath.size() : separator + 1;
        const std::string_view directory = element.empty() ? std::string_view(".") : element;
        if (directory.size() + nameText.size() + 2 > GuestCandidateMax) continue;
        std::string candidate(directory);
        candidate.push_back('/');
        candidate += nameText;
        if (ladder(candidate)) return -1;
    }
    return fail(accessFailure ? GuestEacces : GuestEnoent);
}

int APS5_VABI _execvpe_nid_postfix(const char* name, char* const* argv, char* const* envp) {
    return GuestExecvpe_nid_no_patch(name, argv, envp);
}

int APS5_VABI _wait4_nid_postfix(int pid, int* status, int options, void* usage) {
    return wait4_nid_postfix(pid, status, options, usage);
}

int APS5_VABI adjtime_nid_postfix(const KernelTimeval* delta, KernelTimeval* oldDelta) {
    constexpr int GuestEperm = 1;
    constexpr int GuestEfault = 14;
    KernelTimeval adjustment{};
    if (delta != nullptr && !GuestCopyIn_nid_no_patch(&adjustment, delta, sizeof(adjustment))) {
        *__error_nid_postfix() = GuestEfault;
        return -1;
    }
    if (delta != nullptr) {
        *__error_nid_postfix() = GuestEperm;
        return -1;
    }
    if (oldDelta != nullptr) {
        const KernelTimeval pending{};
        if (!GuestCopyOut_nid_no_patch(oldDelta, &pending, sizeof(pending))) {
            *__error_nid_postfix() = GuestEfault;
            return -1;
        }
    }
    return 0;
}

int APS5_VABI libkernel_sysUnknown_0CV40L3tznI_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}

APS5_EXPORT("0CV40L3tznI", libkernel_sysUnknown_0CV40L3tznI_nid_no_patch);

std::uint64_t APS5_VABI libkernel_sysUnknown_1WXQlxMpnNA_nid_no_patch() {
    g_flag1WXQlxMpnNA.store(1, std::memory_order_relaxed);
    return 0;
}

APS5_EXPORT("1WXQlxMpnNA", libkernel_sysUnknown_1WXQlxMpnNA_nid_no_patch);

int APS5_VABI libkernel_sysUnknown_1v23w7Rnkic_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}

APS5_EXPORT("1v23w7Rnkic", libkernel_sysUnknown_1v23w7Rnkic_nid_no_patch);

int APS5_VABI libkernel_sysUnknown_1v6DjTPunjs_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}

APS5_EXPORT("1v6DjTPunjs", libkernel_sysUnknown_1v6DjTPunjs_nid_no_patch);

int APS5_VABI sceKernelInternalCreateMapperSharedMemory() {
    NotImplemented_nid_no_patch(__func__);
}


int APS5_VABI libkernel_sysUnknown_2XwZJG3Dkss_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}

APS5_EXPORT("2XwZJG3Dkss", libkernel_sysUnknown_2XwZJG3Dkss_nid_no_patch);

int APS5_VABI libkernel_sysUnknown_2YWCKF9_minus_3gM_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}

APS5_EXPORT("2YWCKF9-3gM", libkernel_sysUnknown_2YWCKF9_minus_3gM_nid_no_patch);

int APS5_VABI libkernel_sysUnknown_2_plus_KZ4mZKp6E_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}

APS5_EXPORT("2+KZ4mZKp6E", libkernel_sysUnknown_2_plus_KZ4mZKp6E_nid_no_patch);

int APS5_VABI libkernel_sysUnknown_2fPLezHAV2E_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}

APS5_EXPORT("2fPLezHAV2E", libkernel_sysUnknown_2fPLezHAV2E_nid_no_patch);

int APS5_VABI libkernel_sysUnknown_2wKjID0iGVM_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}

APS5_EXPORT("2wKjID0iGVM", libkernel_sysUnknown_2wKjID0iGVM_nid_no_patch);

int APS5_VABI libkernel_sysUnknown_2xg4JXvaJ6k_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}

APS5_EXPORT("2xg4JXvaJ6k", libkernel_sysUnknown_2xg4JXvaJ6k_nid_no_patch);

int APS5_VABI libkernel_sysUnknown_3BwLcdWeZLs_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}

APS5_EXPORT("3BwLcdWeZLs", libkernel_sysUnknown_3BwLcdWeZLs_nid_no_patch);

int APS5_VABI libkernel_sysUnknown_3YkQPLocySc_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}

APS5_EXPORT("3YkQPLocySc", libkernel_sysUnknown_3YkQPLocySc_nid_no_patch);

int APS5_VABI libkernel_sysUnknown_3jw_minus_vq4gYLs_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}

APS5_EXPORT("3jw-vq4gYLs", libkernel_sysUnknown_3jw_minus_vq4gYLs_nid_no_patch);

int APS5_VABI libkernel_sysUnknown_4AMTAyz32fk_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}

APS5_EXPORT("4AMTAyz32fk", libkernel_sysUnknown_4AMTAyz32fk_nid_no_patch);

int APS5_VABI sceKernelIsCloudkitForDecid() {
    NotImplemented_nid_no_patch(__func__);
}


int APS5_VABI libkernel_sysUnknown_5KHFhRhBR78_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}

APS5_EXPORT("5KHFhRhBR78", libkernel_sysUnknown_5KHFhRhBR78_nid_no_patch);

int APS5_VABI sceKernelIccGetButtonStatus() {
    NotImplemented_nid_no_patch(__func__);
}


int APS5_VABI sceKernelAbortWithContext() {
    NotImplemented_nid_no_patch(__func__);
}


int APS5_VABI sceKernelGetCanvasHandleByPid() {
    NotImplemented_nid_no_patch(__func__);
}


int APS5_VABI libkernel_sysUnknown_7BUHmsuhcq4_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}

APS5_EXPORT("7BUHmsuhcq4", libkernel_sysUnknown_7BUHmsuhcq4_nid_no_patch);

int APS5_VABI libkernel_sysUnknown_7FNl8FfSGNw_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}

APS5_EXPORT("7FNl8FfSGNw", libkernel_sysUnknown_7FNl8FfSGNw_nid_no_patch);

int APS5_VABI libkernel_sysUnknown_7Sd1vnnqEqU_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}

APS5_EXPORT("7Sd1vnnqEqU", libkernel_sysUnknown_7Sd1vnnqEqU_nid_no_patch);

int APS5_VABI sceKernelIccReadWlanArea() {
    NotImplemented_nid_no_patch(__func__);
}


int APS5_VABI libkernel_sysUnknown_8U0nlN0TpDU_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}

APS5_EXPORT("8U0nlN0TpDU", libkernel_sysUnknown_8U0nlN0TpDU_nid_no_patch);

int APS5_VABI libkernel_sysUnknown_8YkbXeSRxnM_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}

APS5_EXPORT("8YkbXeSRxnM", libkernel_sysUnknown_8YkbXeSRxnM_nid_no_patch);

int APS5_VABI libkernel_sysUnknown_940JfV9yJto_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}

APS5_EXPORT("940JfV9yJto", libkernel_sysUnknown_940JfV9yJto_nid_no_patch);

int APS5_VABI sceKernelGetAppMemoryInfo() {
    NotImplemented_nid_no_patch(__func__);
}


int APS5_VABI libkernel_sysUnknown_9ncS0kCRRSA_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}

APS5_EXPORT("9ncS0kCRRSA", libkernel_sysUnknown_9ncS0kCRRSA_nid_no_patch);

int APS5_VABI libkernel_sysUnknown_AGHqSV1gdIY_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}

APS5_EXPORT("AGHqSV1gdIY", libkernel_sysUnknown_AGHqSV1gdIY_nid_no_patch);

int APS5_VABI libkernel_sysUnknown_A_plus_S5E4RCh7Y_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}

APS5_EXPORT("A+S5E4RCh7Y", libkernel_sysUnknown_A_plus_S5E4RCh7Y_nid_no_patch);

int APS5_VABI libkernel_sysUnknown_AhBZlHQYeA8_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}

APS5_EXPORT("AhBZlHQYeA8", libkernel_sysUnknown_AhBZlHQYeA8_nid_no_patch);

int APS5_VABI libkernel_sysUnknown_ApbTseKUKCI_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}

APS5_EXPORT("ApbTseKUKCI", libkernel_sysUnknown_ApbTseKUKCI_nid_no_patch);

int APS5_VABI libkernel_sysUnknown_B70jHzr4Jj4_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}

APS5_EXPORT("B70jHzr4Jj4", libkernel_sysUnknown_B70jHzr4Jj4_nid_no_patch);

int APS5_VABI libkernel_sysUnknown_BJvWKKfmj4w_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}

APS5_EXPORT("BJvWKKfmj4w", libkernel_sysUnknown_BJvWKKfmj4w_nid_no_patch);

int APS5_VABI libkernel_sysUnknown_BjmZn9pYSuk_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}

APS5_EXPORT("BjmZn9pYSuk", libkernel_sysUnknown_BjmZn9pYSuk_nid_no_patch);

int APS5_VABI libkernel_sysUnknown_C3ToW2CA_plus_hM_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}

APS5_EXPORT("C3ToW2CA+hM", libkernel_sysUnknown_C3ToW2CA_plus_hM_nid_no_patch);

int APS5_VABI libkernel_sysUnknown_C3xgqN95IoE_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}

APS5_EXPORT("C3xgqN95IoE", libkernel_sysUnknown_C3xgqN95IoE_nid_no_patch);

std::uint64_t APS5_VABI libkernel_sysUnknown_C49jelxiaVE_nid_no_patch() {
    return 0x20018u;
}

APS5_EXPORT("C49jelxiaVE", libkernel_sysUnknown_C49jelxiaVE_nid_no_patch);

int APS5_VABI sceKernelSwitchToTrinityMode() {
    NotImplemented_nid_no_patch(__func__);
}


int APS5_VABI sceKernelIsGenuineCronosCex() {
    NotImplemented_nid_no_patch(__func__);
}


int APS5_VABI libkernel_sysUnknown_DWWduhQYA3I_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}

APS5_EXPORT("DWWduhQYA3I", libkernel_sysUnknown_DWWduhQYA3I_nid_no_patch);

int APS5_VABI libkernel_sysUnknown_Dv8In6FvzuM_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}

APS5_EXPORT("Dv8In6FvzuM", libkernel_sysUnknown_Dv8In6FvzuM_nid_no_patch);

int APS5_VABI libkernel_sysUnknown_E58GHGcQT7I_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}

APS5_EXPORT("E58GHGcQT7I", libkernel_sysUnknown_E58GHGcQT7I_nid_no_patch);

int APS5_VABI libkernel_sysUnknown_EIz_plus_fSlYCn8_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}

APS5_EXPORT("EIz+fSlYCn8", libkernel_sysUnknown_EIz_plus_fSlYCn8_nid_no_patch);

int APS5_VABI sceKernelTraceMarkerBookmark() {
    NotImplemented_nid_no_patch(__func__);
}


int APS5_VABI libkernel_sysUnknown_EaepepH_minus_LmI_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}

APS5_EXPORT("EaepepH-LmI", libkernel_sysUnknown_EaepepH_minus_LmI_nid_no_patch);

int APS5_VABI libkernel_sysUnknown_FIUPjhnoI_plus_o_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}

APS5_EXPORT("FIUPjhnoI+o", libkernel_sysUnknown_FIUPjhnoI_plus_o_nid_no_patch);

std::uint64_t APS5_VABI libkernel_sysUnknown_FR0odl35yQY_nid_no_patch() {
    return g_flag1WXQlxMpnNA.load(std::memory_order_relaxed);
}

APS5_EXPORT("FR0odl35yQY", libkernel_sysUnknown_FR0odl35yQY_nid_no_patch);

int APS5_VABI sceKernelIccGetExtBDDriveStatus() {
    NotImplemented_nid_no_patch(__func__);
}


int APS5_VABI libkernel_sysUnknown_GTyjVeZcdRM_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}

APS5_EXPORT("GTyjVeZcdRM", libkernel_sysUnknown_GTyjVeZcdRM_nid_no_patch);

int APS5_VABI libkernel_sysUnknown_Gtncco_plus_mxio_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}

APS5_EXPORT("Gtncco+mxio", libkernel_sysUnknown_Gtncco_plus_mxio_nid_no_patch);

int APS5_VABI libkernel_sysUnknown_HRrfx1wytn0_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}

APS5_EXPORT("HRrfx1wytn0", libkernel_sysUnknown_HRrfx1wytn0_nid_no_patch);

int APS5_VABI libkernel_sysUnknown_HnWsxkpJya0_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}

APS5_EXPORT("HnWsxkpJya0", libkernel_sysUnknown_HnWsxkpJya0_nid_no_patch);

int APS5_VABI libkernel_sysUnknown_HruYl9uBnNo_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}

APS5_EXPORT("HruYl9uBnNo", libkernel_sysUnknown_HruYl9uBnNo_nid_no_patch);

int APS5_VABI libkernel_sysUnknown_IJQeoUQ3pb4_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}

APS5_EXPORT("IJQeoUQ3pb4", libkernel_sysUnknown_IJQeoUQ3pb4_nid_no_patch);

int APS5_VABI sceKernelInternalUnlockMapperMemory() {
    NotImplemented_nid_no_patch(__func__);
}


int APS5_VABI libkernel_sysUnknown_Ipau2b5eaHM_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}

APS5_EXPORT("Ipau2b5eaHM", libkernel_sysUnknown_Ipau2b5eaHM_nid_no_patch);

int APS5_VABI libkernel_sysUnknown_Ishr1Sdu024_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}

APS5_EXPORT("Ishr1Sdu024", libkernel_sysUnknown_Ishr1Sdu024_nid_no_patch);

int APS5_VABI sceKernelIdTableGetStats() {
    NotImplemented_nid_no_patch(__func__);
}


int APS5_VABI libkernel_sysUnknown_JFpAREOJnag_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}

APS5_EXPORT("JFpAREOJnag", libkernel_sysUnknown_JFpAREOJnag_nid_no_patch);

int APS5_VABI libkernel_sysUnknown_JN1D3WtbDPs_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}

APS5_EXPORT("JN1D3WtbDPs", libkernel_sysUnknown_JN1D3WtbDPs_nid_no_patch);

int APS5_VABI sceKernelIccNotifyPowerState() {
    NotImplemented_nid_no_patch(__func__);
}


int APS5_VABI libkernel_sysUnknown_KoDy4Jazz40_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}

APS5_EXPORT("KoDy4Jazz40", libkernel_sysUnknown_KoDy4Jazz40_nid_no_patch);

int APS5_VABI libkernel_sysUnknown_KoSCciXA1N0_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}

APS5_EXPORT("KoSCciXA1N0", libkernel_sysUnknown_KoSCciXA1N0_nid_no_patch);

int APS5_VABI libkernel_sysUnknown_L4GARHYRW0U_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}

APS5_EXPORT("L4GARHYRW0U", libkernel_sysUnknown_L4GARHYRW0U_nid_no_patch);

int APS5_VABI libkernel_sysUnknown_LPXIR37gw3c_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}

APS5_EXPORT("LPXIR37gw3c", libkernel_sysUnknown_LPXIR37gw3c_nid_no_patch);

int APS5_VABI libkernel_sysUnknown_LVRydXoXt7U_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}

APS5_EXPORT("LVRydXoXt7U", libkernel_sysUnknown_LVRydXoXt7U_nid_no_patch);

int APS5_VABI libkernel_sysUnknown_LwGBaXgvFaY_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}

APS5_EXPORT("LwGBaXgvFaY", libkernel_sysUnknown_LwGBaXgvFaY_nid_no_patch);

int APS5_VABI libkernel_sysUnknown_MJ_minus_err8l_minus_XY_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}

APS5_EXPORT("MJ-err8l-XY", libkernel_sysUnknown_MJ_minus_err8l_minus_XY_nid_no_patch);

int APS5_VABI libkernel_sysUnknown_MKlp_minus_bW1bz8_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}

APS5_EXPORT("MKlp-bW1bz8", libkernel_sysUnknown_MKlp_minus_bW1bz8_nid_no_patch);

std::uint64_t APS5_VABI libkernel_sysUnknown_N94a0T4F4_plus_U_nid_no_patch() {
    return 0x8002004eu;
}

APS5_EXPORT("N94a0T4F4+U", libkernel_sysUnknown_N94a0T4F4_plus_U_nid_no_patch);

int APS5_VABI libkernel_sysUnknown_NhFlyw_minus_77cY_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}

APS5_EXPORT("NhFlyw-77cY", libkernel_sysUnknown_NhFlyw_minus_77cY_nid_no_patch);

int APS5_VABI libkernel_sysUnknown_OZBJnWxt7d4_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}

APS5_EXPORT("OZBJnWxt7d4", libkernel_sysUnknown_OZBJnWxt7d4_nid_no_patch);

int APS5_VABI libkernel_sysUnknown_PBq_minus_OUg5A5s_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}

APS5_EXPORT("PBq-OUg5A5s", libkernel_sysUnknown_PBq_minus_OUg5A5s_nid_no_patch);

int APS5_VABI libkernel_sysUnknown_PTDuAbCYfm0_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}

APS5_EXPORT("PTDuAbCYfm0", libkernel_sysUnknown_PTDuAbCYfm0_nid_no_patch);

int APS5_VABI libkernel_sysUnknown_QTGIeqEjeFc_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}

APS5_EXPORT("QTGIeqEjeFc", libkernel_sysUnknown_QTGIeqEjeFc_nid_no_patch);

int APS5_VABI libkernel_sysUnknown_Qk38ESxhDEQ_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}

APS5_EXPORT("Qk38ESxhDEQ", libkernel_sysUnknown_Qk38ESxhDEQ_nid_no_patch);

int APS5_VABI libkernel_sysUnknown_R3QLLyp9MEU_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}

APS5_EXPORT("R3QLLyp9MEU", libkernel_sysUnknown_R3QLLyp9MEU_nid_no_patch);

int APS5_VABI libkernel_sysUnknown_RA5mXLPd5V4_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}

APS5_EXPORT("RA5mXLPd5V4", libkernel_sysUnknown_RA5mXLPd5V4_nid_no_patch);

int APS5_VABI libkernel_sysUnknown_RDjusrvQ_minus_44_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}

APS5_EXPORT("RDjusrvQ-44", libkernel_sysUnknown_RDjusrvQ_minus_44_nid_no_patch);

int APS5_VABI libkernel_sysUnknown_RfzgytiWey0_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}

APS5_EXPORT("RfzgytiWey0", libkernel_sysUnknown_RfzgytiWey0_nid_no_patch);

int APS5_VABI libkernel_sysUnknown_SEKlG1AMRGQ_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}

APS5_EXPORT("SEKlG1AMRGQ", libkernel_sysUnknown_SEKlG1AMRGQ_nid_no_patch);

int APS5_VABI sceKernelGetContentVersion() {
    NotImplemented_nid_no_patch(__func__);
}


int APS5_VABI libkernel_sysUnknown_Sesgl6Zl8YI_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}

APS5_EXPORT("Sesgl6Zl8YI", libkernel_sysUnknown_Sesgl6Zl8YI_nid_no_patch);

int APS5_VABI sceKernelTraceMarkerPop() {
    NotImplemented_nid_no_patch(__func__);
}


int APS5_VABI sceKernelInternalLockMapperMemory() {
    NotImplemented_nid_no_patch(__func__);
}


int APS5_VABI libkernel_sysUnknown_TVB77yVa_plus_RU_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}

APS5_EXPORT("TVB77yVa+RU", libkernel_sysUnknown_TVB77yVa_plus_RU_nid_no_patch);

int APS5_VABI libkernel_sysUnknown_Tca8tetUZCw_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}

APS5_EXPORT("Tca8tetUZCw", libkernel_sysUnknown_Tca8tetUZCw_nid_no_patch);

int APS5_VABI libkernel_sysUnknown_U9B58Hc5C_plus_8_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}

APS5_EXPORT("U9B58Hc5C+8", libkernel_sysUnknown_U9B58Hc5C_plus_8_nid_no_patch);

int APS5_VABI sceKernelAddExternalTriggerEvent() {
    NotImplemented_nid_no_patch(__func__);
}


int APS5_VABI libkernel_sysUnknown_V4f3CEBQyuM_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}

APS5_EXPORT("V4f3CEBQyuM", libkernel_sysUnknown_V4f3CEBQyuM_nid_no_patch);

int APS5_VABI libkernel_sysUnknown_VU1YkdCmYto_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}

APS5_EXPORT("VU1YkdCmYto", libkernel_sysUnknown_VU1YkdCmYto_nid_no_patch);

int APS5_VABI libkernel_sysUnknown_W1Cg5WPD4IQ_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}

APS5_EXPORT("W1Cg5WPD4IQ", libkernel_sysUnknown_W1Cg5WPD4IQ_nid_no_patch);

int APS5_VABI libkernel_sysUnknown_W7Vpt62Xtf4_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}

APS5_EXPORT("W7Vpt62Xtf4", libkernel_sysUnknown_W7Vpt62Xtf4_nid_no_patch);

int APS5_VABI libkernel_sysUnknown_WLC94v0XY_plus_0_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}

APS5_EXPORT("WLC94v0XY+0", libkernel_sysUnknown_WLC94v0XY_plus_0_nid_no_patch);

int APS5_VABI libkernel_sysUnknown_WtA8Aq_minus_0cyY_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}

APS5_EXPORT("WtA8Aq-0cyY", libkernel_sysUnknown_WtA8Aq_minus_0cyY_nid_no_patch);

int APS5_VABI libkernel_sysUnknown_WyZULCQhjdY_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}

APS5_EXPORT("WyZULCQhjdY", libkernel_sysUnknown_WyZULCQhjdY_nid_no_patch);

int APS5_VABI libkernel_sysUnknown_X4R_minus_jzzFYac_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}

APS5_EXPORT("X4R-jzzFYac", libkernel_sysUnknown_X4R_minus_jzzFYac_nid_no_patch);

int APS5_VABI libkernel_sysUnknown_Xj9nET9tctI_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}

APS5_EXPORT("Xj9nET9tctI", libkernel_sysUnknown_Xj9nET9tctI_nid_no_patch);

int APS5_VABI libkernel_sysUnknown_XvONqaRmAPE_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}

APS5_EXPORT("XvONqaRmAPE", libkernel_sysUnknown_XvONqaRmAPE_nid_no_patch);

int APS5_VABI libkernel_sysUnknown_YP7qj_plus_uSJwo_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}

APS5_EXPORT("YP7qj+uSJwo", libkernel_sysUnknown_YP7qj_plus_uSJwo_nid_no_patch);

int APS5_VABI libkernel_sysUnknown_ZFhVeS_plus_BQJs_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}

APS5_EXPORT("ZFhVeS+BQJs", libkernel_sysUnknown_ZFhVeS_plus_BQJs_nid_no_patch);

int APS5_VABI libkernel_sysUnknown_Z_plus_8fq02NpM0_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}

APS5_EXPORT("Z+8fq02NpM0", libkernel_sysUnknown_Z_plus_8fq02NpM0_nid_no_patch);

int APS5_VABI libkernel_sysUnknown__minus_PNB3tfnD8c_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}

APS5_EXPORT("-PNB3tfnD8c", libkernel_sysUnknown__minus_PNB3tfnD8c_nid_no_patch);

int APS5_VABI sceKernelGetAppLibcData() {
    NotImplemented_nid_no_patch(__func__);
}


int APS5_VABI libkernel_sysUnknown__minus_sYN8ucoAWw_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}

APS5_EXPORT("-sYN8ucoAWw", libkernel_sysUnknown__minus_sYN8ucoAWw_nid_no_patch);

int APS5_VABI libkernel_sysUnknown__plus_1mj9RYU1rs_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}

APS5_EXPORT("+1mj9RYU1rs", libkernel_sysUnknown__plus_1mj9RYU1rs_nid_no_patch);

int APS5_VABI libkernel_sysUnknown__plus_MZfwzH65JQ_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}

APS5_EXPORT("+MZfwzH65JQ", libkernel_sysUnknown__plus_MZfwzH65JQ_nid_no_patch);

int APS5_VABI sceKernelGetAmprSysInfo() {
    NotImplemented_nid_no_patch(__func__);
}


int APS5_VABI libkernel_sysUnknown_aH3bEULQH54_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}

APS5_EXPORT("aH3bEULQH54", libkernel_sysUnknown_aH3bEULQH54_nid_no_patch);

int APS5_VABI libkernel_sysUnknown_aJK9UjBtwi4_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}

APS5_EXPORT("aJK9UjBtwi4", libkernel_sysUnknown_aJK9UjBtwi4_nid_no_patch);

int APS5_VABI libkernel_sysUnknown_aL9DwvoSDzs_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}

APS5_EXPORT("aL9DwvoSDzs", libkernel_sysUnknown_aL9DwvoSDzs_nid_no_patch);

int APS5_VABI libkernel_sysUnknown_aekYw_plus_lydSg_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}

APS5_EXPORT("aekYw+lydSg", libkernel_sysUnknown_aekYw_plus_lydSg_nid_no_patch);

int APS5_VABI libkernel_sysUnknown_aj7wRdAWFeM_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}

APS5_EXPORT("aj7wRdAWFeM", libkernel_sysUnknown_aj7wRdAWFeM_nid_no_patch);

int APS5_VABI libkernel_sysUnknown_ak6t3JzhOtg_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}

APS5_EXPORT("ak6t3JzhOtg", libkernel_sysUnknown_ak6t3JzhOtg_nid_no_patch);

int APS5_VABI sceKernelGetSandboxConfig() {
    NotImplemented_nid_no_patch(__func__);
}


int APS5_VABI libkernel_sysUnknown_bQ_plus_8uJVyxII_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}

APS5_EXPORT("bQ+8uJVyxII", libkernel_sysUnknown_bQ_plus_8uJVyxII_nid_no_patch);

int APS5_VABI sceKernelIccResetBootTimestamp() {
    NotImplemented_nid_no_patch(__func__);
}


int APS5_VABI libkernel_sysUnknown_cH6vM6sF5YA_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}

APS5_EXPORT("cH6vM6sF5YA", libkernel_sysUnknown_cH6vM6sF5YA_nid_no_patch);

int APS5_VABI libkernel_sysUnknown_cReByBlM5XE_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}

APS5_EXPORT("cReByBlM5XE", libkernel_sysUnknown_cReByBlM5XE_nid_no_patch);

int APS5_VABI libkernel_sysUnknown_cafDxa64sO4_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}

APS5_EXPORT("cafDxa64sO4", libkernel_sysUnknown_cafDxa64sO4_nid_no_patch);

int APS5_VABI libkernel_sysUnknown_ciYlptMdvhc_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}

APS5_EXPORT("ciYlptMdvhc", libkernel_sysUnknown_ciYlptMdvhc_nid_no_patch);

int APS5_VABI sceKernelIccWriteWlanArea() {
    NotImplemented_nid_no_patch(__func__);
}


int APS5_VABI libkernel_sysUnknown_culoiinnxDA_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}

APS5_EXPORT("culoiinnxDA", libkernel_sysUnknown_culoiinnxDA_nid_no_patch);

int APS5_VABI sceKernelCheckProsperoSdkVersion() {
    NotImplemented_nid_no_patch(__func__);
}


int APS5_VABI libkernel_sysUnknown_dnzOA7Vx1AE_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}

APS5_EXPORT("dnzOA7Vx1AE", libkernel_sysUnknown_dnzOA7Vx1AE_nid_no_patch);

int APS5_VABI libkernel_sysUnknown_drv94FQk_minus_Qw_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}

APS5_EXPORT("drv94FQk-Qw", libkernel_sysUnknown_drv94FQk_minus_Qw_nid_no_patch);

int APS5_VABI libkernel_sysUnknown_e7oZmNPlBbU_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}

APS5_EXPORT("e7oZmNPlBbU", libkernel_sysUnknown_e7oZmNPlBbU_nid_no_patch);

int APS5_VABI libkernel_sysUnknown_eSgYzXvTPQI_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}

APS5_EXPORT("eSgYzXvTPQI", libkernel_sysUnknown_eSgYzXvTPQI_nid_no_patch);

int APS5_VABI sceKernelTraceMarkerPlot() {
    NotImplemented_nid_no_patch(__func__);
}


int APS5_VABI libkernel_sysUnknown_ftHBlCfIEQo_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}

APS5_EXPORT("ftHBlCfIEQo", libkernel_sysUnknown_ftHBlCfIEQo_nid_no_patch);

int APS5_VABI libkernel_sysUnknown_g7MGwZ5UxBE_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}

APS5_EXPORT("g7MGwZ5UxBE", libkernel_sysUnknown_g7MGwZ5UxBE_nid_no_patch);

int APS5_VABI libkernel_sysUnknown_gBiaAGKEvHM_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}

APS5_EXPORT("gBiaAGKEvHM", libkernel_sysUnknown_gBiaAGKEvHM_nid_no_patch);

std::uint64_t APS5_VABI libkernel_sysUnknown_gJ5gpW45mU0_nid_no_patch() {
    return 0;
}

APS5_EXPORT("gJ5gpW45mU0", libkernel_sysUnknown_gJ5gpW45mU0_nid_no_patch);

int APS5_VABI libkernel_sysUnknown_gSIiye3ERqo_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}

APS5_EXPORT("gSIiye3ERqo", libkernel_sysUnknown_gSIiye3ERqo_nid_no_patch);

int APS5_VABI libkernel_sysUnknown_gpgi_minus_GwE2Is_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}

APS5_EXPORT("gpgi-GwE2Is", libkernel_sysUnknown_gpgi_minus_GwE2Is_nid_no_patch);

int APS5_VABI libkernel_sysUnknown_guq_plus_otVpgj0_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}

APS5_EXPORT("guq+otVpgj0", libkernel_sysUnknown_guq_plus_otVpgj0_nid_no_patch);

int APS5_VABI libkernel_sysUnknown_gvq6XRoDogE_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}

APS5_EXPORT("gvq6XRoDogE", libkernel_sysUnknown_gvq6XRoDogE_nid_no_patch);

int APS5_VABI libkernel_sysUnknown_h26nEKnW8VI_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}

APS5_EXPORT("h26nEKnW8VI", libkernel_sysUnknown_h26nEKnW8VI_nid_no_patch);

int APS5_VABI libkernel_sysUnknown_h35fpAz5tf4_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}

APS5_EXPORT("h35fpAz5tf4", libkernel_sysUnknown_h35fpAz5tf4_nid_no_patch);

int APS5_VABI sceKernelTraceMarkerPush() {
    NotImplemented_nid_no_patch(__func__);
}


int APS5_VABI libkernel_sysUnknown_hmqw8GlN_plus_tI_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}

APS5_EXPORT("hmqw8GlN+tI", libkernel_sysUnknown_hmqw8GlN_plus_tI_nid_no_patch);

int APS5_VABI sceKernelGetCanvasHandle() {
    NotImplemented_nid_no_patch(__func__);
}


int APS5_VABI sceKernelSetSandboxId() {
    NotImplemented_nid_no_patch(__func__);
}


int APS5_VABI libkernel_sysUnknown_ilQLkGlAjWA_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}

APS5_EXPORT("ilQLkGlAjWA", libkernel_sysUnknown_ilQLkGlAjWA_nid_no_patch);

int APS5_VABI libkernel_sysUnknown_j6bGtwZkxOQ_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}

APS5_EXPORT("j6bGtwZkxOQ", libkernel_sysUnknown_j6bGtwZkxOQ_nid_no_patch);

int APS5_VABI libkernel_sysUnknown_jESbu_plus_uFJYU_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}

APS5_EXPORT("jESbu+uFJYU", libkernel_sysUnknown_jESbu_plus_uFJYU_nid_no_patch);

int APS5_VABI libkernel_sysUnknown_jMuNoBRCPUg_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}

APS5_EXPORT("jMuNoBRCPUg", libkernel_sysUnknown_jMuNoBRCPUg_nid_no_patch);

int APS5_VABI libkernel_sysUnknown_jkLbtOA587w_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}

APS5_EXPORT("jkLbtOA587w", libkernel_sysUnknown_jkLbtOA587w_nid_no_patch);

int APS5_VABI libkernel_sysUnknown_k_plus_J3KBjfkNk_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}

APS5_EXPORT("k+J3KBjfkNk", libkernel_sysUnknown_k_plus_J3KBjfkNk_nid_no_patch);

int APS5_VABI libkernel_sysUnknown_km8Fr2Jn4Gg_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}

APS5_EXPORT("km8Fr2Jn4Gg", libkernel_sysUnknown_km8Fr2Jn4Gg_nid_no_patch);

int APS5_VABI libkernel_sysUnknown_l67TRPy55_minus_U_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}

APS5_EXPORT("l67TRPy55-U", libkernel_sysUnknown_l67TRPy55_minus_U_nid_no_patch);

int APS5_VABI sceKernelDeleteExternalTriggerEvent() {
    NotImplemented_nid_no_patch(__func__);
}


int APS5_VABI libkernel_sysUnknown_l_minus_QGmU3VU_plus_Q_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}

APS5_EXPORT("l-QGmU3VU+Q", libkernel_sysUnknown_l_minus_QGmU3VU_plus_Q_nid_no_patch);

int APS5_VABI libkernel_sysUnknown_mIkJUpP5vKo_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}

APS5_EXPORT("mIkJUpP5vKo", libkernel_sysUnknown_mIkJUpP5vKo_nid_no_patch);

int APS5_VABI libkernel_sysUnknown_ncrollDo5u4_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}

APS5_EXPORT("ncrollDo5u4", libkernel_sysUnknown_ncrollDo5u4_nid_no_patch);

int APS5_VABI libkernel_sysUnknown_nsq826qQ2EI_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}

APS5_EXPORT("nsq826qQ2EI", libkernel_sysUnknown_nsq826qQ2EI_nid_no_patch);

int APS5_VABI libkernel_sysUnknown_o141x2VDeVc_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}

APS5_EXPORT("o141x2VDeVc", libkernel_sysUnknown_o141x2VDeVc_nid_no_patch);

int APS5_VABI sceKernelGetMainSocSubId() {
    NotImplemented_nid_no_patch(__func__);
}


int APS5_VABI sceKernelInternalUnmapMapperSharedMemory() {
    NotImplemented_nid_no_patch(__func__);
}


std::uint64_t APS5_VABI libkernel_sysUnknown_pCbBddNl66Y_nid_no_patch() {
    return 0;
}

APS5_EXPORT("pCbBddNl66Y", libkernel_sysUnknown_pCbBddNl66Y_nid_no_patch);

int APS5_VABI libkernel_sysUnknown_pOClGbxrx94_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}

APS5_EXPORT("pOClGbxrx94", libkernel_sysUnknown_pOClGbxrx94_nid_no_patch);

int APS5_VABI sceKernelSetAppLibcData() {
    NotImplemented_nid_no_patch(__func__);
}


int APS5_VABI libkernel_sysUnknown_pp_plus_G_plus_UZhiqY_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}

APS5_EXPORT("pp+G+UZhiqY", libkernel_sysUnknown_pp_plus_G_plus_UZhiqY_nid_no_patch);

int APS5_VABI libkernel_sysUnknown_qGzuTSD9cPQ_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}

APS5_EXPORT("qGzuTSD9cPQ", libkernel_sysUnknown_qGzuTSD9cPQ_nid_no_patch);

int APS5_VABI libkernel_sysUnknown_qHVs_plus_dxCumg_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}

APS5_EXPORT("qHVs+dxCumg", libkernel_sysUnknown_qHVs_plus_dxCumg_nid_no_patch);

int APS5_VABI libkernel_sysUnknown_qSleeVC6lwI_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}

APS5_EXPORT("qSleeVC6lwI", libkernel_sysUnknown_qSleeVC6lwI_nid_no_patch);

int APS5_VABI libkernel_sysUnknown_q_minus_2Uxlg_minus_faU_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}

APS5_EXPORT("q-2Uxlg-faU", libkernel_sysUnknown_q_minus_2Uxlg_minus_faU_nid_no_patch);

int APS5_VABI libkernel_sysUnknown_qe5JmcCj4zg_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}

APS5_EXPORT("qe5JmcCj4zg", libkernel_sysUnknown_qe5JmcCj4zg_nid_no_patch);

int APS5_VABI libkernel_sysUnknown_qtHOLtFWq7Y_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}

APS5_EXPORT("qtHOLtFWq7Y", libkernel_sysUnknown_qtHOLtFWq7Y_nid_no_patch);

int APS5_VABI libkernel_sysUnknown_r6qUkFmQFr0_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}

APS5_EXPORT("r6qUkFmQFr0", libkernel_sysUnknown_r6qUkFmQFr0_nid_no_patch);

int APS5_VABI libkernel_sysUnknown_rC7zr_minus_oAHCo_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}

APS5_EXPORT("rC7zr-oAHCo", libkernel_sysUnknown_rC7zr_minus_oAHCo_nid_no_patch);

int APS5_VABI libkernel_sysUnknown_rD4PS3f_plus_jKw_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}

APS5_EXPORT("rD4PS3f+jKw", libkernel_sysUnknown_rD4PS3f_plus_jKw_nid_no_patch);

int APS5_VABI libkernel_sysUnknown_rLBfsmdAuvo_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}

APS5_EXPORT("rLBfsmdAuvo", libkernel_sysUnknown_rLBfsmdAuvo_nid_no_patch);

int APS5_VABI sceKernelIccDisableButton() {
    NotImplemented_nid_no_patch(__func__);
}


int APS5_VABI libkernel_sysUnknown_sFW_plus_Ilmw_plus_58_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}

APS5_EXPORT("sFW+Ilmw+58", libkernel_sysUnknown_sFW_plus_Ilmw_plus_58_nid_no_patch);

int APS5_VABI libkernel_sysUnknown_tkZiGUoRJok_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}

APS5_EXPORT("tkZiGUoRJok", libkernel_sysUnknown_tkZiGUoRJok_nid_no_patch);

int APS5_VABI libkernel_sysUnknown_u9lxqerTKeA_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}

APS5_EXPORT("u9lxqerTKeA", libkernel_sysUnknown_u9lxqerTKeA_nid_no_patch);

int APS5_VABI libkernel_sysUnknown_uV_plus_YilcHqAs_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}

APS5_EXPORT("uV+YilcHqAs", libkernel_sysUnknown_uV_plus_YilcHqAs_nid_no_patch);

int APS5_VABI libkernel_sysUnknown_ulVjluL9TzM_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}

APS5_EXPORT("ulVjluL9TzM", libkernel_sysUnknown_ulVjluL9TzM_nid_no_patch);

int APS5_VABI libkernel_sysUnknown_v_plus_qTlNjF6kE_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}

APS5_EXPORT("v+qTlNjF6kE", libkernel_sysUnknown_v_plus_qTlNjF6kE_nid_no_patch);

int APS5_VABI libkernel_sysUnknown_vfYaxkxnJXQ_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}

APS5_EXPORT("vfYaxkxnJXQ", libkernel_sysUnknown_vfYaxkxnJXQ_nid_no_patch);

std::uint64_t APS5_VABI libkernel_sysUnknown_vq2_plus_HyP5U5I_nid_no_patch() {
    return 0;
}

APS5_EXPORT("vq2+HyP5U5I", libkernel_sysUnknown_vq2_plus_HyP5U5I_nid_no_patch);

int APS5_VABI sceKernelSetCanvasHandle() {
    NotImplemented_nid_no_patch(__func__);
}


int APS5_VABI libkernel_sysUnknown_wn0XzOue4EM_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}

APS5_EXPORT("wn0XzOue4EM", libkernel_sysUnknown_wn0XzOue4EM_nid_no_patch);

int APS5_VABI libkernel_sysUnknown_wwWv_plus_0MzhGM_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}

APS5_EXPORT("wwWv+0MzhGM", libkernel_sysUnknown_wwWv_plus_0MzhGM_nid_no_patch);

int APS5_VABI sceKernelDebugPauseWithDebugContext() {
    NotImplemented_nid_no_patch(__func__);
}


int APS5_VABI libkernel_sysUnknown_xpB6yv8NhhE_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}

APS5_EXPORT("xpB6yv8NhhE", libkernel_sysUnknown_xpB6yv8NhhE_nid_no_patch);

int APS5_VABI libkernel_sysUnknown_yWOJkfd_minus_DhA_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}

APS5_EXPORT("yWOJkfd-DhA", libkernel_sysUnknown_yWOJkfd_minus_DhA_nid_no_patch);

int APS5_VABI libkernel_sysUnknown_yn0b42DApAI_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}

APS5_EXPORT("yn0b42DApAI", libkernel_sysUnknown_yn0b42DApAI_nid_no_patch);

int APS5_VABI libkernel_sysUnknown_yzvrk3YrPlE_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}

APS5_EXPORT("yzvrk3YrPlE", libkernel_sysUnknown_yzvrk3YrPlE_nid_no_patch);

int APS5_VABI libkernel_sysUnknown_z0qKN5kkVj0_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}

APS5_EXPORT("z0qKN5kkVj0", libkernel_sysUnknown_z0qKN5kkVj0_nid_no_patch);

std::uint64_t APS5_VABI libkernel_sysUnknown_zAloppPJxy0_nid_no_patch() {
    return 0;
}

APS5_EXPORT("zAloppPJxy0", libkernel_sysUnknown_zAloppPJxy0_nid_no_patch);

std::uint64_t APS5_VABI libkernel_sysUnknown_z_plus_e0vV_minus_tnH4_nid_no_patch() {
    return 0;
}

APS5_EXPORT("z+e0vV-tnH4", libkernel_sysUnknown_z_plus_e0vV_minus_tnH4_nid_no_patch);

int APS5_VABI mmap2_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI pthread_setschedother_np_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceKernelAddDynlibEvent() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceKernelAddFGCInfo() {
    NotImplemented_nid_no_patch(__func__);
}


int APS5_VABI sceKernelCreateUuidV7() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceKernelDeleteDynlibEvent() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceKernelDeleteFGCInfo() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceKernelEnableDmemAliasing3() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceKernelExtendMaximumModuleNumber() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceKernelGetAioDebugRequestInfo() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceKernelGetAioDebugSubmitInfo() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceKernelGetAmprCounter() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceKernelGetAppCategoryTypeByAppId() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceKernelGetAppInfo2() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceKernelGetAppInfoListForMinisyscore() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceKernelGetCpuCoreClock() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceKernelGetGenuineProsperoSystemSwVersion(SceKernelProsperoSystemSwVersion* version) {
    return sceKernelGetProsperoSystemSwVersion_nid_postfix(version);
}

int APS5_VABI sceKernelGetNandInfo() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceKernelGetPackageInfo() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceKernelGetProsperoCompiledSdkVersion(std::int32_t* version) {
    if (version == nullptr) return SCE_KERNEL_ERROR_EINVAL;
    const int saved = errno;
    static const auto* parameters = static_cast<const std::byte*>(ApplicationProcessParameters_nid_no_patch());
    const bool copied = GuestCopyOut_nid_no_patch(version, parameters + 0x14, sizeof(*version));
    errno = saved;
    if (!copied) return SCE_KERNEL_ERROR_EFAULT;
    return 0;
}

int APS5_VABI sceKernelGetProsperoCompiledSdkVersionByPid(int pid, std::int32_t* version) {
    if (pid != getpid_nid_postfix() && pid != -1 && pid != 0) return SCE_KERNEL_ERROR_ESRCH;
    return sceKernelGetProsperoCompiledSdkVersion(version);
}

int APS5_VABI sceKernelGetPs4CompiledSdkVersion(std::int32_t* version) {
    if (version == nullptr) return SCE_KERNEL_ERROR_EINVAL;
    const int saved = errno;
    static const auto* parameters = static_cast<const std::byte*>(ApplicationProcessParameters_nid_no_patch());
    const bool copied = GuestCopyOut_nid_no_patch(version, parameters + 0x10, sizeof(*version));
    errno = saved;
    if (!copied) return SCE_KERNEL_ERROR_EFAULT;
    return 0;
}

int APS5_VABI sceKernelGetPs4CompiledSdkVersionByPid(int pid, std::int32_t* version) {
    if (pid != getpid_nid_postfix() && pid != -1 && pid != 0) return SCE_KERNEL_ERROR_ESRCH;
    return sceKernelGetPs4CompiledSdkVersion(version);
}

int APS5_VABI sceKernelGetResidentFmemCount2() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceKernelGetToolMemoryRange() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceKernelGetUpdatemode() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceKernelIccClearEmcCrashReport() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceKernelIccDisableExtBDDriveOperation() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceKernelIccGetBDDriveChuckingState() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceKernelIccGetBuzzerDimSetting() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceKernelIccGetBuzzerOffSetting() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceKernelIccGetDynamicLedDimSetting() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceKernelIccGetEmcBtDownload() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceKernelIccGetEmcCrashNumOfBlocks() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceKernelIccGetEmcCrashReport() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceKernelIccGetFloydVersion() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceKernelIccGetWlanBtGpio() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceKernelIccIsExtBDDriveAttached() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceKernelIccIsThermalAlert() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceKernelIccSetBootTimestamp() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceKernelIccSetBuzzerDimSetting() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceKernelIccSetBuzzerOffSetting() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceKernelIccSetDynamicLedDimSetting() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceKernelIccSetEmcBtDownload() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceKernelIccSetPriorityBuzzer() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceKernelIccSetPriorityBuzzerEx() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceKernelIccSetWlanBtGpio() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceKernelIccSetWlanBtManuTestMode() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceKernelIsAuthenticTrinity() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceKernelIsCloudDevkit() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceKernelIsCloudkit() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceKernelIsCronos() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceKernelIsEcoChargeAvailable() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceKernelIsSpmemAvailable() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceKernelKernelHeapGetCount() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceKernelKernelHeapUsage2() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceKernelMapFlexibleMemoryInternal() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceKernelMapFlexibleMemoryInternal2() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceKernelMapNamedFlexibleMemoryInternal2() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceKernelMapNamedSystemFlexibleMemory2() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceKernelMapperGetIndirectBufferInfo() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceKernelMapperGetUsageStatsData() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceKernelMapperSetPageTablePoolOccupancyNotificationThreshold() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceKernelPltTrap() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceKernelSetAcKeyRegResult() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceKernelSetAppInfoByAppId() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceKernelSetUpdatemodeDirect() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceKernelTraceAutoCaptureCreate() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceKernelTraceAutoCaptureDelete() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceKernelTraceAutoCaptureMap() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceKernelTraceAutoCaptureStart() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceKernelTraceAutoCaptureStop() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceKernelTraceAutoCaptureStore() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceKernelTraceAutoCaptureUnmap() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceKernelUseCronosKura() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceKernelUseCronosStreaming() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI scePthreadRegisterThreadDeathHook() {
    NotImplemented_nid_no_patch(__func__);
}
std::int64_t APS5_VABI _fpathconf_nid_no_patch(int fd, int name) {
    return _fpathconf_nid_postfix(fd, name);
}

int APS5_VABI _openat_nid_no_patch(int fd, const char* path, int flags, std::uint64_t v0, std::uint64_t v1) {
    return _openat_nid_postfix(fd, path, flags, v0, v1);
}

int APS5_VABI amd64_set_fsbase_nid_no_patch(void) {
    return amd64_set_fsbase_nid_postfix();
}

int APS5_VABI blockpool_batch_nid_no_patch(void) {
    return blockpool_batch_nid_postfix();
}

int APS5_VABI blockpool_map_nid_no_patch(void) {
    return blockpool_map_nid_postfix();
}

int APS5_VABI blockpool_open_nid_no_patch(void) {
    return blockpool_open_nid_postfix();
}

int APS5_VABI blockpool_unmap_nid_no_patch(void) {
    return blockpool_unmap_nid_postfix();
}

int APS5_VABI dynlib_get_obj_member_nid_no_patch(void) {
    return dynlib_get_obj_member_nid_postfix();
}

int APS5_VABI get_authinfo_nid_no_patch(int pid, KernelAuthInfo* info) {
    return get_authinfo_nid_postfix(pid, info);
}

int APS5_VABI get_module_info_list_nid_no_patch(void) {
    return get_module_info_list_nid_postfix();
}

int APS5_VABI get_page_table_stats_nid_no_patch(void) {
    return get_page_table_stats_nid_postfix();
}

int APS5_VABI get_sdk_compiled_version_nid_no_patch(void) {
    return get_sdk_compiled_version_nid_postfix();
}

int APS5_VABI get_self_auth_info_nid_no_patch(KernelAuthInfo* info) {
    return get_self_auth_info_nid_postfix(info);
}

int APS5_VABI get_vm_map_timestamp_nid_no_patch(void) {
    return get_vm_map_timestamp_nid_postfix();
}

int APS5_VABI ipmimgr_call_nid_no_patch(void) {
    return ipmimgr_call_nid_postfix();
}

int APS5_VABI is_in_sandbox_nid_no_patch(void) {
    return is_in_sandbox_nid_postfix();
}

int APS5_VABI ksem_close_nid_no_patch(void) {
    return ksem_close_nid_postfix();
}

int APS5_VABI ksem_open_nid_no_patch(void) {
    return ksem_open_nid_postfix();
}

int APS5_VABI ksem_unlink_nid_no_patch(void) {
    return ksem_unlink_nid_postfix();
}

int APS5_VABI mdbg_service_nid_no_patch(void) {
    return mdbg_service_nid_postfix();
}

int APS5_VABI openintr_nid_no_patch(void) {
    return openintr_nid_postfix();
}

int APS5_VABI profil_nid_no_patch(void* samples, std::size_t size, std::size_t offset, unsigned int scale) {
    return profil_nid_postfix(samples, size, offset, scale);
}

int APS5_VABI pthread_attr_getaffinity_np_nid_no_patch(const PthreadAttr* attr, std::size_t cpusetSize, void* cpuset) {
    return pthread_attr_getaffinity_np_nid_postfix(attr, cpusetSize, cpuset);
}

int APS5_VABI pthread_attr_getsolosched_np_nid_no_patch(const PthreadAttr* attr, int* solosched) {
    return pthread_attr_getsolosched_np_nid_postfix(attr, solosched);
}

int APS5_VABI pthread_attr_setaffinity_np_nid_no_patch(PthreadAttr* attr, std::size_t cpusetSize, const void* cpuset) {
    return pthread_attr_setaffinity_np_nid_postfix(attr, cpusetSize, cpuset);
}

int APS5_VABI pthread_attr_setcreatesuspend_np_nid_no_patch(PthreadAttr* attr) {
    return pthread_attr_setcreatesuspend_np_nid_postfix(attr);
}

int APS5_VABI pthread_attr_setstackaddr_nid_no_patch(PthreadAttr* attr, void* stack_addr) {
    return pthread_attr_setstackaddr_nid_postfix(attr, stack_addr);
}

int APS5_VABI pthread_get_specificarray_np_nid_no_patch(void) {
    return pthread_get_specificarray_np_nid_postfix();
}

int APS5_VABI pthread_get_user_context_np_nid_no_patch(void) {
    return pthread_get_user_context_np_nid_postfix();
}

int APS5_VABI pthread_getstack_np_nid_no_patch(void) {
    return pthread_getstack_np_nid_postfix();
}

int APS5_VABI pthread_main_np_nid_no_patch(void) {
    return pthread_main_np_nid_postfix();
}

int APS5_VABI pthread_multi_np_nid_no_patch(void) {
    return pthread_multi_np_nid_postfix();
}

int APS5_VABI pthread_mutex_init_for_mono_nid_no_patch(void) {
    return pthread_mutex_init_for_mono_nid_postfix();
}

int APS5_VABI pthread_mutexattr_getgen_np_nid_no_patch(const PthreadMutexattr* attr, int* generation) {
    return pthread_mutexattr_getgen_np_nid_postfix(attr, generation);
}

int APS5_VABI pthread_mutexattr_setgen_np_nid_no_patch(PthreadMutexattr* attr, int generation) {
    return pthread_mutexattr_setgen_np_nid_postfix(attr, generation);
}

void APS5_VABI pthread_resume_all_np_nid_no_patch(void) {
    pthread_resume_all_np_nid_postfix();
}

int APS5_VABI pthread_resume_np_nid_no_patch(Pthread thread) {
    return pthread_resume_np_nid_postfix(thread);
}

int APS5_VABI pthread_resume_user_context_np_nid_no_patch(void) {
    return pthread_resume_user_context_np_nid_postfix();
}

int APS5_VABI pthread_set_defaultstacksize_np_nid_no_patch(std::size_t size) {
    return pthread_set_defaultstacksize_np_nid_postfix(size);
}

int APS5_VABI pthread_set_user_context_np_nid_no_patch(void) {
    return pthread_set_user_context_np_nid_postfix();
}

int APS5_VABI pthread_single_np_nid_no_patch(void) {
    return pthread_single_np_nid_postfix();
}

int APS5_VABI pthread_suspend_all_np_nid_no_patch(void) {
    return pthread_suspend_all_np_nid_postfix();
}

int APS5_VABI pthread_suspend_np_nid_no_patch(void) {
    return pthread_suspend_np_nid_postfix();
}

int APS5_VABI pthread_suspend_user_context_np_nid_no_patch(void) {
    return pthread_suspend_user_context_np_nid_postfix();
}

int APS5_VABI pthread_switch_add_np_nid_no_patch(void) {
    return pthread_switch_add_np_nid_postfix();
}

int APS5_VABI pthread_switch_delete_np_nid_no_patch(void) {
    return pthread_switch_delete_np_nid_postfix();
}

int APS5_VABI rtld_printf_nid_no_patch(void) {
    return rtld_printf_nid_postfix();
}

int APS5_VABI sched_rr_get_interval_nid_no_patch(int pid, KernelTimespec* interval) {
    return sched_rr_get_interval_nid_postfix(pid, interval);
}

int APS5_VABI sched_setscheduler_nid_no_patch(int pid, int policy, const KernelSchedParam* param) {
    return sched_setscheduler_nid_postfix(pid, policy, param);
}

int APS5_VABI set_phys_fmem_limit_nid_no_patch(void) {
    return set_phys_fmem_limit_nid_postfix();
}

int APS5_VABI settimeofday_nid_no_patch(const KernelTimeval* tv, const KernelTimezone* tz) {
    return settimeofday_nid_postfix(tv, tz);
}

int APS5_VABI sigqueue_nid_no_patch(void) {
    return sigqueue_nid_postfix();
}

int APS5_VABI sigreturn_nid_no_patch(const UserUcontext* context) {
    return sigreturn_nid_postfix(context);
}

int APS5_VABI sysc_e00_nid_no_patch(void) {
    return sysc_e00_nid_postfix();
}

int APS5_VABI sysc_m00_nid_no_patch(void) {
    return sysc_m00_nid_postfix();
}

int APS5_VABI sysc_s00_nid_no_patch(void) {
    return sysc_s00_nid_postfix();
}

int APS5_VABI tcsetsid_nid_no_patch(int fd, int pid) {
    return tcsetsid_nid_postfix(fd, pid);
}

int APS5_VABI uuidgen_nid_no_patch(GuestUuid* store, int count) {
    return uuidgen_nid_postfix(store, count);
}

int APS5_VABI wait3_nid_no_patch(void) {
    return wait3_nid_postfix(nullptr, 0, nullptr);
}


int APS5_VABI pthread_setschedother_np_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}






int APS5_VABI sceCoredumpDebugForceCoredumpOnAppClose_nid_no_patch(void) {
    return sceCoredumpDebugForceCoredumpOnAppClose();
}

int APS5_VABI sceCoredumpDebugTriggerCoredump_nid_no_patch(void) {
    return sceCoredumpDebugTriggerCoredump();
}

int APS5_VABI sceCoredumpGetStopInfoGpu_nid_no_patch(void) {
    return sceCoredumpGetStopInfoGpu();
}

int APS5_VABI sceCoredumpInternalGetThreadContextInfo_nid_no_patch(void) {
    return sceCoredumpInternalGetThreadContextInfo();
}

int APS5_VABI sceKernelAddCpumodeEvent_nid_no_patch(void) {
    return sceKernelAddCpumodeEvent();
}

int APS5_VABI sceKernelAddExternalTriggerEvent_nid_no_patch(void) {
    return sceKernelAddExternalTriggerEvent();
}

int APS5_VABI sceKernelAddGpuExceptionEvent_nid_no_patch(void) {
    return sceKernelAddGpuExceptionEvent();
}

int APS5_VABI sceKernelAddProcessToCanvasMap_nid_no_patch(void) {
    return sceKernelAddProcessToCanvasMap();
}

int APS5_VABI sceKernelAllocateDirectMemory2_nid_no_patch(void) {
    return sceKernelAllocateDirectMemory2();
}

int APS5_VABI sceKernelAllocateDirectMemoryForMiniApp_nid_no_patch(void) {
    return sceKernelAllocateDirectMemoryForMiniApp();
}

int APS5_VABI sceKernelAllocateToolMemory_nid_no_patch(void) {
    return sceKernelAllocateToolMemory();
}

int APS5_VABI sceKernelAllocateTraceDirectMemory_nid_no_patch(void) {
    return sceKernelAllocateTraceDirectMemory();
}

int APS5_VABI sceKernelAvailableToolMemorySize_nid_no_patch(void) {
    return sceKernelAvailableToolMemorySize();
}

int APS5_VABI sceKernelBacktraceSelf_nid_no_patch(void) {
    return sceKernelBacktraceSelf();
}

int APS5_VABI sceKernelCallIndirectBuffer_nid_no_patch(std::uint32_t priority, void* base, std::uint64_t bytes) {
    return sceKernelCallIndirectBuffer(priority, base, bytes);
}

int APS5_VABI sceKernelCallIndirectBuffer2_nid_no_patch(std::uint32_t priority, void* base, std::uint64_t bytes, KernelMapperSubmitResult* result, std::uint32_t* id) {
    return sceKernelCallIndirectBuffer2(priority, base, bytes, result, id);
}

int APS5_VABI sceKernelCallIndirectBuffer3_nid_no_patch(std::uint32_t priority, void* base, std::uint64_t bytes, std::uint32_t* id) {
    return sceKernelCallIndirectBuffer3(priority, base, bytes, id);
}

int APS5_VABI sceKernelCheckProsperoSdkVersion_nid_no_patch(void) {
    return sceKernelCheckProsperoSdkVersion();
}

int APS5_VABI sceKernelClearBootReqNotifyCount_nid_no_patch(void) {
    return sceKernelClearBootReqNotifyCount();
}

int APS5_VABI sceKernelClearGameDirectMemory_nid_no_patch(void) {
    return sceKernelClearGameDirectMemory();
}

int APS5_VABI sceKernelCloseEport_nid_no_patch(void) {
    return sceKernelCloseEport();
}

int APS5_VABI sceKernelCloseSema_nid_no_patch(void) {
    return sceKernelCloseSema();
}

int APS5_VABI sceKernelCreateEport_nid_no_patch(void) {
    return sceKernelCreateEport();
}

int APS5_VABI sceKernelDebugAcquireAndUpdateDebugRegister_nid_no_patch(void) {
    return sceKernelDebugAcquireAndUpdateDebugRegister();
}

int APS5_VABI sceKernelDebugGetAppStatus_nid_no_patch(void) {
    return sceKernelDebugGetAppStatus();
}

int APS5_VABI sceKernelDebugGetPauseCount_nid_no_patch(void) {
    return sceKernelDebugGetPauseCount();
}

int APS5_VABI sceKernelDebugGpuPaDebugIsInProgress_nid_no_patch(void) {
    return sceKernelDebugGpuPaDebugIsInProgress();
}

int APS5_VABI sceKernelDebugPauseWithDebugContext_nid_no_patch(void) {
    return sceKernelDebugPauseWithDebugContext();
}

int APS5_VABI sceKernelDebugRaiseExceptionWithContext_nid_no_patch(void) {
    return sceKernelDebugRaiseExceptionWithContext();
}

int APS5_VABI sceKernelDebugRaiseExceptionWithInfo_nid_no_patch(void) {
    return sceKernelDebugRaiseExceptionWithInfo();
}

int APS5_VABI sceKernelDebugReleaseDebugContext_nid_no_patch(void) {
    return sceKernelDebugReleaseDebugContext();
}

int APS5_VABI sceKernelDeleteCpumodeEvent_nid_no_patch(void) {
    return sceKernelDeleteCpumodeEvent();
}

int APS5_VABI sceKernelDeleteEport_nid_no_patch(void) {
    return sceKernelDeleteEport();
}

int APS5_VABI sceKernelDeleteExternalTriggerEvent_nid_no_patch(void) {
    return sceKernelDeleteExternalTriggerEvent();
}

int APS5_VABI sceKernelDeleteGpuExceptionEvent_nid_no_patch(void) {
    return sceKernelDeleteGpuExceptionEvent();
}

int APS5_VABI sceKernelDeleteProcessFromCanvasMap_nid_no_patch(void) {
    return sceKernelDeleteProcessFromCanvasMap();
}

int APS5_VABI sceKernelDirectMemoryQueryForId_nid_no_patch(void) {
    return sceKernelDirectMemoryQueryForId();
}

int APS5_VABI sceKernelEnableDmemAliasing2_nid_no_patch(void) {
    return sceKernelEnableDmemAliasing2();
}

int APS5_VABI sceKernelEnablePthreadObjectCheck_nid_no_patch(void) {
    return sceKernelEnablePthreadObjectCheck();
}

int APS5_VABI sceKernelEventLogInit_nid_no_patch(void) {
    return sceKernelEventLogInit();
}

int APS5_VABI sceKernelEventLogPread_nid_no_patch(void) {
    return sceKernelEventLogPread();
}

int APS5_VABI sceKernelEventLogRead_nid_no_patch(void) {
    return sceKernelEventLogRead();
}

int APS5_VABI sceKernelEventLogWrite_nid_no_patch(void) {
    return sceKernelEventLogWrite();
}

int APS5_VABI sceKernelExtendMaximumModuleNumber_nid_no_patch(void) {
    return sceKernelExtendMaximumModuleNumber();
}

int APS5_VABI sceKernelFlock_nid_no_patch(void) {
    return sceKernelFlock();
}

int APS5_VABI sceKernelGetAmprCounter_nid_no_patch(void) {
    return sceKernelGetAmprCounter();
}

int APS5_VABI sceKernelGetAppCategoryType_nid_no_patch(int pid, std::uint32_t* category) {
    return sceKernelGetAppCategoryType(pid, category);
}

int APS5_VABI sceKernelGetAppLibcData_nid_no_patch(void) {
    return sceKernelGetAppLibcData();
}

int APS5_VABI sceKernelGetAslrStatus_nid_no_patch(void) {
    return sceKernelGetAslrStatus();
}

int APS5_VABI sceKernelGetAutoUpdateVersion_nid_no_patch(void) {
    return sceKernelGetAutoUpdateVersion();
}

int APS5_VABI sceKernelGetBackupRestoreMode_nid_no_patch(void) {
    return sceKernelGetBackupRestoreMode();
}

int APS5_VABI sceKernelGetBackupRestoreModeOfNextBoot_nid_no_patch(void) {
    return sceKernelGetBackupRestoreModeOfNextBoot();
}

int APS5_VABI sceKernelGetBasicProductShape_nid_no_patch(void) {
    return sceKernelGetBasicProductShape();
}

int APS5_VABI sceKernelGetBetaUpdateTestForRcmgr_nid_no_patch(void) {
    return sceKernelGetBetaUpdateTestForRcmgr();
}

int APS5_VABI sceKernelGetBioUsageAll_nid_no_patch(void) {
    return sceKernelGetBioUsageAll();
}

int APS5_VABI sceKernelGetBootReqNotifyCount_nid_no_patch(void) {
    return sceKernelGetBootReqNotifyCount();
}

int APS5_VABI sceKernelGetBuildNumber_nid_no_patch(void) {
    return sceKernelGetBuildNumber();
}

int APS5_VABI sceKernelGetCallRecord_nid_no_patch(void) {
    return sceKernelGetCallRecord();
}

int APS5_VABI sceKernelGetCanvasHandle_nid_no_patch(void) {
    return sceKernelGetCanvasHandle();
}

int APS5_VABI sceKernelGetCanvasHandleByPid_nid_no_patch(void) {
    return sceKernelGetCanvasHandleByPid();
}

int APS5_VABI sceKernelGetCompiledSdkVersion_nid_no_patch(std::int32_t* version) {
    return sceKernelGetCompiledSdkVersion(version);
}

int APS5_VABI sceKernelGetCompiledSdkVersionByPath_nid_no_patch(void) {
    return sceKernelGetCompiledSdkVersionByPath();
}

int APS5_VABI sceKernelGetCompiledSdkVersionByPid_nid_no_patch(int pid, std::int32_t* version) {
    return sceKernelGetCompiledSdkVersionByPid(pid, version);
}

int APS5_VABI sceKernelGetCpuCoreClock_nid_no_patch(void) {
    return sceKernelGetCpuCoreClock();
}

int APS5_VABI sceKernelGetCpuUsage_nid_no_patch(void* stats, int32_t* size) {
    return sceKernelGetCpuUsage(stats, size);
}

int APS5_VABI sceKernelGetCpuUsageAll_nid_no_patch(void) {
    return sceKernelGetCpuUsageAll();
}

int APS5_VABI sceKernelGetCpuUsageProc_nid_no_patch(void) {
    return sceKernelGetCpuUsageProc();
}

int APS5_VABI sceKernelGetCpuUsageProc2_nid_no_patch(void) {
    return sceKernelGetCpuUsageProc2();
}

int APS5_VABI sceKernelGetCpuUsageThread_nid_no_patch(void) {
    return sceKernelGetCpuUsageThread();
}

int APS5_VABI sceKernelGetCpumodeGame_nid_no_patch(int* mode) {
    return sceKernelGetCpumodeGame(mode);
}

int APS5_VABI sceKernelGetDataTransferMode_nid_no_patch(void) {
    return sceKernelGetDataTransferMode();
}

int APS5_VABI sceKernelGetDebugMenuMiniModeForRcmgr_nid_no_patch(void) {
    return sceKernelGetDebugMenuMiniModeForRcmgr();
}

int APS5_VABI sceKernelGetDebugMenuModeForPsmForRcmgr_nid_no_patch(void) {
    return sceKernelGetDebugMenuModeForPsmForRcmgr();
}

int APS5_VABI sceKernelGetDebugMenuModeForRcmgr_nid_no_patch(void) {
    return sceKernelGetDebugMenuModeForRcmgr();
}

int APS5_VABI sceKernelGetDefaultToolMemorySize_nid_no_patch(void) {
    return sceKernelGetDefaultToolMemorySize();
}

int APS5_VABI sceKernelGetExtLibcHandle_nid_no_patch(void) {
    return sceKernelGetExtLibcHandle();
}

int APS5_VABI sceKernelGetFakeFinalizeMenuForRcmgr_nid_no_patch(void) {
    return sceKernelGetFakeFinalizeMenuForRcmgr();
}

int APS5_VABI sceKernelGetFlagedUpdaterForRcmgr_nid_no_patch(void) {
    return sceKernelGetFlagedUpdaterForRcmgr();
}

int APS5_VABI sceKernelGetForceUpdateModeForRcmgr_nid_no_patch(void) {
    return sceKernelGetForceUpdateModeForRcmgr();
}

int APS5_VABI sceKernelGetGPO_nid_no_patch(void) {
    return sceKernelGetGPO();
}

int APS5_VABI sceKernelGetGenuineProsperoSystemSwVersion_nid_no_patch(void) {
    return sceKernelGetGenuineProsperoSystemSwVersion();
}

int APS5_VABI sceKernelGetHwFeatureInfoForDecid_nid_no_patch(void) {
    return sceKernelGetHwFeatureInfoForDecid();
}

int APS5_VABI sceKernelGetIdPs_nid_no_patch(void) {
    return sceKernelGetIdPs();
}

int APS5_VABI sceKernelGetIdTableCurrentCount_nid_no_patch(void) {
    return sceKernelGetIdTableCurrentCount();
}

int APS5_VABI sceKernelGetIpcPath_nid_no_patch(void) {
    return sceKernelGetIpcPath();
}

int APS5_VABI sceKernelGetKnobs_nid_no_patch(void) {
    return sceKernelGetKnobs();
}

int APS5_VABI sceKernelGetModuleInfoInternal_nid_no_patch(KernelModule handle, ModuleInfoEx* info) {
    return sceKernelGetModuleInfoInternal(handle, info);
}

int APS5_VABI sceKernelGetModuleListInternal_nid_no_patch(KernelModule* handles, std::size_t count, std::size_t* actual) {
    return sceKernelGetModuleListInternal(handles, count, actual);
}

int APS5_VABI sceKernelGetOpenPsIdForSystem_nid_no_patch(void* id) {
    return sceKernelGetOpenPsIdForSystem(id);
}

int APS5_VABI sceKernelGetPagingStatsOfAllObjects_nid_no_patch(void) {
    return sceKernelGetPagingStatsOfAllObjects();
}

int APS5_VABI sceKernelGetPagingStatsOfAllThreads_nid_no_patch(void) {
    return sceKernelGetPagingStatsOfAllThreads();
}

int APS5_VABI sceKernelGetPhysPageSize_nid_no_patch(void) {
    return sceKernelGetPhysPageSize();
}

int APS5_VABI sceKernelGetProcessName_nid_no_patch(void) {
    return sceKernelGetProcessName();
}

int APS5_VABI sceKernelGetProcessType_nid_no_patch(int pid) {
    return sceKernelGetProcessType(pid);
}

int APS5_VABI sceKernelGetProductCode_nid_no_patch(void) {
    return sceKernelGetProductCode();
}

int APS5_VABI sceKernelGetProductStr_nid_no_patch(void) {
    return sceKernelGetProductStr();
}

int APS5_VABI sceKernelGetProsperoCompiledSdkVersion_nid_no_patch(std::int32_t* version) {
    return sceKernelGetProsperoCompiledSdkVersion(version);
}

int APS5_VABI sceKernelGetProsperoCompiledSdkVersionByPid_nid_no_patch(int pid, std::int32_t* version) {
    return sceKernelGetProsperoCompiledSdkVersionByPid(pid, version);
}

int APS5_VABI sceKernelGetPs4CompiledSdkVersion_nid_no_patch(std::int32_t* version) {
    return sceKernelGetPs4CompiledSdkVersion(version);
}

int APS5_VABI sceKernelGetPs4CompiledSdkVersionByPid_nid_no_patch(int pid, std::int32_t* version) {
    return sceKernelGetPs4CompiledSdkVersionByPid(pid, version);
}

int APS5_VABI sceKernelGetPs4SystemSwVersion_nid_no_patch(void) {
    return sceKernelGetPs4SystemSwVersion();
}

int APS5_VABI sceKernelGetPsmIntdevModeForRcmgr_nid_no_patch(void) {
    return sceKernelGetPsmIntdevModeForRcmgr();
}

int APS5_VABI sceKernelGetPsnAccessTraceLogForRcmgr_nid_no_patch(void) {
    return sceKernelGetPsnAccessTraceLogForRcmgr();
}

int APS5_VABI sceKernelGetQafExpirationTimeNotafterForRcmgr_nid_no_patch(void) {
    return sceKernelGetQafExpirationTimeNotafterForRcmgr();
}

int APS5_VABI sceKernelGetQafExpirationTimeNotbeforeForRcmgr_nid_no_patch(void) {
    return sceKernelGetQafExpirationTimeNotbeforeForRcmgr();
}

int APS5_VABI sceKernelGetQafGenerationForRcmgr_nid_no_patch(void) {
    return sceKernelGetQafGenerationForRcmgr();
}

int APS5_VABI sceKernelGetQafNameForRcmgr_nid_no_patch(void) {
    return sceKernelGetQafNameForRcmgr();
}

int APS5_VABI sceKernelGetRenderingMode_nid_no_patch(void) {
    return sceKernelGetRenderingMode();
}

int APS5_VABI sceKernelGetResidentCount_nid_no_patch(void) {
    return sceKernelGetResidentCount();
}

int APS5_VABI sceKernelGetSafemode_nid_no_patch(void) {
    return sceKernelGetSafemode();
}

int APS5_VABI sceKernelGetSanitizerMallocReplace_nid_no_patch(void) {
    return sceKernelGetSanitizerMallocReplace();
}

int APS5_VABI sceKernelGetSanitizerNewReplace_nid_no_patch(void) {
    return sceKernelGetSanitizerNewReplace();
}

int APS5_VABI sceKernelGetSocClock_nid_no_patch(void) {
    return sceKernelGetSocClock();
}

int APS5_VABI sceKernelGetSpecialIForRcmgr_nid_no_patch(void) {
    return sceKernelGetSpecialIForRcmgr();
}

int APS5_VABI sceKernelGetSubsysId_nid_no_patch(void) {
    return sceKernelGetSubsysId();
}

int APS5_VABI sceKernelGetSubsysSubId_nid_no_patch(void) {
    return sceKernelGetSubsysSubId();
}

int APS5_VABI sceKernelGetSystemLevelDebuggerModeForRcmgr_nid_no_patch(void) {
    return sceKernelGetSystemLevelDebuggerModeForRcmgr();
}

int APS5_VABI sceKernelGetSystemSwBeta_nid_no_patch(void) {
    return sceKernelGetSystemSwBeta();
}

int APS5_VABI sceKernelGetThreadName_nid_no_patch(uint32_t id, char* name) {
    return sceKernelGetThreadName(id, name);
}

int APS5_VABI sceKernelGetToolMemoryRange_nid_no_patch(void) {
    return sceKernelGetToolMemoryRange();
}

int APS5_VABI sceKernelGetUniversalMode_nid_no_patch(void) {
    return sceKernelGetUniversalMode();
}

int APS5_VABI sceKernelGetUpdVersion_nid_no_patch(std::uint32_t* version) {
    return sceKernelGetUpdVersion(version);
}

int APS5_VABI sceKernelGetUtokenDataExecutionForRcmgr_nid_no_patch(void) {
    return sceKernelGetUtokenDataExecutionForRcmgr();
}

int APS5_VABI sceKernelGetUtokenExpirationTimeNotafterForRcmgr_nid_no_patch(void) {
    return sceKernelGetUtokenExpirationTimeNotafterForRcmgr();
}

int APS5_VABI sceKernelGetUtokenExpirationTimeNotbeforeForRcmgr_nid_no_patch(void) {
    return sceKernelGetUtokenExpirationTimeNotbeforeForRcmgr();
}

int APS5_VABI sceKernelGetUtokenFakeSharefactoryForRcmgr_nid_no_patch(void) {
    return sceKernelGetUtokenFakeSharefactoryForRcmgr();
}

int APS5_VABI sceKernelGetUtokenFlagedUpdaterForRcmgr_nid_no_patch(void) {
    return sceKernelGetUtokenFlagedUpdaterForRcmgr();
}

int APS5_VABI sceKernelGetUtokenNpEnvSwitchingForRcmgr_nid_no_patch(void) {
    return sceKernelGetUtokenNpEnvSwitchingForRcmgr();
}

int APS5_VABI sceKernelGetUtokenSaveDataRepairForRcmgr_nid_no_patch(void) {
    return sceKernelGetUtokenSaveDataRepairForRcmgr();
}

int APS5_VABI sceKernelGetUtokenStoreModeForRcmgr_nid_no_patch(void) {
    return sceKernelGetUtokenStoreModeForRcmgr();
}

int APS5_VABI sceKernelGetUtokenUseSoftwagnerForAcmgr_nid_no_patch(void) {
    return sceKernelGetUtokenUseSoftwagnerForAcmgr();
}

int APS5_VABI sceKernelGetUtokenUseSoftwagnerForRcmgr_nid_no_patch(void) {
    return sceKernelGetUtokenUseSoftwagnerForRcmgr();
}

int APS5_VABI sceKernelGetUtokenWeakenedPortRestrictionForRcmgr_nid_no_patch(void) {
    return sceKernelGetUtokenWeakenedPortRestrictionForRcmgr();
}

int APS5_VABI sceKernelGetVrCaptureSize_nid_no_patch(void) {
    return sceKernelGetVrCaptureSize();
}

int APS5_VABI sceKernelGiveDirectMemoryToMapper_nid_no_patch(std::int64_t searchStart, std::int64_t searchEnd, std::size_t size, std::size_t alignment, int usage, std::int64_t* offset) {
    return sceKernelGiveDirectMemoryToMapper(searchStart, searchEnd, size, alignment, usage, offset);
}

int APS5_VABI sceKernelHasNeoMode_nid_no_patch(void) {
    return sceKernelHasNeoMode();
}

int APS5_VABI sceKernelHwGetBdDriveInfo_nid_no_patch(void) {
    return sceKernelHwGetBdDriveInfo();
}

int APS5_VABI sceKernelHwHasOpticalOut_nid_no_patch(void) {
    return sceKernelHwHasOpticalOut();
}

int APS5_VABI sceKernelHwHasWlanBt_nid_no_patch(void) {
    return sceKernelHwHasWlanBt();
}

int APS5_VABI sceKernelIccControlBDPowerState_nid_no_patch(void) {
    return sceKernelIccControlBDPowerState();
}

int APS5_VABI sceKernelIccControlUSBPowerState_nid_no_patch(void) {
    return sceKernelIccControlUSBPowerState();
}

int APS5_VABI sceKernelIccGetBDDriveChuckingState_nid_no_patch(void) {
    return sceKernelIccGetBDDriveChuckingState();
}

int APS5_VABI sceKernelIccGetBDPowerState_nid_no_patch(void) {
    return sceKernelIccGetBDPowerState();
}

int APS5_VABI sceKernelIccGetCPMode_nid_no_patch(void) {
    return sceKernelIccGetCPMode();
}

int APS5_VABI sceKernelIccGetCountTime_nid_no_patch(void) {
    return sceKernelIccGetCountTime();
}

int APS5_VABI sceKernelIccGetCpuInfoBit_nid_no_patch(void) {
    return sceKernelIccGetCpuInfoBit();
}

int APS5_VABI sceKernelIccGetErrLog_nid_no_patch(void) {
    return sceKernelIccGetErrLog();
}

int APS5_VABI sceKernelIccGetHwInfo_nid_no_patch(void) {
    return sceKernelIccGetHwInfo();
}

int APS5_VABI sceKernelIccGetPowerNumberOfBootShutdown_nid_no_patch(void) {
    return sceKernelIccGetPowerNumberOfBootShutdown();
}

int APS5_VABI sceKernelIccGetPowerOperatingTime_nid_no_patch(void) {
    return sceKernelIccGetPowerOperatingTime();
}

int APS5_VABI sceKernelIccGetPowerUpCause_nid_no_patch(void) {
    return sceKernelIccGetPowerUpCause();
}

int APS5_VABI sceKernelIccGetSysEventLog_nid_no_patch(void) {
    return sceKernelIccGetSysEventLog();
}

int APS5_VABI sceKernelIccGetUSBPowerState_nid_no_patch(void) {
    return sceKernelIccGetUSBPowerState();
}

int APS5_VABI sceKernelIccIndicatorBootDone_nid_no_patch(void) {
    return sceKernelIccIndicatorBootDone();
}

int APS5_VABI sceKernelIccIndicatorShutdown_nid_no_patch(void) {
    return sceKernelIccIndicatorShutdown();
}

int APS5_VABI sceKernelIccIndicatorStandby_nid_no_patch(void) {
    return sceKernelIccIndicatorStandby();
}

int APS5_VABI sceKernelIccIndicatorStandbyBoot_nid_no_patch(void) {
    return sceKernelIccIndicatorStandbyBoot();
}

int APS5_VABI sceKernelIccIndicatorStandbyShutdown_nid_no_patch(void) {
    return sceKernelIccIndicatorStandbyShutdown();
}

int APS5_VABI sceKernelIccNotifyBootStatus_nid_no_patch(void) {
    return sceKernelIccNotifyBootStatus();
}

int APS5_VABI sceKernelIccNvsFlush_nid_no_patch(void) {
    return sceKernelIccNvsFlush();
}

int APS5_VABI sceKernelIccReadPowerBootMessage_nid_no_patch(void) {
    return sceKernelIccReadPowerBootMessage();
}

int APS5_VABI sceKernelIccSetBuzzer_nid_no_patch(void) {
    return sceKernelIccSetBuzzer();
}

int APS5_VABI sceKernelIccSetCPMode_nid_no_patch(void) {
    return sceKernelIccSetCPMode();
}

int APS5_VABI sceKernelIccSetCpuInfoBit_nid_no_patch(void) {
    return sceKernelIccSetCpuInfoBit();
}

int APS5_VABI sceKernelIccSetDownloadMode_nid_no_patch(void) {
    return sceKernelIccSetDownloadMode();
}

int APS5_VABI sceKernelIccSetPriorityBuzzer_nid_no_patch(void) {
    return sceKernelIccSetPriorityBuzzer();
}

int APS5_VABI sceKernelInternalCreateMapperSharedMemory_nid_no_patch(void) {
    return sceKernelInternalCreateMapperSharedMemory();
}

int APS5_VABI sceKernelInternalGetKmemStatistics_nid_no_patch(void) {
    return sceKernelInternalGetKmemStatistics();
}

int APS5_VABI sceKernelInternalGetMapStatistics_nid_no_patch(void) {
    return sceKernelInternalGetMapStatistics();
}

int APS5_VABI sceKernelInternalHeapPrintBacktraceWithModuleInfo_nid_no_patch(void) {
    return sceKernelInternalHeapPrintBacktraceWithModuleInfo();
}

int APS5_VABI sceKernelInternalLockMapperMemory_nid_no_patch(void) {
    return sceKernelInternalLockMapperMemory();
}

int APS5_VABI sceKernelInternalMapDirectMemory_nid_no_patch(void) {
    return sceKernelInternalMapDirectMemory();
}

int APS5_VABI sceKernelInternalMapNamedDirectMemory_nid_no_patch(void) {
    return sceKernelInternalMapNamedDirectMemory();
}

int APS5_VABI sceKernelInternalResumeDirectMemoryRelease_nid_no_patch(void) {
    return sceKernelInternalResumeDirectMemoryRelease();
}

int APS5_VABI sceKernelInternalSuspendDirectMemoryRelease_nid_no_patch(void) {
    return sceKernelInternalSuspendDirectMemoryRelease();
}

int APS5_VABI sceKernelInternalUnlockMapperMemory_nid_no_patch(void) {
    return sceKernelInternalUnlockMapperMemory();
}

int APS5_VABI sceKernelInternalUnmapMapperSharedMemory_nid_no_patch(void) {
    return sceKernelInternalUnmapMapperSharedMemory();
}

int APS5_VABI sceKernelIsAllowedToSelectDvdRegion_nid_no_patch(void) {
    return sceKernelIsAllowedToSelectDvdRegion();
}

int APS5_VABI sceKernelIsAuthenticNeo_nid_no_patch(void) {
    return sceKernelIsAuthenticNeo();
}

int APS5_VABI sceKernelIsCEX_nid_no_patch(void) {
    return sceKernelIsCEX();
}

int APS5_VABI sceKernelIsCronos_nid_no_patch(void) {
    return sceKernelIsCronos();
}

int APS5_VABI sceKernelIsDebuggerAttached_nid_no_patch(void) {
    return sceKernelIsDebuggerAttached();
}

int APS5_VABI sceKernelIsDevKit_nid_no_patch(void) {
    return sceKernelIsDevKit();
}

int APS5_VABI sceKernelIsExperimentalBeta_nid_no_patch(void) {
    return sceKernelIsExperimentalBeta();
}

int APS5_VABI sceKernelIsGenuineCEX_nid_no_patch(void) {
    return sceKernelIsGenuineCEX();
}

int APS5_VABI sceKernelIsGenuineCronosCex_nid_no_patch(void) {
    return sceKernelIsGenuineCronosCex();
}

int APS5_VABI sceKernelIsGenuineDevKit_nid_no_patch(void) {
    return sceKernelIsGenuineDevKit();
}

int APS5_VABI sceKernelIsGenuineKratosCex_nid_no_patch(void) {
    return sceKernelIsGenuineKratosCex();
}

int APS5_VABI sceKernelIsGenuineN_nid_no_patch(void) {
    return sceKernelIsGenuineN();
}

int APS5_VABI sceKernelIsGenuineTestKit_nid_no_patch(void) {
    return sceKernelIsGenuineTestKit();
}

int APS5_VABI sceKernelIsInSandbox_nid_no_patch(void) {
    return sceKernelIsInSandbox();
}

int APS5_VABI sceKernelIsKratos_nid_no_patch(void) {
    return sceKernelIsKratos();
}

int APS5_VABI sceKernelIsMainOnStanbyMode_nid_no_patch(void) {
    return sceKernelIsMainOnStanbyMode();
}

int APS5_VABI sceKernelIsMainOnStandbyMode_nid_no_patch(void) {
    return sceKernelIsMainOnStandbyMode();
}

int APS5_VABI sceKernelIsModuleLoaded_nid_no_patch(const char* name) {
    return sceKernelIsModuleLoaded(name);
}

int APS5_VABI sceKernelIsNeoMode_nid_no_patch(void) {
    return sceKernelIsNeoMode();
}

int APS5_VABI sceKernelIsProspero_nid_no_patch(void) {
    return sceKernelIsProspero();
}

int APS5_VABI sceKernelIsPs4Process_nid_no_patch(void) {
    return sceKernelIsPs4Process();
}

int APS5_VABI sceKernelIsTestKit_nid_no_patch(void) {
    return sceKernelIsTestKit();
}

int APS5_VABI sceKernelJitGetSharedMemoryInfo_nid_no_patch(void) {
    return sceKernelJitGetSharedMemoryInfo();
}

int APS5_VABI sceKernelKernelHeapUsage_nid_no_patch(void) {
    return sceKernelKernelHeapUsage();
}

KernelModule APS5_VABI sceKernelLoadStartModuleForSysmodule_nid_no_patch(const char* moduleFileName, std::size_t args, const void* argp, std::uint32_t flags, const KernelLoadModuleOpt* opt, int* res) {
    return sceKernelLoadStartModuleForSysmodule(moduleFileName, args, argp, flags, opt, res);
}

int APS5_VABI sceKernelLwfsAllocateBlock_nid_no_patch(int32_t fd, int64_t size) {
    return sceKernelLwfsAllocateBlock(fd, size);
}

int64_t APS5_VABI sceKernelLwfsLseek_nid_no_patch(int32_t fd, int64_t offset, int32_t whence) {
    return sceKernelLwfsLseek(fd, offset, whence);
}

int APS5_VABI sceKernelLwfsSetAttribute_nid_no_patch(int32_t fd, int32_t attribute) {
    return sceKernelLwfsSetAttribute(fd, attribute);
}

int APS5_VABI sceKernelLwfsTrimBlock_nid_no_patch(void) {
    return sceKernelLwfsTrimBlock();
}

int64_t APS5_VABI sceKernelLwfsWrite_nid_no_patch(int32_t fd, const void* data, std::size_t size) {
    return sceKernelLwfsWrite(fd, data, size);
}

int APS5_VABI sceKernelMapSanitizerShadowMemory_nid_no_patch(void) {
    return sceKernelMapSanitizerShadowMemory();
}

int APS5_VABI sceKernelMapToolMemory_nid_no_patch(void) {
    return sceKernelMapToolMemory();
}

int APS5_VABI sceKernelMapTraceMemory_nid_no_patch(void) {
    return sceKernelMapTraceMemory();
}

int APS5_VABI sceKernelMapperGetIndirectBufferInfo_nid_no_patch(void) {
    return sceKernelMapperGetIndirectBufferInfo();
}

int APS5_VABI sceKernelMapperGetParam_nid_no_patch(std::uint64_t* param) {
    return sceKernelMapperGetParam(param);
}

int APS5_VABI sceKernelMapperGetUsageStatsData_nid_no_patch(void) {
    return sceKernelMapperGetUsageStatsData();
}

int APS5_VABI sceKernelMapperSetPageTablePoolOccupancyNotificationThreshold_nid_no_patch(void) {
    return sceKernelMapperSetPageTablePoolOccupancyNotificationThreshold();
}

int APS5_VABI sceKernelMemoryPoolMove_nid_no_patch(void) {
    return sceKernelMemoryPoolMove();
}

int APS5_VABI sceKernelMlockall_nid_no_patch(void) {
    return sceKernelMlockall();
}

int APS5_VABI sceKernelMsync_nid_no_patch(void* address, std::size_t length, int flags) {
    return sceKernelMsync(address, length, flags);
}

int APS5_VABI sceKernelNormalizePath_nid_no_patch(void) {
    return sceKernelNormalizePath();
}

int APS5_VABI sceKernelNotifyAppStateChanged_nid_no_patch(void) {
    return sceKernelNotifyAppStateChanged();
}

int APS5_VABI sceKernelNotifySystemSuspendResumeProgress_nid_no_patch(void) {
    return sceKernelNotifySystemSuspendResumeProgress();
}

int APS5_VABI sceKernelNotifySystemSuspendStart_nid_no_patch(void) {
    return sceKernelNotifySystemSuspendStart();
}

int APS5_VABI sceKernelOpenEport_nid_no_patch(void) {
    return sceKernelOpenEport();
}

int APS5_VABI sceKernelOpenInternal_nid_no_patch(void) {
    return sceKernelOpenInternal();
}

int APS5_VABI sceKernelOpenSema_nid_no_patch(void) {
    return sceKernelOpenSema();
}

int APS5_VABI sceKernelPltTrap_nid_no_patch(void) {
    return sceKernelPltTrap();
}

int APS5_VABI sceKernelProtectDirectMemory_nid_no_patch(void) {
    return sceKernelProtectDirectMemory();
}

int APS5_VABI sceKernelQueryToolMemory_nid_no_patch(void) {
    return sceKernelQueryToolMemory();
}

int APS5_VABI sceKernelQueryTraceMemory_nid_no_patch(void) {
    return sceKernelQueryTraceMemory();
}

int APS5_VABI sceKernelRandomizedPath_nid_no_patch(void) {
    return sceKernelRandomizedPath();
}

int APS5_VABI sceKernelReboot_nid_no_patch(void) {
    return sceKernelReboot();
}

int APS5_VABI sceKernelReleaseToolMemory_nid_no_patch(void) {
    return sceKernelReleaseToolMemory();
}

int APS5_VABI sceKernelReleaseTraceDirectMemory_nid_no_patch(void) {
    return sceKernelReleaseTraceDirectMemory();
}

int APS5_VABI sceKernelReportUnpatchedFunctionCall_nid_no_patch(void) {
    return sceKernelReportUnpatchedFunctionCall();
}

int APS5_VABI sceKernelReserve2mbPage_nid_no_patch(void) {
    return sceKernelReserve2mbPage();
}

int APS5_VABI sceKernelReserveSystemDirectMemory_nid_no_patch(void) {
    return sceKernelReserveSystemDirectMemory();
}

int APS5_VABI sceKernelResumeDirectMemoryRelease_nid_no_patch(void) {
    return sceKernelResumeDirectMemoryRelease();
}

int APS5_VABI sceKernelRtldControl_nid_no_patch(void) {
    return sceKernelRtldControl();
}

int APS5_VABI sceKernelSandboxPath_nid_no_patch(void) {
    return sceKernelSandboxPath();
}

int APS5_VABI sceKernelSetAppInfo_nid_no_patch(void) {
    return sceKernelSetAppInfo();
}

int APS5_VABI sceKernelSetAppLibcData_nid_no_patch(void) {
    return sceKernelSetAppLibcData();
}

int APS5_VABI sceKernelSetBackupRestoreMode_nid_no_patch(void) {
    return sceKernelSetBackupRestoreMode();
}

int APS5_VABI sceKernelSetBaseModeClock_nid_no_patch(void) {
    return sceKernelSetBaseModeClock();
}

int APS5_VABI sceKernelSetBesteffort_nid_no_patch(void) {
    return sceKernelSetBesteffort();
}

int APS5_VABI sceKernelSetBootReqNotify_nid_no_patch(void) {
    return sceKernelSetBootReqNotify();
}

int APS5_VABI sceKernelSetCallRecord_nid_no_patch(void) {
    return sceKernelSetCallRecord();
}

int APS5_VABI sceKernelSetCanvasHandle_nid_no_patch(void) {
    return sceKernelSetCanvasHandle();
}

int APS5_VABI sceKernelSetCompressionAttribute_nid_no_patch(int32_t fd, int32_t attribute) {
    return sceKernelSetCompressionAttribute(fd, attribute);
}

int APS5_VABI sceKernelSetCpumodeGame_nid_no_patch(int mode) {
    return sceKernelSetCpumodeGame(mode);
}

int APS5_VABI sceKernelSetDataTransferMode_nid_no_patch(void) {
    return sceKernelSetDataTransferMode();
}

int APS5_VABI sceKernelSetFsstParam_nid_no_patch(int32_t fd, void* param) {
    return sceKernelSetFsstParam(fd, param);
}

int APS5_VABI sceKernelSetGPI_nid_no_patch(void) {
    return sceKernelSetGPI();
}

int APS5_VABI sceKernelSetGameDirectMemoryLimit_nid_no_patch(void) {
    return sceKernelSetGameDirectMemoryLimit();
}

int APS5_VABI sceKernelSetGpuCu_nid_no_patch(void) {
    return sceKernelSetGpuCu();
}

int APS5_VABI sceKernelSetKnobs_nid_no_patch(void) {
    return sceKernelSetKnobs();
}

int APS5_VABI sceKernelSetMemoryPstate_nid_no_patch(void) {
    return sceKernelSetMemoryPstate();
}

int APS5_VABI sceKernelSetNeoModeClock_nid_no_patch(void) {
    return sceKernelSetNeoModeClock();
}

int APS5_VABI sceKernelSetPhysFmemLimit_nid_no_patch(void) {
    return sceKernelSetPhysFmemLimit();
}

int APS5_VABI sceKernelSetProcessName_nid_no_patch(void) {
    return sceKernelSetProcessName();
}

int APS5_VABI sceKernelSetProcessProperty_nid_no_patch(void) {
    return sceKernelSetProcessProperty();
}

int APS5_VABI sceKernelSetProcessPropertyString_nid_no_patch(void) {
    return sceKernelSetProcessPropertyString();
}

int APS5_VABI sceKernelSetSafemode_nid_no_patch(void) {
    return sceKernelSetSafemode();
}

int APS5_VABI sceKernelSetTimezoneInfo_nid_no_patch(void) {
    return sceKernelSetTimezoneInfo();
}

int APS5_VABI sceKernelSetVmContainer_nid_no_patch(void) {
    return sceKernelSetVmContainer();
}

int APS5_VABI sceKernelSettimeofday_nid_no_patch(const KernelTimeval* tv, const KernelTimezone* tz) {
    return sceKernelSettimeofday(tv, tz);
}

int APS5_VABI sceKernelSlvNotifyError_nid_no_patch(void) {
    return sceKernelSlvNotifyError();
}

int APS5_VABI sceKernelSuspendDirectMemoryRelease_nid_no_patch(void) {
    return sceKernelSuspendDirectMemoryRelease();
}

int APS5_VABI sceKernelTerminateSysCore_nid_no_patch(void) {
    return sceKernelTerminateSysCore();
}

int APS5_VABI sceKernelTitleWorkaroundIsEnabled_nid_no_patch(const KernelTitleWorkaround* workaround, std::int32_t bit, std::int32_t* enabled) {
    return sceKernelTitleWorkaroundIsEnabled(workaround, bit, enabled);
}

int APS5_VABI sceKernelTitleWorkdaroundIsEnabled_nid_no_patch(const KernelTitleWorkaround* workaround, std::int32_t bit, std::int32_t* enabled) {
    return sceKernelTitleWorkdaroundIsEnabled(workaround, bit, enabled);
}

int APS5_VABI sceKernelTraceMemoryTypeProtect_nid_no_patch(void) {
    return sceKernelTraceMemoryTypeProtect();
}

int APS5_VABI sceKernelTriggerEport_nid_no_patch(void) {
    return sceKernelTriggerEport();
}

int APS5_VABI sceKernelUseCronosKura_nid_no_patch(void) {
    return sceKernelUseCronosKura();
}

int APS5_VABI sceKernelUseCronosStreaming_nid_no_patch(void) {
    return sceKernelUseCronosStreaming();
}

int APS5_VABI sceKernelVirtualQueryAll_nid_no_patch(void) {
    return sceKernelVirtualQueryAll();
}

int APS5_VABI sceKernelWaitCommandBufferCompletion_nid_no_patch(std::uint32_t id) {
    return sceKernelWaitCommandBufferCompletion(id);
}

int APS5_VABI sceKernelWriteEventQueueOnCompletionCommand_nid_no_patch(void* dst, std::uint64_t equeue, std::int32_t ident, std::uint64_t data, std::uint64_t* outSize) {
    return sceKernelWriteEventQueueOnCompletionCommand(dst, equeue, ident, data, outSize);
}

int APS5_VABI sceKernelWriteMapCommand_nid_no_patch(void* dst, std::uint64_t address, std::uint64_t size, std::uint64_t type, std::uint64_t protection, std::uint64_t* outSize) {
    return sceKernelWriteMapCommand(dst, address, size, type, protection, outSize);
}

int APS5_VABI sceKernelWriteMapCommand2_nid_no_patch(void* dst, std::uint64_t address, std::uint64_t size, std::int32_t type, std::int32_t protection, std::int32_t prt, std::uint64_t* outSize) {
    return sceKernelWriteMapCommand2(dst, address, size, type, protection, prt, outSize);
}

int APS5_VABI sceKernelWriteMapDirectCommand_nid_no_patch(void* dst, std::uint64_t address, std::uint64_t directOffset, std::uint64_t size, std::uint64_t type, std::uint64_t protection, std::uint64_t* outSize) {
    return sceKernelWriteMapDirectCommand(dst, address, directOffset, size, type, protection, outSize);
}

int APS5_VABI sceKernelWriteMapDirectWithGpuMaskIdCommand_nid_no_patch(void* dst, std::uint64_t address, std::uint64_t directOffset, std::uint64_t size, std::uint64_t type, std::uint64_t protection, std::uint64_t gpuMaskId, std::uint64_t* outSize) {
    return sceKernelWriteMapDirectWithGpuMaskIdCommand(dst, address, directOffset, size, type, protection, gpuMaskId, outSize);
}

int APS5_VABI sceKernelWriteMapWithGpuMaskIdCommand_nid_no_patch(void* dst, std::uint64_t address, std::uint64_t size, std::uint64_t type, std::uint64_t protection, std::uint64_t gpuMaskId, std::uint64_t* outSize) {
    return sceKernelWriteMapWithGpuMaskIdCommand(dst, address, size, type, protection, gpuMaskId, outSize);
}

int APS5_VABI sceKernelWriteModifyMtypeProtectWithGpuMaskIdCommand_nid_no_patch(void* dst, std::uint64_t address, std::uint64_t size, std::uint64_t type, std::uint64_t protection, std::uint64_t mask, std::uint64_t gpuMaskId, std::uint64_t* outSize) {
    return sceKernelWriteModifyMtypeProtectWithGpuMaskIdCommand(dst, address, size, type, protection, mask, gpuMaskId, outSize);
}

int APS5_VABI sceKernelWriteModifyProtectCommand_nid_no_patch(void* dst, std::uint64_t address, std::uint64_t size, std::uint64_t protection, std::uint64_t mask, std::uint64_t* outSize) {
    return sceKernelWriteModifyProtectCommand(dst, address, size, protection, mask, outSize);
}

int APS5_VABI sceKernelWriteModifyProtectWithGpuMaskIdCommand_nid_no_patch(void* dst, std::uint64_t address, std::uint64_t size, std::uint64_t protection, std::uint64_t mask, std::uint64_t gpuMaskId, std::uint64_t* outSize) {
    return sceKernelWriteModifyProtectWithGpuMaskIdCommand(dst, address, size, protection, mask, gpuMaskId, outSize);
}

int APS5_VABI sceKernelWriteMultiMapCommand_nid_no_patch(void* dst, std::uint64_t address, std::uint64_t alias, std::uint64_t size, std::uint64_t protection, std::uint64_t* outSize) {
    return sceKernelWriteMultiMapCommand(dst, address, alias, size, protection, outSize);
}

int APS5_VABI sceKernelWriteMultiMapWithGpuMaskIdCommand_nid_no_patch(void* dst, std::uint64_t address, std::uint64_t alias, std::uint64_t size, std::uint64_t protection, std::uint64_t gpuMaskId, std::uint64_t* outSize) {
    return sceKernelWriteMultiMapWithGpuMaskIdCommand(dst, address, alias, size, protection, gpuMaskId, outSize);
}

int APS5_VABI sceKernelWriteRemapCommand_nid_no_patch(void* dst, std::uint64_t address, std::uint64_t source, std::uint64_t size, std::uint64_t protection, std::uint64_t* outSize) {
    return sceKernelWriteRemapCommand(dst, address, source, size, protection, outSize);
}

int APS5_VABI sceKernelWriteRemapWithGpuMaskIdCommand_nid_no_patch(void* dst, std::uint64_t address, std::uint64_t source, std::uint64_t size, std::uint64_t protection, std::uint64_t gpuMaskId, std::uint64_t* outSize) {
    return sceKernelWriteRemapWithGpuMaskIdCommand(dst, address, source, size, protection, gpuMaskId, outSize);
}

int APS5_VABI sceKernelWriteSdkEventLog_nid_no_patch(void) {
    return sceKernelWriteSdkEventLog();
}

int APS5_VABI sceKernelWriteSdkEventLogV_nid_no_patch(void) {
    return sceKernelWriteSdkEventLogV();
}

int APS5_VABI sceKernelWriteUnmapCommand_nid_no_patch(void* dst, std::uint64_t address, std::uint64_t size, std::uint64_t* outSize) {
    return sceKernelWriteUnmapCommand(dst, address, size, outSize);
}

int APS5_VABI sceKernelWriteWaitOnAddressCommand_nid_no_patch(void* dst, std::uint64_t address, std::uint64_t reference, std::int32_t compare, std::int32_t flush, std::uint64_t* outSize) {
    return sceKernelWriteWaitOnAddressCommand(dst, address, reference, compare, flush, outSize);
}

int APS5_VABI sceKernelWriteWaitOnCounterCommand_nid_no_patch(void* dst, std::uint8_t counter, std::uint32_t reference, std::int32_t compare, std::int32_t flush, std::uint64_t* outSize) {
    return sceKernelWriteWaitOnCounterCommand(dst, counter, reference, compare, flush, outSize);
}

int APS5_VABI sceKernelWriteWriteAddressCommand_nid_no_patch(void* dst, std::uint64_t address, std::uint64_t value, std::uint64_t* outSize) {
    return sceKernelWriteWriteAddressCommand(dst, address, value, outSize);
}

int APS5_VABI sceKernelWriteWriteCounterCommand_nid_no_patch(void* dst, std::uint8_t counter, std::uint32_t value, std::uint64_t* outSize) {
    return sceKernelWriteWriteCounterCommand(dst, counter, value, outSize);
}

int APS5_VABI sceKernelYieldCpumode_nid_no_patch(void) {
    return sceKernelYieldCpumode();
}

int APS5_VABI sceLibcMspaceCreateForMonoMutex_nid_no_patch(void) {
    return sceLibcMspaceCreateForMonoMutex();
}

int APS5_VABI scePthreadAtfork_nid_no_patch(void (APS5_VABI* prepare)(), void (APS5_VABI* parent)(), void (APS5_VABI* child)()) {
    return scePthreadAtfork(prepare, parent, child);
}

int APS5_VABI scePthreadAttrGetscope_nid_no_patch(const PthreadAttr* attr, int* scope) {
    return scePthreadAttrGetscope(attr, scope);
}

int APS5_VABI scePthreadAttrSetcreatesuspend_nid_no_patch(PthreadAttr* attr) {
    return scePthreadAttrSetcreatesuspend(attr);
}

int APS5_VABI scePthreadAttrSetscope_nid_no_patch(PthreadAttr* attr, int scope) {
    return scePthreadAttrSetscope(attr, scope);
}

int APS5_VABI scePthreadBarrierDestroy_nid_no_patch(PthreadBarrierPrivate** barrier) {
    return scePthreadBarrierDestroy(barrier);
}

int APS5_VABI scePthreadBarrierInit_nid_no_patch(PthreadBarrierPrivate** barrier, PthreadBarrierattrPrivate* const* attr, unsigned count, const char* name) {
    return scePthreadBarrierInit(barrier, attr, count, name);
}

int APS5_VABI scePthreadBarrierWait_nid_no_patch(PthreadBarrierPrivate** barrier) {
    return scePthreadBarrierWait(barrier);
}

int APS5_VABI scePthreadBarrierattrDestroy_nid_no_patch(PthreadBarrierattrPrivate** attr) {
    return scePthreadBarrierattrDestroy(attr);
}

int APS5_VABI scePthreadBarrierattrGetpshared_nid_no_patch(PthreadBarrierattrPrivate* const* attr, int* pshared) {
    return scePthreadBarrierattrGetpshared(attr, pshared);
}

int APS5_VABI scePthreadBarrierattrInit_nid_no_patch(PthreadBarrierattrPrivate** attr) {
    return scePthreadBarrierattrInit(attr);
}

int APS5_VABI scePthreadBarrierattrSetpshared_nid_no_patch(PthreadBarrierattrPrivate** attr, int pshared) {
    return scePthreadBarrierattrSetpshared(attr, pshared);
}

int APS5_VABI scePthreadCondattrGetclock_nid_no_patch(const PthreadCondattr* attr, KernelClockid* clockId) {
    return scePthreadCondattrGetclock(attr, clockId);
}

int APS5_VABI scePthreadGetconcurrency_nid_no_patch(void) {
    return scePthreadGetconcurrency();
}

int APS5_VABI scePthreadGetcpuclockid_nid_no_patch(Pthread thread, int* clockId) {
    return scePthreadGetcpuclockid(thread, clockId);
}

int APS5_VABI scePthreadMain_nid_no_patch(void) {
    return scePthreadMain();
}

int APS5_VABI scePthreadMulti_nid_no_patch(void) {
    return scePthreadMulti();
}

int APS5_VABI scePthreadMutexInitForInternalLibc_nid_no_patch(PthreadMutex* mutex, const PthreadMutexattr* attr) {
    return scePthreadMutexInitForInternalLibc(mutex, attr);
}

int APS5_VABI scePthreadMutexIsowned_nid_no_patch(PthreadMutex* mutex) {
    return scePthreadMutexIsowned(mutex);
}

int APS5_VABI scePthreadMutexattrGetgen_nid_no_patch(const PthreadMutexattr* attr, int* generation) {
    return scePthreadMutexattrGetgen(attr, generation);
}

int APS5_VABI scePthreadMutexattrInitForInternalLibc_nid_no_patch(PthreadMutexattr* attr) {
    return scePthreadMutexattrInitForInternalLibc(attr);
}

int APS5_VABI scePthreadMutexattrSetgen_nid_no_patch(PthreadMutexattr* attr, int generation) {
    return scePthreadMutexattrSetgen(attr, generation);
}

int APS5_VABI scePthreadRegisterThreadDeathHook_nid_no_patch(void) {
    return scePthreadRegisterThreadDeathHook();
}

int APS5_VABI scePthreadResume_nid_no_patch(Pthread thread) {
    return scePthreadResume(thread);
}

void APS5_VABI scePthreadResumeAll_nid_no_patch(void) {
    scePthreadResumeAll();
}

int APS5_VABI scePthreadSetBesteffort_nid_no_patch(void) {
    return scePthreadSetBesteffort();
}

int APS5_VABI scePthreadSetDefaultstacksize_nid_no_patch(std::size_t size) {
    return scePthreadSetDefaultstacksize(size);
}

void APS5_VABI scePthreadSetName_nid_no_patch(Pthread thread, const char* name) {
    scePthreadSetName(thread, name);
}

int APS5_VABI scePthreadSetconcurrency_nid_no_patch(int level) {
    return scePthreadSetconcurrency(level);
}

int APS5_VABI scePthreadSingle_nid_no_patch(void) {
    return scePthreadSingle();
}

int APS5_VABI scePthreadSuspend_nid_no_patch(void) {
    return scePthreadSuspend();
}

int APS5_VABI scePthreadSuspendAll_nid_no_patch(void) {
    return scePthreadSuspendAll();
}

int APS5_VABI scePthreadTimedjoin_nid_no_patch(Pthread thread, void** value, KernelUseconds usec) {
    return scePthreadTimedjoin(thread, value, usec);
}


#if defined(__ELF__)
__asm__(".protected _fpathconf_nid_postfix");
__asm__(".protected _openat_nid_postfix");
__asm__(".protected amd64_set_fsbase_nid_postfix");
__asm__(".protected blockpool_batch_nid_postfix");
__asm__(".protected blockpool_map_nid_postfix");
__asm__(".protected blockpool_open_nid_postfix");
__asm__(".protected blockpool_unmap_nid_postfix");
__asm__(".protected dynlib_get_obj_member_nid_postfix");
__asm__(".protected get_authinfo_nid_postfix");
__asm__(".protected get_module_info_list_nid_postfix");
__asm__(".protected get_page_table_stats_nid_postfix");
__asm__(".protected get_sdk_compiled_version_nid_postfix");
__asm__(".protected get_self_auth_info_nid_postfix");
__asm__(".protected get_vm_map_timestamp_nid_postfix");
__asm__(".protected ipmimgr_call_nid_postfix");
__asm__(".protected is_in_sandbox_nid_postfix");
__asm__(".protected ksem_close_nid_postfix");
__asm__(".protected ksem_open_nid_postfix");
__asm__(".protected ksem_unlink_nid_postfix");
__asm__(".protected mdbg_service_nid_postfix");
__asm__(".protected openintr_nid_postfix");
__asm__(".protected profil_nid_postfix");
__asm__(".protected pthread_attr_getaffinity_np_nid_postfix");
__asm__(".protected pthread_attr_getsolosched_np_nid_postfix");
__asm__(".protected pthread_attr_setaffinity_np_nid_postfix");
__asm__(".protected pthread_attr_setcreatesuspend_np_nid_postfix");
__asm__(".protected pthread_attr_setstackaddr_nid_postfix");
__asm__(".protected pthread_get_specificarray_np_nid_postfix");
__asm__(".protected pthread_get_user_context_np_nid_postfix");
__asm__(".protected pthread_getstack_np_nid_postfix");
__asm__(".protected pthread_main_np_nid_postfix");
__asm__(".protected pthread_multi_np_nid_postfix");
__asm__(".protected pthread_mutex_init_for_mono_nid_postfix");
__asm__(".protected pthread_mutexattr_getgen_np_nid_postfix");
__asm__(".protected pthread_mutexattr_setgen_np_nid_postfix");
__asm__(".protected pthread_resume_all_np_nid_postfix");
__asm__(".protected pthread_resume_np_nid_postfix");
__asm__(".protected pthread_resume_user_context_np_nid_postfix");
__asm__(".protected pthread_set_defaultstacksize_np_nid_postfix");
__asm__(".protected pthread_set_user_context_np_nid_postfix");
__asm__(".protected pthread_single_np_nid_postfix");
__asm__(".protected pthread_suspend_all_np_nid_postfix");
__asm__(".protected pthread_suspend_np_nid_postfix");
__asm__(".protected pthread_suspend_user_context_np_nid_postfix");
__asm__(".protected pthread_switch_add_np_nid_postfix");
__asm__(".protected pthread_switch_delete_np_nid_postfix");
__asm__(".protected rtld_printf_nid_postfix");
__asm__(".protected sched_rr_get_interval_nid_postfix");
__asm__(".protected sched_setscheduler_nid_postfix");
__asm__(".protected set_phys_fmem_limit_nid_postfix");
__asm__(".protected settimeofday_nid_postfix");
__asm__(".protected sigqueue_nid_postfix");
__asm__(".protected sigreturn_nid_postfix");
__asm__(".protected sysc_e00_nid_postfix");
__asm__(".protected sysc_m00_nid_postfix");
__asm__(".protected sysc_s00_nid_postfix");
__asm__(".protected tcsetsid_nid_postfix");
__asm__(".protected uuidgen_nid_postfix");
__asm__(".protected wait3_nid_postfix");
__asm__(".protected sceCoredumpDebugForceCoredumpOnAppClose");
__asm__(".protected sceCoredumpDebugTriggerCoredump");
__asm__(".protected sceCoredumpGetStopInfoGpu");
__asm__(".protected sceCoredumpInternalGetThreadContextInfo");
__asm__(".protected sceKernelAddCpumodeEvent");
__asm__(".protected sceKernelAddExternalTriggerEvent");
__asm__(".protected sceKernelAddGpuExceptionEvent");
__asm__(".protected sceKernelAddProcessToCanvasMap");
__asm__(".protected sceKernelAllocateDirectMemory2");
__asm__(".protected sceKernelAllocateDirectMemoryForMiniApp");
__asm__(".protected sceKernelAllocateToolMemory");
__asm__(".protected sceKernelAllocateTraceDirectMemory");
__asm__(".protected sceKernelAvailableToolMemorySize");
__asm__(".protected sceKernelBacktraceSelf");
__asm__(".protected sceKernelCallIndirectBuffer");
__asm__(".protected sceKernelCallIndirectBuffer2");
__asm__(".protected sceKernelCallIndirectBuffer3");
__asm__(".protected sceKernelCheckProsperoSdkVersion");
__asm__(".protected sceKernelClearBootReqNotifyCount");
__asm__(".protected sceKernelClearGameDirectMemory");
__asm__(".protected sceKernelCloseEport");
__asm__(".protected sceKernelCloseSema");
__asm__(".protected sceKernelCreateEport");
__asm__(".protected sceKernelDebugAcquireAndUpdateDebugRegister");
__asm__(".protected sceKernelDebugGetAppStatus");
__asm__(".protected sceKernelDebugGetPauseCount");
__asm__(".protected sceKernelDebugGpuPaDebugIsInProgress");
__asm__(".protected sceKernelDebugPauseWithDebugContext");
__asm__(".protected sceKernelDebugRaiseExceptionWithContext");
__asm__(".protected sceKernelDebugRaiseExceptionWithInfo");
__asm__(".protected sceKernelDebugReleaseDebugContext");
__asm__(".protected sceKernelDeleteCpumodeEvent");
__asm__(".protected sceKernelDeleteEport");
__asm__(".protected sceKernelDeleteExternalTriggerEvent");
__asm__(".protected sceKernelDeleteGpuExceptionEvent");
__asm__(".protected sceKernelDeleteProcessFromCanvasMap");
__asm__(".protected sceKernelDirectMemoryQueryForId");
__asm__(".protected sceKernelEnableDmemAliasing2");
__asm__(".protected sceKernelEnablePthreadObjectCheck");
__asm__(".protected sceKernelEventLogInit");
__asm__(".protected sceKernelEventLogPread");
__asm__(".protected sceKernelEventLogRead");
__asm__(".protected sceKernelEventLogWrite");
__asm__(".protected sceKernelExtendMaximumModuleNumber");
__asm__(".protected sceKernelFlock");
__asm__(".protected sceKernelGetAmprCounter");
__asm__(".protected sceKernelGetAppCategoryType");
__asm__(".protected sceKernelGetAppLibcData");
__asm__(".protected sceKernelGetAslrStatus");
__asm__(".protected sceKernelGetAutoUpdateVersion");
__asm__(".protected sceKernelGetBackupRestoreMode");
__asm__(".protected sceKernelGetBackupRestoreModeOfNextBoot");
__asm__(".protected sceKernelGetBasicProductShape");
__asm__(".protected sceKernelGetBetaUpdateTestForRcmgr");
__asm__(".protected sceKernelGetBioUsageAll");
__asm__(".protected sceKernelGetBootReqNotifyCount");
__asm__(".protected sceKernelGetBuildNumber");
__asm__(".protected sceKernelGetCallRecord");
__asm__(".protected sceKernelGetCanvasHandle");
__asm__(".protected sceKernelGetCanvasHandleByPid");
__asm__(".protected sceKernelGetCompiledSdkVersion");
__asm__(".protected sceKernelGetCompiledSdkVersionByPath");
__asm__(".protected sceKernelGetCompiledSdkVersionByPid");
__asm__(".protected sceKernelGetCpuCoreClock");
__asm__(".protected sceKernelGetCpuUsage");
__asm__(".protected sceKernelGetCpuUsageAll");
__asm__(".protected sceKernelGetCpuUsageProc");
__asm__(".protected sceKernelGetCpuUsageProc2");
__asm__(".protected sceKernelGetCpuUsageThread");
__asm__(".protected sceKernelGetCpumodeGame");
__asm__(".protected sceKernelGetDataTransferMode");
__asm__(".protected sceKernelGetDebugMenuMiniModeForRcmgr");
__asm__(".protected sceKernelGetDebugMenuModeForPsmForRcmgr");
__asm__(".protected sceKernelGetDebugMenuModeForRcmgr");
__asm__(".protected sceKernelGetDefaultToolMemorySize");
__asm__(".protected sceKernelGetExtLibcHandle");
__asm__(".protected sceKernelGetFakeFinalizeMenuForRcmgr");
__asm__(".protected sceKernelGetFlagedUpdaterForRcmgr");
__asm__(".protected sceKernelGetForceUpdateModeForRcmgr");
__asm__(".protected sceKernelGetGPO");
__asm__(".protected sceKernelGetGenuineProsperoSystemSwVersion");
__asm__(".protected sceKernelGetHwFeatureInfoForDecid");
__asm__(".protected sceKernelGetIdPs");
__asm__(".protected sceKernelGetIdTableCurrentCount");
__asm__(".protected sceKernelGetIpcPath");
__asm__(".protected sceKernelGetKnobs");
__asm__(".protected sceKernelGetModuleInfoInternal");
__asm__(".protected sceKernelGetModuleListInternal");
__asm__(".protected sceKernelGetOpenPsIdForSystem");
__asm__(".protected sceKernelGetPagingStatsOfAllObjects");
__asm__(".protected sceKernelGetPagingStatsOfAllThreads");
__asm__(".protected sceKernelGetPhysPageSize");
__asm__(".protected sceKernelGetProcessName");
__asm__(".protected sceKernelGetProcessType");
__asm__(".protected sceKernelGetProductCode");
__asm__(".protected sceKernelGetProductStr");
__asm__(".protected sceKernelGetProsperoCompiledSdkVersion");
__asm__(".protected sceKernelGetProsperoCompiledSdkVersionByPid");
__asm__(".protected sceKernelGetPs4CompiledSdkVersion");
__asm__(".protected sceKernelGetPs4CompiledSdkVersionByPid");
__asm__(".protected sceKernelGetPs4SystemSwVersion");
__asm__(".protected sceKernelGetPsmIntdevModeForRcmgr");
__asm__(".protected sceKernelGetPsnAccessTraceLogForRcmgr");
__asm__(".protected sceKernelGetQafExpirationTimeNotafterForRcmgr");
__asm__(".protected sceKernelGetQafExpirationTimeNotbeforeForRcmgr");
__asm__(".protected sceKernelGetQafGenerationForRcmgr");
__asm__(".protected sceKernelGetQafNameForRcmgr");
__asm__(".protected sceKernelGetRenderingMode");
__asm__(".protected sceKernelGetResidentCount");
__asm__(".protected sceKernelGetSafemode");
__asm__(".protected sceKernelGetSanitizerMallocReplace");
__asm__(".protected sceKernelGetSanitizerNewReplace");
__asm__(".protected sceKernelGetSocClock");
__asm__(".protected sceKernelGetSpecialIForRcmgr");
__asm__(".protected sceKernelGetSubsysId");
__asm__(".protected sceKernelGetSubsysSubId");
__asm__(".protected sceKernelGetSystemLevelDebuggerModeForRcmgr");
__asm__(".protected sceKernelGetSystemSwBeta");
__asm__(".protected sceKernelGetThreadName");
__asm__(".protected sceKernelGetToolMemoryRange");
__asm__(".protected sceKernelGetUniversalMode");
__asm__(".protected sceKernelGetUpdVersion");
__asm__(".protected sceKernelGetUtokenDataExecutionForRcmgr");
__asm__(".protected sceKernelGetUtokenExpirationTimeNotafterForRcmgr");
__asm__(".protected sceKernelGetUtokenExpirationTimeNotbeforeForRcmgr");
__asm__(".protected sceKernelGetUtokenFakeSharefactoryForRcmgr");
__asm__(".protected sceKernelGetUtokenFlagedUpdaterForRcmgr");
__asm__(".protected sceKernelGetUtokenNpEnvSwitchingForRcmgr");
__asm__(".protected sceKernelGetUtokenSaveDataRepairForRcmgr");
__asm__(".protected sceKernelGetUtokenStoreModeForRcmgr");
__asm__(".protected sceKernelGetUtokenUseSoftwagnerForAcmgr");
__asm__(".protected sceKernelGetUtokenUseSoftwagnerForRcmgr");
__asm__(".protected sceKernelGetUtokenWeakenedPortRestrictionForRcmgr");
__asm__(".protected sceKernelGetVrCaptureSize");
__asm__(".protected sceKernelGiveDirectMemoryToMapper");
__asm__(".protected sceKernelHasNeoMode");
__asm__(".protected sceKernelHwGetBdDriveInfo");
__asm__(".protected sceKernelHwHasOpticalOut");
__asm__(".protected sceKernelHwHasWlanBt");
__asm__(".protected sceKernelIccControlBDPowerState");
__asm__(".protected sceKernelIccControlUSBPowerState");
__asm__(".protected sceKernelIccGetBDDriveChuckingState");
__asm__(".protected sceKernelIccGetBDPowerState");
__asm__(".protected sceKernelIccGetCPMode");
__asm__(".protected sceKernelIccGetCountTime");
__asm__(".protected sceKernelIccGetCpuInfoBit");
__asm__(".protected sceKernelIccGetErrLog");
__asm__(".protected sceKernelIccGetHwInfo");
__asm__(".protected sceKernelIccGetPowerNumberOfBootShutdown");
__asm__(".protected sceKernelIccGetPowerOperatingTime");
__asm__(".protected sceKernelIccGetPowerUpCause");
__asm__(".protected sceKernelIccGetSysEventLog");
__asm__(".protected sceKernelIccGetUSBPowerState");
__asm__(".protected sceKernelIccIndicatorBootDone");
__asm__(".protected sceKernelIccIndicatorShutdown");
__asm__(".protected sceKernelIccIndicatorStandby");
__asm__(".protected sceKernelIccIndicatorStandbyBoot");
__asm__(".protected sceKernelIccIndicatorStandbyShutdown");
__asm__(".protected sceKernelIccNotifyBootStatus");
__asm__(".protected sceKernelIccNvsFlush");
__asm__(".protected sceKernelIccReadPowerBootMessage");
__asm__(".protected sceKernelIccSetBuzzer");
__asm__(".protected sceKernelIccSetCPMode");
__asm__(".protected sceKernelIccSetCpuInfoBit");
__asm__(".protected sceKernelIccSetDownloadMode");
__asm__(".protected sceKernelIccSetPriorityBuzzer");
__asm__(".protected sceKernelInternalCreateMapperSharedMemory");
__asm__(".protected sceKernelInternalGetKmemStatistics");
__asm__(".protected sceKernelInternalGetMapStatistics");
__asm__(".protected sceKernelInternalHeapPrintBacktraceWithModuleInfo");
__asm__(".protected sceKernelInternalLockMapperMemory");
__asm__(".protected sceKernelInternalMapDirectMemory");
__asm__(".protected sceKernelInternalMapNamedDirectMemory");
__asm__(".protected sceKernelInternalResumeDirectMemoryRelease");
__asm__(".protected sceKernelInternalSuspendDirectMemoryRelease");
__asm__(".protected sceKernelInternalUnlockMapperMemory");
__asm__(".protected sceKernelInternalUnmapMapperSharedMemory");
__asm__(".protected sceKernelIsAllowedToSelectDvdRegion");
__asm__(".protected sceKernelIsAuthenticNeo");
__asm__(".protected sceKernelIsCEX");
__asm__(".protected sceKernelIsCronos");
__asm__(".protected sceKernelIsDebuggerAttached");
__asm__(".protected sceKernelIsDevKit");
__asm__(".protected sceKernelIsExperimentalBeta");
__asm__(".protected sceKernelIsGenuineCEX");
__asm__(".protected sceKernelIsGenuineCronosCex");
__asm__(".protected sceKernelIsGenuineDevKit");
__asm__(".protected sceKernelIsGenuineKratosCex");
__asm__(".protected sceKernelIsGenuineN");
__asm__(".protected sceKernelIsGenuineTestKit");
__asm__(".protected sceKernelIsInSandbox");
__asm__(".protected sceKernelIsKratos");
__asm__(".protected sceKernelIsMainOnStanbyMode");
__asm__(".protected sceKernelIsMainOnStandbyMode");
__asm__(".protected sceKernelIsModuleLoaded");
__asm__(".protected sceKernelIsNeoMode");
__asm__(".protected sceKernelIsProspero");
__asm__(".protected sceKernelIsPs4Process");
__asm__(".protected sceKernelIsTestKit");
__asm__(".protected sceKernelJitGetSharedMemoryInfo");
__asm__(".protected sceKernelKernelHeapUsage");
__asm__(".protected sceKernelLoadStartModuleForSysmodule");
__asm__(".protected sceKernelLwfsAllocateBlock");
__asm__(".protected sceKernelLwfsLseek");
__asm__(".protected sceKernelLwfsSetAttribute");
__asm__(".protected sceKernelLwfsTrimBlock");
__asm__(".protected sceKernelLwfsWrite");
__asm__(".protected sceKernelMapSanitizerShadowMemory");
__asm__(".protected sceKernelMapToolMemory");
__asm__(".protected sceKernelMapTraceMemory");
__asm__(".protected sceKernelMapperGetIndirectBufferInfo");
__asm__(".protected sceKernelMapperGetParam");
__asm__(".protected sceKernelMapperGetUsageStatsData");
__asm__(".protected sceKernelMapperSetPageTablePoolOccupancyNotificationThreshold");
__asm__(".protected sceKernelMemoryPoolMove");
__asm__(".protected sceKernelMlockall");
__asm__(".protected sceKernelMsync");
__asm__(".protected sceKernelNormalizePath");
__asm__(".protected sceKernelNotifyAppStateChanged");
__asm__(".protected sceKernelNotifySystemSuspendResumeProgress");
__asm__(".protected sceKernelNotifySystemSuspendStart");
__asm__(".protected sceKernelOpenEport");
__asm__(".protected sceKernelOpenInternal");
__asm__(".protected sceKernelOpenSema");
__asm__(".protected sceKernelPltTrap");
__asm__(".protected sceKernelProtectDirectMemory");
__asm__(".protected sceKernelQueryToolMemory");
__asm__(".protected sceKernelQueryTraceMemory");
__asm__(".protected sceKernelRandomizedPath");
__asm__(".protected sceKernelReboot");
__asm__(".protected sceKernelReleaseToolMemory");
__asm__(".protected sceKernelReleaseTraceDirectMemory");
__asm__(".protected sceKernelReportUnpatchedFunctionCall");
__asm__(".protected sceKernelReserve2mbPage");
__asm__(".protected sceKernelReserveSystemDirectMemory");
__asm__(".protected sceKernelResumeDirectMemoryRelease");
__asm__(".protected sceKernelRtldControl");
__asm__(".protected sceKernelSandboxPath");
__asm__(".protected sceKernelSetAppInfo");
__asm__(".protected sceKernelSetAppLibcData");
__asm__(".protected sceKernelSetBackupRestoreMode");
__asm__(".protected sceKernelSetBaseModeClock");
__asm__(".protected sceKernelSetBesteffort");
__asm__(".protected sceKernelSetBootReqNotify");
__asm__(".protected sceKernelSetCallRecord");
__asm__(".protected sceKernelSetCanvasHandle");
__asm__(".protected sceKernelSetCompressionAttribute");
__asm__(".protected sceKernelSetCpumodeGame");
__asm__(".protected sceKernelSetDataTransferMode");
__asm__(".protected sceKernelSetFsstParam");
__asm__(".protected sceKernelSetGPI");
__asm__(".protected sceKernelSetGameDirectMemoryLimit");
__asm__(".protected sceKernelSetGpuCu");
__asm__(".protected sceKernelSetKnobs");
__asm__(".protected sceKernelSetMemoryPstate");
__asm__(".protected sceKernelSetNeoModeClock");
__asm__(".protected sceKernelSetPhysFmemLimit");
__asm__(".protected sceKernelSetProcessName");
__asm__(".protected sceKernelSetProcessProperty");
__asm__(".protected sceKernelSetProcessPropertyString");
__asm__(".protected sceKernelSetSafemode");
__asm__(".protected sceKernelSetTimezoneInfo");
__asm__(".protected sceKernelSetVmContainer");
__asm__(".protected sceKernelSettimeofday");
__asm__(".protected sceKernelSlvNotifyError");
__asm__(".protected sceKernelSuspendDirectMemoryRelease");
__asm__(".protected sceKernelTerminateSysCore");
__asm__(".protected sceKernelTitleWorkaroundIsEnabled");
__asm__(".protected sceKernelTitleWorkdaroundIsEnabled");
__asm__(".protected sceKernelTraceMemoryTypeProtect");
__asm__(".protected sceKernelTriggerEport");
__asm__(".protected sceKernelUseCronosKura");
__asm__(".protected sceKernelUseCronosStreaming");
__asm__(".protected sceKernelVirtualQueryAll");
__asm__(".protected sceKernelWaitCommandBufferCompletion");
__asm__(".protected sceKernelWriteEventQueueOnCompletionCommand");
__asm__(".protected sceKernelWriteMapCommand");
__asm__(".protected sceKernelWriteMapCommand2");
__asm__(".protected sceKernelWriteMapDirectCommand");
__asm__(".protected sceKernelWriteMapDirectWithGpuMaskIdCommand");
__asm__(".protected sceKernelWriteMapWithGpuMaskIdCommand");
__asm__(".protected sceKernelWriteModifyMtypeProtectWithGpuMaskIdCommand");
__asm__(".protected sceKernelWriteModifyProtectCommand");
__asm__(".protected sceKernelWriteModifyProtectWithGpuMaskIdCommand");
__asm__(".protected sceKernelWriteMultiMapCommand");
__asm__(".protected sceKernelWriteMultiMapWithGpuMaskIdCommand");
__asm__(".protected sceKernelWriteRemapCommand");
__asm__(".protected sceKernelWriteRemapWithGpuMaskIdCommand");
__asm__(".protected sceKernelWriteSdkEventLog");
__asm__(".protected sceKernelWriteSdkEventLogV");
__asm__(".protected sceKernelWriteUnmapCommand");
__asm__(".protected sceKernelWriteWaitOnAddressCommand");
__asm__(".protected sceKernelWriteWaitOnCounterCommand");
__asm__(".protected sceKernelWriteWriteAddressCommand");
__asm__(".protected sceKernelWriteWriteCounterCommand");
__asm__(".protected sceKernelYieldCpumode");
__asm__(".protected sceLibcMspaceCreateForMonoMutex");
__asm__(".protected scePthreadAtfork");
__asm__(".protected scePthreadAttrGetscope");
__asm__(".protected scePthreadAttrSetcreatesuspend");
__asm__(".protected scePthreadAttrSetscope");
__asm__(".protected scePthreadBarrierDestroy");
__asm__(".protected scePthreadBarrierInit");
__asm__(".protected scePthreadBarrierWait");
__asm__(".protected scePthreadBarrierattrDestroy");
__asm__(".protected scePthreadBarrierattrGetpshared");
__asm__(".protected scePthreadBarrierattrInit");
__asm__(".protected scePthreadBarrierattrSetpshared");
__asm__(".protected scePthreadCondattrGetclock");
__asm__(".protected scePthreadGetconcurrency");
__asm__(".protected scePthreadGetcpuclockid");
__asm__(".protected scePthreadMain");
__asm__(".protected scePthreadMulti");
__asm__(".protected scePthreadMutexInitForInternalLibc");
__asm__(".protected scePthreadMutexIsowned");
__asm__(".protected scePthreadMutexattrGetgen");
__asm__(".protected scePthreadMutexattrInitForInternalLibc");
__asm__(".protected scePthreadMutexattrSetgen");
__asm__(".protected scePthreadRegisterThreadDeathHook");
__asm__(".protected scePthreadResume");
__asm__(".protected scePthreadResumeAll");
__asm__(".protected scePthreadSetBesteffort");
__asm__(".protected scePthreadSetDefaultstacksize");
__asm__(".protected scePthreadSetName");
__asm__(".protected scePthreadSetconcurrency");
__asm__(".protected scePthreadSingle");
__asm__(".protected scePthreadSuspend");
__asm__(".protected scePthreadSuspendAll");
__asm__(".protected scePthreadTimedjoin");
#endif

}
