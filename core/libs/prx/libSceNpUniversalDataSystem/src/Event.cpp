#include <cstddef>
#include <cstdint>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

int APS5_VABI sceNpUniversalDataSystemCreateEvent(const char* event_name, const NpUniversalDataSystemEventPropertyObject* prop, NpUniversalDataSystemEvent** new_event, NpUniversalDataSystemEventPropertyObject** prop_ptr) {
    (void)event_name;
    (void)prop;
    (void)new_event;
    (void)prop_ptr;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpUniversalDataSystemDestroyEvent(NpUniversalDataSystemEvent* event) {
    (void)event;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpUniversalDataSystemPostEvent(int context, int handle, const void* event, uint64_t options) {
    (void)context;
    (void)handle;
    (void)event;
    (void)options;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpUniversalDataSystemEventEstimateSize(const NpUniversalDataSystemEvent* event, size_t* size) {
    (void)event;
    (void)size;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpUniversalDataSystemEventToString(const NpUniversalDataSystemEvent* event, char* buf, size_t buf_size, size_t* string_size) {
    (void)event;
    (void)buf;
    (void)buf_size;
    (void)string_size;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

}
