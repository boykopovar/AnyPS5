#include "prx/libc/include/exceptions/Unwind.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>

static void Require(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "unwind accessor check failed at line %d\n", line);
        std::abort();
    }
}
#define Check(value) Require((value), __LINE__)

constexpr int RegisterCount = 17;
constexpr int InstructionPointer = 16;

static std::uintptr_t Pattern(int index) {
    return 0x1000u + static_cast<std::uintptr_t>(index) * 0x111u;
}

int main() {
    {
        _Unwind_Context context{};
        for (int index = 0; index < RegisterCount; ++index) {
            Check(_Unwind_GetGR_nid_postfix(&context, index) == 0);
            _Unwind_SetGR_nid_postfix(&context, index, Pattern(index));
            for (int other = 0; other < RegisterCount; ++other)
                Check(_Unwind_GetGR_nid_postfix(&context, other) == (other <= index ? Pattern(other) : 0));
        }
        for (int index = 0; index < RegisterCount; ++index) Check(_Unwind_GetGR_nid_postfix(&context, index) == Pattern(index));
    }
    {
        _Unwind_Context context{};
        _Unwind_SetGR_nid_postfix(&context, 3, 0x33);
        _Unwind_SetIP_nid_postfix(&context, 0x4000);
        Check(_Unwind_GetIP_nid_postfix(&context) == 0x4000);
        Check(_Unwind_GetGR_nid_postfix(&context, InstructionPointer) == 0x4000);
        Check(_Unwind_GetGR_nid_postfix(&context, 3) == 0x33);
        _Unwind_SetGR_nid_postfix(&context, InstructionPointer, 0x5000);
        Check(_Unwind_GetIP_nid_postfix(&context) == 0x5000);
    }
    {
        _Unwind_Context context{};
        _Unwind_SetIP_nid_postfix(&context, 0x6000);
        int before = 99;
        Check(_Unwind_GetIPInfo_nid_postfix(&context, &before) == 0x6000 && before == 0);
        context.signalFrame = true;
        before = 99;
        Check(_Unwind_GetIPInfo_nid_postfix(&context, &before) == 0x6000 && before == 1);
        Check(_Unwind_GetIPInfo_nid_postfix(&context, nullptr) == 0x6000);
        _Unwind_SetIP_nid_postfix(&context, 0x7000);
        Check(_Unwind_GetIPInfo_nid_postfix(&context, &before) == 0x7000);
    }
    {
        _Unwind_Context context{};
        context.cfa = 0x8100;
        context.lsda = 0x8200;
        context.region = 0x8300;
        context.dataBase = 0x8400;
        context.textBase = 0x8500;
        Check(_Unwind_GetCFA_nid_postfix(&context) == 0x8100);
        Check(_Unwind_GetLanguageSpecificData_nid_postfix(&context) == 0x8200);
        Check(_Unwind_GetRegionStart_nid_postfix(&context) == 0x8300);
        Check(_Unwind_GetDataRelBase_nid_postfix(&context) == 0x8400);
        Check(_Unwind_GetTextRelBase_nid_postfix(&context) == 0x8500);
        for (int index = 0; index < RegisterCount; ++index) Check(_Unwind_GetGR_nid_postfix(&context, index) == 0);
    }
    return 0;
}
