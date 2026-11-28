# libSceNpCppWebApi

## change [sce::Np::CppWebApi::TitleCloudStorage::V1 and InGameCatalog::V5]

- [sce::Np::CppWebApi::TitleCloudStorage::V1 and InGameCatalog::V5](../../../core/libs/prx/libSceNpCppWebApi/Export.cpp) (libSceNpCppWebApi) - console behaviour not measured. `ContainerApi::ParameterToGetContainer` allocates a host 0x88-byte object with `Common::String` at +0x0/+0x58; `setkeepHtmlTag`/`setuseCurrencySymbol` store the value and is-set byte at +0x29/+0x2a and +0x53/+0x54; the other setters store nothing. `initialize`, `terminate` and the string setters return the PPSA21564 codes: 0x80553503, 0x80553502, 0x8055350f, 0x80553510 and 0x80553512. The upload and set-data-info parameters only track initialization. Offsets come from PPSA21564/PPSA23732; the host object uses the host allocator. `DataStatus` getters and the `UploadDataResponseHeaders` getters read measured offsets; other response accessors throw.

## add Functional

- [TitleCloudStorage::V1 slot parameters](../../../core/libs/prx/libSceNpCppWebApi/Export.cpp) use the host allocator for the owned account, slot, range, conditional and filter strings in download, multi-slot status and multi-slot deletion parameters. PPSA28050 supplies the observed object layouts and normal error precedence. Callers must explicitly terminate before destruction to release the owned buffers. In the PPSA28050 module all three D1 destructors leave the 16-byte handle and its live allocations intact. D1 after termination also leaves the handle and allocation balance unchanged. Guest custom-allocator behavior and other module versions remain unverified. Remote submissions still report the signed-out error.
