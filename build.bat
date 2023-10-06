@echo off
if not exist build mkdir build

pushd build

set COMPILER_FLAGS=-nologo -W4 -WX -wd4100 -MP -Zi -I../ext

cl %COMPILER_FLAGS% /Od -Fe:porfavor ../src/main.c
cl %COMPILER_FLAGS% -D_CRT_SECURE_NO_WARNINGS /O2 -Fe:haversine ../src/haversine.cpp

echo Build complete.

popd