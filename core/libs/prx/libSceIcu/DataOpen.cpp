#include "prx/libSceIcu/DataInfo.hpp"
#include "prx/libc/include/General.hpp"
#include <unicode/udata.h>
#include <algorithm>
#include <bit>
#include <cerrno>
#include <exception>
#include <limits>
#include <memory>
#include <string>

namespace {

struct DataChoice {
    Icu::DataMemoryIsAcceptable callback;
    void* context;
    const char* type;
    const char* name;
    std::exception_ptr exception;
};

UBool U_CALLCONV AcceptData(void* context, const char*, const char*, const UDataInfo* info) noexcept {
    auto& choice = *static_cast<DataChoice*>(context);
    if (choice.exception) return false;
    const auto guestInfo = std::bit_cast<Icu::DataInfo>(*info);
    try {
        return choice.callback(choice.context, choice.type, choice.name, &guestInfo);
    } catch (...) {
        choice.exception = std::current_exception();
        return false;
    }
}

const char* CopyText(const char* source, std::string& text, UErrorCode& status, const char* caller) {
    if (!source || U_FAILURE(status)) return nullptr;
    constexpr int GuestOutOfMemory = 12;
    constexpr int GuestNameTooLong = 63;
    constexpr std::size_t maxSize = std::numeric_limits<std::int32_t>::max();
    for (std::size_t size = 256;; size = std::min(size * 2, maxSize)) {
        const int savedErrno = errno;
        const int error = CopyGuestString_nid_no_patch(source, size, text);
        errno = savedErrno;
        if (error == 0) return text.c_str();
        if (error == GuestOutOfMemory) {
            status = U_MEMORY_ALLOCATION_ERROR;
            return nullptr;
        }
        if (error != GuestNameTooLong) NotImplemented_nid_no_patch(caller);
        if (size == maxSize) {
            status = U_ILLEGAL_ARGUMENT_ERROR;
            return nullptr;
        }
    }
}

UDataMemory* OpenData(const char* path, const char* type, const char* name, std::int32_t* status,
                     DataChoice* choice, const char* caller) {
    if (!status) return nullptr;
    std::int32_t incoming = 0;
    const int savedErrno = errno;
    const bool readable = GuestCopyIn_nid_no_patch(&incoming, status, sizeof(incoming));
    errno = savedErrno;
    if (!readable) NotImplemented_nid_no_patch(caller);
    if (incoming > 0) return nullptr;
    const std::int32_t entered = incoming < U_ERROR_WARNING_START ? 0 : incoming;
    auto hostStatus = static_cast<UErrorCode>(entered);

    std::string nameCopy, pathCopy, typeCopy;
    const char* hostName = CopyText(name, nameCopy, hostStatus, caller);
    const char* hostPath = nullptr;
    const char* hostType = "";
    if (hostName && *hostName && (!choice || choice->callback)) {
        hostPath = CopyText(path, pathCopy, hostStatus, caller);
        if (type) hostType = CopyText(type, typeCopy, hostStatus, caller);
    }

    std::unique_ptr<UDataMemory, decltype(&udata_close)> data(
        choice ? udata_openChoice(hostPath, hostType, hostName,
                                 choice->callback ? AcceptData : nullptr, choice, &hostStatus)
               : udata_open(hostPath, hostType, hostName, &hostStatus),
        udata_close);
    if (choice && choice->exception) std::rethrow_exception(choice->exception);
    const auto outgoing = static_cast<std::int32_t>(hostStatus);
    if (outgoing != entered) {
        const bool written = GuestCopyOut_nid_no_patch(status, &outgoing, sizeof(outgoing));
        if (!written) NotImplemented_nid_no_patch(caller);
    }
    return data.release();
}

}

extern "C" UDataMemory* APS5_VABI udata_open_nid_postfix(const char* path, const char* type, const char* name,
                                                        std::int32_t* status) {
    return OpenData(path, type, name, status, nullptr, __func__);
}

extern "C" UDataMemory* APS5_VABI udata_openChoice_nid_postfix(const char* path, const char* type, const char* name,
                                                              Icu::DataMemoryIsAcceptable callback, void* context,
                                                              std::int32_t* status) {
    DataChoice choice{callback, context, type, name, {}};
    return OpenData(path, type, name, status, &choice, __func__);
}
