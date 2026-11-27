# libSceFreeTypeOt

## add Pending dependency exports

- [FT_Attach_File](../../../core/libs/prx/libSceFreeTypeOt/ForwardExports.cpp) (NID `21vkVuOP2lk`) has an unverified console ABI and error behavior. Windows forwards to the [libSceFreeType adapter](../../../core/libs/prx/libSceFreeType/Export.cpp), which resolves a guest path and attaches the bytes through the native FreeType memory stream. A file above the host `FT_Long` range throws. Successful attachment publishes the face fields updated by FreeType.
