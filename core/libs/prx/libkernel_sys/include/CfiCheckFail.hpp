/* ReportCfiCheckFail is adapted for AnyPS5 from compiler-rt/lib/ubsan/ubsan_handlers.cpp and
 * ubsan_handlers_cxx.cpp of LLVM 18.1.8
 * (https://github.com/llvm/llvm-project/tree/llvmorg-18.1.8/compiler-rt/lib/ubsan).
 * Altered: it reads the guest LP64 check data, prints without a symbolizer or summary line,
 * does not read the dynamic type behind a valid vtable, and throws where compiler-rt calls Die().
 *
 * Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
 * See https://llvm.org/LICENSE.txt for license information.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#ifndef CORE_LIBS_PRX_LIBKERNEL_SYS_INCLUDE_CFICHECKFAIL_HPP
#define CORE_LIBS_PRX_LIBKERNEL_SYS_INCLUDE_CFICHECKFAIL_HPP

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <iterator>
#include <stdexcept>
#include <string>

namespace Ubsan {

struct SourceLocation {
    const char* file;
    std::uint32_t line;
    std::uint32_t column;
};

struct TypeDescriptor {
    std::uint16_t kind;
    std::uint16_t info;
    char name[1];
};

struct CfiCheckFailData {
    std::uint8_t kind;
    SourceLocation location;
    const TypeDescriptor* type;
};

static_assert(sizeof(SourceLocation) == 16 && offsetof(SourceLocation, column) == 12);
static_assert(offsetof(TypeDescriptor, name) == 4);
static_assert(offsetof(CfiCheckFailData, location) == 8 && offsetof(CfiCheckFailData, type) == 24);
static_assert(sizeof(CfiCheckFailData) == 32);

inline std::string RenderLocation(const SourceLocation& location, std::uint32_t column) {
    if (!location.file) return "<unknown>";
    std::string text = location.file;
    if (location.line) {
        text += ':' + std::to_string(location.line);
        if (column) text += ':' + std::to_string(column);
    }
    return text;
}

inline void ReportCfiCheckFail(CfiCheckFailData* data, std::uintptr_t value, std::uintptr_t validVtable, bool fatal) {
    if (!data) throw std::invalid_argument("__ubsan_handle_cfi_check_fail: null check data");
    static constexpr const char* Kinds[] = {
        "virtual call",
        "non-virtual call",
        "base-to-derived cast",
        "cast to unrelated type",
        "indirect function call",
        "non-virtual pointer to member function call",
        "virtual pointer to member function call",
    };
    if (data->kind >= std::size(Kinds))
        throw std::runtime_error("__ubsan_handle_cfi_check_fail: unknown check kind " + std::to_string(data->kind));
    const bool call = data->kind == 4 || data->kind == 5;
    const std::uint32_t column = std::atomic_ref<std::uint32_t>(data->location.column).exchange(~std::uint32_t{0}, std::memory_order_relaxed);
    if (column != ~std::uint32_t{0}) {
        std::string report = RenderLocation(data->location, column) + ": runtime error: control flow integrity check for type '" +
            (data->type ? std::string(data->type->name) : std::string("<unknown>")) + "' failed during " + Kinds[data->kind];
        if (call) {
            report += "\n<unknown>: note: (unknown) defined here\n";
        } else {
            char address[32];
            std::snprintf(address, sizeof(address), "0x%012llx", static_cast<unsigned long long>(value));
            report += std::string(" (vtable address ") + address + ")\n<unknown>: note: " +
                (validVtable ? "vtable is of an unknown type" : "invalid vtable") + "\n";
        }
        std::fputs(report.c_str(), stderr);
        std::fflush(stderr);
    }
    if (fatal) throw std::runtime_error("UndefinedBehaviorSanitizer: control flow integrity check failed");
}

}

#endif
