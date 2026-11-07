#include "Runtime.hpp"

#include <ps5lift/Bridge.hpp>
#include <ps5lift/State.h>

#include <algorithm>
#include <cinttypes>
#include <cstdint>
#include <cstring>
#include <string_view>

extern "C" std::uint32_t GuestCpuMxcsr_nid_no_patch(void) {
    return ps5lift_read_mxcsr(ps5lift_current_state());
}

extern "C" void GuestCpuSetMxcsr_nid_no_patch(const std::uint32_t value) {
    auto* state = ps5lift_current_state();
    state->Mxcsr = value;
    ps5lift_apply_mxcsr(state);
}

extern "C" std::uint16_t GuestCpuX87Control_nid_no_patch(void) {
    return ps5lift_current_state()->X87Control;
}

extern "C" void GuestCpuSetX87Control_nid_no_patch(const std::uint16_t value) {
    ps5lift_current_state()->X87Control = value;
}

extern "C" std::uint16_t GuestCpuX87Status_nid_no_patch(void) {
    return ps5lift_current_state()->X87Status;
}

extern "C" void GuestCpuSetX87Status_nid_no_patch(const std::uint16_t value) {
    ps5lift_current_state()->X87Status = value;
}

extern "C" void GuestCpuX87Wait_nid_no_patch(void) {
    Ps5Lift::RequireNoPendingX87Exceptions(*ps5lift_current_state());
}

extern "C" std::uint32_t GuestCpuMxcsrMask_nid_no_patch(void) {
    return PS5LIFT_MXCSR_MASK;
}

#ifdef PS5LIFT_HOST_LONG_DOUBLE_HOLDS_EXTENDED
extern "C" void GuestCpuStoreExtended_nid_no_patch(const long double* host, void* guest) {
    ps5lift_extended_from_host(ps5lift_current_state(), host, guest);
}
#endif

extern "C" void GuestCpuExtendedAdd_nid_no_patch(const void* left, const void* right, void* result) {
    ps5lift_extended_operation(ps5lift_current_state(), PS5LIFT_X87_ADD, left, right, result);
}

extern "C" void GuestCpuExtendedSubtract_nid_no_patch(const void* left, const void* right, void* result) {
    ps5lift_extended_operation(ps5lift_current_state(), PS5LIFT_X87_SUBTRACT, left, right, result);
}

extern "C" void GuestCpuExtendedMultiply_nid_no_patch(const void* left, const void* right, void* result) {
    ps5lift_extended_operation(ps5lift_current_state(), PS5LIFT_X87_MULTIPLY, left, right, result);
}

extern "C" void GuestCpuExtendedDivide_nid_no_patch(const void* left, const void* right, void* result) {
    ps5lift_extended_operation(ps5lift_current_state(), PS5LIFT_X87_DIVIDE, left, right, result);
}

extern "C" void GuestCpuExtendedSquareRoot_nid_no_patch(const void* value, void* result) {
    ps5lift_extended_operation(ps5lift_current_state(), PS5LIFT_X87_SQUARE_ROOT, value, value, result);
}

extern "C" void GuestCpuExtendedRoundToIntegral_nid_no_patch(const void* value, void* result) {
    ps5lift_extended_operation(ps5lift_current_state(), PS5LIFT_X87_ROUND, value, value, result);
}

extern "C" double GuestCpuExtendedToDouble_nid_no_patch(const void* value) {
    double result;
    ps5lift_extended_store(ps5lift_current_state(), PS5LIFT_X87_FLOAT64, value, &result);
    return result;
}

extern "C" float GuestCpuExtendedToFloat_nid_no_patch(const void* value) {
    float result;
    ps5lift_extended_store(ps5lift_current_state(), PS5LIFT_X87_FLOAT32, value, &result);
    return result;
}

extern "C" std::int64_t GuestCpuExtendedToInt64_nid_no_patch(const void* value) {
    std::int64_t result;
    ps5lift_extended_store(ps5lift_current_state(), PS5LIFT_X87_INTEGER64 | PS5LIFT_X87_TRUNCATE, value, &result);
    return result;
}

