#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"

extern "C" {

int32_t sceAudioPropagationRoomCreate(AudioPropagationHandle system_handle, AudioPropagationHandle* out_room_handle) {
 (void)system_handle;
 (void)out_room_handle;
 return 0;
}

int32_t sceAudioPropagationSystemCreate(const void* options, AudioPropagationSystemMemory* memory, AudioPropagationHandle* out_system_handle) {
 (void)options;
 (void)memory;
 (void)out_system_handle;
 return 0;
}

int32_t sceAudioPropagationSystemGetRays(AudioPropagationHandle system_handle, void* rays, uint32_t* num_rays) {
 (void)system_handle;
 (void)rays;
 (void)num_rays;
 return 0;
}

int32_t sceAudioPropagationSystemQueryMemory(const void* options, AudioPropagationSystemMemory* out_memory) {
 (void)options;
 (void)out_memory;
 return 0;
}

int32_t sceAudioPropagationSystemRegisterMaterial(AudioPropagationHandle system_handle, const void* material, AudioPropagationHandle* out_material_handle) {
 (void)system_handle;
 (void)material;
 (void)out_material_handle;
 return 0;
}

int32_t sceAudioPropagationSystemSetAttributes(AudioPropagationHandle system_handle, const void* attributes, uint32_t num_attributes) {
 (void)system_handle;
 (void)attributes;
 (void)num_attributes;
 return 0;
}

}
