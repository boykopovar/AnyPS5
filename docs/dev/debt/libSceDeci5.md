# libSceDeci5 technical debt

### Unknown function info

The 7 exports in [Export.cpp](../../../core/libs/prx/libSceDeci5/Export.cpp) come from the `libSceDeci5` list in [claimore22/ps5rs](https://github.com/claimore22/ps5rs/blob/5f252a658ff470bcb2b6cd15205fd8ca64ae4477/data/stubs/by_library/libSceDeci5_nosubmission.txt) (commit 5f252a658ff470bcb2b6cd15205fd8ca64ae4477), a catalogue of PS5 symbols per library whose provenance the project does not document, and no PS5 export inventory of this module is available to confirm the list is complete or exact. Every name was kept only because its hash equals the listed NID. Prototypes and behaviour are unverified: every function takes no parameters in the import-resolution convention and throws through `NotImplemented_nid_no_patch`.

- [sceDeci5Write](../../../core/libs/prx/libSceDeci5/Export.cpp) (libSceDeci5) - unknown signature (NID 5IVqeThOi2s)
- [sceDeci5Read](../../../core/libs/prx/libSceDeci5/Export.cpp) (libSceDeci5) - unknown signature (NID 8T8VUmTIKE0)
- [sceDeci5CreateHostProcessAndWait](../../../core/libs/prx/libSceDeci5/Export.cpp) (libSceDeci5) - unknown signature (NID Ez2iQHniESc)
- [sceDeci5Close](../../../core/libs/prx/libSceDeci5/Export.cpp) (libSceDeci5) - unknown signature (NID gYg5m+zII9w)
- [sceDeci5CreateHostProcess](../../../core/libs/prx/libSceDeci5/Export.cpp) (libSceDeci5) - unknown signature (NID nS4i1N-HEws)
- [sceDeci5Open](../../../core/libs/prx/libSceDeci5/Export.cpp) (libSceDeci5) - unknown signature (NID sLfxYROuNZU)
- [sceDeci5GetStatus](../../../core/libs/prx/libSceDeci5/Export.cpp) (libSceDeci5) - unknown signature (NID v-VB0EguQWg)
