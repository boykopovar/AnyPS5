#include "prx/libSceAddressSanitizer/include/Api.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <csignal>
#include <functional>
#include <string>
#include <sys/wait.h>
#include <unistd.h>

namespace {

void Require(bool value, const char* what) {
    if (value) return;
    std::fprintf(stdout, "failed: %s\n", what);
    std::fflush(stdout);
    std::abort();
}

struct Outcome {
    std::string output;
    int status = 0;
};

Outcome Run(const char* options, const std::function<void()>& body) {
    int channel[2] = {-1, -1};
    Require(::pipe(channel) == 0, "pipe");
    std::fflush(nullptr);
    const pid_t child = ::fork();
    Require(child >= 0, "fork");
    if (child == 0) {
        ::close(channel[0]);
        if (::dup2(channel[1], 2) != 2) ::_exit(70);
        ::close(channel[1]);
        if (options != nullptr) ::setenv("APS5_GUEST_ASAN_OPTIONS", options, 1);
        else ::unsetenv("APS5_GUEST_ASAN_OPTIONS");
        __asan_init();
        body();
        std::fflush(nullptr);
        ::_exit(0);
    }
    ::close(channel[1]);
    Outcome outcome;
    char buffer[512];
    for (ssize_t count; (count = ::read(channel[0], buffer, sizeof(buffer))) > 0;) outcome.output.append(buffer, static_cast<std::size_t>(count));
    ::close(channel[0]);
    Require(::waitpid(child, &outcome.status, 0) == child, "waitpid");
    return outcome;
}

bool ExitedWith(const Outcome& outcome, int code) {
    return WIFEXITED(outcome.status) && WEXITSTATUS(outcome.status) == code;
}

void Fail(const Outcome& outcome, const char* what, const std::string& detail) {
    std::fprintf(stdout, "%s: %s; status %d, output:\n%s---\n", what, detail.c_str(), outcome.status, outcome.output.c_str());
    std::fflush(stdout);
    std::abort();
}

void Expect(const Outcome& outcome, int code, std::initializer_list<std::string> fragments, const char* what) {
    if (!ExitedWith(outcome, code)) Fail(outcome, what, "wrong exit status, expected " + std::to_string(code));
    for (const auto& fragment : fragments)
        if (outcome.output.find(fragment) == std::string::npos) Fail(outcome, what, "missing \"" + fragment + "\"");
}

void ExpectAbort(const Outcome& outcome, const char* what) {
    if (!WIFSIGNALED(outcome.status) || WTERMSIG(outcome.status) != SIGABRT) Fail(outcome, what, "expected an abort");
}

void ExpectClean(const Outcome& outcome, const char* what) {
    if (!ExitedWith(outcome, 0) || !outcome.output.empty()) Fail(outcome, what, "expected a silent clean exit");
}

std::string Hex(const void* pointer) {
    char text[32];
    std::snprintf(text, sizeof(text), "0x%012zx", static_cast<std::size_t>(reinterpret_cast<std::uintptr_t>(pointer)));
    return text;
}

std::string Raw(const void* pointer) {
    char text[32];
    std::snprintf(text, sizeof(text), "0x%zx", static_cast<std::size_t>(reinterpret_cast<std::uintptr_t>(pointer)));
    return text;
}

std::uintptr_t ShadowAddress(const void* address) {
    return (reinterpret_cast<std::uintptr_t>(address) >> 3) + (std::uintptr_t{1} << 40);
}

std::uint8_t ShadowByte(const void* address) {
    return *reinterpret_cast<const std::uint8_t*>((reinterpret_cast<std::uintptr_t>(address) >> 3) + (std::uintptr_t{1} << 40));
}

alignas(64) unsigned char arena[512];

void Poisoning() {
    ExpectClean(Run(nullptr, [] {
                    std::uintptr_t scale = 0;
                    std::uintptr_t offset = 0;
                    __asan_get_shadow_mapping(&scale, &offset);
                    Require(scale == 3 && offset == (std::uintptr_t{1} << 40), "PS shadow mapping is scale 3 at 1 << 40");
                    __asan_get_shadow_mapping(nullptr, nullptr);
                    Require(__asan_address_is_poisoned(arena) == 0, "fresh memory is addressable");
                    __asan_poison_memory_region(arena + 16, 32);
                    Require(ShadowByte(arena + 16) == 0xf7 && ShadowByte(arena + 40) == 0xf7 && ShadowByte(arena + 48) == 0, "aligned user poison writes f7");
                    Require(__asan_address_is_poisoned(arena + 16) == 1 && __asan_address_is_poisoned(arena + 47) == 1, "poisoned bytes report as poisoned");
                    Require(__asan_address_is_poisoned(arena + 15) == 0 && __asan_address_is_poisoned(arena + 48) == 0, "neighbours stay addressable");
                    Require(__asan_region_is_poisoned(reinterpret_cast<std::uintptr_t>(arena), 64) == reinterpret_cast<std::uintptr_t>(arena + 16), "first poisoned byte of a region");
                    Require(__asan_region_is_poisoned(reinterpret_cast<std::uintptr_t>(arena), 16) == 0, "clean region");
                    Require(__asan_region_is_poisoned(reinterpret_cast<std::uintptr_t>(arena), 0) == 0, "empty region");
                    __asan_unpoison_memory_region(arena + 16, 32);
                    Require(ShadowByte(arena + 16) == 0 && ShadowByte(arena + 40) == 0, "unpoison clears the shadow");
                    __asan_poison_memory_region(arena + 67, 5);
                    Require(ShadowByte(arena + 64) == 3, "poisoning a granule tail keeps the addressable prefix");
                    __asan_poison_memory_region(arena + 67, 5);
                    __asan_poison_memory_region(arena + 75, 13);
                    Require(ShadowByte(arena + 72) == 3 && ShadowByte(arena + 80) == 0xf7, "poisoning a granule tail keeps its prefix");
                    Require(__asan_address_is_poisoned(arena + 74) == 0 && __asan_address_is_poisoned(arena + 75) == 1, "partial granule boundary");
                    __asan_unpoison_memory_region(arena + 72, 6);
                    Require(ShadowByte(arena + 72) == 6, "progressive unpoison extends the prefix");
                    __asan_unpoison_memory_region(arena + 72, 2);
                    Require(ShadowByte(arena + 72) == 6, "unpoison never shrinks the addressable prefix");
                    __asan_unpoison_memory_region(arena + 72, 16);
                    Require(ShadowByte(arena + 72) == 0 && ShadowByte(arena + 80) == 0, "full unpoison");
                    __asan_poison_memory_region(arena + 100, 20);
                    Require(ShadowByte(arena + 96) == 4 && ShadowByte(arena + 104) == 0xf7 && ShadowByte(arena + 112) == 0xf7 && ShadowByte(arena + 120) == 0, "unaligned start, aligned end");
                    __asan_unpoison_memory_region(arena + 96, 32);
                    Require(ShadowByte(arena + 96) == 0 && ShadowByte(arena + 112) == 0, "unpoison from an aligned start");
                }),
                "poisoning");
    ExpectClean(Run("allow_user_poisoning=0", [] {
                    __asan_poison_memory_region(arena, 64);
                    Require(ShadowByte(arena) == 0, "allow_user_poisoning=0 ignores user poisoning");
                }),
                "user poisoning disabled");
}

void Accesses() {
    ExpectClean(Run(nullptr, [] {
                    __asan_load1(reinterpret_cast<std::uintptr_t>(arena));
                    __asan_store16(reinterpret_cast<std::uintptr_t>(arena));
                    __asan_loadN(reinterpret_cast<std::uintptr_t>(arena), 64);
                    __asan_poison_memory_region(arena + 13, 3);
                    __asan_load4(reinterpret_cast<std::uintptr_t>(arena + 8));
                    __asan_load1(reinterpret_cast<std::uintptr_t>(arena + 12));
                    __asan_exp_store2(reinterpret_cast<std::uintptr_t>(arena + 11), 7);
                    __asan_storeN(reinterpret_cast<std::uintptr_t>(arena), 13);
                }),
                "clean accesses");
    Expect(Run(nullptr, [] {
               __asan_poison_memory_region(arena + 13, 3);
               __asan_load4(reinterpret_cast<std::uintptr_t>(arena + 10));
               std::puts("survived");
           }),
           1,
           {"=================================================================\n", "ERROR: AddressSanitizer: unknown-crash on address " + Hex(arena + 10) + " at pc 0x",
            "READ of size 4 at " + Hex(arena + 10) + " thread T-1\n", "    #0 0x", "Address " + Hex(arena + 10) + " is a wild pointer inside of access range of size 0x000000000004.\n",
            "SUMMARY: AddressSanitizer: unknown-crash (<unknown module>)\n", "Shadow bytes around the buggy address:\n", "=>", "[05]", "Shadow byte legend (one shadow byte represents 8 application bytes):\n",
            "  Poisoned by user:        f7\n", "ABORTING\n"},
           "partial granule load");
    Expect(Run(nullptr, [] {
               __asan_poison_memory_region(arena + 24, 8);
               __asan_store16(reinterpret_cast<std::uintptr_t>(arena + 16));
           }),
           1, {"use-after-poison on address " + Hex(arena + 16), "WRITE of size 16 at " + Hex(arena + 16)}, "16-byte store checks the second granule");
    Expect(Run("print_legend=0:print_summary=0:exitcode=9", [] {
               __asan_poison_memory_region(arena + 32, 32);
               __asan_loadN_noabort(reinterpret_cast<std::uintptr_t>(arena + 20), 20);
           }),
           9, {"use-after-poison on address " + Hex(arena + 32), "READ of size 20"}, "loadN reports the first bad byte");
    const auto quiet = Run("print_legend=0:print_summary=0", [] {
        __asan_poison_memory_region(arena, 8);
        __asan_load1(reinterpret_cast<std::uintptr_t>(arena));
    });
    if (quiet.output.find("Shadow byte legend") != std::string::npos || quiet.output.find("SUMMARY") != std::string::npos) Fail(quiet, "flags", "legend or summary printed");
    Expect(Run(nullptr, [] { __asan_report_store8(reinterpret_cast<std::uintptr_t>(arena)); }), 1, {"unknown-crash on address " + Hex(arena), "WRITE of size 8"}, "report on clean memory");
    Expect(Run(nullptr, [] { __asan_load8((std::uintptr_t{1} << 40) + 64); }), 1, {"unknown-crash on address 0x010000000040"}, "an access into the shadow itself");
    Expect(Run(nullptr, [] { __asan_report_error(0x1234, 0x5678, 0x9abc, reinterpret_cast<std::uintptr_t>(arena), 1, 3, 0); }), 1,
           {"at pc 0x000000001234 bp 0x000000005678 sp 0x000000009abc", "WRITE of size 3"}, "report_error passes the caller's registers");
}

std::string callbackReport;
std::uintptr_t callbackAddress = 0;
std::uintptr_t callbackSize = 0;
int callbackWrite = -1;
int callbackPresent = -1;
std::string callbackDescription;
int callbackCount = 0;

void APS5_VABI OnError(const char* report) {
    ++callbackCount;
    callbackReport = report;
    callbackPresent = __asan_report_present();
    callbackAddress = __asan_get_report_address();
    callbackSize = __asan_get_report_access_size();
    callbackWrite = __asan_get_report_access_type();
    callbackDescription = __asan_get_report_description();
    Require(__asan_get_report_pc() != 0 && __asan_get_report_sp() != 0, "report registers are recorded");
}

volatile int afterLoad = 0;

[[gnu::noinline]] void LoadOnce(std::uintptr_t address) {
    __asan_load2_noabort(address);
    afterLoad = 0;
}

void APS5_VABI OnDeath() {
    std::fputs("death callback ran\n", stderr);
}

void Recovery() {
    Expect(Run("halt_on_error=0", [] {
               __asan_set_error_report_callback(OnError);
               __asan_poison_memory_region(arena + 40, 8);
               for (int i = 0; i < 3; ++i) LoadOnce(reinterpret_cast<std::uintptr_t>(arena + 40));
               Require(callbackCount == 1, "suppress_equal_pcs reports one pc once");
               Require(callbackPresent == 1 && callbackAddress == reinterpret_cast<std::uintptr_t>(arena + 40) && callbackSize == 2 && callbackWrite == 0, "report fields in the callback");
               Require(callbackDescription == "use-after-poison", "report description");
               Require(callbackReport.find("ERROR: AddressSanitizer: use-after-poison") != std::string::npos, "callback receives the report text");
               Require(callbackReport.find("ABORTING") == std::string::npos, "recoverable report does not abort");
               Require(__asan_report_present() == 0 && __asan_get_report_address() == 0 && std::strcmp(__asan_get_report_description(), "") == 0, "report is cleared after recovery");
               __asan_store1_noabort(reinterpret_cast<std::uintptr_t>(arena + 41));
               Require(callbackCount == 2, "a different pc reports again");
               std::fputs("recovered\n", stderr);
           }),
           0, {"recovered\n"}, "halt_on_error=0");
    Expect(Run("halt_on_error=0:suppress_equal_pcs=0", [] {
               __asan_set_error_report_callback(OnError);
               __asan_poison_memory_region(arena + 40, 8);
               for (int i = 0; i < 3; ++i) LoadOnce(reinterpret_cast<std::uintptr_t>(arena + 40));
               Require(callbackCount == 3, "suppress_equal_pcs=0 reports every time");
           }),
           0, {}, "suppress_equal_pcs=0");
    Expect(Run("halt_on_error=0", [] {
               __asan_poison_memory_region(arena + 40, 8);
               __asan_load1(reinterpret_cast<std::uintptr_t>(arena + 40));
               std::fputs("survived\n", stderr);
           }),
           1, {"ABORTING\n"}, "fatal callbacks abort even with halt_on_error=0");
    const auto death = Run(nullptr, [] {
        __sanitizer_set_death_callback(OnDeath);
        __asan_report_load1(reinterpret_cast<std::uintptr_t>(arena));
    });
    Expect(death, 1, {"ABORTING\ndeath callback ran\n"}, "death callback runs after the report");
    ExpectAbort(Run("halt_on_error=maybe", [] { __asan_poison_memory_region(arena, 8); }), "a malformed option is rejected");
    ExpectAbort(Run("exitcode=4294967297", [] { __asan_report_load1(reinterpret_cast<std::uintptr_t>(arena)); }), "an out-of-range integer option is rejected");
    ExpectAbort(Run("detect_leaks=1", [] { __asan_poison_memory_region(arena, 8); }), "an unsupported option is rejected");
}

struct GlobalV8 {
    std::uintptr_t beg;
    std::uintptr_t size;
    std::uintptr_t sizeWithRedzone;
    const char* name;
    const char* moduleName;
    std::uintptr_t hasDynamicInit;
    std::uintptr_t location;
    std::uintptr_t odrIndicator;
};

struct GlobalV6 {
    std::uintptr_t beg;
    std::uintptr_t size;
    std::uintptr_t sizeWithRedzone;
    const char* name;
    const char* moduleName;
    std::uintptr_t hasDynamicInit;
    std::uintptr_t location;
};

alignas(64) char greeting[64] = "hello";
alignas(64) unsigned char table[96];
alignas(64) char letters[64] = {'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h'};
alignas(64) GlobalV8 descriptors[2];
alignas(64) GlobalV6 oldDescriptors[2];
const char moduleName[] = "game.elf";

void Globals() {
    ExpectClean(Run(nullptr, [] {
                    descriptors[0] = GlobalV8{reinterpret_cast<std::uintptr_t>(greeting), 6, 64, "greeting", moduleName, 0, 0, 0};
                    descriptors[1] = GlobalV8{reinterpret_cast<std::uintptr_t>(table), 37, 96, "table", moduleName, 0, 0, 0};
                    __asan_version_mismatch_check_v8();
                    __asan_register_globals(descriptors, 2);
                    Require(ShadowByte(greeting) == 6 && ShadowByte(greeting + 8) == 0xf9 && ShadowByte(greeting + 56) == 0xf9, "string global redzone");
                    Require(ShadowByte(table + 32) == 5 && ShadowByte(table + 40) == 0xf9 && ShadowByte(table + 88) == 0xf9 && ShadowByte(table + 24) == 0, "table global redzone");
                    Require(ShadowByte(descriptors) == 0xf9 && ShadowByte(reinterpret_cast<unsigned char*>(descriptors) + 120) == 0xf9, "descriptor metadata is poisoned");
                    __asan_unregister_globals(descriptors, 2);
                    Require(ShadowByte(greeting + 8) == 0 && ShadowByte(table + 40) == 0 && ShadowByte(descriptors) == 0, "unregister unpoisons");
                }),
                "register globals");
    Expect(Run(nullptr, [] {
               descriptors[0] = GlobalV8{reinterpret_cast<std::uintptr_t>(greeting), 6, 64, "greeting", moduleName, 0, 0, 0};
               __asan_register_globals(descriptors, 1);
               __asan_load1(reinterpret_cast<std::uintptr_t>(greeting + 6));
           }),
           1,
           {"global-buffer-overflow on address " + Hex(greeting + 6), Hex(greeting + 6) + " is located 0 bytes after global variable 'greeting' defined in 'game.elf' (" + Raw(greeting) + ") of size 6\n", "  'greeting' is ascii string 'hello'\n", "  Global redzone:          f9\n", "SUMMARY: AddressSanitizer: global-buffer-overflow"},
           "global overflow description");
    Expect(Run(nullptr, [] {
               descriptors[0] = GlobalV8{reinterpret_cast<std::uintptr_t>(table), 37, 96, "table", moduleName, 0, 0, 0};
               __asan_register_globals(descriptors, 1);
               __asan_describe_address(reinterpret_cast<std::uintptr_t>(table) - 8);
               __asan_describe_address(reinterpret_cast<std::uintptr_t>(arena));
           }),
           0,
           {Hex(table - 8) + " is located 8 bytes before global variable 'table' defined in 'game.elf'", "of size 37\n",
            "AddressSanitizer can not describe address in more detail (wild memory access suspected).\n"},
           "describe address");
    const auto unterminated = Run(nullptr, [] {
        descriptors[0] = GlobalV8{reinterpret_cast<std::uintptr_t>(letters), 8, 64, "letters", moduleName, 0, 0, 0};
        __asan_register_globals(descriptors, 1);
        __asan_load1(reinterpret_cast<std::uintptr_t>(letters + 8));
    });
    Expect(unterminated, 1, {"global-buffer-overflow on address " + Hex(letters + 8), "global variable 'letters'"}, "unterminated global");
    if (unterminated.output.find("is ascii string") != std::string::npos) Fail(unterminated, "unterminated global", "described as an ascii string");
    ExpectClean(Run(nullptr, [] {
                    oldDescriptors[0] = GlobalV6{reinterpret_cast<std::uintptr_t>(greeting), 6, 64, "greeting", moduleName, 0, 0};
                    oldDescriptors[1] = GlobalV6{reinterpret_cast<std::uintptr_t>(table), 37, 96, "table", moduleName, 0, 0};
                    __asan_version_mismatch_check_v6();
                    __asan_register_globals(oldDescriptors, 2);
                    Require(ShadowByte(greeting + 8) == 0xf9 && ShadowByte(table + 40) == 0xf9, "v6 modules use the 56-byte descriptor");
                    __asan_version_mismatch_check_v7();
                }),
                "v6 globals");
    ExpectClean(Run(nullptr, [] {
                    std::uintptr_t flag = 0;
                    descriptors[0] = GlobalV8{reinterpret_cast<std::uintptr_t>(greeting), 6, 64, "greeting", moduleName, 0, 0, 0};
                    descriptors[1] = GlobalV8{reinterpret_cast<std::uintptr_t>(table), 37, 96, "table", moduleName, 0, 0, 0};
                    __asan_register_elf_globals(&flag, descriptors, descriptors + 2);
                    Require(flag == 1 && ShadowByte(table + 40) == 0xf9, "ELF globals register once");
                    __asan_register_elf_globals(&flag, nullptr, nullptr);
                    __asan_unregister_elf_globals(&flag, descriptors, descriptors + 2);
                    Require(flag == 0 && ShadowByte(table + 40) == 0, "ELF globals unregister");
                    __asan_unregister_elf_globals(&flag, descriptors, descriptors + 2);
                }),
                "ELF globals");
    ExpectClean(Run("report_globals=0", [] {
                    descriptors[0] = GlobalV8{reinterpret_cast<std::uintptr_t>(greeting), 6, 64, "greeting", moduleName, 0, 0, 0};
                    __asan_register_globals(descriptors, 1);
                    Require(ShadowByte(greeting + 8) == 0, "report_globals=0 skips registration");
                }),
                "report_globals=0");
    ExpectClean(Run("check_initialization_order=1", [] {
                    static const char other[] = "other.elf";
                    descriptors[0] = GlobalV8{reinterpret_cast<std::uintptr_t>(table), 37, 96, "table", moduleName, 1, 0, 0};
                    __asan_register_globals(descriptors, 1);
                    __asan_before_dynamic_init(other);
                    Require(ShadowByte(table) == 0xf6 && ShadowByte(table + 88) == 0xf6, "foreign dynamic-init globals are poisoned");
                    __asan_after_dynamic_init();
                    Require(ShadowByte(table) == 0 && ShadowByte(table + 32) == 5 && ShadowByte(table + 40) == 0xf9, "after init restores the redzones");
                    __asan_before_dynamic_init(moduleName);
                    Require(ShadowByte(table) == 0, "own module globals stay addressable");
                    __asan_before_dynamic_init(other);
                    Require(ShadowByte(table) == 0, "initialized globals are not poisoned again");
                }),
                "init order");
    ExpectClean(Run(nullptr, [] {
                    descriptors[0] = GlobalV8{reinterpret_cast<std::uintptr_t>(table), 37, 96, "table", moduleName, 1, 0, 0};
                    __asan_register_globals(descriptors, 1);
                    __asan_before_dynamic_init("other.elf");
                    Require(ShadowByte(table) == 0, "init order checking is off by default");
                }),
                "init order off");
}

void Stack() {
    Expect(Run(nullptr, [] {
               __asan_poison_stack_memory(reinterpret_cast<std::uintptr_t>(arena), 13);
               Require(ShadowByte(arena) == 0xf8 && ShadowByte(arena + 8) == 0, "use-after-scope poisons whole granules only");
               __asan_unpoison_stack_memory(reinterpret_cast<std::uintptr_t>(arena), 13);
               Require(ShadowByte(arena) == 0, "scope unpoison");
               __asan_set_shadow_05(ShadowAddress(arena + 8), 1);
               __asan_poison_stack_memory(reinterpret_cast<std::uintptr_t>(arena), 13);
               Require(ShadowByte(arena + 8) == 0xf8, "a short tail granule is poisoned");
               __asan_unpoison_stack_memory(reinterpret_cast<std::uintptr_t>(arena), 13);
               Require(ShadowByte(arena + 8) == 5, "unpoison keeps the larger tail");
               __asan_set_shadow_00(ShadowAddress(arena), 64);
               __asan_poison_stack_memory(reinterpret_cast<std::uintptr_t>(arena), 16);
               __asan_load1(reinterpret_cast<std::uintptr_t>(arena + 3));
           }),
           1, {"stack-use-after-scope on address " + Hex(arena + 3), "  Stack use after scope:   f8\n"}, "stack use after scope");
    struct Case {
        void(APS5_VABI* set)(std::uintptr_t, std::uintptr_t);
        const char* bug;
    };
    static const Case cases[] = {{__asan_set_shadow_f1, "stack-buffer-underflow"}, {__asan_set_shadow_f2, "stack-buffer-overflow"}, {__asan_set_shadow_f3, "stack-buffer-overflow"},
                                 {__asan_set_shadow_f5, "stack-use-after-return"}, {__asan_set_shadow_f8, "stack-use-after-scope"}};
    for (const auto& entry : cases) {
        static const Case* current = nullptr;
        current = &entry;
        Expect(Run(nullptr, [] {
                   current->set(ShadowAddress(arena + 64), 2);
                   __asan_store4(reinterpret_cast<std::uintptr_t>(arena + 72));
               }),
               1, {std::string(entry.bug) + " on address " + Hex(arena + 72)}, entry.bug);
    }
    ExpectClean(Run(nullptr, [] {
                    __asan_set_shadow_01(ShadowAddress(arena), 1);
                    __asan_set_shadow_02(ShadowAddress(arena) + 1, 1);
                    __asan_set_shadow_03(ShadowAddress(arena) + 2, 1);
                    __asan_set_shadow_04(ShadowAddress(arena) + 3, 1);
                    __asan_set_shadow_06(ShadowAddress(arena) + 4, 1);
                    __asan_set_shadow_07(ShadowAddress(arena) + 5, 1);
                    Require(ShadowByte(arena) == 1 && ShadowByte(arena + 8) == 2 && ShadowByte(arena + 16) == 3 && ShadowByte(arena + 24) == 4 && ShadowByte(arena + 32) == 6 &&
                                ShadowByte(arena + 40) == 7,
                            "set_shadow writes partial values");
                    __asan_load1(reinterpret_cast<std::uintptr_t>(arena + 40 + 6));
                }),
                "set_shadow partial values");
    Expect(Run(nullptr, [] {
               __asan_alloca_poison(reinterpret_cast<std::uintptr_t>(arena + 64), 13);
               Require(ShadowByte(arena + 32) == 0xca && ShadowByte(arena + 56) == 0xca, "left alloca redzone");
               Require(ShadowByte(arena + 64) == 0 && ShadowByte(arena + 72) == 5 && ShadowByte(arena + 80) == 0xcb && ShadowByte(arena + 120) == 0xcb, "right alloca redzone");
               Require(ShadowByte(arena + 128) == 0, "alloca redzone stops after 32 bytes");
               __asan_allocas_unpoison(reinterpret_cast<std::uintptr_t>(arena + 32), reinterpret_cast<std::uintptr_t>(arena + 128));
               Require(ShadowByte(arena + 32) == 0 && ShadowByte(arena + 120) == 0, "allocas unpoison");
               __asan_allocas_unpoison(0, 64);
               __asan_allocas_unpoison(128, 64);
               __asan_alloca_poison(reinterpret_cast<std::uintptr_t>(arena + 64), 16);
               __asan_load1(reinterpret_cast<std::uintptr_t>(arena + 80));
           }),
           1, {"dynamic-stack-buffer-overflow on address " + Hex(arena + 80), "  Right alloca redzone:    cb\n"}, "alloca");
}

void Intrinsics() {
    ExpectClean(Run(nullptr, [] {
                    char source[16] = "abcdefghijklmno";
                    char target[16] = {};
                    Require(__asan_memcpy(target, source, 16) == target && std::memcmp(target, source, 16) == 0, "memcpy copies");
                    Require(__asan_memcpy(target, target, 16) == target, "memcpy to itself is not an overlap");
                    Require(__asan_memmove(target + 1, target, 8) == target + 1 && std::memcmp(target + 1, "abcdefgh", 8) == 0, "memmove allows overlap");
                    Require(__asan_memset(target, 'z', 4) == target && std::memcmp(target, "zzzz", 4) == 0, "memset fills");
                    __asan_memcpy(target, source, 0);
                }),
                "intrinsics");
    Expect(Run(nullptr, [] { __asan_memcpy(arena + 8, arena, 16); }), 1,
           {"ERROR: AddressSanitizer: memcpy-param-overlap: memory ranges [" + Hex(arena + 8) + "," + Hex(arena + 24) + ") and [" + Hex(arena) + ", " + Hex(arena + 16) + ") overlap\n",
            "SUMMARY: AddressSanitizer: memcpy-param-overlap"},
           "memcpy overlap");
    Expect(Run(nullptr, [] {
               __asan_poison_memory_region(arena + 32, 8);
               __asan_memset(arena + 24, 0, 16);
           }),
           1, {"use-after-poison on address " + Hex(arena + 32), "WRITE of size 16"}, "memset into poison");
    Expect(Run(nullptr, [] {
               __asan_poison_memory_region(arena + 200, 8);
               __asan_memmove(arena, arena + 190, 16);
           }),
           1, {"use-after-poison on address " + Hex(arena + 200), "READ of size 16"}, "memmove from poison");
    Expect(Run(nullptr, [] { __asan_memset(arena + 16, 0, ~std::uintptr_t{0} - 8); }), 1, {"ERROR: AddressSanitizer: negative-size-param: (size=-9)\n"}, "negative size");
}

void Objects() {
    ExpectClean(Run(nullptr, [] {
                    std::uintptr_t* cookie = reinterpret_cast<std::uintptr_t*>(arena + 128);
                    *cookie = 42;
                    __asan_poison_cxx_array_cookie(reinterpret_cast<std::uintptr_t>(cookie));
                    Require(ShadowByte(cookie) == 0xac, "array cookie poison");
                    Require(__asan_load_cxx_array_cookie(cookie) == 42, "poisoned cookie loads");
                    __asan_set_shadow_00(ShadowAddress(cookie), 1);
                    Require(__asan_load_cxx_array_cookie(cookie) == 42, "plain cookie loads");
                }),
                "array cookie");
    Expect(Run(nullptr, [] {
               std::uintptr_t* cookie = reinterpret_cast<std::uintptr_t*>(arena + 128);
               *cookie = 42;
               *reinterpret_cast<std::uint8_t*>(ShadowAddress(cookie)) = 0xfd;
               Require(__asan_load_cxx_array_cookie(cookie) == 0, "freed cookie loads 0");
           }),
           0, {"AddressSanitizer: loaded array cookie from free-d memory; expect a double-free report\n"}, "freed array cookie");
    ExpectClean(Run("poison_array_cookie=0", [] {
                    __asan_poison_cxx_array_cookie(reinterpret_cast<std::uintptr_t>(arena + 128));
                    Require(ShadowByte(arena + 128) == 0, "poison_array_cookie=0");
                }),
                "array cookie off");
    Expect(Run(nullptr, [] {
               __asan_poison_intra_object_redzone(reinterpret_cast<std::uintptr_t>(arena + 3), 21);
               Require(ShadowByte(arena) == 3 && ShadowByte(arena + 8) == 0xbb && ShadowByte(arena + 16) == 0xbb && ShadowByte(arena + 24) == 0, "intra-object redzone");
               __asan_unpoison_intra_object_redzone(reinterpret_cast<std::uintptr_t>(arena + 3), 21);
               Require(ShadowByte(arena) == 0 && ShadowByte(arena + 16) == 0, "intra-object unpoison");
               __asan_poison_intra_object_redzone(reinterpret_cast<std::uintptr_t>(arena + 8), 8);
               __asan_store8(reinterpret_cast<std::uintptr_t>(arena + 8));
           }),
           1, {"intra-object-overflow on address " + Hex(arena + 8)}, "intra-object overflow");
    ExpectAbort(Run(nullptr, [] { __asan_poison_intra_object_redzone(reinterpret_cast<std::uintptr_t>(arena), 7); }), "intra-object redzone end must be aligned");
    ExpectAbort(Run(nullptr, [] { __asan_poison_stack_memory(reinterpret_cast<std::uintptr_t>(arena + 4), 8); }), "stack poison start must be aligned");
}

void Unaligned() {
    Expect(Run(nullptr, [] {
               std::memset(arena, 0x11, 16);
               Require(__sanitizer_unaligned_load32(arena + 1) == 0x11111111u, "unaligned load reads");
               __asan_poison_memory_region(arena + 8, 8);
               __sanitizer_unaligned_store16(arena + 7, 1);
           }),
           1, {"use-after-poison on address " + Hex(arena + 8), "WRITE of size 2 at " + Hex(arena + 8)}, "unaligned store into poison");
}

void NoReturn() {
    ExpectClean(Run(nullptr, [] {
                    alignas(64) volatile unsigned char frame[64] = {};
                    __asan_poison_memory_region(const_cast<unsigned char*>(frame), 64);
                    __asan_poison_memory_region(arena, 64);
                    Require(ShadowByte(const_cast<unsigned char*>(frame)) == 0xf7, "the stack buffer is poisoned");
                    __asan_handle_no_return();
                    Require(ShadowByte(const_cast<unsigned char*>(frame)) == 0 && ShadowByte(const_cast<unsigned char*>(frame) + 56) == 0, "handle_no_return unpoisons the live stack");
                    Require(ShadowByte(arena) == 0xf7, "handle_no_return leaves memory off the stack alone");
                    frame[0] = 1;
                }),
                "handle_no_return");
}

}

int main() {
#if defined(ANYPS5_ENABLE_ASAN) || defined(__SANITIZE_ADDRESS__)
    std::fputs("skipped: the host AddressSanitizer shadow covers the PS guest shadow range at 1 << 40\n", stderr);
    return 77;
#endif
    Poisoning();
    Accesses();
    Recovery();
    Globals();
    Stack();
    Intrinsics();
    Objects();
    Unaligned();
    NoReturn();
    std::puts("guest_asan_runtime passed");
    return 0;
}
