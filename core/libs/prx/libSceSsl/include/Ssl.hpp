#pragma once

#include "SceTypes.hpp"
#include "prx/libSceSsl/SslTypes.hpp"

using SslVerifyCallback = int (APS5_VABI *)(int, unsigned int, void**, int, void*);

extern "C" {

int APS5_VABI sceSslInit_nid_postfix(std::uint64_t poolSize);
int APS5_VABI sceSslTerm_nid_postfix(int sslCtxId);
int APS5_VABI sceSslGetMemoryPoolStats(int sslCtxId, SceSsl::MemoryPoolStats* stats);
int APS5_VABI sceSslLoadCert(int sslCtxId, int caCertNum, SceSsl::Data** caList, SceSsl::Data* cert, SceSsl::Data* privateKey);
int APS5_VABI sceSslUnloadCert(int sslCtxId);
int APS5_VABI sceSslGetCaList(int sslCtxId, SceSsl::CaList* output);
int APS5_VABI sceSslFreeCaList(int sslCtxId, SceSsl::CaList* list);
int APS5_VABI sceSslGetCaCerts(int sslCtxId, SceSsl::CaCerts* output);
int APS5_VABI sceSslFreeCaCerts(int sslCtxId, SceSsl::CaCerts* certificates);
int APS5_VABI sceSslCreateConnection(int sslCtxId, int sockId, const char* host);
int APS5_VABI sceSslDeleteConnection(int sslConnectionId);
int APS5_VABI sceSslSetVerifyCallback(int sslConnectionId, SslVerifyCallback cbfunc, void* userArg);
int APS5_VABI sceSslConnect(int sslConnectionId);
int APS5_VABI sceSslRead(int sslConnectionId, void* buf, size_t len, int flags);
int APS5_VABI sceSslWrite(int sslConnectionId, void* buf, size_t len, int flags);
int APS5_VABI sceSslClose(int sslConnectionId);
int APS5_VABI sceSslGetSerialNumber(int sslCtxId, void* sslCert, unsigned char* sboData, std::size_t* sboLen);
int APS5_VABI sceSslFreeSslCertName(int sslCtxId, void* certName);
int APS5_VABI sceSslEnableVerifyOption(int sslConnectionId, std::uint32_t option);
int APS5_VABI sceSslDisableVerifyOption(int sslConnectionId, std::uint32_t option);
const char* APS5_VABI sceSslGetAlpnSelected(int sslConnectionId);
int APS5_VABI sceSslSetAlpn(int sslConnectionId, const char* proto);
int APS5_VABI sceSslSetMinSslVersion(int sslConnectionId, int version);
void* APS5_VABI sceSslGetIssuerName(int sslCtxId, void* sslCert);
int APS5_VABI sceSslGetNameEntryCount(int sslCtxId, void* certName);
int APS5_VABI sceSslGetNameEntryInfo(int sslCtxId, void* certName, int entryNum, char* oidname, std::size_t maxOidnameLen, char* value, std::size_t maxValueLen, std::size_t* valueLen);
int APS5_VABI sceSslGetNotBefore(int sslCtxId, void* sslCert, SceSsl::RtcTick* begin);
int APS5_VABI sceSslGetNotAfter(int sslCtxId, void* sslCert, SceSsl::RtcTick* limit);
int APS5_VABI sceSslGetFingerprint(int sslCtxId, void* sslCert, int hashType, unsigned char* fingerprint);
int APS5_VABI sceSslGetPem(int sslCtxId, void* sslCert, char* buf, std::size_t* bufLen);
void* APS5_VABI sceSslGetSubjectName(int sslCtxId, void* sslCert);
int APS5_VABI CA_MGMT_extractKeyBlobEx();
int APS5_VABI CA_MGMT_extractPublicKeyInfo();
int APS5_VABI CA_MGMT_freeKeyBlob();
int APS5_VABI CRYPTO_initAsymmetricKey();
int APS5_VABI CRYPTO_uninitAsymmetricKey();
int APS5_VABI RSA_verifySignature();
int APS5_VABI VLONG_freeVlongQueue();
int APS5_VABI sceSslCheckRecvPending();
int APS5_VABI sceSslCreateSslConnection();
int APS5_VABI sceSslDeleteSslConnection();
int APS5_VABI sceSslDisableOption();
int APS5_VABI sceSslDisableOptionInternal();
int APS5_VABI sceSslDisableOptionInternalInsecure();
int APS5_VABI sceSslEnableOption();
int APS5_VABI sceSslEnableOptionInternal();
int APS5_VABI sceSslGetNanoSSLModuleId();
int APS5_VABI sceSslGetPeerCert();
int APS5_VABI sceSslGetSslError();
int APS5_VABI sceSslLoadRootCACert();
int APS5_VABI sceSslRecv(int sslConnectionId, void* buf, std::size_t fulllen);
int APS5_VABI sceSslReuseConnection(int sslConnectionId, int sockId);
int APS5_VABI sceSslSend(int sslConnectionId, void* msg, std::size_t fulllen);
int APS5_VABI sceSslSetSslVersion();
int APS5_VABI sceSslSetServerName();

}
