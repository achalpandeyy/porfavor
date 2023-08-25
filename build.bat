@echo off
if not exist build mkdir build

pushd build

cl -nologo -W4 -WX -wd4100 -MP -Od -Zi -I../ext -Fe:porfavor ../src/main.c

echo Build complete.

popd