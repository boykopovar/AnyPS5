# libScePadTracker

## change [scePadTrackerCalibrate, scePadTrackerInit, scePadTrackerReadState, scePadTrackerTerm and scePadTrackerUpdate]

- [scePadTrackerInit, scePadTrackerCalibrate and scePadTrackerTerm](../../../core/libs/prx/libScePadTracker/Export.cpp) (libScePadTracker) - follow firmware 13.42 ([reading and harness run](https://github.com/boykopovar/AnyPS5/issues/21753#issuecomment-6932874919), not on a console). `Init(void* memA, void* memB, int32_t a, int32_t b)` returns `0x80c40003` for memory that is null or not 16 KiB aligned or `a` outside 0-6 or `b` outside 0-7, then `0x80c40002` when initialized, else 0. The console then sets up its camera tracker and ignores the result; nothing here tracks, and `Term` doesn't reset the pads' light bars as the console's does. `Calibrate(void)` and `Term(void)` return `0x80c40001` before `Init`, else 0

## add Unknown function info

- [scePadTrackerReadState and scePadTrackerUpdate](../../../core/libs/prx/libScePadTracker/Export.cpp) (libScePadTracker) - `ReadState(int32_t handle, void* state)` returns `0x80c40001` before `Init`, then `0x80c40003` for a null `state`, then `0x80c40004`, as no `Update` fills its handle table here ([reading and harness run](https://github.com/boykopovar/AnyPS5/issues/21753#issuecomment-6932874919)); handle 0 throws, since the table's initial contents weren't read. `Update` returns `0x80c40001` before `Init` and otherwise throws: its 0x40-byte record argument and the tracker aren't implemented
