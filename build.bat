@echo off
if not exist build mkdir build

pushd build

set COMPILER_FLAGS=-nologo -W4 -WX /wd4100 /wd4505 /wd4189 -MP -Zi -I../ext -I../src -D_CRT_SECURE_NO_WARNINGS
set LINKER_FLAGS=/incremental:no

:: NOTE(achal): The simulator
:: cl %COMPILER_FLAGS% /Od -Fe:porfavor ../src/main.c /link %LINKER_FLAGS%

:: NOTE(achal): The haversine stuff
cl %COMPILER_FLAGS% /O2 -Fe:haversine_generator ../src/haversine_generator.cpp /link %LINKER_FLAGS%
cl %COMPILER_FLAGS% /O2 -Fe:haversine           ../src/haversine.cpp           /link %LINKER_FLAGS%

:: NOTE(achal): Repetition Tests
cl %COMPILER_FLAGS% /O2 -Fe:rep_test_file_read   ../src/rep_test_file_read.cpp   /link %LINKER_FLAGS%
cl %COMPILER_FLAGS% /O2 -Fe:rep_test_page_faults ../src/rep_test_page_faults.cpp /link %LINKER_FLAGS%

echo Build complete.

popd