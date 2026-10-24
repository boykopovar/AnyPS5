#ifndef CORE_LIBS_PRX_LIBSCEULT_SRC_READERWRITERLOCK_HPP
#define CORE_LIBS_PRX_LIBSCEULT_SRC_READERWRITERLOCK_HPP

bool UltWaitingPoolExists(void* pool);
bool UltReaderWriterLocksWaitOn(void* pool);
void UltFinalizeReaderWriterLocks();

#endif
