#ifndef CORE_LIBS_PRX_LIBSCEULT_SRC_READERWRITERLOCK_HPP
#define CORE_LIBS_PRX_LIBSCEULT_SRC_READERWRITERLOCK_HPP

struct UltThreadIdentity;

UltThreadIdentity UltCurrentThreadIdentity_nid_no_patch();
bool UltWaitingPoolExists(void* pool);
bool UltReaderWriterLocksWaitOn(void* pool);
void UltFinalizeReaderWriterLocks();

#endif
