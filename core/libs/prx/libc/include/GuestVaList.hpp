#ifndef CORE_LIBS_PRX_LIBC_INCLUDE_GUESTVALIST_HPP
#define CORE_LIBS_PRX_LIBC_INCLUDE_GUESTVALIST_HPP

#include "SceTypes.hpp"
#include <cstdarg>
#include <cstdint>

namespace LibcDetail {

constexpr std::uint32_t HostVaListTag = 0xffffffffu;

inline VaList HostVaList(std::va_list* host) {
    VaList list{};
    list.gp_offset = HostVaListTag;
    list.fp_offset = HostVaListTag;
    list.reg_save_area = host;
    return list;
}

}

#if defined(_WIN32)
#define APS5_VA_BEGIN(last) __builtin_sysv_va_list args; __builtin_sysv_va_start(args, last)
#define APS5_VA_END() __builtin_sysv_va_end(args)
#elif defined(ANYPS5_GUEST_LIFTED)
#define APS5_VA_BEGIN(last) std::va_list hostArgs; va_start(hostArgs, last); VaList args[1] = {LibcDetail::HostVaList(&hostArgs)}
#define APS5_VA_END() va_end(hostArgs)
#else
#define APS5_VA_BEGIN(last) std::va_list args; va_start(args, last)
#define APS5_VA_END() va_end(args)
#endif

#endif
