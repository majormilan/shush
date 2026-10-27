#!/usr/bin/env bash
# Top-level test orchestrator: unit (fast, no subprocess) -> integration
# (real binary over a plain pipe) -> interactive (real binary over a PTY,
# for readline/completion). Run via `make test`. Exits nonzero if anything
# failed.
set -u

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
overall=0

echo "=== unit tests ==="
bash "$ROOT/tests/unit/run_unit_tests.sh"
[ $? -eq 0 ] || overall=1

echo
echo "=== integration tests ==="
python3 "$ROOT/tests/integration/test_integration.py"
[ $? -eq 0 ] || overall=1

echo
echo "=== differential tests (against dash) ==="
python3 "$ROOT/tests/differential/test_differential.py"
[ $? -eq 0 ] || overall=1

echo
echo "=== interactive tests ==="
python3 "$ROOT/tests/interactive/test_interactive.py"
[ $? -eq 0 ] || overall=1

echo
if [ $overall -eq 0 ]; then
    echo "ALL TEST SUITES PASSED"
else
    echo "ONE OR MORE TEST SUITES FAILED"
fi
exit $overall
