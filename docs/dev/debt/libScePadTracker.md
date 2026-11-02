# libScePadTracker technical debt

### Unknown function info

- [scePadTrackerCalibrate, scePadTrackerInit, scePadTrackerReadState, scePadTrackerTerm and scePadTrackerUpdate](../../../core/libs/prx/libScePadTracker/Export.cpp) (libScePadTracker) - names and NIDs are present in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libScePadTracker.c), but no credible prototypes were found. The declarations have no parameters solely to resolve known imports; this is not an ABI claim and callers must not call them until signatures are verified
- [scePadTrackerGetWorkingMemorySize](../../../core/libs/prx/libScePadTracker/Export.cpp) (libScePadTracker) - what the two sizes are for is unknown. It follows firmware 13.42's libScePadTracker.sprx, read from its disassembly and run natively on x86-64 (not a console): a null first or second pointer returns `0x80c40003` and writes nothing; otherwise it stores the 32-bit value 0x800000 through each pointer and returns 0. Both values are module data that no other code in the module references.
