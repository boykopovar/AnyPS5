# libSceNpSns technical debt

### Unknown function info

- [sceNpSnsFacebookAbortRequest, sceNpSnsFacebookCreateRequest, sceNpSnsFacebookDeleteRequest, sceNpSnsFacebookGetAccessToken, sceNpSnsIntAbortRequest, sceNpSnsIntCreateRequest, sceNpSnsIntDeleteRequest, sceNpSnsIntFbGetGameAccessToken, sceNpSnsIntFbGetGameAccessTokenAllowed, sceNpSnsIntFbGetSystemAccessToken, sceNpSnsIntFbGetTitleInfo, sceNpSnsIntLinkedStatus, sceNpSnsIntTwGetSystemAccessToken, sceNpSnsIntUnlink, sceNpSnsIntYtGetAccessToken, sceNpSnsIntYtRefreshMasterToken, sceNpSnsTwitchAbortRequest, sceNpSnsTwitchCreateRequest, sceNpSnsTwitchDeleteRequest, sceNpSnsTwitchGetAccessToken, sceNpSnsYouTubeAbortRequest, sceNpSnsYouTubeCreateRequest, sceNpSnsYouTubeDeleteRequest and sceNpSnsYouTubeGetAccessToken](../../../core/libs/prx/libSceNpSns/Export.cpp) (libSceNpSns) - names and NIDs are present in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceNpSns.c), but no credible prototypes were found. The declarations have no parameters solely to resolve known imports; this is not an ABI claim and callers must not call them until signatures are verified

### Inventory exports declared elsewhere
 sceNpSnsFacebookCreateRequest, sceNpSnsFacebookDeleteRequest and sceNpSnsFacebookGetAccessToken are also 3.20 exports of this module but are currently declared by #6977 in libSceNpSnsFacebookDialog, so they are omitted here to respect the duplicate-export rule; re-add if that PR's module attribution is rejected.
