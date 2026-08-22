#include <cstdint>
#include <cstddef>
#include <cstdio>
#include <cstdarg>
#include "SceTypes.hpp"
#include "VarArgsAbi.hpp"

extern "C" {

int printf_nid_postfix(const char* format, ...) {
    std::va_list args;
    va_start(args, format);
    const int result = std::vprintf(format, args);
    va_end(args);
    return result;
}

int libc_printf_nid_postfix(VA_ARGS) {
    (void)rcx; (void)r8; (void)r9;
    LibcDetail::RegSaveArea regs;
    LibcDetail::FillRegSaveArea(regs, rsi, rdx, rcx, r8, r9, 0,
        xmm0, xmm1, xmm2, xmm3, xmm4, xmm5, xmm6, xmm7);
    LibcDetail::VaListLayout layout;
    std::va_list* va = LibcDetail::BuildVaList(layout, regs, 0u,
        reinterpret_cast<void*>(overflow_arg_area));
    return std::vprintf(reinterpret_cast<const char*>(rdi), *va);
}

int snprintf_nid_postfix(VA_ARGS) {
    LibcDetail::RegSaveArea regs;
    LibcDetail::FillRegSaveArea(regs, rdx, rcx, r8, r9, 0, 0,
        xmm0, xmm1, xmm2, xmm3, xmm4, xmm5, xmm6, xmm7);
    LibcDetail::VaListLayout layout;
    std::va_list* va = LibcDetail::BuildVaList(layout, regs, 0u,
        reinterpret_cast<void*>(overflow_arg_area));
    return std::vsnprintf(
        reinterpret_cast<char*>(rdi),
        static_cast<size_t>(rsi),
        reinterpret_cast<const char*>(rdx),
        *va
    );
}

int sprintf_nid_postfix(VA_ARGS) {
    LibcDetail::RegSaveArea regs;
    LibcDetail::FillRegSaveArea(regs, rsi, rdx, rcx, r8, r9, 0,
        xmm0, xmm1, xmm2, xmm3, xmm4, xmm5, xmm6, xmm7);
    LibcDetail::VaListLayout layout;
    std::va_list* va = LibcDetail::BuildVaList(layout, regs, 0u,
        reinterpret_cast<void*>(overflow_arg_area));
    return std::vsprintf(
        reinterpret_cast<char*>(rdi),
        reinterpret_cast<const char*>(rsi),
        *va
    );
}

int sscanf_nid_postfix(VA_ARGS) {
    LibcDetail::RegSaveArea regs;
    LibcDetail::FillRegSaveArea(regs, rdx, rcx, r8, r9, 0, 0,
        xmm0, xmm1, xmm2, xmm3, xmm4, xmm5, xmm6, xmm7);
    LibcDetail::VaListLayout layout;
    std::va_list* va = LibcDetail::BuildVaList(layout, regs, 0u,
        reinterpret_cast<void*>(overflow_arg_area));
    return std::vsscanf(
        reinterpret_cast<const char*>(rdi),
        reinterpret_cast<const char*>(rsi),
        *va
    );
}

int vprintf_nid_postfix(const char* str, VaList* c) {
    std::va_list* va = reinterpret_cast<std::va_list*>(c);
    return std::vprintf(str, *va);
}

int vsnprintf_nid_postfix(char* str, size_t size, const char* format, VaList* c) {
    std::va_list* va = reinterpret_cast<std::va_list*>(c);
    return std::vsnprintf(str, size, format, *va);
}

int puts_nid_postfix(const char* s) {
    return std::puts(s);
}

}
