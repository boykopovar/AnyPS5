# libScePadTracker technical debt

### Silent stubs

- [scePadTrackerInit and scePadTrackerTerm](../../../core/libs/prx/libScePadTracker/Export.cpp) track initialization state transitions without initializing tracker hardware.

### Unknown function info

- [scePadTrackerCalibrate, scePadTrackerReadState and scePadTrackerUpdate](../../../core/libs/prx/libScePadTracker/Export.cpp) (libScePadTracker) - names and NIDs are present in the PS5 3.20 export inventory. They enforce initialization returning 0x80c40001 when uninitialized; remaining parameters and runtime behavior remain unverified and throw through NotImplemented_nid_no_patch.
- [scePadTrackerGetWorkingMemorySize](../../../core/libs/prx/libScePadTracker/Export.cpp) (libScePadTracker) - what the two sizes are for is unknown. It follows firmware 13.42's libScePadTracker.sprx, read from its disassembly and run natively on x86-64 (not a console): a null first or second pointer returns `0x80c40003` and writes nothing; otherwise it stores the 32-bit value 0x800000 through each pointer and returns 0. Both values are module data that no other code in the module references. Returning `0x80c40003` for an unwritable output is a host-safety fallback; console behaviour for unwritable memory has not been measured.
