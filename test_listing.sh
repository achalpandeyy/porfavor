#!/bin/bash

# NOTE(achal): Usage: test_listing.sh "tests\listing_00*"
# Notice the path in double quotes, this is important because we need to pass raw strings to the script.

export PATH="$PATH:/mnt/c/Program Files/NASM"

build/porfavor.exe "$1" tests/output.asm > /dev/null

nasm.exe tests/output.asm

# NOTE(achal): This is required because fc.exe wants paths with backward slashes.
path_for_fc="${1//\//\\}"
fc.exe "$path_for_fc" tests/output
status="$?"

rm tests/output.asm
rm tests/output

exit $status