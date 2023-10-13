@echo off
if not exist build mkdir build

pushd build

set COMPILER_FLAGS=-nologo -W4 -WX /wd4100 /wd4505 -MP -Zi -I../ext

rem NOTE(achal): Part 1: The simulator
cl %COMPILER_FLAGS% /Od -Fe:porfavor ../src/main.c

rem NOTE(achal): Part 2: The haversine stuff
cl %COMPILER_FLAGS% -D_CRT_SECURE_NO_WARNINGS /O2 -Fe:haversine_generator ../src/haversine_generator.cpp
cl %COMPILER_FLAGS% -D_CRT_SECURE_NO_WARNINGS /O2 -Fe:haversine ../src/haversine.cpp
cl %COMPILER_FLAGS% -D_CRT_SECURE_NO_WARNINGS /O2 -Fe:estimate_cpu_frequency_rdtsc ../src/estimate_cpu_frequency_rdtsc.cpp

rem NOTE(achal): Tests
cl %COMPILER_FLAGS% /O2 ../src/test_haversine_profiler.cpp

echo Build complete.

popd