namespace {

constexpr int DwarfRegisters[16] = {PS5LIFT_RAX, PS5LIFT_RDX, PS5LIFT_RCX, PS5LIFT_RBX, PS5LIFT_RSI, PS5LIFT_RDI, PS5LIFT_RBP, PS5LIFT_RSP,
                                    PS5LIFT_R8, PS5LIFT_R9, PS5LIFT_R10, PS5LIFT_R11, PS5LIFT_R12, PS5LIFT_R13, PS5LIFT_R14, PS5LIFT_R15};
constexpr int DwarfRsp = 7;
constexpr int DwarfRip = 16;

bool SameName(const std::string_view left, const std::string_view right) {
    const auto fold = [](const unsigned char value) { return value >= 'A' && value <= 'Z' ? value + ('a' - 'A') : value; };
    return std::ranges::equal(left, right, [&](const unsigned char a, const unsigned char b) { return fold(a) == fold(b); });
}

struct LoadedImage {
    std::uint64_t Address;
    const char* Name;
    const void* ProgramHeaders;
    std::uint16_t ProgramHeaderCount;
    unsigned long long Loads;
    unsigned long long Unloads;
    std::size_t TlsModule;
    void* TlsData;
};

}

extern "C" void GuestCpuUnwindStart_nid_no_patch(std::uint64_t* registers) {
    const auto* state = ps5lift_current_state();
    for (int i = 0; i < 16; ++i) registers[i] = state->Gpr[DwarfRegisters[i]];
    std::memcpy(&registers[DwarfRip], reinterpret_cast<const void*>(registers[DwarfRsp]), sizeof(registers[DwarfRip]));
    registers[DwarfRsp] += 8;
}

extern "C" int GuestCpuIterateImages_nid_no_patch(int (*callback)(LoadedImage*, std::size_t, void*), void* data) {
    const auto& program = Ps5Lift::Program();
    const auto* state = Ps5Lift::BoundState();
    for (std::uint64_t index = 0; index < program.ImageCount; ++index) {
        const auto& image = program.Images[index];
        LoadedImage loaded{image.LoadBias, image.Name, reinterpret_cast<const void*>(image.ProgramHeaders), static_cast<std::uint16_t>(image.ProgramHeaderCount), program.ImageCount, 0, image.TlsModule,
            image.TlsModule == 0 || state == nullptr ? nullptr : reinterpret_cast<void*>(state->FsBase - image.TlsOffset)};
        if (const int result = callback(&loaded, sizeof(loaded), data); result != 0) return result;
    }
    return 0;
}

extern "C" void GuestCpuMakingWritable_nid_no_patch(const std::uint64_t address, const std::uint64_t length) {
    constexpr std::uint32_t Write = 2;
    const auto& program = Ps5Lift::Program();
    for (std::uint64_t index = 0; index < program.ImageCount; ++index) {
        const auto& image = program.Images[index];
        for (std::uint64_t i = 0; i < image.SegmentCount; ++i) {
            const auto& segment = image.Segments[i];
            if ((segment.Protection & Write) != 0 || address >= segment.GuestAddress + segment.MemorySize || segment.GuestAddress >= address + length) continue;
            ps5lift_fail("the guest makes 0x%" PRIx64 "-0x%" PRIx64 " writable, inside the read-only segment at 0x%" PRIx64 " of %s, whose loads were lifted unordered", address, address + length,
                segment.GuestAddress, image.Name);
        }
    }
}

extern "C" const char* GuestCpuImportName_nid_no_patch(const std::uint64_t address) {
    const auto& program = Ps5Lift::Program();
    for (std::uint64_t index = 0; index < program.ImageCount; ++index) {
        const auto& image = program.Images[index];
        for (std::uint64_t i = 0; i < image.ImportCount; ++i) {
            if (image.Imports[i].GuestAddress == address) return image.Imports[i].Name;
        }
    }
    return nullptr;
}

extern "C" [[noreturn]] void GuestCpuUnwindResume_nid_no_patch(const std::uint64_t* registers) {
    auto* state = ps5lift_current_state();
    for (int i = 0; i < 16; ++i) state->Gpr[DwarfRegisters[i]] = registers[i];
    state->ResumeRip = registers[DwarfRip];
    state->ResumeRsp = registers[DwarfRsp];
    state->Unwinding = 1;
    throw Ps5Lift::Bridge::GuestUnwind{};
}

extern "C" std::uint64_t GuestCpuReadTsc_nid_no_patch(void) {
    return ps5lift_read_tsc();
}

namespace {

std::uint64_t ImageSymbol(const Ps5LiftImage& image, const char* name) {
    const auto* begin = image.Exports;
    const auto* end = begin + image.ExportCount;
    const auto* found = std::lower_bound(begin, end, name, [](const Ps5LiftExport& entry, const char* wanted) { return std::strcmp(entry.Name, wanted) < 0; });
    return found != end && std::strcmp(found->Name, name) == 0 ? found->Address : 0;
}

}

