# libSceNKWebKit

## change [WTF and WebCore value layouts]

- [WTF and WebCore value layouts](../../../core/libs/prx/libSceNKWebKit/src/WtfStringAbi.hpp) (libSceNKWebKit) - `WTF::String`, `StringImpl`, `StringView`, `CString`, `URL` and the WebCore geometry, transform and `Color` layouts follow the release 64-bit headers at the [pinned source](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WTF/wtf/text/StringImpl.h) and are pinned with `static_assert`; they are tested on Windows, not against console binaries. Atom and symbol strings and external buffers still throw from `StringImpl::destroy` and terminate when their last reference is released. `Expected<unsigned, HTMLIntegerParsingError>` follows the pinned `Expected.h`; its console ABI is unverified. The `MappedFileData` pointer-and-unsigned layout follows the pinned source, not a measured console object.

## change [WebCore::CookieUtil::defaultPathForURL]

- [WebCore::CookieUtil::defaultPathForURL](../../../core/libs/prx/libSceNKWebKit/src/CookieUtil.cpp) - console URL/String layouts and sret ABI remain unverified. Follows [CookieUtil.cpp](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WebCore/platform/network/curl/CookieUtil.cpp#L184-L198): the existing URL path StringView export excludes query/fragment and yields an empty view for invalid URLs. Empty or non-slash-leading paths return `/`; otherwise the last slash and suffix are removed, with `/` when that slash is first. Path and prefix copies preserve character width; fallback StringImpls borrow static literal bytes. Host ABI release is balanced. Tests construct the URL layout already on main, without unmerged parser exports. Closes when: compared with a console module.

## add Functional

- [StringImpl::destroy and adopt](../../../core/libs/prx/libSceNKWebKit/src/WtfString.cpp) (libSceNKWebKit) - not compared with a console module; the console `StringBuffer` layout and `Ref<StringImpl>` return ABI are unverified. `destroy` throws through `NotImplemented_nid_no_patch` for atom and symbol strings and external buffers (no atom table, symbol registry or external free function exists here), and for a null or static string where the source asserts; it frees internal, owned and substring buffers without reading the reference count, as the pinned [StringImpl.cpp](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WTF/wtf/text/StringImpl.cpp#L113-L157) does. `adopt` reads the `{unsigned length; CharType* data}` layout of the pinned `StringBuffer.h` and takes the guest `fastMalloc` buffer without copying, clearing the data and keeping the length; a nonempty buffer without data throws.

## remove | `_ZN3WTF10StringImpl5adoptEONS_12StringBufferIDsEE` | `jnH78zLM6qY` |

## remove | `_ZN3WTF10StringImpl5adoptEONS_12StringBufferIhEE` | `E0H-Rg9JQxQ` |

## remove | `_ZN3WTF10StringImpl7destroyEPS0_` | `yxwlKktzy6Y` |
