# libSceAgcDriver

## add Functional

- [CLEAR_STATE commands and CONTEXT_CONTROL shadowing](../../../core/libs/prx/libSceAgcDriver/Execution/src/Pm4.cpp) (libSceAgcDriver, opcodes 0x12 and 0x28) - `CLEAR_STATE` command 0 clears the context, 1 pushes it, 2 pops it and 3 pushes and clears it, as the `CONTEXT_STATE` NOP operations do. That reading is inferred, not measured: the console's `sceAgcDcbContextStateOp(1)`, which AnyPS5 runs as a push, writes `CLEAR_STATE` 1 (`agcbuild_DcbContextStateOp_b`). Commands 4 to 15 are refused. `CONTEXT_CONTROL` accepts the shadow-control word `0x80018001` that packet carries and keeps no shadow memory, since no load is requested
