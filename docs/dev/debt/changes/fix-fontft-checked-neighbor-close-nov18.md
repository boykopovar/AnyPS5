# libSceFontFt

## add Functional

- [Font object unlink safety](../../../core/libs/prx/libSceFontFt/src/Driver.cpp): closing a font validates its neighboring list links before releasing its FreeType face or allocation, and reports failure through `sceFontCloseFont` while retaining the handle for retry. The surrounding `libSceFont` list traversal still dereferences its nodes directly, and the host-owned FreeType face pointer embedded in a guest font object is not independently validated. Close retries after a racing change to guest page permissions and arbitrary guest corruption have not been measured on a console. Closes when: traversal and face lifetime are audited against unreadable or concurrently modified guest data.
