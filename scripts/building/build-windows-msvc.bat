@echo off

rem Make sure that we build in 64-bit.
set OVERRIDE_ARCH=64

set MIDIEDITOR_RELEASE_DATE=%date:~4%

qmake -project -v || exit /b 1
qmake midieditor.pro || exit /b 1
nmake /nologo

