# libSceLibcInternal

## remove `lOF5jrFNZUA`

## add Functional

- [wide time_put base-object constructor](../../../core/libs/prx/libSceLibcInternal/TimeGetFields.cpp) (libSceLibcInternal, NID `lOF5jrFNZUA`) - firmware body and console execution are unverified. It repeats the complete-object constructor's body next to it in the same file, building the classic `time_put<wchar_t>` facet over `g_timePutWideVtable`; `time_put` has no virtual bases in Dinkumware's [xloctime](https://github.com/microsoft/STL/blob/54d74d7a58e828857a73f979eccc2ba1c02e256d/stl/inc/xloctime), so the [Itanium C++ ABI](https://itanium-cxx-abi.github.io/cxx-abi/abi.html#definitions) gives the base and complete object constructors the same effect, as the narrow `time_put<char>` pair in the same file already assumes. A null facet is rejected through the same `RequireArgument` check as the complete-object form.
