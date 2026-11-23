# libSceAgcDriver

## add Functional

- [Host imports over label read traps](../../../core/libs/prx/libSceAgcDriver/Graphics/src/GuestBufferMemory.cpp) (libSceAgcDriver, macOS): an import whose pages hold a [label read trap](../../../core/libs/prx/libSceAgcDriver/Graphics/src/Texture.cpp), or one another thread is resolving, when its host write would open is not made. That request falls back to copies, the traps go to the write watch's thread instead of being resolved on the importing thread under the import locks, and the refusal is not remembered, so a later request imports the run. An import over pages already trapped when it starts fails its access check and is remembered as failed, as before. The import's page hash skips trapped pages, but a trap set between that check and the hash's read of the page still waits for the resolver while the import locks are held.
