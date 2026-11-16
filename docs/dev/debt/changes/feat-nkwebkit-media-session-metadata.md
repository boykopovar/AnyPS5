# libSceNKWebKit

## add Functional

- [Media session metadata queries](../../../core/libs/prx/libSceNKWebKit/src/MediaSessionMetadata.cpp) (libSceNKWebKit) - `WKMediaSessionMetadataGetTypeID`, `WKMediaSessionMetadataCopyTitle`, `WKMediaSessionMetadataCopyAlbum`, `WKMediaSessionMetadataCopyArtist` and `WKMediaSessionMetadataCopyArtworkURL` inspect media session metadata attributes. Unset strings return empty strings; unset URL returns null. Unmatched or null pointers throw `std::invalid_argument`.
