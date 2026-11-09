#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_DRIVER_INDIRECTDISPATCH_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_DRIVER_INDIRECTDISPATCH_HPP



namespace AgcDriver::DriverDetail {

enum IndirectPath { IndirectGpu = 0, IndirectPendingImage = 1, IndirectCopiedWrite = 2, IndirectNotImported = 3, IndirectThreadDimensions = 4, IndirectFillKernel = 5, IndirectMisaligned = 6, IndirectDisabled = 7, IndirectWorkgroupMemory = 8, IndirectPaths = 9 };

}

#endif
