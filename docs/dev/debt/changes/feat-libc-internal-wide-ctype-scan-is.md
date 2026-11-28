# libSceLibcInternal

## change pSQz254t3ug

- [Wide ctype `do_scan_is`](../../../core/libs/prx/libSceLibcInternal/WideCtype.cpp) (NID `pSQz254t3ug`) has not been compared with the console module. It follows the [Dinkumware wide ctype scan](https://github.com/microsoft/STL/blob/219514876ea86491de191ceaa88632616bbc0f19/stl/inc/xlocale#L2536-L2544) and uses the same guest `do_is` virtual slot and checked wide-character reads as `do_scan_not`.
