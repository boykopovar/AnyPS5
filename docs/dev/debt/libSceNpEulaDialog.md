# libSceNpEulaDialog technical debt

### Silent stubs

- [libSceNpEulaDialog](../../../core/libs/prx/libSceNpEulaDialog/Export.cpp) - the PlayStation Network end-user licence agreement dialog runs the common-dialog state machine but shows nothing and reports the user as having cancelled, as there is no PlayStation Network. Its prototypes are not measured: the names and NIDs are from the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceNpEulaDialog.c), no type database gives the signatures ([Orbital's type database](https://github.com/AlexAltea/orbital/blob/b321fcdbd700cc02da1cb1ad25412781d5ec73c6/tools/ida/db_types.json) has none for this library), and they follow the common-dialog shape Orbital documents for the dialogs it types, so `param` is never read and only the leading `int32_t` result code of `result` is written, the layouts being unknown. Returning `0x80B8000D` for an unwritable result pointer is a host-safety fallback with console behavior unmeasured.

### Unknown function info

- [sceNpEulaDialogCheckVersion and sceNpEulaDialogCheckVersionAbort](../../../core/libs/prx/libSceNpEulaDialog/Export.cpp) (libSceNpEulaDialog) - names and NIDs are present in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceNpEulaDialog.c), but no credible prototypes or behaviour were found and they throw. The declarations have no parameters solely to resolve known imports; this is not an ABI claim and callers must not call them until signatures are verified
