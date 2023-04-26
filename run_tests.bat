@echo off 

pushd tests

if exist out_listing_0037_single_register_mov.asm   del out_listing_0037_single_register_mov.asm
if exist out_listing_0038_many_register_mov.asm     del out_listing_0038_many_register_mov.asm
if exist out_listing_0039_more_movs.asm             del out_listing_0039_more_movs.asm
if exist out_listing_0040_challenge_movs.asm        del out_listing_0040_challenge_movs.asm
if exist out_listing_0041_add_sub_cmp_jnz.asm       del out_listing_0041_add_sub_cmp_jnz.asm

if exist out_listing_0037_single_register_mov       del out_listing_0037_single_register_mov
if exist out_listing_0038_many_register_mov         del out_listing_0038_many_register_mov
if exist out_listing_0039_more_movs                 del out_listing_0039_more_movs
if exist out_listing_0040_challenge_movs            del out_listing_0040_challenge_movs
if exist out_listing_0041_add_sub_cmp_jnz           del out_listing_0041_add_sub_cmp_jnz

nasm listing_0037_single_register_mov.asm
nasm listing_0038_many_register_mov.asm
nasm listing_0039_more_movs.asm
nasm listing_0040_challenge_movs.asm
nasm listing_0041_add_sub_cmp_jnz.asm
popd

build\main.exe tests\listing_0037_single_register_mov
build\main.exe tests\listing_0038_many_register_mov
build\main.exe tests\listing_0039_more_movs
build\main.exe tests\listing_0040_challenge_movs
build\main.exe tests\listing_0041_add_sub_cmp_jnz

pushd tests

nasm out_listing_0037_single_register_mov.asm
nasm out_listing_0038_many_register_mov.asm
nasm out_listing_0039_more_movs.asm
nasm out_listing_0040_challenge_movs.asm
nasm out_listing_0041_add_sub_cmp_jnz.asm

fc out_listing_0037_single_register_mov listing_0037_single_register_mov
fc out_listing_0038_many_register_mov listing_0038_many_register_mov
fc out_listing_0039_more_movs listing_0039_more_movs
fc out_listing_0040_challenge_movs listing_0040_challenge_movs
fc out_listing_0041_add_sub_cmp_jnz listing_0041_add_sub_cmp_jnz

popd