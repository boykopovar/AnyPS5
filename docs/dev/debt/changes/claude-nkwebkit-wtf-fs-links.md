# libSceNKWebKit

## add Functional

- [WTF file links and appends](../../../core/libs/prx/libSceNKWebKit/src/WtfSystem.cpp) (libSceNKWebKit) - not compared with a console module. `hardLink` and `createSymbolicLink` assume the console `create_hard_link` and `create_symlink` are single `link` and `symlink` calls, as in LLVM libc++, and call the guest `link_nid_postfix` and `symlink_nid_postfix`, so a null path fails with `ENOENT` and an absolute link target throws. `appendFileContentsToFileHandle` uses a host buffer, not `fastMalloc`. They follow the pinned [FileSystem.cpp](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WTF/wtf/FileSystem.cpp#L249-L728) and its POSIX `EINTR` retries. Symbolic links are tested on Linux only. Closes when: compared with a console module.

## add Additional callable exports

- [Additional callable exports](../../../core/libs/prx/libSceNKWebKit/Export.cpp) - 5936 of the 8703 inventory declarations listed below throw through `NotImplemented_nid_no_patch`; signatures and behavior are unverified. The later 13.60-labelled image contains all 8703; 8448 match the public 3.20 catalogue, while 255 are absent and their presence in retail 3.20 is unverified. Per-image records show the latter in a separate 3.20-labelled image, but neither image label is authenticated. Identified names match NIDs in the [public symbol database](https://github.com/zecoxao/sce_symbols/blob/2883963a0a514ba6e77407a08ef4730a65e88254/aerolib.csv) or [supplemental catalog](https://github.com/claimore22/ps5rs/blob/2304be5131221bae0dac24b9767cabffda33f747/data/nids.csv). Data exports need separate analysis.

## remove `_ZN3WTF14FileSystemImpl18createSymbolicLinkERKNS_6StringES3_`

## remove `_ZN3WTF14FileSystemImpl30appendFileContentsToFileHandleERKNS_6StringERi`

## remove `_ZN3WTF14FileSystemImpl8hardLinkERKNS_6StringES3_`
