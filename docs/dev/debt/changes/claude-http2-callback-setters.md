# libSceHttp2

## add Functional

- [sceHttp2SetRedirectCallback and sceHttp2SetSslCallback kinds](../../../core/libs/prx/libSceHttp2/Export.cpp) (libSceHttp2) - extend the TLS settings entry: both accept a template or request, and the redirect callback returns 0x817B1100 for a context or cookie box id, the SSL callback for a cookie box id. Inferred, not measured on these two calls: on the 13.42 module every other option, header and callback setter, including `sceHttp2SetAutoRedirect`, returns 0x817B1100 for those kinds ([#10536](https://github.com/boykopovar/AnyPS5/pull/10536)), and [KytyPS5](https://github.com/KytyPS5/KytyPS5/blob/6fd43ac65264468c7279a578a998b0f7c59af7f4/src/libs/libNet.cpp#L868-L880) looks both up among templates and requests only. An SSL callback on a context still throws, as the SSL flags are accepted there (PPSA06866, [#13715](https://github.com/boykopovar/AnyPS5/issues/13715)) and the console's answer is unknown. Both still throw once transport exists
