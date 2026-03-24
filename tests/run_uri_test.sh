#!/bin/bash

# Exit on any error
set -e

# Define paths
URI_C="uri.c"
TEST_URI_C="tests/uri_test.c"
TEMP_URI_C="tests/uri_impl.c"
TEST_EXE="tests/uri_test"

# Extract ContainsMultipleURLs from uri.c
# We look for the function start and end using sed
# This assumes the function is self-contained as described in memory
# We also prepend the necessary types for standalone compilation
echo '#include <wchar.h>' > "$TEMP_URI_C"
echo 'typedef int BOOL;' >> "$TEMP_URI_C"
echo '#define TRUE 1' >> "$TEMP_URI_C"
echo '#define FALSE 0' >> "$TEMP_URI_C"
echo '#ifndef NULL' >> "$TEMP_URI_C"
echo '#define NULL ((void*)0)' >> "$TEMP_URI_C"
echo '#endif' >> "$TEMP_URI_C"

sed -n '/BOOL ContainsMultipleURLs/,/^}/p' "$URI_C" | tr -d '\r' >> "$TEMP_URI_C"

# Compile the test
gcc -o "$TEST_EXE" "$TEST_URI_C" "$TEMP_URI_C"

# Run the test
./"$TEST_EXE"

# Cleanup
rm "$TEMP_URI_C" "$TEST_EXE"
