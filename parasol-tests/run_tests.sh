#!/bin/bash
#
# run_tests.sh - Test runner script for parasol.h integration tests
#
# Created by Sunscreen under the AGPLv3 license; see the README at the
# repository root for more information
#
# Compiles and runs tests with system clang to validate logic correctness.
# Tests rely on standard library (stdio.h) so cannot be compiled for parasol target.
# Parasol.h compatibility with parasol target is validated separately (see below).

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Test files
TEST_FILES=(
    "test_utilities.c"
    "test_array_reductions.c"
    "test_array_sort.c"
)

# Change to the parasol-tests directory
cd "$(dirname "$0")"

echo -e "${BLUE}Parasol Test Suite${NC}\n"

# Track overall results
overall_status=0

# Function to run a test
run_test() {
    local test_file=$1
    local compiler=$2
    local compiler_name=$3
    local extra_flags=$4
    local test_name="${test_file%.c}"
    local executable="${test_name}"

    echo -e "${YELLOW}Testing ${test_name} with ${compiler_name}...${NC}"

    # Compile the test
    if ! $compiler $extra_flags -o "$executable" "$test_file" 2>&1; then
        echo -e "${RED}FAIL: Compilation failed for ${test_name} with ${compiler_name}${NC}"
        return 1
    fi

    # Run the test
    if ./"$executable"; then
        echo -e "${GREEN}PASS: ${test_name} with ${compiler_name}${NC}\n"
        return 0
    else
        echo -e "${RED}FAIL: ${test_name} with ${compiler_name}${NC}\n"
        return 1
    fi
}

echo -e "${BLUE}Running Test Suite with System Clang${NC}\n"

# Test with system clang
for test_file in "${TEST_FILES[@]}"; do
    if ! run_test "$test_file" "clang" "system clang" ""; then
        overall_status=1
    fi
done

# Validate that all parasol.h functions compile for parasol target
# This forces instantiation of all functions, preventing -O2 from optimizing
# them away and hiding compilation errors
PARASOL_CLANG="../build/bin/clang"
if [ -f "$PARASOL_CLANG" ]; then
    echo -e "${BLUE}Validating parasol.h with Parasol Target${NC}\n"
    echo -e "${YELLOW}Compiling all parasol.h functions for parasol target...${NC}"

    if $PARASOL_CLANG -target parasol -O2 -c test_parasol_compilation.c -o /tmp/parasol_compilation_test.o 2>&1; then
        echo -e "${GREEN}PASS: All parasol.h functions compile for parasol target${NC}\n"
        rm -f /tmp/parasol_compilation_test.o
    else
        echo -e "${RED}FAIL: Some parasol.h functions do not compile for parasol target${NC}\n"
        echo -e "${RED}See errors above for details on which functions failed${NC}\n"
        overall_status=1
    fi
else
    echo -e "${YELLOW}Note: Built parasol clang not found at ${PARASOL_CLANG}${NC}"
    echo -e "${YELLOW}Skipping parasol target validation. Build LLVM first with ./compile-llvm.sh${NC}\n"
fi

# Clean up executables
echo -e "${BLUE}Cleaning up test executables...${NC}"
for test_file in "${TEST_FILES[@]}"; do
    test_name="${test_file%.c}"
    rm -f "$test_name"
done

# Final status
echo ""
if [ $overall_status -eq 0 ]; then
    echo -e "${GREEN}All Tests Passed${NC}"
else
    echo -e "${RED}Some Tests Failed${NC}"
fi

exit $overall_status
