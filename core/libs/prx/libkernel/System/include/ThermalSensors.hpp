#ifndef CORE_LIBS_PRX_LIBKERNEL_SYSTEM_THERMALSENSORS_HPP
#define CORE_LIBS_PRX_LIBKERNEL_SYSTEM_THERMALSENSORS_HPP

#include <cstddef>

extern "C" bool StorageDescriptorAllowsTemperatureQuery_nid_no_patch(const void* descriptor, std::size_t bytes);
extern "C" void ReadThermalSensors_nid_no_patch(const char* root, int celsius[3]);

inline void Plausible(int& value) {
    if (value < 0 || value > 130) value = -1;
}

#endif
