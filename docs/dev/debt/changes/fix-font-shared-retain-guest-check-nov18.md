# libSceFont

## add Functional

- [Shared font open reference counts](../../../core/libs/prx/libSceFont/src/Open.cpp): reopening the same face or creating a font instance increments the guest-owned `FontObj` reference count through a checked copy and returns a fatal error without completing the open if that pointer is unwritable. The host leaves the original open handle and count intact for retry. The surrounding `FindSubFont` traversal still dereferences guest list nodes directly, and guest changes to memory protection or ownership racing a checked copy are not modeled. Closes when: unreadable or concurrently modified font lists are handled without unchecked guest access.
