# libSceNKWebKit

## add Functional

- [WTF::JSONImpl::Object](../../../core/libs/prx/libSceNKWebKit/src/WtfJsonObject.cpp) (libSceNKWebKit) - not compared with a console module; the Object vtable `writeJSON` slot throws because `ObjectBase::writeJSON` is not exported. `remove`, `getString` and `memoryCost` edit the `HashMap<String, Ref<Value>>` that inline `setValue` callers fill, so they assume the pinned [HashTable.h](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WTF/wtf/HashTable.h#L585-L605) layout: four metadata words before the buckets, `(i + probeCount) & mask` probing, `-1` deleted keys and halving when `keyCount * 6 < tableSize` above 8 buckets. A null object, or a null key on a non-empty table, throws. Closes when: compared with a console module.

## remove `_ZN3WTF8JSONImpl10ObjectBase6removeERKNS_6StringE`

## remove `_ZN3WTF8JSONImpl6Object6createEv`

## remove `_ZNK3WTF8JSONImpl10ObjectBase10memoryCostEv`

## remove `_ZNK3WTF8JSONImpl10ObjectBase9getStringERKNS_6StringERS2_`
