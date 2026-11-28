#include "prx/libSceIcu/DataInfo.hpp"
#include "prx/libSceIcu/DataName.hpp"
#include "prx/libc/include/General.hpp"
#include "ucmndata.h"
#include "udatamem.h"
#include <unicode/udata.h>
#include <bit>
#include <cerrno>
#include <exception>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>

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
    const char* hostName = Icu::CopyDataName(name, nameCopy, hostStatus, caller);
    const char* hostPath = nullptr;
    const char* hostType = "";
    if (hostName && *hostName && (!choice || choice->callback)) {
        hostPath = Icu::CopyDataName(path, pathCopy, hostStatus, caller);
        if (type) hostType = Icu::CopyDataName(type, typeCopy, hostStatus, caller);
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

std::string AppDataName(const char* packageName, const char* caller) {
    std::string name(packageName);
    const auto separator = name.rfind('/');
    const std::string_view base = separator == std::string::npos ? std::string_view(name) : std::string_view(name).substr(separator + 1);
    if (base == "icudt68l" || base == U_ICUDATA_NAME) NotImplemented_nid_no_patch(caller);
    if constexpr (U_FILE_SEP_CHAR != '/') {
        if (name.find(U_FILE_SEP_CHAR) != std::string::npos) NotImplemented_nid_no_patch(caller);
        std::replace(name.begin(), name.end(), '/', U_FILE_SEP_CHAR);
    }
    return name;
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

extern "C" void APS5_VABI udata_setAppData_nid_postfix(const char* packageName, const void* data, std::int32_t* status) {
    if (!status) return;
    std::int32_t entered = 0;
    const int savedErrno = errno;
    const bool readable = GuestCopyIn_nid_no_patch(&entered, status, sizeof(entered));
    errno = savedErrno;
    if (!readable) throw std::invalid_argument("udata_setAppData: unreadable status");
    if (U_FAILURE(static_cast<UErrorCode>(entered))) return;
    UErrorCode hostStatus = U_ZERO_ERROR;
    if (!data) {
        udata_setAppData(packageName, data, &hostStatus);
    } else {
        UDataMemory memory;
        UDataMemory_init(&memory);
        UDataMemory_setData(&memory, data);
        udata_checkCommonData(&memory, &hostStatus);
        if (U_SUCCESS(hostStatus)) {
            if (!packageName) throw std::invalid_argument("udata_setAppData: null package name");
            const std::string name = AppDataName(packageName, __func__);
            udata_setAppData(name.c_str(), data, &hostStatus);
        }
    }
    if (hostStatus == U_ZERO_ERROR) return;
    const auto outgoing = static_cast<std::int32_t>(hostStatus);
    const int savedWriteErrno = errno;
    const bool written = GuestCopyOut_nid_no_patch(status, &outgoing, sizeof(outgoing));
    errno = savedWriteErrno;
    if (!written) throw std::invalid_argument("udata_setAppData: unwritable status");
}