extern "C" std::uint64_t GuestCpuFindImage_nid_no_patch(const char* path) {
    const auto& program = Ps5Lift::Program();
    const std::string_view wanted(path);
    std::uint64_t found = 0;
    std::size_t matched = 0;
    for (std::uint64_t index = 1; index < program.ImageCount; ++index) {
        const std::string_view name(program.Images[index].Name);
        if (name.size() <= matched || wanted.size() < name.size() || !SameName(wanted.substr(wanted.size() - name.size()), name)) continue;
        if (wanted.size() != name.size() && wanted[wanted.size() - name.size() - 1] != '/') continue;
        found = index + 1;
        matched = name.size();
    }
    return found;
}

extern "C" std::uint64_t GuestCpuImageSymbol_nid_no_patch(const std::uint64_t image, const char* name) {
    const auto& program = Ps5Lift::Program();
    if (image > program.ImageCount) ps5lift_fail("GuestCpuImageSymbol: there is no image %" PRIu64, image);
    if (image != 0) return ImageSymbol(program.Images[image - 1], name);
    for (std::uint64_t index = 0; index < program.ImageCount; ++index) {
        if (const auto address = ImageSymbol(program.Images[index], name)) return address;
    }
    return 0;
}

extern "C" std::uint64_t GuestCpuCall_nid_no_patch(const std::uint64_t target, const std::uint64_t first, const std::uint64_t second, const std::uint64_t third) {
    return Ps5Lift::Bridge::CallGuest<std::uint64_t>(target, first, second, third);
}

extern "C" void GuestCpuUseStack_nid_no_patch(const std::uint64_t stackTop) {
    ps5lift_use_guest_stack(stackTop);
}

extern "C" std::uint64_t GuestCpuHostStackSize_nid_no_patch(void) {
    return ps5lift_host_stack_size();
}

extern "C" void GuestCpuCallInterrupted_nid_no_patch(const std::uint64_t target, const std::uint64_t first, const std::uint64_t second) {
    auto* state = ps5lift_current_state();
    const Ps5LiftState saved = *state;
    const auto mxcsr = ps5lift_read_mxcsr(state);
    state->Gpr[PS5LIFT_RSP] = ps5lift_interrupt_stack();
    state->Unwinding = 0;
    Ps5Lift::Bridge::CallGuest<void>(target, first, second);
    *state = saved;
    state->Mxcsr = mxcsr;
    ps5lift_apply_mxcsr(state);
}

extern "C" void GuestCpuInterruptedContext_nid_no_patch(std::uint64_t* registers, std::uint64_t* flags) {
    static constexpr unsigned FlagBits[PS5LIFT_FLAG_COUNT] = {0, 2, 4, 6, 7, 11, 10};
    const auto* state = ps5lift_current_state();
    for (int i = 0; i < 16; ++i) registers[i] = state->Gpr[DwarfRegisters[i]];
    registers[DwarfRip] = state->Rip;
    std::uint64_t value = 0x202;
    for (int i = 0; i < PS5LIFT_FLAG_COUNT; ++i) value |= static_cast<std::uint64_t>(state->Flags[i] & 1) << FlagBits[i];
    *flags = value;
}

namespace {

constexpr int FiberRegisters[] = {PS5LIFT_RBX, PS5LIFT_RBP, PS5LIFT_R12, PS5LIFT_R13, PS5LIFT_R14, PS5LIFT_R15, PS5LIFT_RSP};

}

extern "C" void GuestCpuSaveFiber_nid_no_patch(std::uint64_t* context) {
    auto* state = ps5lift_current_state();
    for (int i = 0; i < 7; ++i) context[i] = state->Gpr[FiberRegisters[i]];
    context[7] = static_cast<std::uint64_t>(ps5lift_read_mxcsr(state)) | static_cast<std::uint64_t>(state->X87Control) << 32;
}

extern "C" void GuestCpuRestoreFiber_nid_no_patch(const std::uint64_t* context) {
    auto* state = ps5lift_current_state();
    for (int i = 0; i < 7; ++i) state->Gpr[FiberRegisters[i]] = context[i];
    state->Mxcsr = static_cast<std::uint32_t>(context[7]);
    state->X87Control = static_cast<std::uint16_t>(context[7] >> 32);
    state->Unwinding = 0;
    ps5lift_apply_mxcsr(state);
}

extern "C" void GuestCpuStartFiber_nid_no_patch(const std::uint64_t stackTop) {
    auto* state = ps5lift_current_state();
    if (stackTop != 0) state->Gpr[PS5LIFT_RSP] = stackTop & ~std::uint64_t{15};
    state->Unwinding = 0;
}
