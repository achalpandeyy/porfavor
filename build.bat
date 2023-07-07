@echo off
if not exist build mkdir build

pushd build

cl -nologo -W4 -WX -wd4100 -MP -Od -Zi ../src/main.c

echo Build complete.

popd