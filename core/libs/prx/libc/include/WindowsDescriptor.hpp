#ifndef CORE_LIBS_PRX_LIBC_INCLUDE_WINDOWSDESCRIPTOR_HPP
#define CORE_LIBS_PRX_LIBC_INCLUDE_WINDOWSDESCRIPTOR_HPP

#include <cstdint>
#include <io.h>
#include <stdexcept>
#include <string>
#include <windows.h>
#include <winternl.h>

extern "C" _invalid_parameter_handler _set_thread_local_invalid_parameter_handler(_invalid_parameter_handler);

static inline HANDLE WindowsDescriptorHandle_nid_no_patch(int descriptor) {
    const auto ignore = +[](const wchar_t*, const wchar_t*, const wchar_t*, unsigned int, std::uintptr_t) {};
    const auto previous = _set_thread_local_invalid_parameter_handler(ignore);
    const auto handle = ::_get_osfhandle(descriptor);
    _set_thread_local_invalid_parameter_handler(previous);
    return reinterpret_cast<HANDLE>(handle);
}

static inline ACCESS_MASK WindowsHandleAccess_nid_no_patch(HANDLE handle, int descriptor, const char* function) {
    using QueryObject = NTSTATUS(NTAPI*)(HANDLE, OBJECT_INFORMATION_CLASS, PVOID, ULONG, PULONG);
    static const auto query = reinterpret_cast<QueryObject>(reinterpret_cast<void*>(::GetProcAddress(::GetModuleHandleW(L"ntdll.dll"), "NtQueryObject")));
    PUBLIC_OBJECT_BASIC_INFORMATION information{};
    const NTSTATUS status = query == nullptr ? -1 : query(handle, ObjectBasicInformation, &information, sizeof(information), nullptr);
    if (status < 0)
        throw std::runtime_error(std::string(function) + ": cannot read the access of descriptor " + std::to_string(descriptor) + ", status=" + std::to_string(status));
    return information.GrantedAccess;
}

static inline std::uint32_t WindowsFileNumber_nid_no_patch(std::uint64_t fileId) {
    const std::uint32_t folded = static_cast<std::uint32_t>(fileId) ^ (static_cast<std::uint32_t>(fileId >> 32) * 0x9E3779B1u);
    return folded == 0 ? 1 : folded;
}

static inline std::uint32_t WindowsFileNumber_nid_no_patch(const BY_HANDLE_FILE_INFORMATION& identity) {
    return WindowsFileNumber_nid_no_patch((static_cast<std::uint64_t>(identity.nFileIndexHigh) << 32) | identity.nFileIndexLow);
}

static inline bool WindowsHandleOnlyAppends_nid_no_patch(ACCESS_MASK access) {
    return (access & (FILE_WRITE_DATA | FILE_APPEND_DATA)) == FILE_APPEND_DATA;
}

static inline int WindowsDescriptorAccess_nid_no_patch(HANDLE handle, int descriptor, const char* function) {
    const ACCESS_MASK access = WindowsHandleAccess_nid_no_patch(handle, descriptor, function);
    const bool read = (access & FILE_READ_DATA) != 0;
    const bool write = (access & (FILE_WRITE_DATA | FILE_APPEND_DATA)) != 0;
    if (read && write) return 2;
    if (write) return 1;
    if (read) return 0;
    throw std::runtime_error(std::string(function) + ": descriptor " + std::to_string(descriptor) + " grants neither read nor write access");
}

#endif
