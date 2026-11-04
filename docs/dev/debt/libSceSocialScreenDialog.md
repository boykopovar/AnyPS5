# libSceSocialScreenDialog technical debt

### Silent stubs

- [libSceSocialScreenDialog](../../../core/libs/prx/libSceSocialScreenDialog/Export.cpp) - the social screen dialog runs the common-dialog state machine but shows nothing and reports the user as having cancelled, as there is no second-screen viewer. In `sceSocialScreenDialogGetResult`, PS5 firmware 13.42 module disassembly (offset 0x360) and native execution data confirm that a null result pointer is checked before NOT_FINISHED (returning COMMON_DIALOG_ERROR_ARG_NULL). The signatures are from [Orbital's type database](https://github.com/AlexAltea/orbital/blob/b321fcdbd700cc02da1cb1ad25412781d5ec73c6/tools/ida/db_types.json); the result layout beyond the leading `int32_t` result code is not known, so nothing else is written.
