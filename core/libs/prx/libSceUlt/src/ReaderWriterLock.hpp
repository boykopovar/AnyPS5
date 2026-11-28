#ifndef CORE_LIBS_PRX_LIBSCEULT_SRC_READERWRITERLOCK_HPP
#define CORE_LIBS_PRX_LIBSCEULT_SRC_READERWRITERLOCK_HPP

#include <cerrno>
#include "prx/libSceUlt/SceUltTypes.hpp"

struct UltRestoreErrno {
    int value = errno;
    ~UltRestoreErrno() { errno = value; }
};

template <typename TInfo, typename TWriteTrailing>
int WriteUltInfoWithTrailing(TInfo* info, const TInfo& localInfo, bool hasTrailing, TWriteTrailing&& writeTrailing) {
    TInfo previousInfo{};
    if (hasTrailing && !GuestCopyIn_nid_no_patch(&previousInfo, info, sizeof(previousInfo))) return ULT_ERROR_NULL;
    if (!GuestCopyOut_nid_no_patch(info, &localInfo, sizeof(localInfo))) return ULT_ERROR_NULL;
    if (hasTrailing) {
        const int writeStatus = writeTrailing();
        if (writeStatus != ULT_OK) {
            GuestCopyOut_nid_no_patch(info, &previousInfo, sizeof(previousInfo));
            return writeStatus;
        }
    }
    return ULT_OK;
}

UltThreadIdentity UltCurrentThreadIdentity_nid_no_patch();
UltThreadIdentity UltCurrentOwnerIdentity_nid_no_patch();
bool CurrentWaitRecord(UltWaitRecord& record, std::int32_t waitValue);
void RemoveWaitRecord(std::vector<UltWaitRecord>& records, const UltWaitRecord& record);
int ValidateWaitRecords(const UltWaitingThreadInfo* records, std::uint32_t capacity);
int WriteWaitRecords(const std::vector<UltWaitRecord>& records, UltWaitingThreadInfo* destination, std::uint32_t capacity);
bool UltWaitingPoolExists(void* pool);
bool UltReaderWriterLocksWaitOn(void* pool);
void UltFinalizeReaderWriterLocks();

#endif
