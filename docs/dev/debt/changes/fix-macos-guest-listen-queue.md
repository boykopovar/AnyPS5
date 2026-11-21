# libSceNet

## add Functional

- [sceNetListen, sceNetConnect, sceNetAccept](../../../core/libs/prx/libSceNet/Export.cpp) (libSceNet) model FreeBSD 11.4's listen queue: `sonewconn` drops a connection once `so_qlen > 3 * so_qlimit / 2` ([uipc_socket.c:570](https://github.com/freebsd/freebsd-src/blob/8c639945a44588f6cfdddfb057bb5aad88735da0/sys/kern/uipc_socket.c#L570)), and `solisten_proto` keeps the backlog as `so_qlimit`, so a backlog of 0 completes one connection and drops the rest. Each guest listener counts completed but unaccepted connections, `sceNetAccept` releases one, and a blocking `sceNetConnect` with a connect timeout past that bound waits the timeout and returns `EAGAIN`. Not covered: a non-blocking connect, a connection from outside the guest socket set, a negative backlog clamped to FreeBSD's default `somaxconn` 128, and the count resetting when the listener closes. Not measured on a console.
