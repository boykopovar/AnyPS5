# libSceFreeTypeOt

## add Functional

- [`ft_validator_run`](../../../core/libs/prx/libSceFreeTypeOt/Export.cpp) has an unmeasured console ABI and behavior. The PS5 3.20 inventory lists NID `5DQUuOYK190`. [FreeType 2.4.12](https://github.com/freetype/freetype/blob/cecf93ef90c660a8b0b45e5adbbfb5ea443fb6b9/src/base/ftobjs.c#L94-L101) and the bundled FreeType both return `-1` without accessing the validator.
