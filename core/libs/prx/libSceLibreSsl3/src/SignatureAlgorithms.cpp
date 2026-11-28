#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include "prx/libc/include/GuestPtr.hpp"
#include "prx/libSceLibreSsl/include/ObjectTable.hpp"
#include "prx/libc/include/General.hpp"

#include <algorithm>
#include <array>
#include <cerrno>

namespace {

constexpr std::array<std::array<int, 3>, 33> SignatureAlgorithms = {{
    {7, 3, 6},
    {8, 4, 6},
    {42, 41, 6},
    {65, 64, 6},
    {66, 41, 116},
    {70, 64, 67},
    {96, 95, 6},
    {104, 4, 19},
    {113, 64, 116},
    {115, 64, 19},
    {119, 117, 6},
    {396, 257, 6},
    {416, 64, 408},
    {668, 672, 6},
    {669, 673, 6},
    {670, 674, 6},
    {671, 675, 6},
    {791, 0, 408},
    {792, 0, 408},
    {793, 675, 408},
    {794, 672, 408},
    {795, 673, 408},
    {796, 674, 408},
    {802, 675, 116},
    {803, 672, 116},
    {807, 809, 811},
    {808, 809, 812},
    {852, 809, 850},
    {853, 809, 851},
    {912, 0, 912},
    {948, 941, 811},
    {949, 942, 811},
    {952, 0, 952},
}};

static_assert(std::all_of(SignatureAlgorithms.begin(), SignatureAlgorithms.end(), [](const auto& entry) {
    return std::all_of(entry.begin(), entry.end(), [](int nid) { return nid >= 0 && nid < LibreSslObjectCount; });
}));

template <typename T>
bool CopyGuestInPreservingErrno(T& destination, const T* source) {
    return GuestPtr<const T>(source).CopyIn(destination);
}

template <typename T>
bool CopyGuestOutPreservingErrno(T* destination, const T& value) {
    return GuestPtr<T>(destination).CopyOut(value);
}

}

extern "C" {

int APS5_VABI OBJ_find_sigid_algs_nid_postfix(int signatureNid, int* digestNid, int* keyNid) {
    const auto found = std::lower_bound(SignatureAlgorithms.begin(), SignatureAlgorithms.end(), signatureNid,
        [](const auto& entry, int value) { return entry[0] < value; });
    if (found == SignatureAlgorithms.end() || (*found)[0] != signatureNid)
        return 0;
    int previousDigest = 0;
    const bool hadDigest = digestNid && CopyGuestInPreservingErrno(previousDigest, digestNid);
    if (digestNid && !CopyGuestOutPreservingErrno(digestNid, (*found)[1]))
        NotImplemented_nid_no_patch(__func__);
    if (keyNid && !CopyGuestOutPreservingErrno(keyNid, (*found)[2])) {
        if (hadDigest)
            CopyGuestOutPreservingErrno(digestNid, previousDigest);
        NotImplemented_nid_no_patch(__func__);
    }
    return 1;
}

}
