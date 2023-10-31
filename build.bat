@echo off
if not exist build mkdir build

pushd build

set COMPILER_FLAGS=-nologo -W4 -WX /wd4100 /wd4505 /wd4189 -MP -Zi -I../ext
set LINKER_FLAGS=/incremental:no

:: NOTE(achal): Part 1: The simulator
:: cl %COMPILER_FLAGS% /Od -Fe:porfavor ../src/main.c /link %LINKER_FLAGS%

:: NOTE(achal): Part 2: The haversine stuff
:: cl %COMPILER_FLAGS% -D_CRT_SECURE_NO_WARNINGS /O2 -Fe:haversine_generator ../src/haversine_generator.cpp /link %LINKER_FLAGS%
cl %COMPILER_FLAGS% -D_CRT_SECURE_NO_WARNINGS /O2 -Fe:haversine ../src/haversine.cpp /link %LINKER_FLAGS%
:: cl %COMPILER_FLAGS% -D_CRT_SECURE_NO_WARNINGS /O2 -Fe:estimate_cpu_frequency_rdtsc ../src/estimate_cpu_frequency_rdtsc.cpp /link %LINKER_FLAGS%

:: NOTE(achal): Tests
:: cl %COMPILER_FLAGS% /O2 ../src/test_haversine_profiler.cpp /link %LINKER_FLAGS%

echo Build complete.

popd