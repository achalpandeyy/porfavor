@echo off
if not exist build mkdir build

pushd build

REM Specifying unwind semantics (-EHsc) because I use C++ STL which use exceptions.
cl -nologo -EHsc -W4 -WX -MP -std:c++20 -Od -Zi ../src/main.cpp

echo Build complete.

popd