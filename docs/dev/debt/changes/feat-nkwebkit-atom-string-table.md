# libSceNKWebKit

## add Functional

- [WTF atom string table](../../../core/libs/prx/libSceNKWebKit/src/WtfAtomString.cpp) (libSceNKWebKit) - the guest `AtomStringTable` is not modelled: each thread has a host table, and `addSlowCase(AtomStringTable&, StringImpl&)` ignores the table passed. Releasing an atom from another thread's table calls `std::terminate`. The rest follows [AtomStringImpl.cpp](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WTF/wtf/text/AtomStringImpl.cpp#L72-L543), [AtomString.cpp](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WTF/wtf/text/AtomString.cpp#L39-L129) and [StringImpl.cpp](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WTF/wtf/text/StringImpl.cpp#L113-L157) (PS5 11.x).
- [WTF string release](../../../core/libs/prx/libSceNKWebKit/src/WtfString.cpp) (libSceNKWebKit) - dropping the last reference to an atom now removes it from the table and frees it, instead of calling `std::terminate`. Symbols still terminate.

## remove | `_ZN3WTF10AtomString16fromUTF8InternalEPKcS2_` | `e3wIdaiVEEM` |

## remove | `_ZN3WTF10AtomString6numberEd` | `CLzum2VlVAQ` |

## remove | `_ZN3WTF10AtomString6numberEf` | `hUrC-aKSSHw` |

## remove | `_ZN3WTF10AtomString6numberEi` | `UKKtpWz--YA` |

## remove | `_ZN3WTF10AtomString6numberEj` | `p-7NOjeWS7g` |

## remove | `_ZN3WTF10AtomString6numberEm` | `58J79QECdTA` |

## remove | `_ZN3WTF10AtomString6numberEy` | `YWAonY+kUEQ` |

## remove | `_ZN3WTF10StringImpl7destroyEPS0_` | `yxwlKktzy6Y` |

## remove | `_ZN3WTF14AtomStringImpl10addLiteralEPKcj` | `m+-AT4JRFBw` |

## remove | `_ZN3WTF14AtomStringImpl11addSlowCaseERNS_10StringImplE` | `47H3Xt0ke9s` |

## remove | `_ZN3WTF14AtomStringImpl11addSlowCaseERNS_15AtomStringTableERNS_10StringImplE` | `cUU3-6ewPWM` |

## remove | `_ZN3WTF14AtomStringImpl14lookUpSlowCaseERNS_10StringImplE` | `57WAc7JREYI` |

## remove | `_ZN3WTF14AtomStringImpl3addEPKDsj` | `QHaMM3hoxqk` |

## remove | `_ZN3WTF14AtomStringImpl3addEPKhj` | `4SU1b9bcL18` |

## remove | `_ZN3WTF14AtomStringImpl3addEPKNS_10StringImpl16StaticStringImplE` | `CHSR+Bn4MmA` |

## remove | `_ZN3WTF14AtomStringImpl3addEPNS_10StringImplEjj` | `Rt3lL1byIzc` |

## remove | `_ZN3WTF14AtomStringImpl6lookUpEPKDsj` | `F8WuSFdu4is` |

## remove | `_ZN3WTF14AtomStringImpl6lookUpEPKhj` | `BuAIhZn+1Rs` |

## remove | `_ZNK3WTF10AtomString23convertToASCIILowercaseEv` | `MUpSmygnJSM` |

## remove | `_ZNK3WTF10AtomString23convertToASCIIUppercaseEv` | `uXJeEurJF8c` |
