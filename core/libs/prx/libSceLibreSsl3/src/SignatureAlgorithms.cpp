#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"

#include <algorithm>
#include <array>

namespace {

constexpr std::array<std::array<int, 3>, 51> SignatureAlgorithms = {{
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
    {980, 64, 990},
    {981, 675, 990},
    {982, 672, 990},
    {983, 673, 990},
    {984, 674, 990},
    {985, 64, 991},
    {986, 675, 991},
    {987, 672, 991},
    {988, 673, 991},
    {989, 674, 991},
    {1045, 1031, 408},
    {1046, 1032, 408},
    {1047, 1033, 408},
    {1048, 1034, 408},
    {1049, 1031, 6},
    {1050, 1032, 6},
    {1051, 1033, 6},
    {1052, 1034, 6},
}};

}

extern "C" {

int APS5_VABI OBJ_find_sigid_algs_nid_postfix(int signatureNid, int* digestNid, int* keyNid) {
    const auto found = std::lower_bound(SignatureAlgorithms.begin(), SignatureAlgorithms.end(), signatureNid,
        [](const auto& entry, int value) { return entry[0] < value; });
    if (found == SignatureAlgorithms.end() || (*found)[0] != signatureNid)
        return 0;
    if (digestNid)
        *digestNid = (*found)[1];
    if (keyNid)
        *keyNid = (*found)[2];
    return 1;
}

}
