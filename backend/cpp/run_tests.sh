#!/usr/bin/env bash
# Run C++ tests.
#
# Usage:
#   ./run_tests.sh                   build + run all suites
#   ./run_tests.sh test_robot        build + run one suite
#   ./run_tests.sh -n                run all suites (skip build)
#   ./run_tests.sh -n test_robot     run one suite (skip build)

set -euo pipefail

DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD="$DIR/build"

GREEN='\033[0;32m'
RED='\033[0;31m'
BOLD='\033[1m'
NC='\033[0m'

# Parse flags
NO_BUILD=0
FILTER=""
for arg in "$@"; do
    case "$arg" in
        -n|--no-build) NO_BUILD=1 ;;
        *) FILTER="$arg" ;;
    esac
done

# Build
if [ "$NO_BUILD" -eq 0 ]; then
    echo -e "${BOLD}Building...${NC}"
    /usr/bin/cmake -S "$DIR" -B "$BUILD" -DCMAKE_BUILD_TYPE=Debug -DCMAKE_BUILD_QUIET=ON 2>&1 \
        | grep -v '^--' || true
    /usr/bin/cmake --build "$BUILD" -- -j"$(nproc)"
    echo ""
fi

# All known test suites (unit first, then integration)
ALL_SUITES=(
    test_aisle
    test_aisle_heuristic
    test_metadata_update
    test_robot
    test_integration
    test_integration_retrieval
)

SUITES=("${ALL_SUITES[@]}")
[ -n "$FILTER" ] && SUITES=("$FILTER")

ok=0
fail=0

for t in "${SUITES[@]}"; do
    echo -e "${BOLD}━━━ $t ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
    if "$BUILD/$t"; then
        echo -e "${GREEN}✓ passed${NC}"
        ok=$((ok + 1))
    else
        echo -e "${RED}✗ failed${NC}"
        fail=$((fail + 1))
    fi
done

echo -e "${BOLD}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
total=$((ok + fail))
if [ "$fail" -eq 0 ]; then
    echo -e "${GREEN}${BOLD}All $total suites passed${NC}"
else
    echo -e "${RED}${BOLD}$fail/$total suites failed${NC}"
fi

[ "$fail" -eq 0 ]
