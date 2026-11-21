# shader

## add Unknown function info

- [GCN DPP](../../../core/shader/recompiler/RdnaDecoder/src/GcnInstructionDecoder.cpp) (shader, PS4) - a VOP1, VOP2 or VOPC instruction whose first source is 0xFA (DPP) throws as VOP1_DPP, VOP2_DPP or VOPC_DPP. No title is known to use it, and some GCN DPP controls (wave shifts and rotates, row_bcast) have no RDNA form ([LLVM](https://github.com/llvm/llvm-project/blob/48d0dc8f61a7680190a144ef89bd80543c0ec4f9/llvm/lib/Target/AMDGPU/AsmParser/AMDGPUAsmParser.cpp#L10313-L10315)). Closes when: a title uses it and its controls are measured on a PS4 Pro.
