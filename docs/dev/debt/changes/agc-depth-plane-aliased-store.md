# libSceAgcDriver

## add Functional

- [Driver stores over a drawn depth plane](../../../core/libs/prx/libSceAgcDriver/Graphics/src/BdaResources.cpp) (libSceAgcDriver): the completion stamps of BDA writes (`markWrittenPages`) carry no record order, so a BDA write recorded before a depth plane's draw that completes after it reloads the drawn, unstored units it touches at the next sample or bind, where the console keeps the drawn depth. Stores of storage image results and the completion stamps of direct and copied-back buffer writes are ordered when recorded: they leave the units whose last draw was recorded after them (`KeepNewerDepthUnits`), and binding or sampling a plane first stores the pending images over it.
