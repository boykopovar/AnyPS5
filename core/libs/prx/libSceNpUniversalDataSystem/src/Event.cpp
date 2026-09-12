#include <cstddef>
#include <cstdint>
#include <cstring>
#include "NpUniversalDataSystem.hpp"
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

int APS5_VABI sceNpUniversalDataSystemCreateEvent(
    const char* event_name,
    const NpUniversalDataSystemEventPropertyObject* prop,
    NpUniversalDataSystemEvent** new_event,
    NpUniversalDataSystemEventPropertyObject** prop_ptr)
{
    if (event_name == nullptr || new_event == nullptr) {
        return SCE_NP_UNIVERSAL_DATA_SYSTEM_ERROR_INVALID_ARGUMENT;
    }
    *new_event = new NpUniversalDataSystemEvent;
    if (prop_ptr != nullptr) {
        *prop_ptr = (prop != nullptr
            ? const_cast<NpUniversalDataSystemEventPropertyObject*>(prop)
            : new NpUniversalDataSystemEventPropertyObject);
    }
    return SCE_NP_UNIVERSAL_DATA_SYSTEM_OK;
}

int APS5_VABI sceNpUniversalDataSystemDestroyEvent(NpUniversalDataSystemEvent* event) {
    delete event;
    return SCE_NP_UNIVERSAL_DATA_SYSTEM_OK;
}

int APS5_VABI sceNpUniversalDataSystemPostEvent(int context, int handle, const void* event, uint64_t options) {
    return SCE_NP_UNIVERSAL_DATA_SYSTEM_OK;
}

int APS5_VABI sceNpUniversalDataSystemEventEstimateSize(const NpUniversalDataSystemEvent* event, size_t* size) {
    if (event == nullptr || size == nullptr) {
        return SCE_NP_UNIVERSAL_DATA_SYSTEM_ERROR_INVALID_ARGUMENT;
    }
    *size = NP_UNIVERSAL_DATA_SYSTEM_EMPTY_EVENT_SIZE;
    return SCE_NP_UNIVERSAL_DATA_SYSTEM_OK;
}

int APS5_VABI sceNpUniversalDataSystemEventToString(
    const NpUniversalDataSystemEvent* event,
    char* buf,
    size_t buf_size,
    size_t* string_size)
{
    if (event == nullptr) {
        return SCE_NP_UNIVERSAL_DATA_SYSTEM_ERROR_INVALID_ARGUMENT;
    }
    const size_t json_len = std::strlen(NP_UNIVERSAL_DATA_SYSTEM_EMPTY_JSON) + 1;
    if (string_size != nullptr) {
        *string_size = json_len;
    }
    if (buf != nullptr && buf_size > 0) {
        std::snprintf(buf, buf_size, "%s", NP_UNIVERSAL_DATA_SYSTEM_EMPTY_JSON);
    }
    return SCE_NP_UNIVERSAL_DATA_SYSTEM_OK;
}

}
