#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"

#include <stdexcept>

extern "C" void APS5_VABI X509_ALGOR_get0_nid_postfix(const void** outObject, int* outType, const void** outValue,
                                                     const LibreSsl3X509Algor* algor) {
    if (outObject) {
        if (!algor) throw std::invalid_argument("X509_ALGOR_get0 requires an algorithm identifier for its object");
        *outObject = algor->algorithm;
    }
    if (outValue) *outValue = nullptr;
    if (!outType) return;
    if (!algor) throw std::invalid_argument("X509_ALGOR_get0 requires an algorithm identifier for its type");
    int type = LibreSsl3Asn1Undefined;
    const void* value = nullptr;
    if (algor->parameter) {
        type = algor->parameter->type;
        value = algor->parameter->value;
    }
    *outType = type;
    if (outValue) *outValue = value;
}
