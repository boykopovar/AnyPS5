#include "prx/libSceJxr/libSceJxr.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include <array>
#include <cstring>

extern "C" void JxrMetadataCleanup_nid_no_patch(std::int32_t type, void* payload);

extern "C" long JxrMetadataFree_nid_no_patch(void** memory) {
    return static_cast<long>(PKFree_nid_postfix(memory));
}

extern "C" void APS5_VABI FreeDescMetadata_nid_postfix(Jxr::MetadataVariant* value) {
    if (!value) NotImplemented_nid_no_patch(__func__);
    const auto type = value->type;
    if (type != 0 && type != 18 && type != 19 && type != 30 && type != 31)
        NotImplemented_nid_no_patch(__func__);
    std::array<std::byte, sizeof(value->value)> payload;
    std::memcpy(payload.data(), &value->value, payload.size());
    JxrMetadataCleanup_nid_no_patch(type, payload.data());
    if (type == 30 || type == 31)
        std::memcpy(&value->value, payload.data(), payload.size());
}
