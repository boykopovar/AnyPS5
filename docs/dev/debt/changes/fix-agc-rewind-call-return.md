# libSceAgcDriver

## add Functional

- [Packets after a call that stops at a REWIND](../../../core/libs/prx/libSceAgcDriver/Execution/src/Driver/Queues/Submission.cpp) (libSceAgcDriver) - the CP reads a caller's packets after an INDIRECT_BUFFER when the call returns; when the called buffer stops at a REWIND, they are copied at submit instead, innermost caller first, and run once the REWIND is released and the called buffer's tail has run. A title that rewrites them before releasing the REWIND runs the copy. A COND_INDIRECT_BUFFER branch replaces the rest of its buffer, so only the packets after the calls that reached it run. A further REWIND among them is waited on, and its tail read, at the address the submitted buffer gave it: the title's own for a PS5 title, on the Gnm path the temporary merged copy, which is freed when the submission returns, so there the wait reads freed memory and never sees the title's release.
