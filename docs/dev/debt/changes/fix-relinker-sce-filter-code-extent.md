# relinker

## change scans for syscalls and import call sites

- In executables with a `PT_SCE_DYNLIBDATA` segment, the PS4 layout, the executable segment also holds read-only data after the code (CUSA00900: 37 MiB after its last function). The [relinker pipeline](../../../core/relinker/relinker/src/pipeline/RelinkerPipeline.cpp) therefore scans for syscalls, import call sites, and unused NID analyses (`unused-filter=1` and `unused-filter=2`) only up to the end of the PLT that follows the last function of the unwind table; code after the PLT without an unwind entry is not scanned. `--to-intel` is not limited this way.
