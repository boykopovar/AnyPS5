#include "prx/libSceJxr/libSceJxr.hpp"
#include "prx/libc/include/General.hpp"

extern "C" std::int64_t JxrEnumerateConversions_nid_no_patch(const void* source, std::uint32_t index, const void** target);

extern "C" std::int64_t APS5_VABI PKFormatConverter_EnumConversions_nid_postfix(
    const JxrGuestGuid* source, std::uint32_t index, const JxrGuestGuid** target) {
    if (!source || !target) NotImplemented_nid_no_patch(__func__);
    const auto input = reinterpret_cast<std::uintptr_t>(source);
    const auto output = reinterpret_cast<std::uintptr_t>(target);
    if ((output >= input && output - input < sizeof(*source)) ||
        (input > output && input - output < sizeof(*target)))
        NotImplemented_nid_no_patch(__func__);
    const void* native = nullptr;
    const auto result = JxrEnumerateConversions_nid_no_patch(source, index, &native);
    *target = static_cast<const JxrGuestGuid*>(native);
    return result;
}
