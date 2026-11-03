# libfreetype technical debt

### Callable exports

- [Callable exports](../../../core/libs/prx/libfreetype/Export.cpp) - five callable NIDs appear in a 3.20-labelled `libfreetype` image and a later 13.60-labelled `libSceVshFreeType` image. The images have different module and library identities; matching NIDs do not establish forwarding or equivalent behavior. Neither version label is authenticated. Names, signatures and behavior are unknown; these declarations throw and implement no runtime functionality.

- [Older callable NIDs](../../../core/libs/prx/libfreetype/Export.cpp): 19 unidentified exports have unverified names, signatures and behavior. These are throwing import-resolution placeholders; no functionality is implemented. No matching NIDs were found in the later module inventory.
