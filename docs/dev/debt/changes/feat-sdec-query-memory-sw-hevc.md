# libSceVdecShevc

## change sceSdecCreateSwHevc, sceSdecDestroySwHevc, sceSdecExecuteSwHevc, sceSdecGetFrameSwHevc, sceSdecQueryMemorySwHevc and sceSdecResetSwHevc](

- [sceSdecCreateSwHevc, sceSdecDestroySwHevc, sceSdecExecuteSwHevc, sceSdecGetFrameSwHevc and sceSdecResetSwHevc](../../../core/libs/prx/libSceVdecShevc/Export.cpp) (libSceVdecShevc) - names and NIDs are present in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceVdecShevc.c), but no credible prototypes were found. The declarations have no parameters solely to resolve known imports; this is not an ABI claim and callers must not call them until signatures are verified

## add Functional

- [sceSdecQueryMemorySwHevc](../../../core/libs/prx/libSceVdecShevc/Export.cpp) (libSceVdecShevc) follows [readings of firmware 13.42's libSceVdecShevc.sprx](https://github.com/boykopovar/AnyPS5/issues/21865#issuecomment-6955701260), run natively, not measured on a console: the argument, depth, instance-count, magic, extra-pixel and level checks (3, setting `*tooBig` to 1 when non-null on a frame-size overflow), `*dpbFrames` and all three block sizes for `instanceMode` 0..5 and negative dimensions follow the module's closed forms. The `.native.sprx` variant has different code and is not modelled. Host choices: unreadable or unwritable guest pointers return 3 instead of faulting, a refused query leaves the memory output unwritten, and `instanceMode > 5` throws since those modes were not read end-to-end.
