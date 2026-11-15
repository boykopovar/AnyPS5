# libSceNKWebKit

## add Functional

- [WTF::StringPrintStream](../../../core/libs/prx/libSceNKWebKit/src/WtfStringPrintStream.cpp) (libSceNKWebKit) - not compared with a console module. The 160-byte object, the buffer growth and the 16-byte `Expected<String, UTF8ConversionError>` follow the pinned [StringPrintStream.cpp](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WTF/wtf/StringPrintStream.cpp) and `Expected.h`, measured on a Linux build of the same code. The constructor installs a vtable built here (complete and deleting destructors, `vprintf`, an empty `flush`, and the `PrintStream::begin` and `PrintStream::end` exports), not the module's, so printing through `print` throws until `PrintStream::end` is implemented. `vprintf` formats with guest `vsnprintf`. A null object, format or argument list throws, and so does a failed format, where the source would move its write position backwards.

## remove | `_ZN3WTF17StringPrintStreamC1Ev` | `wNhdQWqZALM` |

## remove | `_ZN3WTF17StringPrintStreamC2Ev` | `zQLVD+vqI0Y` |

## remove | `_ZN3WTF17StringPrintStreamD0Ev` | `eSdAHApUsso` |

## remove | `_ZN3WTF17StringPrintStreamD1Ev` | `EOmLhk6TiW4` |

## remove | `_ZN3WTF17StringPrintStreamD2Ev` | `DyzD9dy1J9M` |

## remove | `_ZN3WTF17StringPrintStream7vprintfEPKcP13__va_list_tag` | `iXgnlJmCSP4` |

## remove | `_ZN3WTF17StringPrintStream9toCStringEv` | `7RZABO0cMFI` |

## remove | `_ZN3WTF17StringPrintStream11tryToStringEv` | `TrwAHMfEpsw` |

## remove | `_ZN3WTF17StringPrintStream8toStringEv` | `52xCgnSgAkY` |

## remove | `_ZN3WTF17StringPrintStream26toStringWithLatin1FallbackEv` | `cE9rGwgKhic` |

## remove | `_ZN3WTF17StringPrintStream5resetEv` | `6PPm2rZiMcs` |
