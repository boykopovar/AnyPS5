#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"
#include "prx/libSceNKWebKit/src/Types.hpp"

extern "C" {

const void* APS5_VABI WKMediaSessionMetadataCopyAlbum(const void* metadataRef) {
    const auto* metadata = NKWebKit::Cast<NKWebKit::MediaSessionMetadata>(metadataRef, __func__);
    return metadata->album ? NKWebKit::ToRef(metadata->album) : NKWebKit::ToRef(NKWebKit::String::FromUtf8(""));
}

const void* APS5_VABI WKMediaSessionMetadataCopyArtist(const void* metadataRef) {
    const auto* metadata = NKWebKit::Cast<NKWebKit::MediaSessionMetadata>(metadataRef, __func__);
    return metadata->artist ? NKWebKit::ToRef(metadata->artist) : NKWebKit::ToRef(NKWebKit::String::FromUtf8(""));
}

const void* APS5_VABI WKMediaSessionMetadataCopyArtworkURL(const void* metadataRef) {
    const auto* metadata = NKWebKit::Cast<NKWebKit::MediaSessionMetadata>(metadataRef, __func__);
    return metadata->artworkURL ? NKWebKit::ToRef(metadata->artworkURL) : nullptr;
}

const void* APS5_VABI WKMediaSessionMetadataCopyTitle(const void* metadataRef) {
    const auto* metadata = NKWebKit::Cast<NKWebKit::MediaSessionMetadata>(metadataRef, __func__);
    return metadata->title ? NKWebKit::ToRef(metadata->title) : NKWebKit::ToRef(NKWebKit::String::FromUtf8(""));
}

}
