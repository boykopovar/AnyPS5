# libSceSystemService

## add Functional

- [sceAppMessagingSendMsg, ReceiveMsg and event flags](../../../core/libs/prx/libSceSystemService/src/AppMessaging.cpp) - the sends validate the payload size and pointer and then drop the message: no system component receives it, so the receive queue has no producer and `ReceiveMsg` blocks until a message is delivered. The receive forms return `0x80C50005` for an empty queue and `0x80C50004` for a null output; one global 64-bit event flag implements Set, Clear and TryGet. The return codes `0x80C50001`, `0x80C50004` and `0x80C50005` are chosen values, not taken from firmware or an SDK header, and the PS5 system-component backend and per-application queues are not modelled or measured. Closes when: compared with the console.
