#include "prx/libSceLibreSsl/include/Api.hpp"
#include "prx/libc/include/GuestPtr.hpp"
#include "prx/libc/include/General.hpp"

#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stdexcept>

namespace {

constexpr int Negotiated = 1;
constexpr int NoOverlap = 2;

struct SelectionMatch {
    const unsigned char* entry = nullptr;
    unsigned char length = 0;
};

template <typename T>
bool CopyGuestInPreservingErrno(T& destination, const T* source) {
    return GuestPtr<const T>(source).CopyIn(destination);
}

template <typename T>
bool CopyGuestOutPreservingErrno(T* destination, const T& value) {
    return GuestPtr<T>(destination).CopyOut(value);
}

SelectionMatch FirstOffered_nid_no_patch(const unsigned char* peer, std::uint32_t peerLength,
    const unsigned char* supported, std::uint32_t supportedLength, const char* function) {
    for (std::uint64_t offered = 0; offered < peerLength;) {
        unsigned char peerItemLength = 0;
        if (!CopyGuestInPreservingErrno(peerItemLength, peer + offered))
            NotImplemented_nid_no_patch(function);
        for (std::uint64_t local = 0; local < supportedLength;) {
            unsigned char supportedItemLength = 0;
            if (!CopyGuestInPreservingErrno(supportedItemLength, supported + local))
                NotImplemented_nid_no_patch(function);
            if (peerItemLength == supportedItemLength) {
                if (peerItemLength == 0)
                    return {peer + offered, 0};
                unsigned char peerBytes[255]{};
                unsigned char supportedBytes[255]{};
                if (!GuestCopyIn_nid_no_patch(peerBytes, peer + offered + 1, peerItemLength) ||
                    !GuestCopyIn_nid_no_patch(supportedBytes, supported + local + 1, supportedItemLength))
                    NotImplemented_nid_no_patch(function);
                if (std::memcmp(peerBytes, supportedBytes, peerItemLength) == 0)
                    return {peer + offered, peerItemLength};
            }
            local += std::uint64_t{1} + supportedItemLength;
        }
        offered += std::uint64_t{1} + peerItemLength;
    }
    return {};
}

}

extern "C" int APS5_VABI SSL_select_next_proto_nid_postfix(unsigned char** output, unsigned char* outputLength,
    const unsigned char* peer, std::uint32_t peerLength, const unsigned char* supported, std::uint32_t supportedLength) {
    if (!output || !outputLength) throw std::invalid_argument("SSL_select_next_proto: null output");
    if (!supported || (!peer && peerLength != 0)) throw std::invalid_argument("SSL_select_next_proto: null protocol list");
    const SelectionMatch match = FirstOffered_nid_no_patch(peer, peerLength, supported, supportedLength, __func__);
    const int status = match.entry ? Negotiated : NoOverlap;
    const unsigned char* result = match.entry;
    unsigned char chosenLength = match.length;
    if (!result) {
        result = supported;
        if (!CopyGuestInPreservingErrno(chosenLength, result))
            NotImplemented_nid_no_patch(__func__);
    }
    unsigned char* const chosenOutput = const_cast<unsigned char*>(result) + 1;
    unsigned char* previousOutput = nullptr;
    const bool hadOutput = CopyGuestInPreservingErrno(previousOutput, output);
    if (!CopyGuestOutPreservingErrno(output, chosenOutput))
        NotImplemented_nid_no_patch(__func__);
    if (!CopyGuestOutPreservingErrno(outputLength, chosenLength)) {
        if (hadOutput)
            CopyGuestOutPreservingErrno(output, previousOutput);
        NotImplemented_nid_no_patch(__func__);
    }
    return status;
}
