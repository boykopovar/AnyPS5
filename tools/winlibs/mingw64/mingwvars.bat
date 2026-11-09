@ECHO OFF
ECHO Setting up environment for using MinGW-w64 with GCC from %~dp0
SET PATH=%~dp0bin;%PATH%
SET CC=%~dp0bin/x86_64-w64-mingw32-gcc.exe
SET CXX=%~dp0bin/x86_64-w64-mingw32-g++.exe
SET FC=%~dp0bin/x86_64-w64-mingw32-gfortran.exe
SET F77=%~dp0bin/x86_64-w64-mingw32-gfortran.exe
SET LD=%~dp0bin/ld.exe
SET AR=%~dp0bin/x86_64-w64-mingw32-gcc-ar.exe
SET RANLIB=%~dp0bin/x86_64-w64-mingw32-gcc-ranlib.exe
SET WINDRES=%~dp0bin/windres.exe
SET RC=%~dp0bin/windres.exe
SET NM=%~dp0bin/x86_64-w64-mingw32-gcc-nm.exe
SET DLLTOOL=%~dp0bin/dlltool.exe
SET STRIP=%~dp0bin/strip.exe
SET PKG_CONFIG=%~dp0bin/pkgconf.exe
REM SET PKG_CONFIG_PATH=
REM To enable ccache remove REM from the next lines
REM SET CC=%~dp0bin/ccache.exe %CC%
REM SET CXX=%~dp0bin/ccache.exe %CXX%
