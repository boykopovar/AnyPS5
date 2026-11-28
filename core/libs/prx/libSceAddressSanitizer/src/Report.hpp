// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#pragma once
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <string>

namespace AsanRuntime {

inline const char* DescribeBug_nid_no_patch(const std::uint8_t* shadow, std::uintptr_t accessSize, std::uint8_t& shadowValue) {
    shadowValue = 0;
    if (accessSize == 0) {
        return "unknown-crash";
    }
    const std::uint8_t* p = shadow;
    if (*p == 0 && accessSize > 8) {
        ++p;
    }
    if (*p > 0 && *p < 128) {
        ++p;
    }
    shadowValue = *p;
    switch (shadowValue) {
    case 0xfa:
    case 0xac:
        return "heap-buffer-overflow";
    case 0xfd:
        return "heap-use-after-free";
    case 0xf1:
        return "stack-buffer-underflow";
    case 0xf6:
        return "initialization-order-fiasco";
    case 0xf2:
    case 0xf3:
        return "stack-buffer-overflow";
    case 0xf5:
        return "stack-use-after-return";
    case 0xf7:
        return "use-after-poison";
    case 0xfc:
        return "container-overflow";
    case 0xf8:
        return "stack-use-after-scope";
    case 0xf9:
        return "global-buffer-overflow";
    case 0xbb:
        return "intra-object-overflow";
    case 0xca:
    case 0xcb:
        return "dynamic-stack-buffer-overflow";
    default:
        return "unknown-crash";
    }
}

inline void AppendPointer_nid_no_patch(std::string& out, std::uintptr_t value) {
    char buffer[32];
    std::snprintf(buffer, sizeof(buffer), "0x%012llx", static_cast<unsigned long long>(value));
    out += buffer;
}

inline void AppendReportHeader_nid_no_patch(std::string& out, const char* bug, std::uintptr_t address, std::uintptr_t pc, std::uintptr_t bp, std::uintptr_t sp, bool isWrite, std::uintptr_t accessSize) {
    out += "=================================================================\n";
    out += "ERROR: AddressSanitizer: ";
    out += bug;
    out += " on address ";
    AppendPointer_nid_no_patch(out, address);
    out += " at pc ";
    AppendPointer_nid_no_patch(out, pc);
    out += " bp ";
    AppendPointer_nid_no_patch(out, bp);
    out += " sp ";
    AppendPointer_nid_no_patch(out, sp);
    out += "\n";
    out += accessSize == 0 ? "ACCESS" : (isWrite ? "WRITE" : "READ");
    out += " of size ";
    out += std::to_string(accessSize);
    out += " at ";
    AppendPointer_nid_no_patch(out, address);
    out += " thread T-1\n";
}

inline void AppendShadowRow_nid_no_patch(std::string& out, const char* prefix, std::uintptr_t rowApplicationAddress, const std::uint8_t* bytes, const std::uint8_t* guilty) {
    static constexpr char digits[] = "0123456789abcdef";
    out += prefix;
    AppendPointer_nid_no_patch(out, rowApplicationAddress);
    out += ":";
    for (std::size_t i = 0; i < 16; ++i) {
        const std::uint8_t* p = bytes + i;
        out += p == guilty ? "[" : (p - 1 == guilty && i != 0) ? "" : " ";
        out += digits[*p >> 4];
        out += digits[*p & 15];
        if (p == guilty) {
            out += "]";
        }
    }
    out += "\n";
}

inline void AppendShadowLegend_nid_no_patch(std::string& out) {
    out += "Shadow byte legend (one shadow byte represents 8 application bytes):\n"
           "  Addressable:           00\n"
           "  Partially addressable: 01 02 03 04 05 06 07 \n"
           "  Heap left redzone:       fa\n"
           "  Freed heap region:       fd\n"
           "  Stack left redzone:      f1\n"
           "  Stack mid redzone:       f2\n"
           "  Stack right redzone:     f3\n"
           "  Stack after return:      f5\n"
           "  Stack use after scope:   f8\n"
           "  Global redzone:          f9\n"
           "  Global init order:       f6\n"
           "  Poisoned by user:        f7\n"
           "  Container overflow:      fc\n"
           "  Array cookie:            ac\n"
           "  Intra object redzone:    bb\n"
           "  ASan internal:           fe\n"
           "  Left alloca redzone:     ca\n"
           "  Right alloca redzone:    cb\n";
}

inline void AppendShadowMemory_nid_no_patch(std::string& out, std::uintptr_t shadowBase, std::uintptr_t shadowSize, std::uintptr_t address) {
    const std::uintptr_t shadowAddress = shadowBase + (address >> 3);
    const std::uintptr_t aligned = shadowAddress & ~static_cast<std::uintptr_t>(15);
    out += "Shadow bytes around the buggy address:\n";
    for (int i = -5; i <= 5; ++i) {
        const std::uintptr_t row = aligned + static_cast<std::uintptr_t>(static_cast<std::intptr_t>(i) * 16);
        if (row < shadowBase || row + 16 > shadowBase + shadowSize) {
            continue;
        }
        AppendShadowRow_nid_no_patch(out, i == 0 ? "=>" : "  ", (row - shadowBase) << 3, reinterpret_cast<const std::uint8_t*>(row), reinterpret_cast<const std::uint8_t*>(shadowAddress));
    }
    AppendShadowLegend_nid_no_patch(out);
}

}  // namespace AsanRuntime
