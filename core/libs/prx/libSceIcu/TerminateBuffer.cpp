#include "prx/libSceIcu/libSceIcu.hpp"
#include <cstdint>
#include <stdexcept>

namespace {

template<typename TUnit>
std::int32_t Terminate(TUnit* destination, std::int32_t capacity, std::int32_t length, std::int32_t* status) {
    if (!status || *status > 0 || length < 0) return length;
    if (length < capacity) {
        if (!destination) throw std::invalid_argument("ICU string termination: null destination");
        destination[length] = 0;
        if (*status == -124) *status = 0;
    } else if (length == capacity) {
        *status = -124;
    } else {
        *status = 15;
    }
    return length;
}

}

extern "C" {

std::int32_t APS5_VABI u_terminateChars_nid_postfix(char* destination, std::int32_t capacity, std::int32_t length, std::int32_t* status) {
    return Terminate(destination, capacity, length, status);
}

std::int32_t APS5_VABI u_terminateUChar32s_nid_postfix(std::int32_t* destination, std::int32_t capacity, std::int32_t length, std::int32_t* status) {
    return Terminate(destination, capacity, length, status);
}

}
