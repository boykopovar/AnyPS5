#ifndef CORE_LIBS_PRX_LIBSCEULT_SRC_READERWRITERLOCK_HPP
#define CORE_LIBS_PRX_LIBSCEULT_SRC_READERWRITERLOCK_HPP

#include "prx/libSceUlt/SceUltTypes.hpp"

UltThreadIdentity UltCurrentThreadIdentity_nid_no_patch();
bool CurrentWaitRecord(UltWaitRecord& record, std::int32_t waitValue);
void RemoveWaitRecord(std::vector<UltWaitRecord>& records, const UltWaitRecord& record);
int ValidateWaitRecords(const UltWaitingThreadInfo* records, std::uint32_t capacity);
int WriteWaitRecords(const std::vector<UltWaitRecord>& records, UltWaitingThreadInfo* destination, std::uint32_t capacity);
bool UltWaitingPoolExists(void* pool);
bool UltReaderWriterLocksWaitOn(void* pool);
void UltFinalizeReaderWriterLocks();

#endif
