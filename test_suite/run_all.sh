#!/bin/bash
# ===========================================================================
# Flux Test Runner
# Runs all .flux test files and reports pass/fail status.
#
# Usage:
#   ./test_suite/run_all.sh               Run all tests
#   ./test_suite/run_all.sh basics/        Run tests in a category
#   ./test_suite/run_all.sh --verbose      Show full output on failure
# ===========================================================================

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
FLUX="$PROJECT_DIR/build/flux"

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[0;33m'
CYAN='\033[0;36m'
BOLD='\033[1m'
RESET='\033[0m'

# Counters
PASS=0
FAIL=0
SKIP=0
TOTAL=0

# Options
VERBOSE=false
FILTER=""

# Parse arguments
for arg in "$@"; do
    case "$arg" in
        --verbose|-v)
            VERBOSE=true
            ;;
        --help|-h)
            echo "Usage: $0 [OPTIONS] [FILTER]"
            echo ""
            echo "Options:"
            echo "  --verbose, -v    Show full output on failure"
            echo "  --help, -h       Show this help message"
            echo ""
            echo "Filter:"
            echo "  Provide a path fragment to filter tests (e.g. 'basics/' or 'oop/')"
            exit 0
            ;;
        *)
            FILTER="$arg"
            ;;
    esac
done

# Check that the binary exists
if [[ ! -x "$FLUX" ]]; then
    echo -e "${RED}Error: Flux binary not found at $FLUX${RESET}"
    echo "Run 'make' first to build Flux."
    exit 1
fi

echo -e "${BOLD}${CYAN}═══════════════════════════════════════════════${RESET}"
echo -e "${BOLD}${CYAN}          Flux Test Suite Runner               ${RESET}"
echo -e "${BOLD}${CYAN}═══════════════════════════════════════════════${RESET}"
echo ""

# Find all .flux test files
while IFS= read -r -d '' testfile; do
    # Apply filter if specified
    if [[ -n "$FILTER" ]] && [[ "$testfile" != *"$FILTER"* ]]; then
        continue
    fi

    TOTAL=$((TOTAL + 1))

    # Get relative path for display
    relpath="${testfile#$SCRIPT_DIR/}"

    # Check for .expected file (if it exists, compare output)
    expected_file="${testfile%.flux}.expected"

    # Run the test with a timeout
    output=$("$FLUX" run "$testfile" 2>&1) && exit_code=$? || exit_code=$?

    if [[ $exit_code -eq 0 ]]; then
        # If there's an expected output file, compare
        if [[ -f "$expected_file" ]]; then
            expected=$(cat "$expected_file")
            if [[ "$output" == "$expected" ]]; then
                echo -e "  ${GREEN}✓${RESET} $relpath"
                PASS=$((PASS + 1))
            else
                echo -e "  ${RED}✗${RESET} $relpath ${YELLOW}(output mismatch)${RESET}"
                FAIL=$((FAIL + 1))
                if $VERBOSE; then
                    echo -e "    ${YELLOW}Expected:${RESET}"
                    echo "$expected" | sed 's/^/      /'
                    echo -e "    ${YELLOW}Got:${RESET}"
                    echo "$output" | sed 's/^/      /'
                fi
            fi
        else
            # No .expected file — pass if exit code is 0
            echo -e "  ${GREEN}✓${RESET} $relpath"
            PASS=$((PASS + 1))
        fi
    else
        echo -e "  ${RED}✗${RESET} $relpath ${RED}(exit code $exit_code)${RESET}"
        FAIL=$((FAIL + 1))
        if $VERBOSE; then
            echo -e "    ${RED}Output:${RESET}"
            echo "$output" | sed 's/^/      /'
        fi
    fi

done < <(find "$SCRIPT_DIR" -name "*.flux" -type f -print0 | sort -z)

# Summary
echo ""
echo -e "${BOLD}${CYAN}═══════════════════════════════════════════════${RESET}"
echo -e "  Total:   $TOTAL"
echo -e "  ${GREEN}Passed:  $PASS${RESET}"
if [[ $FAIL -gt 0 ]]; then
    echo -e "  ${RED}Failed:  $FAIL${RESET}"
else
    echo -e "  Failed:  0"
fi
if [[ $SKIP -gt 0 ]]; then
    echo -e "  ${YELLOW}Skipped: $SKIP${RESET}"
fi
echo -e "${BOLD}${CYAN}═══════════════════════════════════════════════${RESET}"

# Exit with failure if any tests failed
if [[ $FAIL -gt 0 ]]; then
    exit 1
fi

exit 0
