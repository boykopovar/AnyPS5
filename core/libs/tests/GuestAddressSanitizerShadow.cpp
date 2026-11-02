#include "prx/libSceAddressSanitizer/libSceAddressSanitizer.hpp"
#include "prx/libSceAddressSanitizer/src/ShadowCheck.hpp"
#include "prx/libSceAddressSanitizer/src/ErrorReport.hpp"
#include "prx/libSceAddressSanitizer/src/Report.hpp"
#include <string>
#include <string_view>
#include <utility>
#include <array>
#include <cstdint>
#include <cstdio>
#include <exception>
#include <limits>

static void APS5_VABI TestErrorReportCallback(const char*) {}

int main() {
    {
        std::uint8_t value = 0;
        const std::array<std::pair<std::uint8_t, std::string_view>, 18> bugs{{
            {0xfa, "heap-buffer-overflow"}, {0xac, "heap-buffer-overflow"}, {0xfd, "heap-use-after-free"},
            {0xf1, "stack-buffer-underflow"}, {0xf6, "initialization-order-fiasco"}, {0xf2, "stack-buffer-overflow"},
            {0xf3, "stack-buffer-overflow"}, {0xf5, "stack-use-after-return"}, {0xf7, "use-after-poison"},
            {0xfc, "container-overflow"}, {0xf8, "stack-use-after-scope"}, {0xf9, "global-buffer-overflow"},
            {0xbb, "intra-object-overflow"}, {0xca, "dynamic-stack-buffer-overflow"}, {0xcb, "dynamic-stack-buffer-overflow"},
            {0x80, "unknown-crash"}, {0xfe, "unknown-crash"}, {0x00, "unknown-crash"}}};
        for (const auto& [magic, name] : bugs) {
            const std::array<std::uint8_t, 2> shadow{magic, 0};
            if (AsanRuntime::DescribeBug_nid_no_patch(shadow.data(), 4, value) != name || value != magic) return 140;
        }
        const std::array<std::uint8_t, 2> partial{4, 0xfa};
        if (std::string_view(AsanRuntime::DescribeBug_nid_no_patch(partial.data(), 4, value)) != "heap-buffer-overflow" || value != 0xfa) return 141;
        const std::array<std::uint8_t, 2> wide{0, 0xf7};
        if (std::string_view(AsanRuntime::DescribeBug_nid_no_patch(wide.data(), 16, value)) != "use-after-poison" || value != 0xf7) return 142;
        if (std::string_view(AsanRuntime::DescribeBug_nid_no_patch(wide.data(), 8, value)) != "unknown-crash" || value != 0) return 143;
        if (std::string_view(AsanRuntime::DescribeBug_nid_no_patch(partial.data(), 0, value)) != "unknown-crash" || value != 0) return 144;
        std::string text;
        AsanRuntime::AppendPointer_nid_no_patch(text, 0x1234);
        if (text != "0x000000001234") return 145;
        text.clear();
        AsanRuntime::AppendPointer_nid_no_patch(text, 0x123456789abcdef0ull);
        if (text != "0x123456789abcdef0") return 146;
        text.clear();
        AsanRuntime::AppendReportHeader_nid_no_patch(text, "use-after-poison", 0x30000004, 0x1000, 0x2000, 0x3000, false, 4);
        if (text != "=================================================================\n"
                    "ERROR: AddressSanitizer: use-after-poison on address 0x000030000004 at pc 0x000000001000 bp 0x000000002000 sp 0x000000003000\n"
                    "READ of size 4 at 0x000030000004 thread T-1\n") return 147;
        text.clear();
        AsanRuntime::AppendReportHeader_nid_no_patch(text, "x", 0, 0, 0, 0, true, 1);
        if (text.find("\nWRITE of size 1 at ") == std::string::npos) return 148;
        text.clear();
        AsanRuntime::AppendReportHeader_nid_no_patch(text, "x", 0, 0, 0, 0, true, 0);
        if (text.find("\nACCESS of size 0 at ") == std::string::npos) return 149;
        std::array<std::uint8_t, 16> row{};
        for (std::size_t i = 0; i < row.size(); ++i) row[i] = static_cast<std::uint8_t>(i);
        text.clear();
        AsanRuntime::AppendShadowRow_nid_no_patch(text, "=>", 0x30000000, row.data(), row.data() + 3);
        if (text != "=>0x000030000000: 00 01 02[03]04 05 06 07 08 09 0a 0b 0c 0d 0e 0f\n") return 150;
        text.clear();
        AsanRuntime::AppendShadowRow_nid_no_patch(text, "  ", 0, row.data(), row.data());
        if (text != "  0x000000000000:[00]01 02 03 04 05 06 07 08 09 0a 0b 0c 0d 0e 0f\n") return 151;
        alignas(16) std::array<std::uint8_t, 256> fake{};
        fake[100] = 0xf7;
        text.clear();
        AsanRuntime::AppendShadowMemory_nid_no_patch(text, reinterpret_cast<std::uintptr_t>(fake.data()), fake.size(), 8 * 100);
        if (text.rfind("Shadow bytes around the buggy address:\n", 0) != 0) return 152;
        if (text.find("=>0x000000000300: 00 00 00 00[f7]00") == std::string::npos) return 153;
        std::size_t rows = 0;
        for (std::size_t at = 0; at < text.size(); at = text.find('\n', at) + 1) {
            if (text.compare(at, 4, "  0x") == 0 || text.compare(at, 4, "=>0x") == 0) ++rows;
            if (text.find('\n', at) == std::string::npos) break;
        }
        if (rows != 11) return 154;
        if (!std::string_view(text).ends_with("  Right alloca redzone:    cb\n")) return 155;
        text.clear();
        AsanRuntime::AppendShadowMemory_nid_no_patch(text, reinterpret_cast<std::uintptr_t>(fake.data()), fake.size(), 0);
        if (text.rfind("Shadow bytes around the buggy address:\n=>0x000000000000:[00]", 0) != 0) return 156;
    }
    {
        if (AsanRuntime::LoadErrorReportCallback_nid_no_patch() != nullptr) return 130;
        __asan_set_error_report_callback(TestErrorReportCallback);
        if (AsanRuntime::LoadErrorReportCallback_nid_no_patch() != TestErrorReportCallback) return 131;
        __asan_set_error_report_callback(nullptr);
        if (AsanRuntime::LoadErrorReportCallback_nid_no_patch() != nullptr) return 132;
    }

    {
        if (__asan_report_present() != 0) {
            return 40;
        }
        if (__asan_get_report_pc() != nullptr) {
            return 41;
        }
        if (__asan_get_report_bp() != nullptr) {
            return 42;
        }
        if (__asan_get_report_sp() != nullptr) {
            return 43;
        }
        if (__asan_get_report_address() != nullptr) {
            return 44;
        }
        if (__asan_get_report_access_type() != 0) {
            return 45;
        }
        if (__asan_get_report_access_size() != 0) {
            return 46;
        }
        if (__asan_get_report_description() != nullptr) {
            return 47;
        }

        *AsanRuntime::CurrentErrorReport_nid_no_patch() = {
            .generic = true,
            .pc = 0x1000,
            .bp = 0x2000,
            .sp = 0x3000,
            .address = 0x4000,
            .isWrite = true,
            .accessSize = 8,
            .description = "heap-buffer-overflow",
        };

        if (__asan_report_present() != 1) {
            return 48;
        }
        if (__asan_get_report_pc() != reinterpret_cast<void*>(static_cast<std::uintptr_t>(0x1000))) {
            return 49;
        }
        if (__asan_get_report_bp() != reinterpret_cast<void*>(static_cast<std::uintptr_t>(0x2000))) {
            return 50;
        }
        if (__asan_get_report_sp() != reinterpret_cast<void*>(static_cast<std::uintptr_t>(0x3000))) {
            return 51;
        }
        if (__asan_get_report_address() != reinterpret_cast<void*>(static_cast<std::uintptr_t>(0x4000))) {
            return 52;
        }
        if (__asan_get_report_access_type() != 1) {
            return 53;
        }
        if (__asan_get_report_access_size() != 8) {
            return 54;
        }
        if (__asan_get_report_description() != AsanRuntime::CurrentErrorReport_nid_no_patch()->description) {
            return 55;
        }

        AsanRuntime::CurrentErrorReport_nid_no_patch()->isWrite = false;
        if (__asan_get_report_access_type() != 0) {
            return 56;
        }

        *AsanRuntime::CurrentErrorReport_nid_no_patch() = {};
        if (__asan_report_present() != 0) {
            return 57;
        }
    }

    if (__sanitizer_get_unmapped_bytes() != 0) return 23;

    for (std::size_t size : {std::size_t{0}, std::size_t{1}, std::size_t{4096}, std::numeric_limits<std::size_t>::max()}) {
        if (__sanitizer_get_estimated_allocated_size(size) != size) return 22;
    }

    __sanitizer_malloc_hook(nullptr, 0);
    __sanitizer_free_hook(nullptr);

    if (__ubsan_default_options() == nullptr || __ubsan_default_options()[0] != '\0') return 20;
    if (__sancov_default_options() == nullptr || __sancov_default_options()[0] != '\0') return 21;

    {
        std::size_t scale = 0;
        std::size_t offset = 0;
        __asan_get_shadow_mapping(&scale, &offset);
        if (scale != 3 || offset != (1ull << 40)) return 18;
        __asan_get_shadow_mapping(nullptr, nullptr);
        __asan_get_shadow_mapping(&scale, nullptr);
        __asan_get_shadow_mapping(nullptr, &offset);
        __asan_get_shadow_mapping(&scale, &scale);
        if (scale != offset) return 19;
    }

    {
        const std::array<std::int8_t, 4> shadow{0, 3, -8, 7};
        if (FindFirstPoisoned_nid_no_patch(shadow.data(), shadow.size(), 0, 11).has_value()) return 2;
        if (FindFirstPoisoned_nid_no_patch(shadow.data(), shadow.size(), 0, 16) != 11) return 3;
        if (FindFirstPoisoned_nid_no_patch(shadow.data(), shadow.size(), 16, 1) != 16) return 4;
        if (FindFirstPoisoned_nid_no_patch(shadow.data(), shadow.size(), 24, 8) != 31) return 5;
        if (FindFirstPoisoned_nid_no_patch(nullptr, 0, 0, 0).has_value()) return 6;
        try {
            (void)FindFirstPoisoned_nid_no_patch(shadow.data(), shadow.size(), 31, 2);
            return 7;
        } catch (const std::exception&) {
        }
        try {
            (void)FindFirstPoisoned_nid_no_patch(shadow.data(), shadow.size(), std::numeric_limits<std::size_t>::max(), 2);
            return 8;
        } catch (const std::exception&) {
        }
    }

    {
        std::array<std::int8_t, 5> shadow{0, 0, 2, 0, 0};
        UpdateStackShadow_nid_no_patch(shadow.data(), shadow.size(), 8, 10, true);
        if (shadow[0] != 0 || shadow[1] != -8 || shadow[2] != -8 || shadow[3] != 0) return 9;
        UpdateStackShadow_nid_no_patch(shadow.data(), shadow.size(), 8, 10, false);
        if (shadow[1] != 0 || shadow[2] != 2) return 10;
        try {
            UpdateStackShadow_nid_no_patch(shadow.data(), shadow.size(), 32, 9, true);
            return 11;
        } catch (const std::exception&) {
            if (shadow[4] != 0) return 12;
        }
    }
    struct Entry {
        decltype(&__asan_set_shadow_00) fn;
        std::uint8_t val;
    };
    const Entry table[] = {
        {__asan_set_shadow_00, 0x00}, {__asan_set_shadow_01, 0x01},
        {__asan_set_shadow_02, 0x02}, {__asan_set_shadow_03, 0x03},
        {__asan_set_shadow_04, 0x04}, {__asan_set_shadow_05, 0x05},
        {__asan_set_shadow_06, 0x06}, {__asan_set_shadow_07, 0x07},
        {__asan_set_shadow_f1, 0xf1}, {__asan_set_shadow_f2, 0xf2},
        {__asan_set_shadow_f3, 0xf3}, {__asan_set_shadow_f5, 0xf5},
        {__asan_set_shadow_f8, 0xf8}
    };
    for (const auto& e : table) {
        try {
            e.fn(0, 8);
            std::fprintf(stderr, "Null address did not throw\n");
            return 1;
        } catch (const std::exception&) {
        }
    }
    for (const auto& e : table) {
        try {
            e.fn(std::numeric_limits<std::uintptr_t>::max() - 1, 4);
            std::fprintf(stderr, "Wrapped address range did not throw\n");
            return 1;
        } catch (const std::exception&) {
        }
    }
    std::array<std::uint8_t, 80> buf;
    for (const auto& e : table) {
        for (std::size_t off : {0ULL, 1ULL, 7ULL, 16ULL}) {
            for (std::size_t len : {0ULL, 1ULL, 7ULL, 8ULL, 31ULL, 63ULL}) {
                buf.fill(0xAA);
                auto addr = reinterpret_cast<std::uintptr_t>(buf.data() + off);
                e.fn(addr, len);
                e.fn(addr, len);
                for (std::size_t i = 0; i < buf.size(); ++i) {
                    std::uint8_t exp = (i >= off && i < off + len) ? e.val : 0xAA;
                    if (buf[i] != exp) {
                        std::fprintf(stderr, "Shadow byte verification error at %zu\n", i);
                        return 1;
                    }
                }
            }
        }
    }
    return 0;
}
