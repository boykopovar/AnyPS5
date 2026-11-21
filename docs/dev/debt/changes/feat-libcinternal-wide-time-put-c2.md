# libSceLibcInternal

## remove `lOF5jrFNZUA`

## add Functional

- [wide time_put base-object constructor](../../../core/libs/prx/libSceLibcInternal/TimePutConstructors.cpp) (libSceLibcInternal, NID `lOF5jrFNZUA`) - firmware body and console execution are unverified. It forwards to the complete-object constructor in [TimeGetFields.cpp](../../../core/libs/prx/libSceLibcInternal/TimeGetFields.cpp), which builds the classic `time_put<wchar_t>` facet over `g_timePutWideVtable`; `time_put` has no virtual bases in Dinkumware's [xlocnum](https://github.com/microsoft/STL/blob/54d74d7a58e828857a73f979eccc2ba1c02e256d/stl/inc/xlocnum), so the [Itanium C++ ABI](https://itanium-cxx-abi.github.io/cxx-abi/abi.html#definitions) gives the base and complete object constructors the same effect, as the narrow `time_put<char>` pair in the same file already assumes. A null facet throws through the same argument check as the complete-object form.
