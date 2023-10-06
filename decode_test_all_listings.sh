#!/bin/bash

# NOTE(achal): This is just for quick and collective testing of all the listings.
# It will not show you any specific errors. For that run the test_listing.sh or
# test_listing.bat with the listing you want.

# TODO(achal): I'll probably get this automatically in the future.
assembled_listings=(
    "listing_0037_single_register_mov"
    "listing_0038_many_register_mov"
    "listing_0039_more_movs"
    "listing_0040_challenge_movs"
    "listing_0041_add_sub_cmp_jnz"
    "listing_0042_completionist_decode"
    "listing_0043_immediate_movs"
	"listing_0044_register_movs"
	"listing_0045_challenge_register_movs"
	"listing_0046_add_sub_cmp"
	"listing_0047_challenge_flags"
	"listing_0048_ip_register"
	"listing_0049_conditional_jumps"
	"listing_0050_challenge_jumps"
	"listing_0051_memory_mov"
	"listing_0052_memory_add_loop"
	"listing_0053_add_loop_challenge"
	"listing_0054_draw_rectangle"
	"listing_0055_challenge_rectangle"
	"listing_0056_estimating_cycles"
	"listing_0057_challenge_cycles"
)

RED='\033[0;31m'
GREEN='\033[0;32m'
NC='\033[0m' # No Color

for file in "${assembled_listings[@]}"; do
	./decode_test_listing.sh "tests/$file" > /dev/null 2>&1
	status=$?
	if [ $status -eq 0 ]; then
		echo -e "$file ${GREEN}[Passed]${NC}"
	else
		echo -e "$file ${RED}[Failed]${NC}"
	fi
done
