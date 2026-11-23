#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include "prx/libc/include/GuestPtr.hpp"
#include "prx/libc/include/General.hpp"

#include <cerrno>
#include <stdexcept>

namespace {

template <typename T>
bool CopyGuestInPreservingErrno(T& destination, const T* source) {
    return GuestPtr<const T>(source).CopyIn(destination);
}

template <typename T>
bool CopyGuestOutPreservingErrno(T* destination, const T& value) {
    return GuestPtr<T>(destination).CopyOut(value);
}

}

extern "C" void APS5_VABI X509_ALGOR_get0_nid_postfix(const void** outObject, int* outType, const void** outValue,
                                                     const LibreSsl3X509Algor* algor) {
    if (outObject && !algor) throw std::invalid_argument("X509_ALGOR_get0 requires an algorithm identifier for its object");
    if (outType && !algor) throw std::invalid_argument("X509_ALGOR_get0 requires an algorithm identifier for its type");

    LibreSsl3X509Algor guestAlgor{};
    if ((outObject || outType) && !CopyGuestInPreservingErrno(guestAlgor, algor))
        NotImplemented_nid_no_patch(__func__);

    int type = LibreSsl3Asn1Undefined;
    const void* value = nullptr;
    if (outType && guestAlgor.parameter) {
        LibreSsl3Asn1Type guestParameter{};
        if (!CopyGuestInPreservingErrno(guestParameter, static_cast<const LibreSsl3Asn1Type*>(guestAlgor.parameter)))
            NotImplemented_nid_no_patch(__func__);
        type = guestParameter.type;
        value = guestParameter.value;
    }

    const void* previousObject = nullptr;
    const bool hadObject = outObject && CopyGuestInPreservingErrno(previousObject, outObject);
    if (outObject && !CopyGuestOutPreservingErrno(outObject, static_cast<const void*>(guestAlgor.algorithm)))
        NotImplemented_nid_no_patch(__func__);

    const void* previousValue = nullptr;
    const bool hadValue = outValue && CopyGuestInPreservingErrno(previousValue, outValue);
    if (outValue && !CopyGuestOutPreservingErrno(outValue, value)) {
        if (hadObject) CopyGuestOutPreservingErrno(outObject, previousObject);
        NotImplemented_nid_no_patch(__func__);
    }

    if (outType && !CopyGuestOutPreservingErrno(outType, type)) {
        if (hadValue) CopyGuestOutPreservingErrno(outValue, previousValue);
        if (hadObject) CopyGuestOutPreservingErrno(outObject, previousObject);
        NotImplemented_nid_no_patch(__func__);
    }
}
