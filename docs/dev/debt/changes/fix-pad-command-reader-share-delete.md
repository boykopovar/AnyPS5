# libScePad

## add Functional

- [controller command-file replacement while the runtime reads](../../../core/libs/prx/libScePad/include/InputCommandFile.hpp) (libScePad) - the Windows reader shares deletion, and the Python queuing tools use `ReplaceFileW` if `os.replace` is denied by the live reader. A separately opened handle that does not share deletion, including `_wfopen(..., L"rb")`, can still prevent replacement; the original command remains intact in that case. Console behavior is not involved.
