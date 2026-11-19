FROM python:3.13-slim@sha256:e48f59cdba69a3cbd9ed2cf0f348d90d45a8d350d98134cf1070bc106ee9ed90 AS certificates
FROM ubuntu:24.04@sha256:73b999393d07f2bf65a40373778a44dc9a6d54ad7718d29247d8ff9413c4d6a8 AS tooling
ENV DEBIAN_FRONTEND=noninteractive WINEDEBUG=-all WINEARCH=win64 WINEPREFIX=/opt/wine WINEDLLOVERRIDES=mscoree,mshtml=
COPY --from=certificates /etc/ssl/certs/ca-certificates.crt /etc/ssl/certs/ca-certificates.crt
RUN printf 'Types: deb\nURIs: https://snapshot.ubuntu.com/ubuntu/20261118T000000Z/\nSuites: noble noble-updates noble-security noble-backports\nComponents: main universe restricted multiverse\nSigned-By: /usr/share/keyrings/ubuntu-archive-keyring.gpg\nCheck-Valid-Until: no\n' > /etc/apt/sources.list.d/ubuntu.sources && apt-get update && apt-get install -y --no-install-recommends wine64=9.0~repack-4build3 wine=9.0~repack-4build3 libwine=9.0~repack-4build3 xvfb xauth ca-certificates curl unzip python3 git time && rm -rf /var/lib/apt/lists/*
RUN mkdir -p /opt/toolchain && curl --fail --location --retry 3 --retry-delay 3 -o /tmp/winlibs.zip https://github.com/brechtsanders/winlibs_mingw/releases/download/15.2.0posix-14.0.0-ucrt-r7/winlibs-x86_64-posix-seh-gcc-15.2.0-mingw-w64ucrt-14.0.0-r7.zip && echo 'cb2fbad6162540cdf5e1facdce08d4dac359e8cf64f7f696a99274291763b815  /tmp/winlibs.zip' | sha256sum -c - && unzip -q /tmp/winlibs.zip -d /opt/toolchain && rm /tmp/winlibs.zip
RUN xvfb-run -a wineboot --init && wineserver -w
COPY tools/engine/sources.py tools/engine/compatibility.py tools/engine/bootstrap.py tools/engine/dependencies.lock /opt/buildscripts/
RUN python3 /opt/buildscripts/bootstrap.py
FROM tooling AS supplied
ARG ANYPS5_UPSTREAM_REF=3c07e1d05e545cb5988041da0fa63ea8dd36673e
ADD --keep-git-dir=true https://github.com/boykopovar/AnyPS5.git#${ANYPS5_UPSTREAM_REF} /upstream
COPY . /supplied
FROM supplied AS compile
ARG ANYPS5_BUILD_JOBS=4
ARG ANYPS5_CACHE_SCOPE=default
ARG ANYPS5_UPSTREAM_REF=3c07e1d05e545cb5988041da0fa63ea8dd36673e
ENV ANYPS5_BUILD_JOBS=${ANYPS5_BUILD_JOBS}
ENV ANYPS5_UPSTREAM_REF=${ANYPS5_UPSTREAM_REF}
COPY tools/engine/build.py tools/engine/package.py tools/engine/bundle.py tools/engine/pe.py tools/engine/upstream.py tools/engine/upstream.lock /opt/buildscripts/
RUN --mount=type=cache,target=/work,id=anyps5-winlibs-r7-${ANYPS5_CACHE_SCOPE},sharing=locked python3 -u /opt/buildscripts/build.py
FROM python:3.13-slim@sha256:e48f59cdba69a3cbd9ed2cf0f348d90d45a8d350d98134cf1070bc106ee9ed90
COPY --from=compile /payload /payload
COPY tools/engine/export.py tools/engine/bundle.py tools/engine/pe.py /app/
CMD ["python", "/app/export.py"]
